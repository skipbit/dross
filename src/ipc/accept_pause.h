#pragma once

#include "ipc/backoff.h"

#include <algorithm>
#include <chrono>
#include <optional>

namespace dross {

// Hides a declaration from the library's exported symbol table, as
// poller.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_ACCEPT_PAUSE_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_ACCEPT_PAUSE_HIDDEN
#endif

// When accepting resumes after it failed: the wait grows with each failure
// in a row, as a backoff does. Takes the time as an argument.
class DROSS_ACCEPT_PAUSE_HIDDEN accept_pause final {
public:
    using time_point = std::chrono::steady_clock::time_point;

    // Starts a wait at now, or lengthens the one that is on.
    void pause(const time_point now) noexcept
    {
        _until = now + _waits.next();
    }

    // How long the wait still lasts at now; none while not paused.
    std::optional<std::chrono::milliseconds> remaining(const time_point now) const noexcept
    {
        if (! _until) {
            return std::nullopt;
        }
        return std::max(std::chrono::ceil<std::chrono::milliseconds>(*_until - now), std::chrono::milliseconds::zero());
    }

    // Whether a wait is on and has run out at now.
    bool due(const time_point now) const noexcept
    {
        return _until && (now >= *_until);
    }

    // Ends the wait, once accepting is watched again.
    void clear() noexcept
    {
        _until.reset();
    }

    // Starts the next wait from the shortest, once a connection was accepted.
    void accepted() noexcept
    {
        _waits.reset();
    }

private:
    backoff _waits;
    std::optional<time_point> _until;
};

#undef DROSS_ACCEPT_PAUSE_HIDDEN

}  // namespace dross
