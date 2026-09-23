#pragma once

#include "dross/thread/runloop.h"
#include "dross/thread/timer.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

// The state behind a runloop handle, and the timer bookkeeping that shares
// its lock and condition variable. Given its own header, rather than living
// only in runloop.cpp, because timer.cpp needs the complete type too: a
// timer installs and removes itself here directly.
namespace dross {

class runloop::storage final {
public:
    static std::shared_ptr<storage> main_loop();
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

    // Adds which to this loop's installed timers, due at first_deadline.
    // Returns false, without installing it, once this loop is finished.
    bool install_timer(std::shared_ptr<timer::storage> which, std::chrono::steady_clock::time_point first_deadline);

    // Drops which from this loop's installed timers, if it is still there.
    // A no-op, not an error, when it already fired (one-shot) or was
    // already removed.
    void remove_timer(const timer::storage* which);

    std::size_t timer_count() const;

private:
    // Tracks how many running calls are nested on this loop, so a quit() is
    // consumed only by the outermost one and a run from inside a task leaves
    // the outer run's is_running() answer standing when it returns.
    class running_mark final {
    public:
        explicit running_mark(storage& owner);
        ~running_mark();

        running_mark(const running_mark& other) = delete;
        running_mark& operator=(const running_mark& other) = delete;

        bool outermost() const noexcept;

    private:
        storage& _owner;
        bool _outermost;
    };

    // A timer installed on this loop, and the deadline it is next due at.
    struct timer_slot {
        std::shared_ptr<timer::storage> handle;
        std::chrono::steady_clock::time_point deadline;
    };

    // One pass of a running call over what is due: a boundary snapshot and
    // the timers already taken since it was taken. run_until() and
    // run_one() each own one and pass it into next() by reference across
    // however many calls the pass lasts, so a timer already taken in this
    // pass is not taken again until the next one, even if it reschedules
    // itself immediately due again (a zero interval, say). That is what
    // stops one always-due timer from starving every other timer and the
    // task queue: see next()'s own comment.
    struct pass {
        std::chrono::steady_clock::time_point boundary{ std::chrono::steady_clock::now() };
        std::vector<std::uint64_t> handled;
    };

    // Takes the next task or due timer for current_pass, waiting until the
    // deadline. Returns false when quit() was seen, which consumes the
    // request only when consume_quit is true, or when the deadline passed
    // with nothing to run.
    //
    // Within current_pass, a due timer takes priority over a queued task,
    // and the earliest-due timer not yet taken this pass wins over any
    // other due timer, so an overdue one is not starved by one that keeps
    // rescheduling itself sooner. Taking a task ends the pass: the caller's
    // next call starts a fresh one. Finding nothing due or queued also ends
    // it, since a fresh boundary may find what a stale one would miss.
    bool next(std::unique_lock<std::mutex>& lock,
              std::function<void()>& out,
              std::chrono::steady_clock::time_point deadline,
              bool consume_quit,
              pass& current_pass);

    // Runs one task or timer fire with the lock released, and takes the
    // lock back after. The captures go too, so their own code runs outside
    // the lock as well. If the work throws, the lock stays released.
    void run_released(std::unique_lock<std::mutex>& lock, std::function<void()>& work);

    std::size_t run_until(std::chrono::steady_clock::time_point deadline);

    // The earliest deadline among installed timers, or kNoDeadline when none
    // are installed. Must hold _mutex.
    std::chrono::steady_clock::time_point earliest_timer_deadline() const;

    // Finds the timer with the earliest deadline that is due at or before
    // boundary and whose id is not already in handled, advances (repeating)
    // or removes (one-shot) its entry, records its id in handled, and
    // returns a work item that fires it with the lock released. Empty when
    // none remain. Must hold _mutex.
    //
    // Matched by id, not by address: a one-shot's storage can be released
    // between passes, and a later allocation could reuse its address.
    std::function<void()> take_due_timer(std::chrono::steady_clock::time_point boundary, std::vector<std::uint64_t>& handled);

    // A loop that is already finished, shared by every call made after this
    // thread's own bookkeeping has been torn down; see for_current_thread().
    static std::shared_ptr<storage> finished_placeholder();

    mutable std::mutex _mutex;
    std::condition_variable _wake;
    std::deque<std::pair<std::uint64_t, std::function<void()>>> _pending;
    std::vector<timer_slot> _timers;
    std::uint64_t _next_sequence{ 0 };
    std::size_t _depth{ 0 };
    bool _quit{ false };
    bool _finished{ false };
};

}  // namespace dross
