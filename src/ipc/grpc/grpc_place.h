#pragma once

#include "dross/ipc/message_acceptor.h"
#include "dross/ipc/message_connector.h"
#include "dross/type/error.h"

#include <chrono>
#include <expected>
#include <grpcpp/security/credentials.h>
#include <grpcpp/security/server_credentials.h>
#include <memory>
#include <string>

namespace dross::ipc {

// Hides a declaration from the library's exported symbol table, as
// poller.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_GRPC_PLACE_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_GRPC_PLACE_HIDDEN
#endif

// The gRPC part: each connection is one stream of the internal Messages
// service, carrying a message per stream message, with the limits of
// unix_socket. Its calls come from gRPC's threads. It never calls
// message_acceptor_events::stopped(), as a started gRPC server does not
// stop on its own. Listening and connecting start the part's cleanup thread
// unless it runs already, and fail as dross::thread::create() does when it
// does not start.

// Starts a gRPC server at address, which gRPC names as it does for
// grpc::ServerBuilder::AddListeningPort(). A server gRPC does not start
// fails with std::errc::address_not_available; gRPC does not say why.
DROSS_GRPC_PLACE_HIDDEN std::expected<std::unique_ptr<message_acceptor>, error> grpc_listen_at(const std::string& address, std::shared_ptr<::grpc::ServerCredentials> credentials, std::shared_ptr<message_acceptor_events> events);

// Connects to target, which gRPC names as it does for grpc::CreateChannel().
// A try that is not connected within try_limit fails with
// std::errc::timed_out.
DROSS_GRPC_PLACE_HIDDEN std::shared_ptr<message_connector> grpc_connector_to(std::string target, std::shared_ptr<::grpc::ChannelCredentials> credentials, std::chrono::milliseconds try_limit);

#undef DROSS_GRPC_PLACE_HIDDEN

}  // namespace dross::ipc
