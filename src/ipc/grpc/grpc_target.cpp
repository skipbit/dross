#include "dross/ipc/grpc/grpc_target.h"

#include "ipc/grpc/grpc_place.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace dross::ipc {

namespace {

constexpr std::chrono::milliseconds try_limit = std::chrono::seconds{ 5 };

}  // namespace

grpc_target::grpc_target(std::string target, std::shared_ptr<grpc::ChannelCredentials> credentials)
    : _target{ std::move(target) }
    , _credentials{ std::move(credentials) }
{
    if (! _credentials) {
        std::fputs("dross: grpc_target: the credentials are null\n", stderr);
        std::abort();
    }
}

const std::string& grpc_target::target() const noexcept
{
    return _target;
}

std::shared_ptr<message_connector> grpc_target::connector() const
{
    return grpc_connector_to(_target, _credentials, try_limit);
}

}  // namespace dross::ipc
