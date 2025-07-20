#include "dross/type/boolean.h"
#include <algorithm>
#include <cctype>

namespace dross {

// Boolean type implementation using Pimpl idiom

namespace {
    // Helper function to parse string to boolean
    bool parse_boolean_string(const std::string& str)
    {
        if (str.empty()) {
            return false;
        }

        // Convert to lowercase for case-insensitive comparison
        std::string lower_str = str;
        std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(),
                      [](unsigned char c) { return std::tolower(c); });

        // Accept "true", "1" as true
        if (lower_str == "true" || lower_str == "1") {
            return true;
        }

        // Accept "false", "0" as false (explicitly, though default is false)
        if (lower_str == "false" || lower_str == "0") {
            return false;
        }

        // Any other string defaults to false
        return false;
    }
}

class boolean::storage {
public:
    bool value{false};
};

boolean::boolean()
    : _store(std::make_unique<storage>())
{
}

boolean::boolean(const boolean& other)
    : _store(std::make_unique<storage>(*other._store))
{
}

boolean::boolean(bool value)
    : _store(std::make_unique<storage>())
{
    _store->value = value;
}

boolean::boolean(int value)
    : _store(std::make_unique<storage>())
{
    _store->value = (value != 0);
}

boolean::boolean(const std::string& str)
    : _store(std::make_unique<storage>())
{
    _store->value = parse_boolean_string(str);
}

boolean::boolean(const char* str)
    : boolean(std::string(str))
{
}

boolean::~boolean() = default;

bool boolean::value() const
{
    return _store->value;
}

bool boolean::equals(const boolean& other) const
{
    return _store->value == other._store->value;
}

bool boolean::equals(bool value) const
{
    return _store->value == value;
}

bool boolean::operator==(const boolean& other) const
{
    return equals(other);
}

bool boolean::operator!=(const boolean& other) const
{
    return !equals(other);
}

bool boolean::operator==(bool value) const
{
    return equals(value);
}

bool boolean::operator!=(bool value) const
{
    return !equals(value);
}

std::strong_ordering boolean::operator<=>(const boolean& other) const noexcept
{
    // false < true
    if (_store->value == other._store->value) {
        return std::strong_ordering::equal;
    }
    return _store->value ? std::strong_ordering::greater : std::strong_ordering::less;
}

boolean& boolean::operator=(const boolean& other)
{
    if (this != &other) {
        _store->value = other._store->value;
    }
    return *this;
}

boolean& boolean::operator=(bool value)
{
    _store->value = value;
    return *this;
}

boolean boolean::operator!() const
{
    return boolean(!_store->value);
}

boolean boolean::operator&&(const boolean& other) const
{
    return boolean(_store->value && other._store->value);
}

boolean boolean::operator||(const boolean& other) const
{
    return boolean(_store->value || other._store->value);
}

boolean::operator bool() const
{
    return _store->value;
}

boolean::operator std::string() const
{
    return _store->value ? "true" : "false";
}

boolean boolean::T()
{
    return boolean(true);
}

boolean boolean::F()
{
    return boolean(false);
}

std::ostream& operator<<(std::ostream& os, const boolean& b)
{
    return os << static_cast<std::string>(b);
}

}
