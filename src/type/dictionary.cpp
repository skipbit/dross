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

class dictionary::iterator {
public:
    using iterator_type = std::map<std::string, dross::value>::iterator;
    using value_type = std::pair<const std::string&, dross::value&>;

    iterator(iterator_type it) : _it(it) {}

    value_type operator*() const {
        return {_it->first, _it->second};
    }

    iterator& operator++() {
        ++_it;
        return *this;
    }

    iterator operator++(int) {
        iterator tmp = *this;
        ++_it;
        return tmp;
    }

    bool operator==(const iterator& other) const {
        return _it == other._it;
    }

    bool operator!=(const iterator& other) const {
        return _it != other._it;
    }

private:
    iterator_type _it;
};

class dictionary::const_iterator {
public:
    using iterator_type = std::map<std::string, dross::value>::const_iterator;
    using value_type = std::pair<const std::string&, const dross::value&>;

    const_iterator(iterator_type it) : _it(it) {}

    value_type operator*() const {
        return {_it->first, _it->second};
    }

    const_iterator& operator++() {
        ++_it;
        return *this;
    }

    const_iterator operator++(int) {
        const_iterator tmp = *this;
        ++_it;
        return tmp;
    }

    bool operator==(const const_iterator& other) const {
        return _it == other._it;
    }

    bool operator!=(const const_iterator& other) const {
        return _it != other._it;
    }

private:
    iterator_type _it;
};

dictionary::iterator dictionary::begin()
{
    return iterator(_store->properties.begin());
}

dictionary::iterator dictionary::end()
{
    return iterator(_store->properties.end());
}

dictionary::const_iterator dictionary::begin() const
{
    return const_iterator(_store->properties.begin());
}

dictionary::const_iterator dictionary::end() const
{
    return const_iterator(_store->properties.end());
}

dictionary::const_iterator dictionary::cbegin() const
{
    return const_iterator(_store->properties.cbegin());
}

dictionary::const_iterator dictionary::cend() const
{
    return const_iterator(_store->properties.cend());
}

}
