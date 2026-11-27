#pragma once

#include "dross/thread/runloop.h"
#include "dross/thread/timer.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

// The state behind a timer handle. Given its own header, rather than living
// only in timer.cpp, because runloop.cpp needs the complete type too: a
// loop fires a timer's callback and reschedules or drops it directly.
namespace dross {

class timer::storage final {
public:
    storage(std::chrono::milliseconds interval, bool repeats, std::function<void(timer)> callback, std::weak_ptr<runloop::storage> loop);

    // The pointer the first handle to store holds. Every handle shares one
    // owner, apart from the loop's own reference, and the owner stops the
    // timer when the last handle goes. Called once, before store is
    // installed on its loop.
    static std::shared_ptr<storage> first_handle(const std::shared_ptr<storage>& store);

    // Stops this timer, the same as mark_invalid(), and, if it is still
    // installed, asks its loop to drop it. Callable from any thread;
    // idempotent.
    void invalidate();

    bool valid() const;
    bool repeats() const;
    std::chrono::milliseconds interval() const;

    // A process-wide, monotonically increasing id, distinct for every timer
    // ever constructed. Used in place of this object's address to identify
    // it within a pass: an address can be reused once a one-shot's storage
    // is released, an id never is.
    std::uint64_t id() const noexcept;

    // Runs the callback with a handle to this timer, unless the last handle
    // has already gone. Called by the loop with its own lock released, the
    // same as a task. Releases the callback afterward if the timer is a
    // one-shot or has stopped meanwhile.
    void fire();

    // Marks this timer invalid and releases its callback, unless a fire is
    // running it, without touching its loop. Used by the loop itself when
    // it already knows the timer is gone: right after a one-shot fires, and
    // when the loop drops every installed timer as it ends.
    void mark_invalid();

private:
    class handles;

    const std::chrono::milliseconds _interval;
    const bool _repeats;
    // Empty while a fire is running it and once the timer has stopped, so
    // a callback holding a handle to its own timer does not keep it alive.
    std::mutex _callback_mutex;
    std::function<void(timer)> _callback;

    // Set once, at construction, and never reassigned, so concurrent calls
    // to invalidate() from any thread can read it without their own lock.
    const std::weak_ptr<runloop::storage> _loop;

    const std::uint64_t _id;

    // Shares the handles' owner; expired once the last handle has gone.
    // Set by first_handle() before the timer is installed, and only read
    // after, by the loop that fires it.
    std::weak_ptr<storage> _handle;

    // Written only under _callback_mutex; read without it.
    std::atomic<bool> _valid{ true };
};

}  // namespace dross
