#pragma once

#include <atomic>

namespace dross {

// Hides a declaration from the library's exported symbol table, as
// poller.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_CHANNEL_CALLS_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_CHANNEL_CALLS_HIDDEN
#endif

// Checks that a part calls one ipc::message_channel_events as it promises:
// one call at a time, and nothing after closed(). A part that does not is
// misused, and the program stops.
class DROSS_CHANNEL_CALLS_HIDDEN channel_calls final {
public:
    // Marks a call as under way for as long as it is held.
    class call final {
    public:
        explicit call(std::atomic<bool>& calling) noexcept;
        ~call();

        call(const call& other) = delete;
        call& operator=(const call& other) = delete;

    private:
        std::atomic<bool>& _calling;
    };

    [[nodiscard]] call received();
    [[nodiscard]] call closed();

private:
    call enter(const char* name, bool closing);

    std::atomic<bool> _calling{ false };
    std::atomic<bool> _closed{ false };
};

#undef DROSS_CHANNEL_CALLS_HIDDEN

}  // namespace dross
