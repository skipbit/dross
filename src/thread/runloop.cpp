#include "dross/thread/runloop.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>

namespace dross {

namespace {

// Waiting without a deadline. A time point rather than a separate flag, so
// the waiting logic has one shape.
constexpr auto kNoDeadline = std::chrono::steady_clock::time_point::max();

}

class runloop::storage final {
public:
    // What the main thread is, decided once. The loop is held here so that
    // main_runloop() can hand it out to a thread that has no other way to
    // reach it.
    struct record final {
        std::thread::id id;
        std::shared_ptr<storage> loop;
    };

    static record& main_record();
    static std::shared_ptr<storage> for_current_thread();

    bool enqueue(std::function<void()>&& task);

    std::size_t run();
    bool run_one();
    std::size_t run_pending();
    std::size_t run_for(std::chrono::milliseconds timeout);

    void quit();
    void clear();
    void finish();

    std::size_t pending_count() const;
    bool is_running() const;

private:
    // Restores what is_running() answered before, so a loop run from inside
    // a task leaves the outer run's answer standing when it returns.
    class running_mark final {
    public:
        explicit running_mark(storage& owner);
        ~running_mark();

        running_mark(const running_mark& other) = delete;
        running_mark& operator=(const running_mark& other) = delete;

    private:
        storage& _owner;
        bool _previous;
    };

    // Takes the next task, waiting until the deadline. Returns false when
    // quit() was seen, which consumes the request, or when the deadline
    // passed with nothing to run.
    bool next(std::unique_lock<std::mutex>& lock, std::function<void()>& out,
              std::chrono::steady_clock::time_point deadline);

    std::size_t run_until(std::chrono::steady_clock::time_point deadline);

    mutable std::mutex _mutex;
    std::condition_variable _wake;
    std::deque<std::function<void()>> _pending;
    bool _running{false};
    bool _quit{false};
    bool _finished{false};
};

runloop::storage::running_mark::running_mark(storage& owner) : _owner{owner}
{
    const std::lock_guard<std::mutex> guard{_owner._mutex};
    _previous = _owner._running;
    _owner._running = true;
}

runloop::storage::running_mark::~running_mark()
{
    const std::lock_guard<std::mutex> guard{_owner._mutex};
    _owner._running = _previous;
}

runloop::storage::record& runloop::storage::main_record()
{
    // Never destroyed. A thread still running when the program ends would
    // otherwise reach a destroyed loop, and thread storage is destroyed
    // before anything with static storage duration. The static pointer keeps
    // it reachable, so a leak checker does not report it.
    static record* const the_record =
        new record{std::this_thread::get_id(), std::make_shared<storage>()};
    return *the_record;
}

std::shared_ptr<runloop::storage> runloop::storage::for_current_thread()
{
    // Destroyed when the thread ends, which is how a loop learns that no
    // task of its will ever run again. This works for threads dross did not
    // start.
    struct holder final {
        std::shared_ptr<storage> loop;

        ~holder() { loop->finish(); }
    };

    static thread_local const holder current{
        std::this_thread::get_id() == main_record().id ? main_record().loop
                                                       : std::make_shared<storage>()};

    return current.loop;
}

bool runloop::storage::enqueue(std::function<void()>&& task)
{
    if (!task) {
        return false;
    }

    {
        const std::lock_guard<std::mutex> guard{_mutex};
        if (_finished) {
            return false;
        }
        _pending.push_back(std::move(task));
    }

    _wake.notify_one();
    return true;
}

bool runloop::storage::next(std::unique_lock<std::mutex>& lock, std::function<void()>& out,
                            std::chrono::steady_clock::time_point deadline)
{
    while (true) {
        if (_quit) {
            _quit = false;
            return false;
        }

        if (!_pending.empty()) {
            out = std::move(_pending.front());
            _pending.pop_front();
            return true;
        }

        if (deadline == kNoDeadline) {
            _wake.wait(lock);
            continue;
        }

        if (_wake.wait_until(lock, deadline) == std::cv_status::timeout) {
            // One last look: the deadline and a task can land together.
            if (_quit) {
                _quit = false;
            } else if (!_pending.empty()) {
                out = std::move(_pending.front());
                _pending.pop_front();
                return true;
            }
            return false;
        }
    }
}

std::size_t runloop::storage::run_until(std::chrono::steady_clock::time_point deadline)
{
    const running_mark mark{*this};

    std::size_t ran = 0;
    std::unique_lock<std::mutex> lock{_mutex};
    std::function<void()> task;

    while (next(lock, task, deadline)) {
        lock.unlock();
        task();
        task = nullptr;  // release the captures outside the lock
        ++ran;
        lock.lock();
    }

    return ran;
}

std::size_t runloop::storage::run()
{
    return run_until(kNoDeadline);
}

std::size_t runloop::storage::run_for(std::chrono::milliseconds timeout)
{
    return run_until(std::chrono::steady_clock::now() + timeout);
}

bool runloop::storage::run_one()
{
    const running_mark mark{*this};

    std::unique_lock<std::mutex> lock{_mutex};
    std::function<void()> task;
    if (!next(lock, task, kNoDeadline)) {
        return false;
    }

    lock.unlock();
    task();
    return true;
}

std::size_t runloop::storage::run_pending()
{
    const running_mark mark{*this};

    std::size_t ran = 0;
    std::unique_lock<std::mutex> lock{_mutex};

    // Only what is already queued, so a task that posts another does not
    // keep this call going. A pending quit() is left alone: it belongs to
    // the next run(), not to this drain.
    for (std::size_t budget = _pending.size(); budget > 0 && !_pending.empty(); --budget) {
        std::function<void()> task = std::move(_pending.front());
        _pending.pop_front();

        lock.unlock();
        task();
        task = nullptr;  // release the captures outside the lock
        ++ran;
        lock.lock();
    }

    return ran;
}

void runloop::storage::quit()
{
    {
        const std::lock_guard<std::mutex> guard{_mutex};
        _quit = true;
    }
    _wake.notify_all();
}

void runloop::storage::clear()
{
    std::deque<std::function<void()>> discarded;
    {
        const std::lock_guard<std::mutex> guard{_mutex};
        discarded.swap(_pending);
    }
    // Destroyed outside the lock: a task's captures run their own code.
}

void runloop::storage::finish()
{
    {
        const std::lock_guard<std::mutex> guard{_mutex};
        _finished = true;
    }
    _wake.notify_all();
}

std::size_t runloop::storage::pending_count() const
{
    const std::lock_guard<std::mutex> guard{_mutex};
    return _pending.size();
}

bool runloop::storage::is_running() const
{
    const std::lock_guard<std::mutex> guard{_mutex};
    return _running;
}

runloop::runloop(std::shared_ptr<storage> store) noexcept : _store{std::move(store)}
{
}

runloop::runloop(const runloop& other) = default;

runloop::~runloop() = default;

runloop& runloop::operator=(const runloop& other) = default;

bool runloop::perform(std::function<void()> task)
{
    return _store->enqueue(std::move(task));
}

std::size_t runloop::run()
{
    return _store->run();
}

bool runloop::run_one()
{
    return _store->run_one();
}

std::size_t runloop::run_pending()
{
    return _store->run_pending();
}

std::size_t runloop::run_for(std::chrono::milliseconds timeout)
{
    return _store->run_for(timeout);
}

void runloop::quit()
{
    _store->quit();
}

void runloop::clear()
{
    _store->clear();
}

std::size_t runloop::pending_count() const
{
    return _store->pending_count();
}

bool runloop::empty() const
{
    return _store->pending_count() == 0;
}

bool runloop::is_running() const
{
    return _store->is_running();
}

bool runloop::operator==(const runloop& other) const noexcept
{
    return _store == other._store;
}

runloop main_runloop()
{
    return runloop{runloop::storage::main_record().loop};
}

runloop current_runloop()
{
    return runloop{runloop::storage::for_current_thread()};
}

namespace {

// Decides which thread is the main one, on the thread that loads the
// library and before main() runs. Leaving it to the first caller would let
// whichever thread asked first claim the role. It also puts the main
// thread's holder in place, so that loop is marked finished when the main
// thread ends.
[[maybe_unused]] const bool kMainThreadRecorded = (current_runloop(), true);

}

}
