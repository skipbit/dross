#include "dross/thread/thread.h"

#include "dross/thread/runloop.h"
#include "thread/deadline.h"
#include "thread/native.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace dross {

class thread::storage final {
public:
    static std::shared_ptr<storage> start_loop_thread();
    static std::shared_ptr<storage> start_one_shot_thread(std::function<void()> body);
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

private:
    // Installed in the calling thread's slot below, so current_thread()
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

    static std::shared_ptr<storage> start(std::function<void()> body, bool run_loop);
    static std::shared_ptr<storage> make_adopted();
    static void run_on_new_thread(std::shared_ptr<storage> self, std::function<void()> body, bool run_loop);

    static current_holder& this_thread_holder();
    static bool& torn_down_flag();
    static std::shared_ptr<storage> finished_placeholder();
    static std::mutex& registry_mutex();
    static std::vector<std::shared_ptr<storage>>& registry();
    static void register_self(const std::shared_ptr<storage>& self);

    void set_native_id(std::uint64_t id);
    void ensure_native_id_known();

    void finish();
    void deregister();

    mutable std::mutex _mutex;
    std::condition_variable _ready_cv;
    std::condition_variable _done_cv;
    bool _published{ false };
    bool _finished{ false };

    // Set only on the record main_thread_storage() builds. is_current()
    // branches on it instead of on whether the calling thread has been
    // adopted, since a thread can reach this record through all_threads()
    // without ever going through current_thread_storage() itself.
    bool _is_main{ false };

    std::optional<std::uint64_t> _native_id;
    std::optional<runloop> _loop;

    // A plain flag rather than std::stop_source: <stop_token> is not in the
    // standard library Apple Clang ships, and nothing here needs more than
    // setting a flag and reading it.
    std::atomic<bool> _stop_requested{ false };
};

thread::storage::current_holder::~current_holder()
{
    if (_store) {
        _store->finish();
    }
    torn_down_flag() = true;
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

bool& thread::storage::torn_down_flag()
{
    // Trivially destructible, so it has no destructor of its own and stays
    // readable no matter what order thread-locals on this thread are torn
    // down in; see runloop::storage::for_current_thread() for the same
    // technique and current_thread()'s doc comment for what it is for.
    thread_local bool torn_down = false;
    return torn_down;
}

std::shared_ptr<thread::storage> thread::storage::finished_placeholder()
{
    // Shared by every thread that reaches it, rather than kept thread_local:
    // nothing is ever installed on it, so nothing needs it to be distinct
    // per thread. Never destroyed, for the same reason as
    // main_thread_storage().
    static const std::shared_ptr<storage>* const the_thread = []() {
        auto self = std::make_shared<storage>();
        self->finish();
        return new std::shared_ptr<storage>{ std::move(self) };
    }();
    return *the_thread;
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
        return entry.get() == this;
    });
}

void thread::storage::finish()
{
    // Out of the registry before anyone can see the thread as finished. The
    // other order lets a caller return from join_for() and still find the
    // thread in all_threads(), which is the opposite of what both promise.
    deregister();

    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        _finished = true;
    }
    _done_cv.notify_all();
}

std::shared_ptr<thread::storage> thread::storage::start(std::function<void()> body, bool run_loop)
{
    auto self = std::make_shared<storage>();

    std::thread runner{ [self, body = std::move(body), run_loop]() mutable {
        run_on_new_thread(std::move(self), std::move(body), run_loop);
    } };
    runner.detach();

    std::unique_lock<std::mutex> lock{ self->_mutex };
    self->_ready_cv.wait(lock, [&self]() {
        return self->_published;
    });

    return self;
}

void thread::storage::run_on_new_thread(std::shared_ptr<storage> self, std::function<void()> body, bool run_loop)
{
    // Installed before anything else, so a call to current_thread() from
    // inside body or the loop returns this exact storage rather than
    // adopting a fresh one.
    this_thread_holder().install(self);
    self->set_native_id(native::thread_id());
    if (run_loop) {
        self->_loop = current_runloop();
    }
    register_self(self);

    {
        const std::lock_guard<std::mutex> guard{ self->_mutex };
        self->_published = true;
    }
    self->_ready_cv.notify_all();

    if (run_loop) {
        self->_loop->run();
    } else if (body) {
        body();
    }

    // The thread ends here. this_thread_holder()'s destructor, running as
    // this OS thread's storage is torn down, is what marks self finished
    // and removes it from the registry.
}

std::shared_ptr<thread::storage> thread::storage::start_loop_thread()
{
    return start(nullptr, true);
}

std::shared_ptr<thread::storage> thread::storage::start_one_shot_thread(std::function<void()> body)
{
    return start(std::move(body), false);
}

std::shared_ptr<thread::storage> thread::storage::make_adopted()
{
    auto self = std::make_shared<storage>();
    self->set_native_id(native::thread_id());
    self->_loop = current_runloop();
    register_self(self);
    return self;
}

std::shared_ptr<thread::storage> thread::storage::main_thread_storage()
{
    // Never destroyed, for the same reason as runloop::storage::main_loop():
    // a thread still running at process exit must not reach a destroyed
    // record.
    //
    // Built lazily, on whichever thread asks main_thread() or
    // current_thread() first. Which thread that is no longer decides whose
    // id or loop this holds: native::main_thread_id() and main_runloop() are
    // both defined to answer correctly regardless of the calling thread, the
    // former possibly empty (see native_id()'s doc comment), the latter
    // never.
    static const std::shared_ptr<storage>* const the_main = new std::shared_ptr<storage>{ []() {
        auto self = std::make_shared<storage>();
        self->_is_main = true;
        if (const auto id = native::main_thread_id()) {
            self->set_native_id(*id);
        }
        self->_loop = main_runloop();
        register_self(self);
        return self;
    }() };
    return *the_main;
}

std::shared_ptr<thread::storage> thread::storage::current_thread_storage()
{
    if (torn_down_flag()) {
        return finished_placeholder();
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

    if (! _loop) {
        return false;
    }
    return _loop->perform(std::move(task));
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
    return ! _finished;
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
    // through all_threads() without ever having been adopted.
    if (_is_main) {
        return native::is_main_thread();
    }

    const std::lock_guard<std::mutex> guard{ _mutex };
    return _native_id && *_native_id == native::thread_id();
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
    _done_cv.wait(lock, [this]() {
        return _finished;
    });
}

bool thread::storage::join_for(std::chrono::milliseconds timeout)
{
    if (is_current()) {
        return finished();
    }

    if (timeout <= std::chrono::milliseconds::zero()) {
        const std::lock_guard<std::mutex> guard{ _mutex };
        return _finished;
    }

    // wait_for() would hand steady_clock::now() + timeout to the clock
    // unclamped; for a timeout as large as milliseconds::max() that
    // overflows.
    const auto deadline = deadline::after(std::chrono::steady_clock::now(), timeout);

    std::unique_lock<std::mutex> lock{ _mutex };
    return _done_cv.wait_until(lock, deadline, [this]() {
        return _finished;
    });
}

thread::thread(std::shared_ptr<storage> store) noexcept
    : _store{ std::move(store) }
{
}

thread::thread()
    : thread{ storage::start_loop_thread() }
{
}

thread::thread(std::function<void()> body)
    : thread{ storage::start_one_shot_thread(std::move(body)) }
{
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

bool thread::is_current_thread() const
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

thread main_thread()
{
    // Asked on the main thread, this goes the long way round so the
    // thread's holder is put in place, the same reason main_runloop() does.
    if (native::on_main_thread()) {
        return current_thread();
    }

    return thread{ thread::storage::main_thread_storage() };
}

thread current_thread()
{
    return thread{ thread::storage::current_thread_storage() };
}

std::vector<thread> all_threads()
{
    auto stores = thread::storage::all();

    std::vector<thread> result;
    result.reserve(stores.size());
    for (auto& store : stores) {
        result.emplace_back(thread{ std::move(store) });
    }

    return result;
}

}  // namespace dross
