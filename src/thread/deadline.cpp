#include "thread/deadline.h"

namespace dross {

std::chrono::steady_clock::time_point deadline_after(std::chrono::steady_clock::time_point base,
                                                      std::chrono::milliseconds timeout)
{
    constexpr auto kMaxDeadline = std::chrono::steady_clock::time_point::max();
    const auto limit = kMaxDeadline - std::chrono::steady_clock::duration{1};
    const auto room = std::chrono::duration_cast<std::chrono::milliseconds>(limit - base);
    return timeout < room ? base + timeout : limit;
}

}
