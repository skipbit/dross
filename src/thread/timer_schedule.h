#pragma once

#include "thread/deadline.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

// The timers installed on one loop, and the deadline each is next due at.
// Every time point comes in as an argument and none is read from a clock, so
// the rule for when a repeating timer is next due is checked without one.
//
// A template over the handle so that a test can use a handle of its own:
// timer::storage is private to timer. The loop uses
// std::shared_ptr<timer::storage>. A handle is a pointer-like type whose
// target has id(), repeats() and interval().
namespace dross {

template <typename Handle>
class timer_schedule final {
public:
    using time_point = std::chrono::steady_clock::time_point;
    using element_type = typename Handle::element_type;

    // Adds which, due at deadline.
    void install(Handle which, time_point deadline);

    // Takes out the timer which points at, and returns its handle so the
    // caller can release it outside its own lock. Empty when it is not
    // installed.
    Handle remove(const element_type* which);

    // Takes out every timer, in the same way.
    std::vector<Handle> remove_all();

    // The earliest deadline among the installed timers, or time_point::max()
    // when none are installed.
    time_point earliest() const;

    // Claims the earliest timer due at or before boundary whose id is not in
    // handled, and records its id there. A repeating timer is next due
    // interval after claimed_at; a one-shot is taken out. Empty when none
    // remains.
    Handle take_due(time_point boundary, time_point claimed_at, std::vector<std::uint64_t>& handled);

    std::size_t size() const;
    bool empty() const;

private:
    struct slot {
        Handle handle;
        time_point deadline;
    };

    std::vector<slot> _slots;
};

template <typename Handle>
void timer_schedule<Handle>::install(Handle which, time_point deadline)
{
    _slots.push_back({ std::move(which), deadline });
}

template <typename Handle>
Handle timer_schedule<Handle>::remove(const element_type* which)
{
    const auto it = std::find_if(_slots.begin(), _slots.end(), [which](const slot& each) {
        return (each.handle.get() == which);
    });
    if (it == _slots.end()) {
        return Handle{};
    }

    Handle removed = std::move(it->handle);
    _slots.erase(it);
    return removed;
}

template <typename Handle>
std::vector<Handle> timer_schedule<Handle>::remove_all()
{
    std::vector<Handle> removed;
    removed.reserve(_slots.size());
    for (auto& each : _slots) {
        removed.push_back(std::move(each.handle));
    }
    _slots.clear();
    return removed;
}

template <typename Handle>
typename timer_schedule<Handle>::time_point timer_schedule<Handle>::earliest() const
{
    auto earliest = time_point::max();
    for (const auto& each : _slots) {
        earliest = std::min(earliest, each.deadline);
    }
    return earliest;
}

template <typename Handle>
Handle timer_schedule<Handle>::take_due(time_point boundary, time_point claimed_at, std::vector<std::uint64_t>& handled)
{
    // The earliest-due candidate wins, not just any due one: taking the
    // first match in _slots order let whichever timer happened to be
    // installed first starve every other timer that was also due, since a
    // repeating one reschedules itself back into contention before the
    // scan ever reaches the others.
    //
    // Matched by id, not by the handle's address: a one-shot's storage can
    // be released, and its address reused by an unrelated allocation,
    // between one pass and the next, and an address match would then skip
    // the wrong timer.
    auto earliest = _slots.end();
    for (auto it = _slots.begin(); it != _slots.end(); ++it) {
        if (it->deadline > boundary) {
            continue;
        }
        if (std::find(handled.begin(), handled.end(), it->handle->id()) != handled.end()) {
            continue;
        }
        if (earliest == _slots.end() || it->deadline < earliest->deadline) {
            earliest = it;
        }
    }

    if (earliest == _slots.end()) {
        return Handle{};
    }

    Handle which = earliest->handle;
    handled.push_back(which->id());

    if (which->repeats()) {
        // From claimed_at, not from the deadline that was due, and not from
        // boundary either: boundary is stale once anything else in the pass
        // has taken a while, such as an earlier fire's callback blocking,
        // and rescheduling from it would make this one due again at once
        // instead of waiting a genuine interval. A callback that blocks, or
        // a loop that is not run for a while, does not make up the fires it
        // missed by catching up all at once.
        earliest->deadline = deadline::after(claimed_at, which->interval());
        return which;
    }

    // A one-shot is taken out before it fires, not after: nothing that runs
    // while the loop's lock is released can find it here and fire it again.
    _slots.erase(earliest);
    return which;
}

template <typename Handle>
std::size_t timer_schedule<Handle>::size() const
{
    return _slots.size();
}

template <typename Handle>
bool timer_schedule<Handle>::empty() const
{
    return _slots.empty();
}

}  // namespace dross
