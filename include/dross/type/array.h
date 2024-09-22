#pragma once

#include <any>
#include <initializer_list>
#include <memory>

namespace dross {

class value;

class array final {
public:
    class iterator;
    class const_iterator;

    array();
    array(const array&);
    array(const std::initializer_list<value>&);
    ~array();

    bool empty() const;
    size_t length() const;
    bool equals(const array&) const;

    size_t index_of(const value&) const;
    value& value_at(const size_t) const;

    void append(const value&);
    void remove(const value&);

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;
    const_iterator cbegin() const;
    const_iterator cend() const;

    bool operator==(const array&) const;
    bool operator!=(const array&) const;

    array& operator=(const array&);

    value& operator[](const size_t) const&;
    value& operator[](const size_t) &;

private:
    class storage;
    std::unique_ptr<storage> _store;
};

class array::iterator final {
public:
    using value_type = value;
    using difference_type = std::ptrdiff_t;
    using pointer = value*;
    using reference = value&;
    using iterator_category = std::forward_iterator_tag;

    ~iterator();

    iterator& operator++();
    value& operator*();
    bool operator==(const iterator&) const;
    bool operator!=(const iterator&) const;

private:
    friend class array;
    iterator(const std::any&);

private:
    class impl;
    std::unique_ptr<impl> _impl;
};

class array::const_iterator final {
public:
    using value_type = value;
    using difference_type = std::ptrdiff_t;
    using pointer = const value*;
    using reference = const value&;
    using iterator_category = std::forward_iterator_tag;

    ~const_iterator();

    const_iterator& operator++();
    const value& operator*() const;
    bool operator==(const const_iterator&) const;
    bool operator!=(const const_iterator&) const;

private:
    friend class array;
    const_iterator(const std::any&);

private:
    class impl;
    std::unique_ptr<impl> _impl;
};

}
