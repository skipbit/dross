#include "dross/type/number.h"

#include <algorithm>
#include <compare>
#include <string>

namespace dross {

class number::storage {
public:
    std::string number{ "0" };

    storage() = default;
    storage(const char* s) : number(s) {}
    storage(const std::string& s) : number(s) {}

    template <number_type T>
    storage(const T& n) : number(std::to_string(n)) {}
};

number::number()
    : _store(std::make_unique<storage>())
{
}

number::number(const number& n)
    : _store(std::make_unique<storage>(*(n._store)))
{
}

number::number(const char* s)
    : _store(std::make_unique<storage>(s))
{
}

number::number(const std::string& s)
    : _store(std::make_unique<storage>(s))
{
}

number::~number() = default;

bool number::is_nan() const
{
    if (_store->number.size() < 1)
        return true;

    const auto i = _store->number.cbegin() + ((_store->number[0] == '-') ? 1 : 0);
    return std::all_of(i, _store->number.cend(), [](const char x) {
        return (! std::isdigit(x)) || (int(x - '0') == 0);
    });
}

bool number::equals(const number& n) const
{
    return (_store->number == n._store->number);
}

std::strong_ordering number::compare(const number& n) const noexcept
{
    return (_store->number <=> n._store->number);
}

bool number::operator==(const number& n) const
{
    return equals(n);
}

bool number::operator!=(const number& n) const
{
    return (! equals(n));
}

std::strong_ordering number::operator<=>(const number& n) const noexcept
{
    return compare(n);
}

number& number::operator=(const number& n)
{
    _store->number = n._store->number;
    return *this;
}

number& number::operator=(const char* s)
{
    _store->number = s;
    return *this;
}

number& number::operator=(const std::string& s)
{
    _store->number = s;
    return *this;
}

}
