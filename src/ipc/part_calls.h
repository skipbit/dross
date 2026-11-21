#pragma once

#include <atomic>

namespace dross {

// Hides a declaration from the library's exported symbol table, as
// poller.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_PART_CALLS_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_PART_CALLS_HIDDEN
#endif

// Checks that a part calls one of the events the library gives it as it
// promises: one call at a time, and nothing after the last, such as
// closed(). A part that does not is misused, and the program stops.
class DROSS_PART_CALLS_HIDDEN part_calls final {
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

    // For the calls of a message_channel_events.
    [[nodiscard]] static part_calls for_channel_events() noexcept;

    // For the calls of a message_acceptor_events.
    [[nodiscard]] static part_calls for_acceptor_events() noexcept;

    // A call to name, after which more may come.
    [[nodiscard]] call any(const char* name);

    // A call to name, after which nothing more may come.
    [[nodiscard]] call last(const char* name);

private:
    // events names the class the part calls, for the message.
    explicit part_calls(const char* events) noexcept;

    call enter(const char* name, bool last);

    const char* const _events;
    std::atomic<bool> _calling{ false };
    std::atomic<const char*> _last{ nullptr };
};

#undef DROSS_PART_CALLS_HIDDEN

}  // namespace dross
