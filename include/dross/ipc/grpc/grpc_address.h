#pragma once

#include "dross/ipc/message_acceptor.h"
#include "dross/type/error.h"

#include <expected>
#include <grpcpp/security/server_credentials.h>
#include <memory>
#include <string>

namespace dross::ipc {

/**
 * @brief A place to listen at over gRPC: an address and the credentials
 *        that guard it.
 *
 * A broadcaster is published there with dross::broadcaster::create(), and
 * a listener hears it through a dross::ipc::grpc_target naming the same
 * address:
 * @code
 * #include <dross/ipc/grpc/grpc_address.h>
 *
 * int main()
 * {
 *     const dross::ipc::grpc_address place{ "127.0.0.1:50051", grpc::InsecureServerCredentials() };
 *     return place.address().empty() ? 1 : 0;
 * }
 * @endcode
 *
 * It is part of dross::grpc, built when dross is configured with
 * DROSS_WITH_GRPC on.
 *
 * The credentials decide who may connect and whether the connection is
 * encrypted. Insecure credentials let anyone who reaches the address read
 * the notifications and send its own, so they fit only a network where
 * every peer is trusted.
 *
 * Only dross talks to dross here: what is carried on the stream is not a
 * format other programs are promised.
 *
 * Connections:
 * - A message is at most 64 KiB, and at most 1 MiB waits to be sent on one
 *   connection, as at a dross::ipc::unix_socket
 * - gRPC calls the events from threads of its own
 */
class grpc_address final {
public:
    /**
     * @brief Construct the place at address, guarded by credentials.
     * @param address Where to listen, in the form gRPC takes, such as
     *        "0.0.0.0:50051"
     * @param credentials Who may connect, never null: null ends the
     *        process as a misuse
     */
    grpc_address(std::string address, std::shared_ptr<grpc::ServerCredentials> credentials);

    /**
     * @brief Get where to listen.
     * @return The address the place was made with
     */
    const std::string& address() const noexcept;

    /**
     * @brief Listen at the address.
     *
     * A port another server listens at is not shared with it. An address
     * starting with "unix:" names a socket file, which gRPC takes over from
     * whatever listened there before; dross::ipc::unix_socket is the place
     * for processes on one machine.
     *
     * @param events What to tell about the connections
     * @return What listens, for as long as it is held, or why listening
     *         failed: std::errc::address_not_available when gRPC does not
     *         start a server there, as when another listens at the port.
     *         gRPC does not say why. Listening also starts the thread the
     *         gRPC part cleans up on, unless it runs already, and fails with
     *         std::errc::resource_unavailable_try_again when the system will
     *         not start it, or std::errc::not_enough_memory when there is
     *         no memory to start it
     */
    std::expected<std::unique_ptr<message_acceptor>, error> listen(std::shared_ptr<message_acceptor_events> events) const;

private:
    std::string _address;
    std::shared_ptr<grpc::ServerCredentials> _credentials;
};

}  // namespace dross::ipc
