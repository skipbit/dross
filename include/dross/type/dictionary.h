#pragma once

#include <any>
#include <memory>
#include <string>

namespace dross {

class value;

class dictionary final {
public:
    dictionary();
    dictionary(const dictionary&);
    ~dictionary();

    bool equals(const dictionary&) const;
    bool contains(const std::string& key) const;
    bool empty() const;
    size_t size() const;

    bool operator==(const dictionary&) const;
    bool operator!=(const dictionary&) const;

    dictionary& operator=(const dictionary&);

    value& operator[](const std::string& key);
    const value& operator[](const std::string& key) const;

    class iterator;
    class const_iterator;

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;
    const_iterator cbegin() const;
    const_iterator cend() const;

private:
    class storage;
    std::unique_ptr<storage> _store;
};

class dictionary::iterator final {
public:
    using value_type = std::pair<const std::string&, value&>;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type;
    using iterator_category = std::forward_iterator_tag;

    iterator(const iterator&);
    ~iterator();

    iterator& operator++();
    value_type operator*() const;
    bool operator==(const iterator&) const;
    bool operator!=(const iterator&) const;

private:
    friend class dictionary;
    iterator(const std::any&);

private:
    class impl;
    std::unique_ptr<impl> _impl;
};

class dictionary::const_iterator final {
public:
    using value_type = std::pair<const std::string&, const value&>;
    using difference_type = std::ptrdiff_t;
    using pointer = const value_type*;
    using reference = value_type;
    using iterator_category = std::forward_iterator_tag;

    const_iterator(const const_iterator&);
    ~const_iterator();

    const_iterator& operator++();
    value_type operator*() const;
    bool operator==(const const_iterator&) const;
    bool operator!=(const const_iterator&) const;

private:
    friend class dictionary;
    const_iterator(const std::any&);

private:
    class impl;
    std::unique_ptr<impl> _impl;
};

}
