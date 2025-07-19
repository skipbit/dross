#pragma once

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

}
