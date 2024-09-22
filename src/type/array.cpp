#include "dross/type/array.h"

#include "dross/type/value.h"
#include <algorithm>
#include <vector>

namespace dross {

class array::storage {
public:
    std::vector<value> contents;

    storage() = default;
    storage(const std::initializer_list<value>& list) : contents(list.begin(), list.end()) {}
};

array::array()
    : _store(std::make_unique<storage>())
{
}

array::array(const array& v)
    : _store(std::make_unique<storage>(*(v._store)))
{
}

array::array(const std::initializer_list<value>& list)
    : _store(std::make_unique<storage>(list))
{
}

array::~array() = default;

bool array::empty() const
{
    return _store->contents.empty();
}

size_t array::length() const
{
    return _store->contents.size();
}

bool array::equals(const array& a) const
{
    return _store->contents == a._store->contents;
}

size_t array::index_of(const value& v) const
{
    const auto i = std::find(_store->contents.begin(), _store->contents.end(), v);
    return (i != _store->contents.end()) ? std::distance(_store->contents.begin(), i) : -1;
}

value& array::value_at(const size_t i) const
{
    return _store->contents.at(i);
}

void array::append(const value& v)
{
    _store->contents.push_back(v);
}

void array::remove(const value& v)
{
    std::erase(_store->contents, v);
}

array::iterator array::begin()
{
    return iterator(std::make_any<std::vector<value>::iterator>(_store->contents.begin()));
}

array::iterator array::end()
{
    return iterator(std::make_any<std::vector<value>::iterator>(_store->contents.end()));
}

array::const_iterator array::begin() const
{
    return const_iterator(std::make_any<std::vector<value>::const_iterator>(_store->contents.cbegin()));
}

array::const_iterator array::end() const
{
    return const_iterator(std::make_any<std::vector<value>::const_iterator>(_store->contents.cend()));
}

array::const_iterator array::cbegin() const
{
    return const_iterator(std::make_any<std::vector<value>::const_iterator>(_store->contents.cbegin()));
}

array::const_iterator array::cend() const
{
    return const_iterator(std::make_any<std::vector<value>::const_iterator>(_store->contents.cend()));
}

bool array::operator==(const array& a) const
{
    return equals(a);
}

bool array::operator!=(const array& a) const
{
    return (! equals(a));
}

array& array::operator=(const array& a)
{
    _store->contents = a._store->contents;
    return *this;
}

value& array::operator[](const size_t i) const&
{
    return value_at(i);
}

value& array::operator[](const size_t i) &
{
    return value_at(i);
}

class array::iterator::impl {
public:
    impl(std::vector<value>::iterator i) : cursor(i) {}

    std::vector<value>::iterator cursor;
};

array::iterator::iterator(const std::any& a)
    : _impl(std::make_unique<impl>(std::any_cast<std::vector<value>::iterator>(a)))
{
}

array::iterator::~iterator() = default;

array::iterator& array::iterator::operator++()
{
    ++(_impl->cursor);
    return *this;
}

value& array::iterator::operator*()
{
    return *(_impl->cursor);
}

bool array::iterator::operator==(const iterator& i) const
{
    return (_impl->cursor == i._impl->cursor);
}

bool array::iterator::operator!=(const iterator& i) const
{
    return (_impl->cursor != i._impl->cursor);
}

class array::const_iterator::impl {
public:
    impl(std::vector<value>::const_iterator i) : cursor(i) {}

    std::vector<value>::const_iterator cursor;
};

array::const_iterator::const_iterator(const std::any& a)
    : _impl(std::make_unique<impl>(std::any_cast<std::vector<value>::const_iterator>(a)))
{
}

array::const_iterator::~const_iterator() = default;

array::const_iterator& array::const_iterator::operator++()
{
    ++(_impl->cursor);
    return *this;
}

const value& array::const_iterator::operator*() const
{
    return *(_impl->cursor);
}

bool array::const_iterator::operator==(const const_iterator& i) const
{
    return (_impl->cursor == i._impl->cursor);
}

bool array::const_iterator::operator!=(const const_iterator& i) const
{
    return (_impl->cursor != i._impl->cursor);
}

}
