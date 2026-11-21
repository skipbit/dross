#include "dross/ipc/error.h"

#include <string>
#include <system_error>

namespace dross {

namespace {

class ipc_category_type final : public std::error_category {
public:
    const char* name() const noexcept override
    {
        return "dross.ipc";
    }

    std::string message(int value) const override
    {
        switch (static_cast<ipc_errc>(value)) {
        case ipc_errc::message_too_large:
            return "message is larger than the connection carries";
        case ipc_errc::queue_overflow:
            return "more is waiting to be sent than the connection holds";
        case ipc_errc::peer_not_same_user:
            return "peer runs as another user";
        case ipc_errc::message_cut_short:
            return "connection ended in the middle of a message";
        }
        return "unknown ipc error";
    }
};

}  // namespace

const std::error_category& ipc_category() noexcept
{
    static const ipc_category_type the_category;
    return the_category;
}

std::error_code make_error_code(ipc_errc e) noexcept
{
    return { static_cast<int>(e), ipc_category() };
}

}  // namespace dross
