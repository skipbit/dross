#pragma once

#include "test_support.h"
#include "thread/time_source.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>

namespace dross_test {

// The real clock, reporting each time a wait begins. A test on another thread
// acts once a loop or a join is really waiting, instead of after a sleep it
// hopes was long enough.
//
// A wait is counted before it releases the lock it was given, so whatever the
// test does next under that same lock, such as a quit() or a perform(), lands
// only once the wait is ready to be woken by it.
class observed_time_source final : public dross::time_source {
public:
    time_point now() const override
    {
        return std::chrono::steady_clock::now();
    }

    void wait(std::unique_lock<std::mutex>& lock, std::condition_variable& wake) const override
    {
        entered(true);
        wake.wait(lock);
    }

    void wait_until(std::unique_lock<std::mutex>& lock, std::condition_variable& wake, time_point deadline) const override
    {
        entered(false);
        wake.wait_until(lock, deadline);
    }

    std::size_t waits() const
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        return _timed_waits + _untimed_waits;
    }

    std::size_t untimed_waits() const
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        return _untimed_waits;
    }

    // Each returns false if bound passed before count waits of its kind had
    // begun.
    bool await_waits(std::size_t count, std::chrono::milliseconds bound = kTimeout) const
    {
        std::unique_lock<std::mutex> lock{ _mutex };
        return _changed.wait_for(lock, bound, [this, count]() {
            return (_timed_waits + _untimed_waits >= count);
        });
    }

    bool await_timed_waits(std::size_t count, std::chrono::milliseconds bound = kTimeout) const
    {
        std::unique_lock<std::mutex> lock{ _mutex };
        return _changed.wait_for(lock, bound, [this, count]() {
            return (_timed_waits >= count);
        });
    }

    bool await_untimed_waits(std::size_t count, std::chrono::milliseconds bound = kTimeout) const
    {
        std::unique_lock<std::mutex> lock{ _mutex };
        return _changed.wait_for(lock, bound, [this, count]() {
            return (_untimed_waits >= count);
        });
    }

private:
    void entered(bool untimed) const
    {
        {
            const std::lock_guard<std::mutex> guard{ _mutex };
            ++(untimed ? _untimed_waits : _timed_waits);
        }
        _changed.notify_all();
    }

    mutable std::mutex _mutex;
    mutable std::condition_variable _changed;
    mutable std::size_t _timed_waits{ 0 };
    mutable std::size_t _untimed_waits{ 0 };
};

}  // namespace dross_test
