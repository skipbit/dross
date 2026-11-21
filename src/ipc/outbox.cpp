#include "ipc/outbox.h"

#include "dross/ipc/error.h"
#include "ipc/unix_socket.h"

#include <span>
#include <utility>

namespace dross {

std::expected<void, error> outbox::push(std::vector<std::byte> bytes)
{
    if (bytes.size() > (max_queued_bytes - _size)) {
        return std::unexpected(error{ ipc_errc::queue_overflow });
    }
    if (bytes.empty()) {
        return {};
    }
    _size += bytes.size();
    _queue.push_back(std::move(bytes));
    return {};
}

std::expected<void, error> outbox::flush(const int fd)
{
    while (! _queue.empty()) {
        const auto rest = std::span{ _queue.front() }.subspan(_front_sent);
        const auto sent = send_some(fd, rest);
        if (! sent) {
            return std::unexpected(sent.error());
        }
        if (*sent == 0) {
            return {};
        }
        _front_sent += *sent;
        _size -= *sent;
        if (_front_sent == _queue.front().size()) {
            _queue.pop_front();
            _front_sent = 0;
        }
    }
    return {};
}

bool outbox::empty() const noexcept
{
    return _queue.empty();
}

std::size_t outbox::size() const noexcept
{
    return _size;
}

}  // namespace dross
