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
