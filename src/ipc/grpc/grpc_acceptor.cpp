#include "ipc/grpc/cleanup_thread.h"
#include "ipc/grpc/grpc_place.h"
#include "ipc/grpc/reaction_scope.h"
#include "ipc/grpc/stream_state.h"
#include "messages.grpc.pb.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <grpcpp/grpcpp.h>
#include <mutex>
#include <system_error>
#include <utility>

namespace dross::ipc {

namespace {

using reactor = ::grpc::ServerBidiReactor<grpc_stream::Message, grpc_stream::Message>;

// The server of an acceptor, and the events of what it listens for. The
// streams it serves refer to it, and it outlives them: it is destroyed only
// once the server has shut down and every stream is done.
class acceptor_core final : public grpc_stream::Messages::CallbackService {
public:
    explicit acceptor_core(std::shared_ptr<message_acceptor_events> above)
        : _above{ std::move(above) }
    {
    }

    std::expected<void, error> start(const std::string& address, std::shared_ptr<::grpc::ServerCredentials> credentials)
    {
        ::grpc::ServerBuilder builder;
        builder.AddListeningPort(address, std::move(credentials));
        builder.SetMaxReceiveMessageSize(max_received_size);
        // Another server taking the same port would split the peers between
        // the two without a word, so the port is not shared.
        builder.AddChannelArgument(GRPC_ARG_ALLOW_REUSEPORT, 0);
        builder.RegisterService(this);
        _server = builder.BuildAndStart();
        if (! _server) {
            return std::unexpected(error{ std::errc::address_not_available, "gRPC did not start a server at " + address });
        }
        return {};
    }

    // Stops calling the events at once, and has every stream end. Shutting
    // the server down waits for the streams, which may be inside a call
    // right now, so it is left to finish().
    void stop()
    {
        const std::lock_guard guard{ _lock };
        _gone->store(true);
    }

    // Shuts the server down and waits for every stream to be done.
    void finish()
    {
        if (_server) {
            _server->Shutdown(std::chrono::system_clock::now());
            _server->Wait();
        }
        std::shared_ptr<message_acceptor_events> above;
        const std::lock_guard guard{ _lock };
        above = std::move(_above);
    }

    // Tells the events of a new stream's channel, one call at a time. Null
    // once the acceptor is stopping.
    std::shared_ptr<message_channel_events> accepted(std::shared_ptr<message_channel> channel)
    {
        const std::lock_guard accepting{ _accepting };
        std::shared_ptr<message_acceptor_events> above;
        {
            const std::lock_guard guard{ _lock };
            if (_gone->load()) {
                return nullptr;
            }
            above = _above;
        }
        const reaction_scope scope;
        auto events = above->accepted(std::move(channel));
        if (! events) {
            std::fputs("dross: message_acceptor_events::accepted: returned null\n", stderr);
            std::abort();
        }
        return events;
    }

    bool stopping() const
    {
        return _gone->load();
    }

    std::shared_ptr<const std::atomic<bool>> gone() const
    {
        return _gone;
    }

private:
    reactor* Exchange(::grpc::CallbackServerContext* context) override;

    mutable std::mutex _lock;
    std::mutex _accepting;
    std::shared_ptr<message_acceptor_events> _above;
    const std::shared_ptr<std::atomic<bool>> _gone{ std::make_shared<std::atomic<bool>>(false) };
    std::unique_ptr<::grpc::Server> _server;
};

// One stream the server serves, which deletes itself once gRPC is done
// with it.
class server_stream final : public reactor, private stream_operations {
public:
    server_stream(acceptor_core& core, const ::grpc::CallbackServerContext& context)
        : _core{ core }
        , _context{ context }
        , _state{ std::make_shared<stream_state>(static_cast<stream_operations&>(*this), core.gone()) }
    {
        _events = _core.accepted(std::make_shared<stream_channel>(_state));
        if (! _events) {
            _state->close();
            return;
        }
        StartRead(&_in);
    }

private:
    void write(const grpc_stream::Message& message) override
    {
        StartWrite(&message);
    }

    void end() override
    {
        Finish(::grpc::Status::OK);
    }

    void OnReadDone(const bool ok) override
    {
        if (! ok) {
            _state->peer_ended();
            return;
        }
        if (! _core.stopping() && _state->is_open()) {
            const reaction_scope scope;
            _events->received(bytes_of(_in));
        }
        StartRead(&_in);
    }

    void OnWriteDone(const bool ok) override
    {
        _state->written(ok);
    }

    void OnCancel() override
    {
        _state->peer_ended();
    }

    void OnDone() override
    {
        // Unless this side ended the stream, it ended in order when the peer
        // ended its side and the stream was not cancelled.
        const auto reason = _state->finished().value_or(_context.IsCancelled() ? error{ std::errc::connection_reset } : error{});
        auto events = std::move(_events);
        {
            // Letting the events go may destroy the acceptor.
            const reaction_scope scope;
            if (events && ! _core.stopping()) {
                events->closed(reason);
            }
            events.reset();
        }
        delete this;
    }

    acceptor_core& _core;
    const ::grpc::CallbackServerContext& _context;
    const std::shared_ptr<stream_state> _state;
    std::shared_ptr<message_channel_events> _events;
    grpc_stream::Message _in;
};

reactor* acceptor_core::Exchange(::grpc::CallbackServerContext* context)
{
    return new server_stream(*this, *context);
}

class grpc_acceptor final : public message_acceptor {
public:
    explicit grpc_acceptor(std::shared_ptr<acceptor_core> core)
        : _core{ std::move(core) }
    {
    }

    // gRPC cannot shut a server down from inside one of its reactions, so
    // when destroyed inside one, the cleanup thread finishes the job once
    // that reaction returns.
    ~grpc_acceptor() override
    {
        _core->stop();
        if (! reaction_scope::inside()) {
            _core->finish();
            return;
        }
        clean_up([core = _core] {
            core->finish();
        });
    }

    grpc_acceptor(const grpc_acceptor& other) = delete;
    grpc_acceptor& operator=(const grpc_acceptor& other) = delete;

private:
    const std::shared_ptr<acceptor_core> _core;
};

}  // namespace

std::expected<std::unique_ptr<message_acceptor>, error> grpc_listen_at(const std::string& address, std::shared_ptr<::grpc::ServerCredentials> credentials, std::shared_ptr<message_acceptor_events> events)
{
    if (auto cleanup = start_cleanup_thread(); ! cleanup) {
        return std::unexpected(std::move(cleanup).error());
    }
    auto core = std::make_shared<acceptor_core>(std::move(events));
    if (auto started = core->start(address, std::move(credentials)); ! started) {
        return std::unexpected(std::move(started).error());
    }
    return std::make_unique<grpc_acceptor>(std::move(core));
}

}  // namespace dross::ipc
