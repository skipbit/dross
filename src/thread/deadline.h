#pragma once

#include <chrono>

// A single place to turn a caller-given duration into an absolute
// steady_clock deadline without overflow, used everywhere in this module
// that happens: run_for()'s and join_for()'s own timeout, a timer's first
// deadline, and a repeating timer's reschedule. This is the fourth time
// this module has had the overflow bug fixed by hand in one call site or
// another; from here it is fixed once.
namespace dross::deadline {

// Hides a declaration from the library's exported symbol table, the same
// way native.h does for its own helpers: an implementation detail of a
// header under src/, which is never installed, so nothing outside this
// library can name it, and this keeps it off nm -D's output as well.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_DEADLINE_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_DEADLINE_HIDDEN
#endif

/**
 * @brief Compute base + timeout, clamped so the addition cannot overflow.
 * @param base The point in time to measure from
 * @param timeout How far past base the deadline should be
 * @return base + timeout, clamped to a deadline just under
 * steady_clock::time_point's max when that addition would overflow, and to
 * base itself when timeout is zero or negative
 *
 * steady_clock::duration is nanoseconds, so base + timeout overflows for a
 * timeout as large as milliseconds::max(), and converting a timeout as
 * negative as milliseconds::min() to nanoseconds overflows the same way on
 * the negative side. The positive clamp is computed as the headroom between
 * base and the clock's own max, as a duration, so that comparison cannot
 * overflow either.
 */
DROSS_DEADLINE_HIDDEN std::chrono::steady_clock::time_point after(
    std::chrono::steady_clock::time_point base, std::chrono::milliseconds timeout);

#undef DROSS_DEADLINE_HIDDEN

}
