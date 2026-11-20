#include "dross/notification/error.h"

#include <string>
#include <system_error>

namespace dross {

namespace {

class notification_category_type final : public std::error_category {
public:
    const char* name() const noexcept override
    {
        return "dross.notification";
    }

    std::string message(int value) const override
    {
        switch (static_cast<notification_errc>(value)) {
        case notification_errc::empty_function:
            return "subscriber function is empty";
        case notification_errc::message_too_large:
            return "message is larger than 64 KiB";
        case notification_errc::queue_overflow:
            return "more than 1 MiB is waiting to be sent";
        case notification_errc::peer_not_same_user:
            return "peer runs as another user";
        case notification_errc::message_cut_short:
            return "connection ended in the middle of a message";
        case notification_errc::type_not_convertible:
            return "type has no dross::value_traits to cross between processes";
        }
        return "unknown notification error";
    }
};

}  // namespace

const std::error_category& notification_category() noexcept
{
    static const notification_category_type the_category;
    return the_category;
}

std::error_code make_error_code(notification_errc e) noexcept
{
    return { static_cast<int>(e), notification_category() };
}

}  // namespace dross
