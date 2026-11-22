#include "ipc/grpc/stream_state.h"

#include "dross/ipc/error.h"
#include "ipc/frame.h"
#include "ipc/outbox.h"

#include <system_error>
#include <utility>

namespace dross::ipc {

stream_state::stream_state(stream_operations& operations, std::shared_ptr<const std::atomic<bool>> gone)
    : _operations{ &operations }
    , _gone{ std::move(gone) }
{
}

std::expected<void, error> stream_state::send(const std::span<const std::byte> message)
{
    const std::lock_guard guard{ _lock };
    if (! _open || (_gone && _gone->load())) {
        return std::unexpected(error{ std::errc::not_connected });
    }
    if (message.size() > max_message_size) {
        return std::unexpected(error{ ipc_errc::message_too_large });
    }
    if (message.size() > (max_queued_bytes - _waiting_bytes)) {
        end_locked(error{ ipc_errc::queue_overflow });
        return std::unexpected(error{ ipc_errc::queue_overflow });
    }
    grpc_stream::Message& queued = _waiting.emplace_back();
    queued.set_data(reinterpret_cast<const char*>(message.data()), message.size());
    _waiting_bytes += message.size();
    if (! _writing) {
        _writing = true;
        _operations->write(_waiting.front());
    }
    return {};
}

void stream_state::close()
{
    const std::lock_guard guard{ _lock };
    end_locked(error{});
}

void stream_state::written(const bool ok)
{
    const std::lock_guard guard{ _lock };
    _waiting_bytes -= _waiting.front().data().size();
    _waiting.pop_front();
    if (! ok) {
        end_locked(std::nullopt);
    }
    if (! _open) {
        _writing = false;
        if (! _ended && _operations) {
            _ended = true;
            _operations->end();
        }
        return;
    }
    if (_waiting.empty()) {
        _writing = false;
        return;
    }
    _operations->write(_waiting.front());
}

void stream_state::peer_ended(std::optional<error> reason)
{
    const std::lock_guard guard{ _lock };
    end_locked(std::move(reason));
}

std::optional<error> stream_state::finished()
{
    const std::lock_guard guard{ _lock };
    _open = false;
    _operations = nullptr;
    return _reason;
}

void stream_state::end_locked(std::optional<error> reason)
{
    if (! _open) {
        return;
    }
    _open = false;
    _reason = std::move(reason);
    // The message under way stays until its write finishes.
    while (_waiting.size() > (_writing ? 1 : 0)) {
        _waiting_bytes -= _waiting.back().data().size();
        _waiting.pop_back();
    }
    if (! _writing && _operations) {
        _ended = true;
        _operations->end();
    }
}

stream_channel::stream_channel(std::shared_ptr<stream_state> state)
    : _state{ std::move(state) }
{
}

std::expected<void, error> stream_channel::send(const std::span<const std::byte> message)
{
    return _state->send(message);
}

void stream_channel::close()
{
    _state->close();
}

std::vector<std::byte> bytes_of(const grpc_stream::Message& message)
{
    const auto* data = reinterpret_cast<const std::byte*>(message.data().data());
    return { data, data + message.data().size() };
}

}  // namespace dross::ipc
