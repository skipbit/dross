#include "notification/frame.h"

#include "dross/ipc/error.h"

#include <cstdint>
#include <iterator>

namespace dross {

namespace {

std::size_t length_at(const std::byte* head)
{
    std::uint32_t length = 0;
    for (std::size_t i = 0; i < frame_header_size; ++i) {
        length = (length << 8U) | std::to_integer<std::uint32_t>(head[i]);
    }
    return length;
}

}  // namespace

std::expected<std::vector<std::byte>, error> frame_message(const std::span<const std::byte> message)
{
    if (message.size() > max_message_size) {
        return std::unexpected(error{ ipc_errc::message_too_large });
    }
    const auto length = static_cast<std::uint32_t>(message.size());
    std::vector<std::byte> framed;
    framed.reserve(frame_header_size + message.size());
    for (std::size_t i = frame_header_size; i > 0; --i) {
        framed.push_back(static_cast<std::byte>(length >> ((i - 1) * 8U)));
    }
    framed.insert(framed.end(), message.begin(), message.end());
    return framed;
}

std::expected<std::vector<std::vector<std::byte>>, error> frame_reader::take(const std::span<const std::byte> bytes)
{
    if (_failed) {
        return std::unexpected(error{ ipc_errc::message_too_large });
    }
    _pending.insert(_pending.end(), bytes.begin(), bytes.end());
    std::vector<std::vector<std::byte>> messages;
    std::size_t used = 0;
    while ((_pending.size() - used) >= frame_header_size) {
        const std::size_t length = length_at(_pending.data() + used);
        if (length > max_message_size) {
            _failed = true;
            _pending.clear();
            return std::unexpected(error{ ipc_errc::message_too_large });
        }
        if ((_pending.size() - used - frame_header_size) < length) {
            break;
        }
        const auto start = std::next(_pending.begin(), static_cast<std::ptrdiff_t>(used + frame_header_size));
        messages.emplace_back(start, std::next(start, static_cast<std::ptrdiff_t>(length)));
        used += frame_header_size + length;
    }
    _pending.erase(_pending.begin(), std::next(_pending.begin(), static_cast<std::ptrdiff_t>(used)));
    return messages;
}

bool frame_reader::holds_partial() const noexcept
{
    return ! _pending.empty();
}

}  // namespace dross
