#pragma once

#include "dross/thread/runloop.h"
#include "dross/thread/timer.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>

// The state behind a timer handle. Given its own header, rather than living
// only in timer.cpp, because runloop.cpp needs the complete type too: a
// loop fires a timer's callback and reschedules or drops it directly.
namespace dross {

class timer::storage final : public std::enable_shared_from_this<storage> {
public:
    storage(std::chrono::milliseconds interval, bool repeats, std::function<void(timer)> callback, std::weak_ptr<runloop::storage> loop);

    // Marks this timer invalid and, if it is still installed, asks its loop
    // to drop it. Callable from any thread; idempotent.
    void invalidate();

    bool valid() const;
    bool repeats() const;
    std::chrono::milliseconds interval() const;

    // A process-wide, monotonically increasing id, distinct for every timer
    // ever constructed. Used in place of this object's address to identify
    // it within a pass: an address can be reused once a one-shot's storage
    // is released, an id never is.
    std::uint64_t id() const noexcept;

    // Runs the callback with a handle to this timer. Called by the loop
    // with its own lock released, the same as a task.
    void fire();

    // Marks this timer invalid without touching its loop. Used by the loop
    // itself when it already knows the timer is gone: right after a
    // one-shot fires, and when the loop drops every installed timer as it
    // ends.
    void mark_invalid();

private:
    const std::chrono::milliseconds _interval;
    const bool _repeats;
    const std::function<void(timer)> _callback;

    // Set once, at construction, and never reassigned, so concurrent calls
    // to invalidate() from any thread can read it without their own lock.
    const std::weak_ptr<runloop::storage> _loop;

    const std::uint64_t _id;

    std::atomic<bool> _valid{ true };
};

}  // namespace dross
