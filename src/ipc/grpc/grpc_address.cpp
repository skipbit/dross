#include "dross/ipc/grpc/grpc_address.h"

#include "ipc/grpc/grpc_place.h"

#include <cstdio>
#include <cstdlib>
#include <utility>

namespace dross::ipc {

grpc_address::grpc_address(std::string address, std::shared_ptr<grpc::ServerCredentials> credentials)
    : _address{ std::move(address) }
    , _credentials{ std::move(credentials) }
{
    if (! _credentials) {
        std::fputs("dross: grpc_address: the credentials are null\n", stderr);
        std::abort();
    }
}

const std::string& grpc_address::address() const noexcept
{
    return _address;
}

std::expected<std::unique_ptr<message_acceptor>, error> grpc_address::listen(std::shared_ptr<message_acceptor_events> events) const
{
    return grpc_listen_at(_address, _credentials, std::move(events));
}

}  // namespace dross::ipc
