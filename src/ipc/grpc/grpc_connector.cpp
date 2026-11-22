#include "ipc/frame.h"
#include "ipc/grpc/grpc_place.h"
#include "ipc/grpc/reaction_scope.h"
#include "ipc/grpc/stream_state.h"
#include "messages.grpc.pb.h"

#include <chrono>
#include <grpcpp/grpcpp.h>
#include <system_error>
#include <thread>
#include <utility>

namespace dross::ipc {

namespace {

using reactor = ::grpc::ClientBidiReactor<grpc_stream::Message, grpc_stream::Message>;

// A message as large as a channel carries, with room for the field's tag
// and length.
constexpr int max_received_size = static_cast<int>(max_message_size) + 16;

// One stream to a server, which deletes itself once gRPC is done with it.
// Its channel starts writes from outside its reactions, so it holds the
// stream open until its side has ended.
class client_stream final : public reactor, private stream_operations {
public:
    client_stream(std::shared_ptr<::grpc::Channel> link, std::shared_ptr<message_channel_events> events)
        : _link{ std::move(link) }
        , _stub{ grpc_stream::Messages::NewStub(_link) }
        , _state{ std::make_shared<stream_state>(static_cast<stream_operations&>(*this), nullptr) }
        , _events{ std::move(events) }
    {
    }

    // Starts the stream, and gives the channel for it.
    std::shared_ptr<message_channel> start()
    {
        auto channel = std::make_shared<stream_channel>(_state);
        _stub->async()->Exchange(&_context, this);
        AddHold();
        StartRead(&_in);
        StartCall();
        return channel;
    }

private:
    void write(const grpc_stream::Message& message) override
    {
        StartWrite(&message);
    }

    void end() override
    {
        StartWritesDone();
        RemoveHold();
    }

    void OnReadDone(const bool ok) override
    {
        if (! ok) {
            _state->peer_ended(std::nullopt);
            return;
        }
        {
            const reaction_scope scope;
            _events->received(bytes_of(_in));
        }
        StartRead(&_in);
    }

    void OnWriteDone(const bool ok) override
    {
        _state->written(ok);
    }

    void OnDone(const ::grpc::Status& status) override
    {
        // Unless this side ended the stream, the server's status tells how
        // it ended.
        auto reason = _state->finished().value_or(status.ok() ? error{} : error{ std::errc::connection_reset, status.error_message() });
        auto events = std::move(_events);
        {
            const reaction_scope scope;
            events->closed(std::move(reason));
        }
        events.reset();
        // gRPC without its event engine, such as 1.51, shuts its callback
        // queue down with the last channel and joins the queue's threads,
        // one of which runs this; so the channel goes on a thread of its own.
        auto link = std::move(_link);
        auto stub = std::move(_stub);
        delete this;
        std::thread{ [link = std::move(link), stub = std::move(stub)]() mutable {
            stub.reset();
            link.reset();
        } }.detach();
    }

    std::shared_ptr<::grpc::Channel> _link;
    std::unique_ptr<grpc_stream::Messages::Stub> _stub;
    ::grpc::ClientContext _context;
    const std::shared_ptr<stream_state> _state;
    std::shared_ptr<message_channel_events> _events;
    grpc_stream::Message _in;
};

class grpc_connector final : public message_connector {
public:
    grpc_connector(std::string target, std::shared_ptr<::grpc::ChannelCredentials> credentials, const std::chrono::milliseconds try_limit)
        : _target{ std::move(target) }
        , _credentials{ std::move(credentials) }
        , _try_limit{ try_limit }
    {
    }

    std::expected<std::shared_ptr<message_channel>, error> connect(std::shared_ptr<message_channel_events> events) override
    {
        ::grpc::ChannelArguments arguments;
        arguments.SetMaxReceiveMessageSize(max_received_size);
        auto link = ::grpc::CreateCustomChannel(_target, _credentials, arguments);
        if (! link->WaitForConnected(std::chrono::system_clock::now() + _try_limit)) {
            return std::unexpected(error{ std::errc::timed_out });
        }
        return (new client_stream{ std::move(link), std::move(events) })->start();
    }

private:
    const std::string _target;
    const std::shared_ptr<::grpc::ChannelCredentials> _credentials;
    const std::chrono::milliseconds _try_limit;
};

}  // namespace

std::shared_ptr<message_connector> grpc_connector_to(std::string target, std::shared_ptr<::grpc::ChannelCredentials> credentials, const std::chrono::milliseconds try_limit)
{
    return std::make_shared<grpc_connector>(std::move(target), std::move(credentials), try_limit);
}

}  // namespace dross::ipc
