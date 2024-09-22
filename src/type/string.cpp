#include "dross/type/string.h"
#include <memory>

namespace dross {

class string::storage {
public:
    std::string buffer;

    storage() = default;
    storage(const std::string& s) : buffer(s) {}
};

string::string()
    : _store(std::make_unique<storage>())
{
}

string::string(const string& s)
    : _store(std::make_unique<storage>(*(s._store)))
{
}

string::string(const std::string& s)
    : _store(std::make_unique<storage>(s))
{
}

string::string(const char* c)
    : _store(std::make_unique<storage>(c))
{
}

string::~string() = default;

bool string::starts_with(const std::string& s) const
{
    return (_store->buffer.compare(0, s.size(), s) == 0);
}

size_t string::length() const
{
    return _store->buffer.size();
}

bool string::equals(const string& s) const
{
    return _store->buffer == s._store->buffer;
}

bool string::equals(const std::string& s) const
{
    return _store->buffer == s;
}

bool string::equals(const char* c) const
{
    return _store->buffer == c;
}

bool string::operator==(const string& s) const
{
    return equals(s);
}

bool string::operator!=(const string& s) const
{
    return (! equals(s));
}

bool string::operator==(const std::string& s) const
{
    return equals(s);
}

bool string::operator!=(const std::string& s) const
{
    return (! equals(s));
}

bool string::operator==(const char* c) const
{
    return equals(c);
}

bool string::operator!=(const char* c) const
{
    return (! equals(c));
}

string& string::operator=(const string& s)
{
    _store->buffer = s._store->buffer;
    return *this;
}

string& string::operator=(const std::string& s)
{
    _store->buffer = s;
    return *this;
}

string& string::operator=(const char* c)
{
    _store->buffer = c;
    return *this;
}

string& string::operator+=(const string& s)
{
    _store->buffer += s._store->buffer;
    return *this;
}

string& string::operator+=(const std::string& s)
{
    _store->buffer += s;
    return *this;
}

string& string::operator+=(const char* c)
{
    _store->buffer += c;
    return *this;
}

string::operator std::string() const
{
    return _store->buffer;
}

}
