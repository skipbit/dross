#pragma once

#include "thread/runloop_access.h"
#include "thread/time_source.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>

namespace dross_test {

// A time source that moves only when told to. A wait for a deadline does not
// sleep: it moves the time to that deadline and returns, so time the loop
// would spend idle passes at once, exactly.
//
// For a loop driven from one thread. Work queued from another thread while
// the loop is idle is not what this models.
class manual_time_source final : public dross::time_source {
public:
    time_point now() const override
    {
        return _now.load();
    }

    // Nothing to move the time to, so this waits for a notify like any other
    // source would.
    void wait(std::unique_lock<std::mutex>& lock, std::condition_variable& wake) const override
    {
        wake.wait(lock);
    }

    void wait_until(std::unique_lock<std::mutex>& /*lock*/, std::condition_variable& /*wake*/, time_point deadline) const override
    {
        if (deadline > _now.load()) {
            _now.store(deadline);
        }
    }

    void advance(std::chrono::nanoseconds by)
    {
        _now.store(_now.load() + by);
    }

private:
    // Any fixed point will do; an hour in keeps arithmetic clear of the epoch.
    mutable std::atomic<time_point> _now{ time_point{} + std::chrono::hours{ 1 } };
};

// A loop of its own, reading the time from a clock the test advances.
struct manual_loop {
    std::shared_ptr<manual_time_source> clock = std::make_shared<manual_time_source>();
    dross::runloop loop = dross::runloop_access::standalone(clock);
};

}  // namespace dross_test
