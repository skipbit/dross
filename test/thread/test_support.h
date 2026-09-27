#pragma once

#include "dross/thread/runloop.h"
#include "dross/thread/timer.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>

namespace dross_test {

// Generous enough that a real regression fails instead of flaking, but short
// enough that a genuine hang does not stall the suite. Every wait bounded by
// it ends early on success, so it is spent only when a test is already
// failing.
inline constexpr auto kTimeout = std::chrono::seconds{ 5 };

// Every test that uses the main thread's loop shares it, so it starts from a
// known state: no queued tasks, no quit request left behind. There is no bulk
// equivalent of clear() for timers, so every timer a test installs on it is
// held by an invalidate_on_exit.
inline void reset_main_runloop()
{
    dross::runloop loop = dross::main_runloop();
    loop.clear();
    loop.run_for(std::chrono::milliseconds{ 1 });
}

// Invalidates a timer when the test leaves the scope, however it leaves, so a
// test that fails partway does not leave its timer installed on a shared loop
// for the tests after it.
class invalidate_on_exit final {
public:
    explicit invalidate_on_exit(dross::timer which)
        : _which{ std::move(which) }
    {
    }

    ~invalidate_on_exit()
    {
        _which.invalidate();
    }

    invalidate_on_exit(const invalidate_on_exit& other) = delete;
    invalidate_on_exit& operator=(const invalidate_on_exit& other) = delete;

private:
    dross::timer _which;
};

// Set once, from any thread, and waited for from another.
class event final {
public:
    void set()
    {
        {
            const std::lock_guard<std::mutex> guard{ _mutex };
            _set = true;
        }
        _wake.notify_all();
    }

    // Returns false if bound passed first.
    bool wait(std::chrono::milliseconds bound = kTimeout)
    {
        std::unique_lock<std::mutex> lock{ _mutex };
        return _wake.wait_for(lock, bound, [this]() {
            return _set;
        });
    }

private:
    std::mutex _mutex;
    std::condition_variable _wake;
    bool _set{ false };
};

// Counts the checks of one lock. take_lock is a call that takes that lock.
struct lock_probe final {
    explicit lock_probe(std::function<void()> take)
        : take_lock{ std::move(take) }
    {
    }

    std::function<void()> take_lock;
    std::atomic<int> checked{ 0 };
    std::atomic<int> blocked{ 0 };
};

// Makes another thread call something that takes the lock, and counts it in
// state when that call does not get through within a bound.
inline void probe_lock(lock_probe& state) noexcept
{
    ++state.checked;
    // One is enough to fail; the rest would each wait out the bound.
    if (state.blocked.load() > 0) {
        return;
    }
    auto through = std::make_shared<event>();
    std::thread{ [take = std::optional<std::function<void()>>{ state.take_lock }, through]() mutable {
        (*take)();
        // Drops its copy before it signals, so nothing it holds outlives the
        // check.
        take.reset();
        through->set();
    } }.detach();
    if (! through->wait()) {
        ++state.blocked;
    }
}

// Probes the lock each time it is copied or destroyed. Small enough, and
// copied without throwing, that a std::function may keep it inline, and so
// copy it when it moves.
struct lock_check final {
    std::shared_ptr<lock_probe> state;

    explicit lock_check(std::shared_ptr<lock_probe> shared)
        : state{ std::move(shared) }
    {
    }

    lock_check(const lock_check& other) noexcept
        : state{ other.state }
    {
        probe_lock(*state);
    }

    ~lock_check()
    {
        probe_lock(*state);
    }
};
static_assert(std::is_nothrow_copy_constructible_v<lock_check>);

}  // namespace dross_test
