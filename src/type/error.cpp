#include "dross/type/error.h"

#include <compare>
#include <system_error>

namespace dross {

error::error() = default;

error::error(const error& e)
    : _code(e._code)
{
}

error::error(int value, const std::error_category& category)
    : _code(value, category)
{
}

error::~error() = default;

std::string error::domain() const
{
    return _code.category().name();
}

int error::code() const noexcept
{
    return _code.value();
}

std::string error::message() const
{
    return _code.message();
}

const std::error_category& error::category() const noexcept
{
    return _code.category();
}

std::error_condition error::condition() const noexcept
{
    return _code.default_error_condition();
}

error::operator bool() const noexcept
{
    return _code.operator bool();
}

bool error::operator==(const std::error_category& c) const noexcept
{
    return _code.category() == c;
}

bool error::operator!=(const std::error_category& c) const noexcept
{
    return _code.category() != c;
}

std::strong_ordering error::operator<=>(const error& e) const noexcept
{
    return (_code <=> e._code);
}

}
