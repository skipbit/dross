#pragma once

#include "dross/type/error.h"
#include "ipc/limits.h"

#include <cstddef>
#include <deque>
#include <expected>
#include <vector>

namespace dross {

// Hides a declaration from the library's exported symbol table, as
// poller.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_OUTBOX_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_OUTBOX_HIDDEN
#endif

// The bytes waiting to be sent on one connection, in the order they were
// queued.
class DROSS_OUTBOX_HIDDEN outbox final {
public:
    // Fails with queue_overflow, queuing nothing, when more than
    // max_queued_bytes would be waiting.
    std::expected<void, error> push(std::vector<std::byte> bytes);

    // Sends what the socket takes now, and keeps the rest.
    std::expected<void, error> flush(int fd);

    bool empty() const noexcept;

    // The bytes waiting.
    std::size_t size() const noexcept;

private:
    std::deque<std::vector<std::byte>> _queue;
    std::size_t _front_sent{ 0 };
    std::size_t _size{ 0 };
};

#undef DROSS_OUTBOX_HIDDEN

}  // namespace dross
