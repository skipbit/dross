#include "dross/thread/runloop.h"

#include "thread/deadline.h"
#include "thread/native.h"
#include "thread/runloop_storage.h"
#include "thread/timer_storage.h"

#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <utility>

namespace dross {

namespace {

// Marks "wait with no deadline". It is a time point, rather than a separate
// flag, so the same parameter carries both cases; the branch that checks for
// it exists to avoid handing time_point::max() to wait_until().
constexpr auto kNoDeadline = std::chrono::steady_clock::time_point::max();

}  // namespace

runloop::storage::running_mark::running_mark(storage& owner)
    : _owner{ owner }
{
    const std::lock_guard<std::mutex> guard{ _owner._mutex };
    _outermost = (_owner._depth == 0);
    ++_owner._depth;
}

runloop::storage::running_mark::~running_mark()
{
    const std::lock_guard<std::mutex> guard{ _owner._mutex };
    --_owner._depth;
}

bool runloop::storage::running_mark::outermost() const noexcept
{
    return _outermost;
}

std::shared_ptr<runloop::storage> runloop::storage::main_loop()
{
    // Never destroyed. A thread still running when the program ends would
    // otherwise reach a destroyed loop, and thread storage is destroyed
    // before anything with static storage duration. The static pointer keeps
    // it reachable, so a leak checker does not report it.
    //
    // Built on first use rather than at load time, so a program that links
    // this library without touching a run loop allocates nothing.
    static const std::shared_ptr<storage>* const the_loop = new std::shared_ptr<storage>{ std::make_shared<storage>() };
    return *the_loop;
}

std::shared_ptr<runloop::storage> runloop::storage::finished_placeholder()
{
    // Shared by every thread that reaches it, rather than kept thread_local:
    // nothing is ever installed on it, so nothing needs it to be distinct
    // per thread. Never destroyed, for the same reason as main_loop().
    static const std::shared_ptr<storage>* const the_loop = []() {
        auto loop = std::make_shared<storage>();
        loop->finish();
        return new std::shared_ptr<storage>{ std::move(loop) };
    }();
    return *the_loop;
}

std::shared_ptr<runloop::storage> runloop::storage::for_current_thread()
{
    // Trivially destructible, so it has no destructor of its own and stays
    // readable no matter what order thread-locals on this thread are torn
    // down in. A user's own thread-local destructor may call
    // current_runloop() after the holder below has already run its own
    // destructor; this is what keeps that call defined instead of reaching
    // into a destroyed loop. See current_runloop()'s doc comment.
    static thread_local bool torn_down = false;

    if (torn_down) {
        return finished_placeholder();
    }

    // Destroyed when the thread ends, which is how a loop learns that no
    // task of its will ever run again. This works for threads dross did not
    // start.
    struct holder final {
        std::shared_ptr<storage> loop;

        ~holder()
        {
            // Set before finish(), not after: finish() now runs the
            // destructors of whatever this loop's queued tasks and
            // installed timers captured, and one of those could call back
            // into current_runloop(). That call must see torn_down already
            // true, or it would reach into this very holder while it is
            // mid-destruction.
            torn_down = true;
            loop->finish();
        }
    };

    static thread_local const holder current{ native::on_main_thread() ? main_loop() : std::make_shared<storage>() };

    return current.loop;
}

bool runloop::storage::enqueue(std::function<void()>&& task)
{
    if (! task) {
        return false;
    }

    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        if (_finished) {
            return false;
        }
        _pending.push_back({ _next_sequence++, std::move(task) });
    }

    _wake.notify_one();
    return true;
}

bool runloop::storage::install_timer(std::shared_ptr<timer::storage> which, std::chrono::steady_clock::time_point first_deadline)
{
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        if (_finished) {
            return false;
        }
        _timers.push_back({ std::move(which), first_deadline });
    }

    _wake.notify_one();
    return true;
}

void runloop::storage::remove_timer(const timer::storage* which)
{
    std::shared_ptr<timer::storage> removed;
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        const auto it = std::find_if(_timers.begin(), _timers.end(), [which](const timer_slot& slot) {
            return slot.handle.get() == which;
        });
        if (it != _timers.end()) {
            removed = std::move(it->handle);
            _timers.erase(it);
        }
    }

    // removed is destroyed here, outside the lock: if this was the last
    // reference, its callback's captures run their own destructors, and
    // one of those reaching back into this loop, to invalidate another
    // timer or check timer_count(), must not find _mutex already held by
    // this call. clear(), finish() and take_due_timer() all follow the
    // same rule.

    // Correct and symmetric with install_timer()'s own notify below, but a
    // black-box test cannot isolate this one call: any notify on this
    // condition variable wakes a wait blocked on any deadline, so a test
    // that also installs something afterward, the only way to observe a
    // wake at all, cannot tell this notify apart from that install's.
    _wake.notify_one();
}

std::chrono::steady_clock::time_point runloop::storage::earliest_timer_deadline() const
{
    auto earliest = kNoDeadline;
    for (const auto& slot : _timers) {
        earliest = std::min(earliest, slot.deadline);
    }
    return earliest;
}

std::function<void()> runloop::storage::take_due_timer(std::chrono::steady_clock::time_point boundary, std::vector<std::uint64_t>& handled)
{
    // The earliest-due candidate wins, not just any due one: taking the
    // first match in _timers order let whichever timer happened to be
    // installed first starve every other timer that was also due, since a
    // repeating one reschedules itself back into contention before the
    // scan ever reaches the others.
    //
    // Matched by id, not by the handle's address: a one-shot's storage can
    // be released, and its address reused by an unrelated allocation,
    // between one pass and the next, and an address match would then skip
    // the wrong timer.
    auto earliest = _timers.end();
    for (auto it = _timers.begin(); it != _timers.end(); ++it) {
        if (it->deadline > boundary) {
            continue;
        }
        if (std::find(handled.begin(), handled.end(), it->handle->id()) != handled.end()) {
            continue;
        }
        if (earliest == _timers.end() || it->deadline < earliest->deadline) {
            earliest = it;
        }
    }

    if (earliest == _timers.end()) {
        return nullptr;
    }

    auto which = earliest->handle;
    handled.push_back(which->id());

    if (which->repeats()) {
        // Scheduled from now, the moment this fire is claimed, not from
        // the deadline that was due, and not from the pass boundary
        // either: the boundary is stale once anything else in the pass has
        // taken a while, such as an earlier fire's callback blocking, and
        // rescheduling from it would make this one due again immediately
        // instead of waiting a genuine interval. A callback that blocks,
        // or a loop that is not run for a while, does not make up the
        // fires it missed by catching up all at once.
        earliest->deadline = deadline::after(std::chrono::steady_clock::now(), which->interval());
        return [which]() {
            which->fire();
        };
    }

    // A one-shot is uninstalled before it fires, not after: nothing that
    // runs while the lock is released below can find it in _timers and
    // fire it again.
    _timers.erase(earliest);
    return [which]() {
        const struct invalidate_after final {
            const std::shared_ptr<timer::storage>& target;
            ~invalidate_after()
            {
                target->mark_invalid();
            }
        } guard{ which };
        which->fire();
    };
}

bool runloop::storage::next(std::unique_lock<std::mutex>& lock,
                            std::function<void()>& out,
                            std::chrono::steady_clock::time_point deadline,
                            bool consume_quit,
                            pass& current_pass)
{
    // Set once nothing is due or queued in current_pass and it has already
    // been refreshed once this call: a second empty refresh in a row means
    // there really is nothing to do right now, so it is time to wait rather
    // than spin. Local to the call, not the pass: every fresh call gets its
    // own chance to refresh before waiting.
    bool refreshed_this_call = false;

    while (true) {
        if (_quit) {
            if (consume_quit) {
                _quit = false;
            }
            return false;
        }

        // Every pass operation below costs a steady_clock::now(); skipped
        // entirely on a loop with no installed timers, so a loop that only
        // ever queues tasks pays nothing for a feature it does not use.
        const bool has_timers = ! _timers.empty();

        if (has_timers) {
            if (auto timer_work = take_due_timer(current_pass.boundary, current_pass.handled)) {
                out = std::move(timer_work);
                return true;
            }
        }

        if (! _pending.empty()) {
            out = std::move(_pending.front().second);
            _pending.pop_front();
            if (has_timers) {
                current_pass = pass{};
            }
            return true;
        }

        if (has_timers && ! refreshed_this_call) {
            // Nothing left in this pass. A fresh boundary may find what a
            // stale one would miss, so try once more before deciding there
            // is truly nothing to do right now.
            current_pass = pass{};
            refreshed_this_call = true;
            continue;
        }

        const auto now = std::chrono::steady_clock::now();

        // The earlier of the caller's own deadline and the next timer due,
        // so a timer installed after this call started still wakes it: see
        // install_timer()'s notify below.
        const auto wait_deadline = std::min(deadline, earliest_timer_deadline());
        if (wait_deadline <= now) {
            return false;
        }

        // No predicate here on purpose: recomputing wait_deadline from
        // scratch on every wake, rather than trusting a single wait_until()
        // call to keep re-checking a fixed one, is what lets a newly
        // installed timer with an earlier deadline cut this wait short.
        if (wait_deadline == kNoDeadline) {
            _wake.wait(lock);
        } else {
            _wake.wait_until(lock, wait_deadline);
        }

        // Give the pass another chance to refresh against the time that
        // just passed, before considering waiting again.
        refreshed_this_call = false;
    }
}

void runloop::storage::run_released(std::unique_lock<std::mutex>& lock, std::function<void()>& work)
{
    lock.unlock();
    work();
    work = nullptr;  // release the captures outside the lock
    lock.lock();
}

std::size_t runloop::storage::run_until(std::chrono::steady_clock::time_point deadline)
{
    const running_mark mark{ *this };

    std::size_t ran = 0;
    std::unique_lock<std::mutex> lock{ _mutex };
    std::function<void()> work;
    pass current_pass;

    while (deadline == kNoDeadline || std::chrono::steady_clock::now() < deadline) {
        if (! next(lock, work, deadline, mark.outermost(), current_pass)) {
            break;
        }

        run_released(lock, work);
        ++ran;
    }

    return ran;
}

std::size_t runloop::storage::run()
{
    return run_until(kNoDeadline);
}

std::size_t runloop::storage::run_for(std::chrono::milliseconds timeout)
{
    return run_until(deadline::after(std::chrono::steady_clock::now(), timeout));
}

bool runloop::storage::run_one()
{
    const running_mark mark{ *this };

    std::unique_lock<std::mutex> lock{ _mutex };
    std::function<void()> work;
    pass current_pass;
    if (! next(lock, work, kNoDeadline, mark.outermost(), current_pass)) {
        return false;
    }

    run_released(lock, work);
    return true;
}

std::size_t runloop::storage::run_pending()
{
    const running_mark mark{ *this };

    std::size_t ran = 0;
    std::unique_lock<std::mutex> lock{ _mutex };

    // Both read before anything runs, not just the boundary: a timer fired
    // below can post a task of its own, and that task's sequence number
    // must not be under this call's own cutoff, or it would run inside the
    // same call that queued it.
    const auto boundary = std::chrono::steady_clock::now();
    const std::uint64_t limit = _next_sequence;

    // Only timers already due, and only once each, even a repeating one
    // with a zero interval: boundary is fixed for the whole call, taken
    // once above rather than re-read after each fire.
    std::vector<std::uint64_t> handled;
    while (auto work = take_due_timer(boundary, handled)) {
        run_released(lock, work);
        ++ran;
    }

    // Only what was already queued when the call started, so a task that
    // posts another, even after emptying the queue with clear(), does not
    // keep this call going. A pending quit() is left alone: it belongs to
    // the next run(), not to this drain.
    while (! _pending.empty() && _pending.front().first < limit) {
        std::function<void()> work = std::move(_pending.front().second);
        _pending.pop_front();

        run_released(lock, work);
        ++ran;
    }

    return ran;
}

void runloop::storage::quit()
{
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        _quit = true;
    }
    _wake.notify_all();
}

void runloop::storage::clear()
{
    std::deque<std::pair<std::uint64_t, std::function<void()>>> discarded;
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        discarded.swap(_pending);
    }
    // Destroyed outside the lock: a task's captures run their own code.
}

void runloop::storage::finish()
{
    std::deque<std::pair<std::uint64_t, std::function<void()>>> discarded_tasks;
    std::vector<timer_slot> discarded_timers;
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        _finished = true;
        discarded_tasks.swap(_pending);
        discarded_timers.swap(_timers);
    }

    for (auto& slot : discarded_timers) {
        slot.handle->mark_invalid();
    }
    // discarded_tasks and discarded_timers are destroyed here, outside the
    // lock, on this thread: their captures run their own destructors here,
    // not wherever a handle to this loop last happened to be dropped.
}

std::size_t runloop::storage::pending_count() const
{
    const std::lock_guard<std::mutex> guard{ _mutex };
    return _pending.size();
}

std::size_t runloop::storage::timer_count() const
{
    const std::lock_guard<std::mutex> guard{ _mutex };
    return _timers.size();
}

bool runloop::storage::is_running() const
{
    const std::lock_guard<std::mutex> guard{ _mutex };
    return _depth > 0;
}

runloop::runloop(std::shared_ptr<storage> store) noexcept
    : _store{ std::move(store) }
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

std::size_t runloop::timer_count() const
{
    return _store->timer_count();
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
    // Asked on the main thread, this goes the long way round so the thread's
    // holder is put in place: that holder is what marks the loop finished
    // when the main thread ends, and a program that only ever says
    // main_runloop() would otherwise never install one.
    if (native::on_main_thread()) {
        return current_runloop();
    }

    return runloop{ runloop::storage::main_loop() };
}

runloop current_runloop()
{
    return runloop{ runloop::storage::for_current_thread() };
}

}  // namespace dross
