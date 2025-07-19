#include "dross/type/number.h"

#include <compare>
#include <string>
#include <optional>
#include <cmath>

namespace dross {

namespace {
    // Internal NaN representation (implementation detail)
    constexpr const char* NAN_VALUE = "__invalid__";

    // Helper function to parse number string to double
    std::optional<double> parse_number(const std::string& str) {
        if (str.empty()) return std::nullopt;

        try {
            size_t processed = 0;
            double value = std::stod(str, &processed);

            // Check if entire string was processed
            if (processed == str.length()) {
                return value;
            }
        } catch (const std::exception&) {
            // Invalid number format
        }
        return std::nullopt;
    }
}

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
    return !parse_number(_store->number).has_value();
}

bool number::equals(const number& n) const
{
    auto val1 = parse_number(_store->number);
    auto val2 = parse_number(n._store->number);

    // If both are valid numbers, compare numerically
    if (val1 && val2) {
        return *val1 == *val2;
    }

    // If both are invalid, compare as strings
    if (!val1 && !val2) {
        return _store->number == n._store->number;
    }

    // One valid, one invalid - not equal
    return false;
}

std::strong_ordering number::compare(const number& n) const noexcept
{
    auto val1 = parse_number(_store->number);
    auto val2 = parse_number(n._store->number);

    // If both are valid numbers, compare numerically
    if (val1 && val2) {
        if (*val1 < *val2) return std::strong_ordering::less;
        if (*val1 > *val2) return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }

    // If both are invalid, compare as strings
    if (!val1 && !val2) {
        return _store->number <=> n._store->number;
    }

    // Valid numbers are always greater than invalid ones
    if (val1 && !val2) return std::strong_ordering::greater;
    return std::strong_ordering::less;
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

number::operator std::string() const
{
    return _store->number;
}

number::operator int() const
{
    auto val = parse_number(_store->number);
    return val ? static_cast<int>(*val) : 0;
}

number::operator double() const
{
    auto val = parse_number(_store->number);
    return val ? *val : 0.0;
}

number::operator long long() const
{
    auto val = parse_number(_store->number);
    return val ? static_cast<long long>(*val) : 0LL;
}

// Arithmetic operators
number number::operator+(const number& other) const
{
    auto val1 = parse_number(_store->number);
    auto val2 = parse_number(other._store->number);

    if (val1 && val2) {
        return number{std::to_string(*val1 + *val2)};
    }
    return number::nan();  // Return NaN if either operand is invalid
}

number number::operator-(const number& other) const
{
    auto val1 = parse_number(_store->number);
    auto val2 = parse_number(other._store->number);

    if (val1 && val2) {
        return number{std::to_string(*val1 - *val2)};
    }
    return number::nan();
}

number number::operator*(const number& other) const
{
    auto val1 = parse_number(_store->number);
    auto val2 = parse_number(other._store->number);

    if (val1 && val2) {
        return number{std::to_string(*val1 * *val2)};
    }
    return number::nan();
}

number number::operator/(const number& other) const
{
    auto val1 = parse_number(_store->number);
    auto val2 = parse_number(other._store->number);

    if (val1 && val2 && *val2 != 0.0) {
        return number{std::to_string(*val1 / *val2)};
    }
    return number::nan();
}

number number::operator%(const number& other) const
{
    auto val1 = parse_number(_store->number);
    auto val2 = parse_number(other._store->number);

    if (val1 && val2 && *val2 != 0.0) {
        // Use fmod for floating point modulo
        double result = std::fmod(*val1, *val2);
        return number{std::to_string(result)};
    }
    return number::nan();
}

// Compound assignment operators
number& number::operator+=(const number& other)
{
    *this = *this + other;
    return *this;
}

number& number::operator-=(const number& other)
{
    *this = *this - other;
    return *this;
}

number& number::operator*=(const number& other)
{
    *this = *this * other;
    return *this;
}

number& number::operator/=(const number& other)
{
    *this = *this / other;
    return *this;
}

number& number::operator%=(const number& other)
{
    *this = *this % other;
    return *this;
}

// Static NaN accessor
number number::nan()
{
    return number{NAN_VALUE};
}

}
