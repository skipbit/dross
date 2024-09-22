#pragma once

#include <compare>
#include <memory>
#include <string>
#include <type_traits>

namespace dross {

template <typename T>
concept number_type = std::is_arithmetic_v<T> && ! std::same_as<T, const char*>;

class number {
public:
    number();
    number(const number&);
    number(const char*);
    number(const std::string&);

    template <number_type T>
    number(const T n) : number(std::to_string(n)) {}

    ~number();

    bool is_nan() const;
    bool equals(const number&) const;

    template <number_type T>
    bool equals(const T n) const { return equals(number(n)); }

    std::strong_ordering compare(const number&) const noexcept;

    template <number_type T>
    std::strong_ordering compare(const T n) const noexcept { return compare(number(n)); }

    bool operator==(const number&) const;
    bool operator!=(const number&) const;
    std::strong_ordering operator<=>(const number&) const noexcept;

    template <number_type T>
    bool operator==(const T n) const { return equals(n); }

    template <number_type T>
    bool operator!=(const T n) const { return (! equals(n)); }

    template <number_type T>
    std::strong_ordering operator<=>(const T n) const noexcept { return compare(n); }

    number& operator=(const number&);
    number& operator=(const char*);
    number& operator=(const std::string&);

    template <number_type T>
    number& operator=(const T n) { return operator=(number(n)); }

private:
    class storage;
    std::unique_ptr<storage> _store;
};

}
