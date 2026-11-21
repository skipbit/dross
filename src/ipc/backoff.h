#pragma once

#include <algorithm>
#include <chrono>

namespace dross {

// Hides a declaration from the library's exported symbol table, as
// poller.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_BACKOFF_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_BACKOFF_HIDDEN
#endif

// How long to wait before trying again something that keeps failing: 100
// milliseconds at first, doubled after each try, up to 5 seconds.
class DROSS_BACKOFF_HIDDEN backoff final {
public:
    static constexpr std::chrono::milliseconds first{ 100 };
    static constexpr std::chrono::milliseconds longest{ 5000 };

    // The wait before the next try.
    std::chrono::milliseconds next() noexcept
    {
        _last = (_last == std::chrono::milliseconds::zero()) ? first : std::min(_last * 2, longest);
        return _last;
    }

    // Starts again from the first wait, once a try has succeeded.
    void reset() noexcept
    {
        _last = std::chrono::milliseconds::zero();
    }

private:
    std::chrono::milliseconds _last{ 0 };
};

#undef DROSS_BACKOFF_HIDDEN

}  // namespace dross
