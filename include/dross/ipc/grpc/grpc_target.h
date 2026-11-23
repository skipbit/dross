#pragma once

#include "dross/ipc/message_connector.h"

#include <grpcpp/security/credentials.h>
#include <memory>
#include <string>

namespace dross::ipc {

/**
 * @brief A place to connect to over gRPC: a target and the credentials to
 *        reach it with.
 *
 * A listener hears there with dross::listener::create() the broadcaster
 * published at a dross::ipc::grpc_address:
 * @code
 * #include <dross/ipc/grpc/grpc_target.h>
 *
 * int main()
 * {
 *     const dross::ipc::grpc_target place{ "localhost:50051", grpc::InsecureChannelCredentials() };
 *     return place.target().empty() ? 1 : 0;
 * }
 * @endcode
 *
 * It is part of dross::grpc, built when dross is configured with
 * DROSS_WITH_GRPC on. Connections are as at a dross::ipc::grpc_address.
 */
class grpc_target final {
public:
    /**
     * @brief Construct the place at target, reached with credentials.
     * @param target Where to connect, in the form gRPC takes, such as
     *        "localhost:50051"
     * @param credentials How to reach it, never null: null ends the
     *        process as a misuse
     */
    grpc_target(std::string target, std::shared_ptr<grpc::ChannelCredentials> credentials);

    /**
     * @brief Get where to connect.
     * @return The target the place was made with
     */
    const std::string& target() const noexcept;

    /**
     * @brief Get what connects to the target.
     *
     * A try waits up to 5 seconds for the peer to answer, and fails with
     * std::errc::timed_out when it does not. A try also starts the thread
     * the gRPC part cleans up on, unless it runs already, and fails with
     * std::errc::resource_unavailable_try_again when the system will not
     * start it, and with std::errc::not_enough_memory when there is no
     * memory to start it.
     *
     * @return The connector
     */
    std::shared_ptr<message_connector> connector() const;

private:
    std::string _target;
    std::shared_ptr<grpc::ChannelCredentials> _credentials;
};

}  // namespace dross::ipc
