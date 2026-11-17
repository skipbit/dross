#include "dross/type/value_traits.h"

#include <string>
#include <system_error>

namespace dross {

namespace {

class value_category_type final : public std::error_category {
public:
    const char* name() const noexcept override
    {
        return "dross.value";
    }

    std::string message(int value) const override
    {
        switch (static_cast<value_errc>(value)) {
        case value_errc::kind_mismatch:
            return "value holds another kind";
        case value_errc::out_of_range:
            return "number is out of range for the type";
        }
        return "unknown value error";
    }
};

}  // namespace

const std::error_category& value_category() noexcept
{
    static const value_category_type the_category;
    return the_category;
}

std::error_code make_error_code(value_errc e) noexcept
{
    return { static_cast<int>(e), value_category() };
}

}  // namespace dross
