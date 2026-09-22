#pragma once

#include <chrono>

// A single place to turn a caller-given duration into an absolute
// steady_clock deadline without overflow, used everywhere in this module
// that happens: run_for()'s and join_for()'s own timeout, a timer's first
// deadline, and a repeating timer's reschedule. This is the third time this
// module has had the overflow bug fixed by hand in one call site; from here
// it is fixed once.
namespace dross {

/**
 * @brief Compute base + timeout, clamped so the addition cannot overflow.
 * @param base The point in time to measure from
 * @param timeout How far past base the deadline should be
 * @return base + timeout, or a deadline just under steady_clock::time_point's
 * max when that would overflow
 *
 * steady_clock::duration is nanoseconds, so base + timeout overflows for a
 * timeout as large as milliseconds::max(). The clamp is computed as the
 * headroom between base and the clock's own max, as a duration, so the
 * comparison itself cannot overflow either.
 */
std::chrono::steady_clock::time_point deadline_after(std::chrono::steady_clock::time_point base,
                                                      std::chrono::milliseconds timeout);

}
