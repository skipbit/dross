#include "dross/type/value.h"

#include "dross/type/array.h"
#include "dross/type/boolean.h"
#include "dross/type/dictionary.h"
#include "dross/type/number.h"
#include "dross/type/string.h"
#include "dross/type/datetime.h"

#include <memory>
#include <variant>

namespace dross {

class value::storage {
public:
    std::variant<std::monostate, dross::boolean, dross::number, dross::string, dross::array, dross::dictionary, dross::datetime> value;

    storage() = default;
    storage(const storage& s) : value(s.value) {}
};

value::value()
    : _store(std::make_unique<storage>())
{
}

value::value(const value& v)
    : _store(std::make_unique<storage>(*(v._store)))
{
}

value::value(const boolean& b)
    : _store(std::make_unique<storage>())
{
    _store->value = b;
}

value::value(const number& n)
    : _store(std::make_unique<storage>())
{
    _store->value = n;
}

value::value(const string& s)
    : _store(std::make_unique<storage>())
{
    _store->value = s;
}

value::value(const array& a)
    : _store(std::make_unique<storage>())
{
    _store->value = a;
}

value::value(const dictionary& d)
    : _store(std::make_unique<storage>())
{
    _store->value = d;
}

value::value(const datetime& dt)
    : _store(std::make_unique<storage>())
{
    _store->value = dt;
}

value::value(const std::initializer_list<value>& v)
    : value(array(v))
{
}

value::~value() = default;

bool value::equals(const value& v) const
{
    return (_store->value == v._store->value);
}

bool value::operator==(const value& v) const
{
    return equals(v);
}

bool value::operator!=(const value& v) const
{
    return (! equals(v));
}

value::operator bool() const
{
    return (! std::holds_alternative<std::monostate>(_store->value));
}

value& value::operator=(const std::nullptr_t)
{
    _store->value = std::monostate{};
    return *this;
}

value& value::operator=(const value& v)
{
    _store->value = v._store->value;
    return *this;
}

value& value::operator=(const boolean& b)
{
    _store->value = b;
    return *this;
}

value& value::operator=(const number& n)
{
    _store->value = n;
    return *this;
}

value& value::operator=(const string& s)
{
    _store->value = s;
    return *this;
}

value& value::operator=(const array& a)
{
    _store->value = a;
    return *this;
}

value& value::operator=(const dictionary& d)
{
    _store->value = d;
    return *this;
}

value& value::operator=(const datetime& dt)
{
    _store->value = dt;
    return *this;
}

template <class T>
bool value::is() const noexcept
{
    return std::holds_alternative<T>(_store->value);
}

template <class T>
T value::as() const noexcept
{
    return value_cast<T>(*this);
}

template <class T>
T value_cast(const value& original) noexcept
{
    return (std::holds_alternative<T>(original._store->value) ? std::get<T>(original._store->value) : T());
}

// Explicit instantiations
template bool value::is<boolean>() const noexcept;
template bool value::is<number>() const noexcept;
template bool value::is<string>() const noexcept;
template bool value::is<array>() const noexcept;
template bool value::is<dictionary>() const noexcept;
template bool value::is<datetime>() const noexcept;

template boolean value::as<boolean>() const noexcept;
template number value::as<number>() const noexcept;
template string value::as<string>() const noexcept;
template array value::as<array>() const noexcept;
template dictionary value::as<dictionary>() const noexcept;
template datetime value::as<datetime>() const noexcept;

template boolean value_cast<boolean>(const value&) noexcept;
template number value_cast<number>(const value&) noexcept;
template string value_cast<string>(const value&) noexcept;
template array value_cast<array>(const value&) noexcept;
template dictionary value_cast<dictionary>(const value&) noexcept;
template datetime value_cast<datetime>(const value&) noexcept;

}
