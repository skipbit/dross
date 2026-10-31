#include "dross/type/error.h"

#include <compare>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

namespace dross {

error::error() = default;

error::error(const error& e)
    : _code(e._code)
    , _detail(e._detail)
{
}

error& error::operator=(const error& e)
{
    _code = e._code;
    _detail = e._detail;
    return *this;
}

error::error(int value, const std::error_category& category)
    : _code(value, category)
{
}

error::error(std::errc e)
    : _code(std::make_error_code(e))
{
}

error::error(std::errc e, std::string detail)
    : _code(std::make_error_code(e))
    , _detail(std::make_shared<const std::string>(std::move(detail)))
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
    const std::string d = detail();
    return d.empty() ? _code.message() : _code.message() + ": " + d;
}

std::string error::detail() const
{
    return _detail ? *_detail : std::string{};
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

bool error::operator==(std::errc e) const noexcept
{
    return (_code == e);
}

bool error::operator==(const std::error_condition& c) const noexcept
{
    return (_code == c);
}

bool error::operator==(const std::error_category& c) const noexcept
{
    return _code.category() == c;
}

bool error::operator!=(const std::error_category& c) const noexcept
{
    return _code.category() != c;
}

bool error::operator==(const error& e) const noexcept
{
    return (_code == e._code);
}

bool error::operator!=(const error& e) const noexcept
{
    return (_code != e._code);
}

std::strong_ordering error::operator<=>(const error& e) const noexcept
{
    return (_code <=> e._code);
}

}  // namespace dross
