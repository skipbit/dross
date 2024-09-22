#include "dross/type/value.h"

#include "dross/type/array.h"
#include "dross/type/dictionary.h"
#include "dross/type/number.h"
#include "dross/type/string.h"

#include <memory>
#include <variant>

namespace dross {

class value::storage {
public:
    std::variant<std::monostate, dross::number, dross::string, dross::array, dross::dictionary> value;

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

value& value::operator=(const number& n)
{
    _store->value = n;
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

template <class T>
T value_cast(const value& original) noexcept
{
    return (std::holds_alternative<T>(original._store->value) ? std::get<T>(original._store->value) : T());
}

}
