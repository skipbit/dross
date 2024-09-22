#pragma once

#include "dross/type/number.h"
#include "dross/type/string.h"

#include <initializer_list>
#include <memory>

namespace dross {

class number;
class string;
class array;
class dictionary;

class value final {
public:
    value();
    value(const value&);
    value(const number&);
    value(const string&);
    value(const array&);
    value(const dictionary&);
    value(const std::initializer_list<value>&);
    ~value();

    template <number_type T>
    value(const T n) : value(number(n)) {}

    template <string_type T>
    value(const T s) : value(string(s)) {}

    bool equals(const value&) const;
    bool operator==(const value&) const;
    bool operator!=(const value&) const;

    explicit operator bool() const;

    value& operator=(const std::nullptr_t);
    value& operator=(const value&);
    value& operator=(const number&);
    value& operator=(const array&);
    value& operator=(const dictionary&);

private:
    template <class T>
    friend T value_cast(const value&) noexcept;

    class storage;
    std::unique_ptr<storage> _store;
};

template <class T>
T value_cast(const value&) noexcept;

}
