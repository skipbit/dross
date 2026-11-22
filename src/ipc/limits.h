#pragma once

#include <cstddef>

namespace dross {

// The limits every part that carries messages keeps: the largest message,
// and the most bytes waiting to be sent on one connection.
inline constexpr std::size_t max_message_size = 64 * 1024;
inline constexpr std::size_t max_queued_bytes = 1024 * 1024;

}  // namespace dross
