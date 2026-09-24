#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>

// Where a run loop reads the time and how it waits. Every loop uses steady()
// unless it was made with another, which only a test does: see
// runloop_access.
//
// Time points stay steady_clock's own, so a time_source other than steady()
// only decides which one is "now"; nothing downstream changes type.
namespace dross {

class time_source {
public:
    using time_point = std::chrono::steady_clock::time_point;

    virtual ~time_source() = default;

    virtual time_point now() const = 0;

    // Waits on wake, with lock held, until it is notified. May return early,
    // as a condition variable may.
    virtual void wait(std::unique_lock<std::mutex>& lock, std::condition_variable& wake) const = 0;

    // Waits on wake, with lock held, until it is notified or until deadline
    // has passed as this source counts time. May return early, as a
    // condition variable may.
    virtual void wait_until(std::unique_lock<std::mutex>& lock, std::condition_variable& wake, time_point deadline) const = 0;

    // std::chrono::steady_clock, shared by every loop that uses it.
    static std::shared_ptr<const time_source> steady();
};

}  // namespace dross
