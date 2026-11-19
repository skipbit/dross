#pragma once

#include "dross/type/error.h"

#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <vector>

namespace dross {

// Hides a declaration from the library's exported symbol table, as
// deadline.h does: this header is never installed, so nothing outside the
// library names it, and this keeps it off nm -D's output as well.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_POLLER_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_POLLER_HIDDEN
#endif

// Waits on file descriptors until one can be read or written, on one
// thread, and can be woken from any other. Each platform has its own file:
// poller_epoll.cpp on Linux and poller_kqueue.cpp on macOS. A descriptor is
// added once, and only an added one is changed or removed; the platforms
// differ on what happens otherwise.
class DROSS_POLLER_HIDDEN poller final {
public:
    // What to wait for on a file descriptor.
    enum class interest : std::uint8_t {
        read = 1,
        write = 2,
        read_write = 3,
    };

    // A file descriptor that is ready. hangup is set when the peer has gone
    // or the descriptor is in error; reading then tells which.
    struct event final {
        int fd;
        bool readable;
        bool writable;
        bool hangup;
    };

    static std::expected<poller, error> create();

    poller(poller&& other) noexcept;
    poller& operator=(poller&& other) noexcept;
    ~poller();

    poller(const poller& other) = delete;
    poller& operator=(const poller& other) = delete;

    // The file descriptors stay owned by the caller, who removes one before
    // closing it.
    std::expected<void, error> add(int fd, interest wanted);
    std::expected<void, error> change(int fd, interest wanted);
    void remove(int fd) noexcept;

    // Waits until a descriptor is ready, wake() is called or the timeout
    // passes; without a timeout it waits for one of the other two. Returns
    // the ready descriptors, which is empty when it was woken or timed out.
    std::expected<std::vector<event>, error> wait(std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    // Makes a wait() that is under way, or the next one, return. Safe from
    // any thread.
    void wake() noexcept;

private:
    class storage;

    explicit poller(std::unique_ptr<storage> store) noexcept;

    std::unique_ptr<storage> _store;
};

#undef DROSS_POLLER_HIDDEN

}  // namespace dross
