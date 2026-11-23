#include "thread/start_thread.h"

#include <mutex>
#include <system_error>
#include <utility>

namespace dross {

namespace {

struct failures_to_come final {
    std::mutex lock;
    int after{ 0 };
    int count{ 0 };
};

failures_to_come& planned()
{
    static failures_to_come failures;
    return failures;
}

bool take_failure()
{
    auto& failures = planned();
    const std::lock_guard guard{ failures.lock };
    if (failures.count == 0) {
        return false;
    }
    if (failures.after > 0) {
        --failures.after;
        return false;
    }
    --failures.count;
    return true;
}

}  // namespace

std::expected<std::thread, error> start_thread(std::function<void()> body)
{
    if (take_failure()) {
        return std::unexpected(error{ std::errc::resource_unavailable_try_again });
    }
    // std::thread reports the failure only by throwing.
    try {
        return std::thread{ std::move(body) };
    } catch (const std::system_error&) {
        return std::unexpected(error{ std::errc::resource_unavailable_try_again });
    }
}

void start_thread_access::fail_next(const int count, const int after)
{
    auto& failures = planned();
    const std::lock_guard guard{ failures.lock };
    failures.count = count;
    failures.after = after;
}

}  // namespace dross
