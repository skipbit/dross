#pragma once

#include <compare>
#include <memory>
#include <ostream>
#include <string>
#include <system_error>

namespace dross {

/**
 * @brief An enum that std::error_code can be built from.
 * @tparam E The enum type, with std::is_error_code_enum specialized as true
 */
template <typename E>
concept error_enum_type = std::is_error_code_enum_v<E>;

/**
 * @brief An error code with an optional detail about what failed.
 *
 * Wraps a std::error_code: a numeric value and the category it belongs to.
 * The detail is text that says what failed and where. Comparisons ignore it.
 * Only the constructor that takes a std::errc and a detail sets it.
 */
class error {
public:
    /**
     * @brief Construct the value that means no error.
     *
     * The code is 0 in system_category(), so operator bool returns false.
     */
    error();

    /**
     * @brief Copy constructor.
     * @param other The error to copy from
     */
    error(const error& other);

    /**
     * @brief Move constructor.
     * @param other The error to move from
     */
    error(error&& other) noexcept;

    /**
     * @brief Copy assignment operator.
     * @param other The error to copy from
     * @return Reference to this error
     */
    error& operator=(const error& other);

    /**
     * @brief Move assignment operator.
     * @param other The error to move from
     * @return Reference to this error
     */
    error& operator=(error&& other) noexcept;

    /**
     * @brief Construct from a numeric value and its category.
     * @param value The numeric value of the code
     * @param category The category the value belongs to
     */
    error(int value, const std::error_category& category);

    /**
     * @brief Construct from a std::errc.
     * @param e The error condition to name
     *
     * std::errc names error conditions rather than codes, so it gets its own
     * constructor. The code is std::make_error_code(e), in generic_category().
     */
    error(std::errc e);

    /**
     * @brief Construct from a std::errc with a detail.
     * @param e The error condition to name
     * @param detail What failed and where, such as the line of a parse error
     *
     * Comparisons ignore the detail.
     */
    error(std::errc e, std::string detail);

    /**
     * @brief Destructor.
     */
    virtual ~error();

    /**
     * @brief Construct from an error code enum.
     * @tparam E The enum type
     * @param e The enum value
     *
     * std::error_code's own constructor finds make_error_code by
     * argument-dependent lookup, so an enum from any namespace works.
     */
    template <error_enum_type E>
    error(const E e)
        : _code(e)
    {
    }

    /**
     * @brief Get the name of the category.
     * @return The name of the code's category
     */
    std::string domain() const;

    /**
     * @brief Get the numeric value of the code.
     * @return The value of the code
     */
    int code() const noexcept;

    /**
     * @brief Get the text that describes this error.
     * @return The message of the code, followed by ": " and the detail when
     *         there is one
     */
    std::string message() const;

    /**
     * @brief Get what failed and where.
     * @return The detail, or an empty string when there is none
     */
    std::string detail() const;

    /**
     * @brief Get the category of the code.
     * @return The code's category
     */
    const std::error_category& category() const noexcept;

    /**
     * @brief Get the default error condition of the code.
     * @return The condition that the code's category maps it to
     */
    std::error_condition condition() const noexcept;

    /**
     * @brief Check whether this holds an error.
     * @return true when the value of the code is not 0
     */
    explicit operator bool() const noexcept;

    /**
     * @brief Compare with an error code enum.
     * @tparam E The enum type
     * @param e The enum value to compare with
     * @return true when the category and the value both match, as
     *         std::error_code compares with an error code enum
     */
    template <error_enum_type E>
    bool operator==(const E e) const noexcept
    {
        return (_code == std::error_code(e));
    }

    /**
     * @brief Compare with a std::errc.
     * @param e The error condition to compare with
     * @return true when either category reports e as equivalent, as
     *         std::error_code compares with an error condition, so a
     *         system_category() code matches the std::errc it maps to
     */
    // Kept beside the std::error_condition overload: std::errc converts to
    // both std::error_condition and error, so without an exact match the
    // comparison would be ambiguous.
    bool operator==(std::errc e) const noexcept;

    /**
     * @brief Compare with a std::error_condition.
     * @param c The error condition to compare with
     * @return true when either category reports c as equivalent, the same as
     *         for std::errc
     *
     * An error condition enum from any library converts to it.
     */
    bool operator==(const std::error_condition& c) const noexcept;

    /**
     * @brief Check whether the code belongs to a category.
     * @param c The category to compare with
     * @return true when the code's category is c
     */
    bool operator==(const std::error_category& c) const noexcept;

    /**
     * @brief Check whether the code belongs to another category.
     * @param c The category to compare with
     * @return true when the code's category is not c
     */
    bool operator!=(const std::error_category& c) const noexcept;

    /**
     * @brief Equality comparison operator.
     * @param e The error to compare with
     * @return true when the category and the value both match; the detail is
     *         ignored
     */
    bool operator==(const error& e) const noexcept;

    /**
     * @brief Inequality comparison operator.
     * @param e The error to compare with
     * @return true when the category or the value differs; the detail is
     *         ignored
     */
    bool operator!=(const error& e) const noexcept;

    /**
     * @brief Three-way comparison operator.
     * @param e The error to compare with
     * @return The order of the categories, then of the values
     *
     * Categories order by address, so the order of errors from different
     * categories can differ from one program to another. Weak, as errors
     * that differ only in their detail are equivalent but can be told apart.
     */
    std::weak_ordering operator<=>(const error& e) const noexcept;

private:
    std::error_code _code;
    // Shared and immutable, so a copy does not copy the text.
    std::shared_ptr<const std::string> _detail;
};

/**
 * @brief Write an error to a stream.
 * @tparam C The character type of the stream, which has to be char
 * @tparam T The character traits of the stream
 * @param os The stream to write to
 * @param e The error to write
 * @return The stream
 *
 * Writes domain:code, followed by ": " and the detail when there is one.
 */
template <class C, class T>
std::basic_ostream<C, T>& operator<<(std::basic_ostream<C, T>& os, const error& e)
{
    os << e.domain() << ":" << e.code();
    if (const std::string detail = e.detail(); ! detail.empty()) {
        os << ": " << detail;
    }
    return os;
}

}  // namespace dross
