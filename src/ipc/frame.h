#pragma once

#include "dross/type/error.h"

#include <cstddef>
#include <expected>
#include <span>
#include <vector>

namespace dross {

// Hides a declaration from the library's exported symbol table, as
// poller.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_FRAME_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_FRAME_HIDDEN
#endif

// Messages between processes travel on a stream, each one after a 4-byte
// big-endian length.
inline constexpr std::size_t frame_header_size = 4;
inline constexpr std::size_t max_message_size = 64 * 1024;

// The message with its length in front. A message larger than
// max_message_size fails with message_too_large.
DROSS_FRAME_HIDDEN std::expected<std::vector<std::byte>, error> frame_message(std::span<const std::byte> message);

// Cuts the bytes of a stream into messages, holding a message that has only
// partly arrived until the rest comes.
class DROSS_FRAME_HIDDEN frame_reader final {
public:
    // The messages that bytes completes, in order. A length larger than
    // max_message_size fails with message_too_large, and so does every call
    // after it; the messages before it in the same bytes are not returned.
    std::expected<std::vector<std::vector<std::byte>>, error> take(std::span<const std::byte> bytes);

    // Whether part of a message has arrived and the rest has not.
    bool holds_partial() const noexcept;

private:
    std::vector<std::byte> _pending;
    bool _failed{ false };
};

#undef DROSS_FRAME_HIDDEN

}  // namespace dross
