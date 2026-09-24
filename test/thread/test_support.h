#pragma once

#include "dross/thread/runloop.h"
#include "dross/thread/timer.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
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

}  // namespace dross_test
