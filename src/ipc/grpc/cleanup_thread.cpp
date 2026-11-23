#include "ipc/grpc/cleanup_thread.h"

#include "dross/thread/thread.h"

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <utility>

namespace dross::ipc {

namespace {

struct held_thread final {
    std::mutex lock;
    std::optional<thread> running;
};

held_thread& held()
{
    // Never destroyed, as gRPC's threads may hand work over while the
    // program ends.
    static held_thread* const the_thread = new held_thread();
    return *the_thread;
}

}  // namespace

std::expected<void, error> start_cleanup_thread()
{
    auto& cleanup = held();
    const std::lock_guard guard{ cleanup.lock };
    if (cleanup.running) {
        return {};
    }
    auto made = thread::create();
    if (! made) {
        return std::unexpected(std::move(made).error());
    }
    cleanup.running.emplace(*made);
    return {};
}

void clean_up(std::function<void()> work)
{
    auto& cleanup = held();
    const std::lock_guard guard{ cleanup.lock };
    if (! cleanup.running || ! cleanup.running->perform(std::move(work))) {
        std::fputs("dross: the gRPC part has no thread to clean up on\n", stderr);
        std::abort();
    }
}

void cleanup_thread_access::forget()
{
    auto& cleanup = held();
    const std::lock_guard guard{ cleanup.lock };
    cleanup.running.reset();
}

}  // namespace dross::ipc
