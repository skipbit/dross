#include "dross/thread/thread.h"

#include "dross/thread/runloop.h"
#include "thread/deadline.h"
#include "thread/native.h"
#include "thread/runloop_access.h"
#include "thread/start_thread.h"
#include "thread/teardown.h"
#include "thread/thread_access.h"
#include "thread/time_source.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <expected>
#include <mutex>
#include <new>
#include <optional>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace dross {

class thread::storage final {
public:
    explicit storage(std::shared_ptr<const time_source> source = time_source::steady());
    ~storage();

    storage(const storage& other) = delete;
    storage& operator=(const storage& other) = delete;

    static std::expected<std::shared_ptr<storage>, error> start_loop_thread(std::shared_ptr<const time_source> source);
    static std::expected<std::shared_ptr<storage>, error> start_one_shot_thread(std::function<void()> body,
                                                                                std::shared_ptr<const time_source> source);
    static std::shared_ptr<storage> main_thread_storage();
    static std::shared_ptr<storage> current_thread_storage();
    static std::vector<std::shared_ptr<storage>> all();

    bool perform(std::function<void()> task);

    void quit();
    void cancel();

    void join();
    bool join_for(std::chrono::milliseconds timeout);

    bool running() const;
    bool finished() const;
    bool stop_requested() const;
    bool is_current() const;

    std::optional<std::uint64_t> native_id() const;

    // The thread's loop. Set before the thread is published and never
    // reassigned, so it is read without _mutex.
    const std::optional<runloop>& loop() const noexcept;

private:
    // Installed in the calling thread's slot below, so thread::current()
    // returns the same storage on every call from that thread. Its
    // destructor is the one place a thread's end is noticed, whether dross
    // started the thread or only adopted it.
    class current_holder final {
    public:
        ~current_holder();

        void install(std::shared_ptr<storage> store);
        std::shared_ptr<storage> peek() const;

    private:
        std::shared_ptr<storage> _store;
    };

    static std::expected<std::shared_ptr<storage>, error> start(std::function<void()> body, bool one_shot, std::shared_ptr<const time_source> source);
    static std::shared_ptr<storage> make_adopted();
    static void run_on_new_thread(std::shared_ptr<storage> self, std::function<void()> body, bool one_shot);

    static current_holder& this_thread_holder();
    static std::mutex& registry_mutex();
    static std::vector<std::shared_ptr<storage>>& registry();
    static void register_self(const std::shared_ptr<storage>& self);

    void set_native_id(std::uint64_t id);
    void ensure_native_id_known();

    void finish();
    void deregister();

    // Waits for the OS thread itself to end, once _finished is set. Only
    // the first caller joins it; the others wait for that join to finish.
    // A thread dross did not start has nothing to join. Called with
    // lock held on _mutex, which is held again on return.
    void join_os_thread(std::unique_lock<std::mutex>& lock);

    // How join() and join_for() read the time and wait. Set once, at
    // construction, and never reassigned, so it is read without _mutex.
    const std::shared_ptr<const time_source> _source;

    mutable std::mutex _mutex;
    std::condition_variable _ready_cv;
    std::condition_variable _done_cv;
    bool _published{ false };
    bool _finished{ false };

    // Set only on the record main_thread_storage() builds. is_current()
    // branches on it instead of on whether the calling thread has been
    // adopted, since a thread can reach this record through thread::all()
    // without ever going through current_thread_storage() itself.
    bool _is_main{ false };

    std::optional<std::uint64_t> _native_id;
    std::optional<runloop> _loop;

    // The OS thread, for a thread dross started. Joined by the first
    // join() or join_for() to see the thread finished, and otherwise
    // detached when the last handle goes.
    std::thread _os_thread;
    std::once_flag _join_once;

    // Set with _os_thread and never cleared. join_os_thread() reads this
    // rather than _os_thread.joinable(), which the join itself changes
    // outside _mutex.
    bool _os_thread_kept{ false };

    // A plain flag rather than std::stop_source: <stop_token> is not in the
    // standard library Apple Clang ships, and nothing here needs more than
    // setting a flag and reading it.
    std::atomic<bool> _stop_requested{ false };
};

thread::storage::storage(std::shared_ptr<const time_source> source)
    : _source{ std::move(source) }
{
}

thread::storage::~storage()
{
    // May run on the thread itself, as its own bookkeeping is torn down,
    // where joining would wait on itself.
    if (_os_thread.joinable()) {
        _os_thread.detach();
    }
}

thread::storage::current_holder::~current_holder()
{
    if (_store) {
        _store->finish();
    }
    teardown::torn_down<storage>() = true;
}

void thread::storage::current_holder::install(std::shared_ptr<storage> store)
{
    _store = std::move(store);
}

std::shared_ptr<thread::storage> thread::storage::current_holder::peek() const
{
    return _store;
}

thread::storage::current_holder& thread::storage::this_thread_holder()
{
    thread_local current_holder holder;
    return holder;
}

std::mutex& thread::storage::registry_mutex()
{
    // Never destroyed, alongside the registry it guards; see registry().
    static std::mutex* const the_mutex = new std::mutex();
    return *the_mutex;
}

std::vector<std::shared_ptr<thread::storage>>& thread::storage::registry()
{
    // Never destroyed. A thread still registered when the program ends
    // would otherwise reach a destroyed container, and thread storage is
    // torn down before anything with static storage duration.
    static std::vector<std::shared_ptr<storage>>* const the_registry = new std::vector<std::shared_ptr<storage>>();
    return *the_registry;
}

void thread::storage::register_self(const std::shared_ptr<storage>& self)
{
    const std::lock_guard<std::mutex> guard{ registry_mutex() };
    registry().push_back(self);
}

void thread::storage::deregister()
{
    const std::lock_guard<std::mutex> guard{ registry_mutex() };
    std::erase_if(registry(), [this](const std::shared_ptr<storage>& entry) {
        return (entry.get() == this);
    });
}

void thread::storage::finish()
{
    // Out of the registry before anyone can see the thread as finished. The
    // other order lets a caller return from join_for() and still find the
    // thread in thread::all(), which is the opposite of what both promise.
    deregister();

    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        _finished = true;
    }
    _done_cv.notify_all();
}

std::expected<std::shared_ptr<thread::storage>, error> thread::storage::start(std::function<void()> body, bool one_shot, std::shared_ptr<const time_source> source)
{
    // Running out of memory is reported as start_thread() reports it.
    try {
        auto self = std::make_shared<storage>(std::move(source));

        // Held from before the thread starts, so the new thread waits at its
        // first use of _mutex until _os_thread is set. join() through a
        // handle from thread::all() then never finds it unset.
        std::unique_lock<std::mutex> lock{ self->_mutex };

        auto runner = start_thread([self, body = std::move(body), one_shot]() mutable {
            run_on_new_thread(std::move(self), std::move(body), one_shot);
        });
        if (! runner) {
            return std::unexpected(std::move(runner).error());
        }

        self->_os_thread = std::move(*runner);
        self->_os_thread_kept = true;
        self->_ready_cv.wait(lock, [&self]() {
            return self->_published;
        });

        return self;
    } catch (const std::bad_alloc&) {
        return std::unexpected(error{ std::errc::not_enough_memory });
    }
}

void thread::storage::run_on_new_thread(std::shared_ptr<storage> self, std::function<void()> body, bool one_shot)
{
    // Installed before anything else, so a call to thread::current() from
    // inside body or the loop returns this exact storage rather than
    // adopting a fresh one.
    this_thread_holder().install(self);
    self->set_native_id(native::thread_id());
    self->_loop = runloop::current();
    register_self(self);

    {
        const std::lock_guard<std::mutex> guard{ self->_mutex };
        self->_published = true;
    }
    self->_ready_cv.notify_all();

    // Called directly rather than queued, so quit() never drops body and a
    // run() inside body is the outermost one. Tasks performed while body
    // runs wait on the loop until it returns.
    if (one_shot && body) {
        body();
    }

    // Once the loop stops it refuses tasks, so none is accepted that would
    // never run, and it drops what quit() left queued.
    if (one_shot) {
        runloop_access::run_to_end_when_idle(*self->_loop);
    } else {
        runloop_access::run_to_end(*self->_loop);
    }

    // The thread ends here. this_thread_holder()'s destructor, running as
    // this OS thread's storage is torn down, is what marks self finished
    // and removes it from the registry.
}

std::expected<std::shared_ptr<thread::storage>, error> thread::storage::start_loop_thread(std::shared_ptr<const time_source> source)
{
    return start(nullptr, false, std::move(source));
}

std::expected<std::shared_ptr<thread::storage>, error> thread::storage::start_one_shot_thread(std::function<void()> body,
                                                                                              std::shared_ptr<const time_source> source)
{
    return start(std::move(body), true, std::move(source));
}

std::shared_ptr<thread::storage> thread::storage::make_adopted()
{
    auto self = std::make_shared<storage>();
    self->set_native_id(native::thread_id());
    self->_loop = runloop::current();
    register_self(self);
    return self;
}

std::shared_ptr<thread::storage> thread::storage::main_thread_storage()
{
    // Never destroyed, for the same reason as runloop::storage::main_loop():
    // a thread still running at process exit must not reach a destroyed
    // record.
    //
    // Built lazily, on whichever thread asks thread::main() or
    // thread::current() first. Which thread that is no longer decides whose
    // id or loop this holds: native::main_thread_id() and runloop::main() are
    // both defined to answer correctly regardless of the calling thread, the
    // former possibly empty (see native_id()'s doc comment), the latter
    // never.
    static const std::shared_ptr<storage>* const the_main = new std::shared_ptr<storage>{ []() {
        auto self = std::make_shared<storage>();
        self->_is_main = true;
        if (const auto id = native::main_thread_id()) {
            self->set_native_id(*id);
        }
        self->_loop = runloop::main();
        register_self(self);
        return self;
    }() };
    return *the_main;
}

std::shared_ptr<thread::storage> thread::storage::current_thread_storage()
{
    if (teardown::torn_down<storage>()) {
        return teardown::finished_placeholder(&storage::finish);
    }

    auto& holder = this_thread_holder();
    if (auto existing = holder.peek()) {
        return existing;
    }

    std::shared_ptr<storage> self;
    if (native::on_main_thread()) {
        self = main_thread_storage();
        // Fills in what main_thread_storage() could not, on a platform
        // where the id can only be read from the main thread itself.
        self->ensure_native_id_known();
    } else {
        self = make_adopted();
    }

    holder.install(self);
    return self;
}

std::vector<std::shared_ptr<thread::storage>> thread::storage::all()
{
    const std::lock_guard<std::mutex> guard{ registry_mutex() };
    return registry();
}

bool thread::storage::perform(std::function<void()> task)
{
    {
        // Checked first and on its own: destruction order between this
        // storage's holder and the run loop's is not guaranteed, so a
        // thread this module already considers finished may still have a
        // loop willing to queue the task. This thread's own promise not to
        // run it must hold regardless of what the loop still thinks.
        const std::lock_guard<std::mutex> guard{ _mutex };
        if (_finished) {
            return false;
        }
    }

    // Only the finished placeholder has no loop, and it returned above.
    if (! _loop) {
        return false;
    }
    return _loop->perform(std::move(task));
}

const std::optional<runloop>& thread::storage::loop() const noexcept
{
    return _loop;
}

void thread::storage::quit()
{
    if (_loop) {
        _loop->quit();
    }
}

void thread::storage::cancel()
{
    _stop_requested.store(true);
}

bool thread::storage::stop_requested() const
{
    return _stop_requested.load();
}

bool thread::storage::running() const
{
    const std::lock_guard<std::mutex> guard{ _mutex };
    return (! _finished);
}

bool thread::storage::finished() const
{
    const std::lock_guard<std::mutex> guard{ _mutex };
    return _finished;
}

bool thread::storage::is_current() const
{
    // Does not go through current_thread_storage(): asking whether the
    // caller is this thread must not register the caller or build it a run
    // loop as a side effect, and a thread can reach its own record here
    // through thread::all() without ever having been adopted.
    if (_is_main) {
        return native::is_main_thread();
    }

    const std::lock_guard<std::mutex> guard{ _mutex };
    return (_native_id && (*_native_id == native::thread_id()));
}

void thread::storage::set_native_id(std::uint64_t id)
{
    const std::lock_guard<std::mutex> guard{ _mutex };
    _native_id = id;
}

void thread::storage::ensure_native_id_known()
{
    const std::lock_guard<std::mutex> guard{ _mutex };
    if (! _native_id) {
        _native_id = native::thread_id();
    }
}

std::optional<std::uint64_t> thread::storage::native_id() const
{
    const std::lock_guard<std::mutex> guard{ _mutex };
    return _native_id;
}

void thread::storage::join()
{
    // Waiting on itself can never succeed: the flag can only become true
    // after this call returns.
    if (is_current()) {
        return;
    }

    std::unique_lock<std::mutex> lock{ _mutex };
    while (! _finished) {
        _source->wait(lock, _done_cv);
    }
    join_os_thread(lock);
}

bool thread::storage::join_for(std::chrono::milliseconds timeout)
{
    if (is_current()) {
        return finished();
    }

    // wait_for() would hand steady_clock::now() + timeout to the clock
    // unclamped; for a timeout as large as milliseconds::max() that
    // overflows.
    const auto deadline = deadline::after(_source->now(), timeout);

    std::unique_lock<std::mutex> lock{ _mutex };
    while ((! _finished) && (_source->now() < deadline)) {
        _source->wait_until(lock, _done_cv, deadline);
    }
    if (! _finished) {
        return false;
    }
    // Not bounded by timeout: once _finished is set, the OS thread is
    // only tearing down what is left of it.
    join_os_thread(lock);
    return true;
}

void thread::storage::join_os_thread(std::unique_lock<std::mutex>& lock)
{
    if (! _os_thread_kept) {
        return;
    }

    lock.unlock();
    std::call_once(_join_once, [this]() {
        _os_thread.join();
    });
    lock.lock();
}

thread::thread(std::shared_ptr<storage> store) noexcept
    : _store{ std::move(store) }
{
}

std::expected<thread, error> thread::create()
{
    return thread_access::start(time_source::steady());
}

std::expected<thread, error> thread::create(std::function<void()> body)
{
    return thread_access::start(std::move(body), time_source::steady());
}

thread thread::main()
{
    // Asked on the main thread, this goes the long way round so the
    // thread's holder is put in place, the same reason runloop::main() does.
    if (native::on_main_thread()) {
        return current();
    }

    return thread{ storage::main_thread_storage() };
}

thread thread::current()
{
    return thread{ storage::current_thread_storage() };
}

std::vector<thread> thread::all()
{
    auto stores = storage::all();

    std::vector<thread> result;
    result.reserve(stores.size());
    for (auto& store : stores) {
        result.emplace_back(thread{ std::move(store) });
    }

    return result;
}

thread::thread(const thread& other) = default;

thread::~thread() = default;

thread& thread::operator=(const thread& other) = default;

bool thread::perform(std::function<void()> task)
{
    return _store->perform(std::move(task));
}

void thread::quit()
{
    _store->quit();
}

void thread::cancel()
{
    _store->cancel();
}

void thread::join()
{
    _store->join();
}

bool thread::join_for(std::chrono::milliseconds timeout)
{
    return _store->join_for(timeout);
}

bool thread::running() const
{
    return _store->running();
}

bool thread::finished() const
{
    return _store->finished();
}

bool thread::stop_requested() const
{
    return _store->stop_requested();
}

bool thread::is_current() const
{
    return _store->is_current();
}

std::optional<std::uint64_t> thread::native_id() const
{
    return _store->native_id();
}

bool thread::operator==(const thread& other) const noexcept
{
    return _store == other._store;
}

std::expected<thread, error> thread_access::start(std::shared_ptr<const time_source> source)
{
    return thread::storage::start_loop_thread(std::move(source)).transform([](std::shared_ptr<thread::storage> store) {
        return thread{ std::move(store) };
    });
}

std::expected<thread, error> thread_access::start(std::function<void()> body, std::shared_ptr<const time_source> source)
{
    return thread::storage::start_one_shot_thread(std::move(body), std::move(source)).transform([](std::shared_ptr<thread::storage> store) {
        return thread{ std::move(store) };
    });
}

runloop thread_access::loop(const thread& which)
{
    return *which._store->loop();
}

}  // namespace dross
