#pragma once

#include <compare>
#include <ostream>
#include <string>
#include <system_error>

namespace dross {

template <typename E>
concept error_enum_type = std::is_error_code_enum_v<E>;

class error {
public:
    error();
    error(const error&);
    error(int, const std::error_category&);
    virtual ~error();

    template <error_enum_type E>
    error(const E e) : _code(std::make_error_code(e)) {};

    std::string domain() const;
    int code() const noexcept;
    std::string message() const;

    const std::error_category& category() const noexcept;
    std::error_condition condition() const noexcept;

    explicit operator bool() const noexcept;

    template <error_enum_type E>
    bool operator==(const E e) const noexcept { return _code.default_error_condition() == e; }

    template <error_enum_type E>
    bool operator!=(const E e) const noexcept { return _code.default_error_condition() != e; }

    bool operator==(const std::error_category&) const noexcept;
    bool operator!=(const std::error_category&) const noexcept;

    std::strong_ordering operator<=>(const error&) const noexcept;

private:
    std::error_code _code;
};

template <class C, class T>
std::basic_ostream<C, T>& operator<<(std::basic_ostream<C, T>& os, const error& e) {
    os << e.domain() << ":" << e.code();
}

}
