#include "dross/thread/operation.h"

#include <string>
#include <system_error>

namespace dross {

namespace {

class operation_category_type final : public std::error_category {
public:
    const char* name() const noexcept override
    {
        return "dross.operation";
    }

    std::string message(int value) const override
    {
        switch (static_cast<operation_errc>(value)) {
        case operation_errc::cancelled:
            return "operation was cancelled";
        case operation_errc::not_finished:
            return "operation has not finished";
        case operation_errc::type_mismatch:
            return "operation result is not of the requested type";
        }
        return "unknown operation error";
    }
};

}  // namespace

const std::error_category& operation_category() noexcept
{
    static const operation_category_type the_category;
    return the_category;
}

std::error_code make_error_code(operation_errc e) noexcept
{
    return { static_cast<int>(e), operation_category() };
}

}  // namespace dross
