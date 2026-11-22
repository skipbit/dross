#pragma once

#include "dross/ipc/message_channel.h"
#include "dross/type/error.h"
#include "ipc/limits.h"
#include "messages.pb.h"

#include <atomic>
#include <cstddef>
#include <deque>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

namespace dross::ipc {

// Hides a declaration from the library's exported symbol table, as
// poller.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_STREAM_STATE_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_STREAM_STATE_HIDDEN
#endif

// A message as large as a channel carries, with room for the field's tag
// and length.
inline constexpr int max_received_size = static_cast<int>(max_message_size) + 16;

// The operations a stream_state starts on its gRPC stream, which the
// reactor of the stream implements. They are started under the state's
// lock; gRPC runs no reaction inside them.
class DROSS_STREAM_STATE_HIDDEN stream_operations {
public:
    virtual ~stream_operations() = default;

    // Writes one message, which stays where it is until written() is called.
    virtual void write(const grpc_stream::Message& message) = 0;

    // Ends this side of the stream. Started once, with no write under way.
    virtual void end() = 0;
};

// What one gRPC stream and the channel handed out for it share: the
// messages waiting to be written, one write at a time, and whether the
// stream is still open.
class DROSS_STREAM_STATE_HIDDEN stream_state final {
public:
    // gone, when given, is set once what the stream belongs to is gone,
    // after which send() fails as on a closed stream.
    stream_state(stream_operations& operations, std::shared_ptr<const std::atomic<bool>> gone);

    stream_state(const stream_state& other) = delete;
    stream_state& operator=(const stream_state& other) = delete;

    // As message_channel::send() promises, with the limits of unix_socket.
    std::expected<void, error> send(std::span<const std::byte> message);

    // Ends the stream from this side, dropping what waits to be written.
    void close();

    // The write started last finished; ok is false when it failed.
    void written(bool ok);

    // The peer ended the stream. reason, when given, is what closed() is
    // to tell, unless this side ended it first.
    void peer_ended(std::optional<error> reason);

    // The stream is over and its reactor is going: no operation is started
    // after this. Returns the reason the stream ended with, if one is known.
    std::optional<error> finished();

private:
    void end_locked(std::optional<error> reason);

    std::mutex _lock;
    stream_operations* _operations;
    const std::shared_ptr<const std::atomic<bool>> _gone;
    std::deque<grpc_stream::Message> _waiting;
    std::size_t _waiting_bytes{ 0 };
    bool _open{ true };
    bool _writing{ false };
    bool _ended{ false };
    std::optional<error> _reason;
};

// The channel handed out for one gRPC stream.
class DROSS_STREAM_STATE_HIDDEN stream_channel final : public message_channel {
public:
    explicit stream_channel(std::shared_ptr<stream_state> state);

    std::expected<void, error> send(std::span<const std::byte> message) override;
    void close() override;

private:
    const std::shared_ptr<stream_state> _state;
};

// The bytes a message carries.
DROSS_STREAM_STATE_HIDDEN std::vector<std::byte> bytes_of(const grpc_stream::Message& message);

#undef DROSS_STREAM_STATE_HIDDEN

}  // namespace dross::ipc
