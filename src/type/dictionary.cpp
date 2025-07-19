#include "dross/type/dictionary.h"

#include "dross/type/value.h"
#include <map>
#include <memory>
#include <string>

namespace dross {

class dictionary::storage {
public:
    std::map<std::string, dross::value> properties;
};

dictionary::dictionary()
    : _store(std::make_unique<storage>())
{
}

dictionary::dictionary(const dictionary& v)
    : _store(std::make_unique<storage>(*(v._store)))
{
}

dictionary::~dictionary() = default;

bool dictionary::equals(const dictionary& d) const
{
    return _store->properties == d._store->properties;
}

bool dictionary::contains(const std::string& key) const
{
    return _store->properties.contains(key);
}

bool dictionary::empty() const
{
    return _store->properties.empty();
}

size_t dictionary::size() const
{
    return _store->properties.size();
}

bool dictionary::operator==(const dictionary& d) const
{
    return equals(d);
}

bool dictionary::operator!=(const dictionary& d) const
{
    return (! equals(d));
}

dictionary& dictionary::operator=(const dictionary& d)
{
    _store->properties = d._store->properties;
    return *this;
}

value& dictionary::operator[](const std::string& key)
{
    return _store->properties[key];
}

const value& dictionary::operator[](const std::string& key) const
{
    return _store->properties.at(key);
}


class dictionary::iterator::impl {
public:
    impl(std::map<std::string, dross::value>::iterator i) : cursor(i) {}

    std::map<std::string, dross::value>::iterator cursor;
};

dictionary::iterator::iterator(const std::any& a)
    : _impl(std::make_unique<impl>(std::any_cast<std::map<std::string, dross::value>::iterator>(a)))
{
}

dictionary::iterator::iterator(const iterator& i)
    : _impl(std::make_unique<impl>(*(i._impl)))
{
}

dictionary::iterator::~iterator() = default;

dictionary::iterator& dictionary::iterator::operator++()
{
    ++(_impl->cursor);
    return *this;
}

dictionary::iterator::value_type dictionary::iterator::operator*() const
{
    return {_impl->cursor->first, _impl->cursor->second};
}

bool dictionary::iterator::operator==(const iterator& i) const
{
    return (_impl->cursor == i._impl->cursor);
}

bool dictionary::iterator::operator!=(const iterator& i) const
{
    return (_impl->cursor != i._impl->cursor);
}

class dictionary::const_iterator::impl {
public:
    impl(std::map<std::string, dross::value>::const_iterator i) : cursor(i) {}

    std::map<std::string, dross::value>::const_iterator cursor;
};

dictionary::const_iterator::const_iterator(const std::any& a)
    : _impl(std::make_unique<impl>(std::any_cast<std::map<std::string, dross::value>::const_iterator>(a)))
{
}

dictionary::const_iterator::const_iterator(const const_iterator& i)
    : _impl(std::make_unique<impl>(*(i._impl)))
{
}

dictionary::const_iterator::~const_iterator() = default;

dictionary::const_iterator& dictionary::const_iterator::operator++()
{
    ++(_impl->cursor);
    return *this;
}

dictionary::const_iterator::value_type dictionary::const_iterator::operator*() const
{
    return {_impl->cursor->first, _impl->cursor->second};
}

bool dictionary::const_iterator::operator==(const const_iterator& i) const
{
    return (_impl->cursor == i._impl->cursor);
}

bool dictionary::const_iterator::operator!=(const const_iterator& i) const
{
    return (_impl->cursor != i._impl->cursor);
}

dictionary::iterator dictionary::begin()
{
    return iterator(std::make_any<std::map<std::string, dross::value>::iterator>(_store->properties.begin()));
}

dictionary::iterator dictionary::end()
{
    return iterator(std::make_any<std::map<std::string, dross::value>::iterator>(_store->properties.end()));
}

dictionary::const_iterator dictionary::begin() const
{
    return const_iterator(std::make_any<std::map<std::string, dross::value>::const_iterator>(_store->properties.cbegin()));
}

dictionary::const_iterator dictionary::end() const
{
    return const_iterator(std::make_any<std::map<std::string, dross::value>::const_iterator>(_store->properties.cend()));
}

dictionary::const_iterator dictionary::cbegin() const
{
    return const_iterator(std::make_any<std::map<std::string, dross::value>::const_iterator>(_store->properties.cbegin()));
}

dictionary::const_iterator dictionary::cend() const
{
    return const_iterator(std::make_any<std::map<std::string, dross::value>::const_iterator>(_store->properties.cend()));
}

}
