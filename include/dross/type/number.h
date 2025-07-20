#pragma once

#include <memory>
#include <string>
#include <type_traits>
#include <iostream>

namespace dross {

/**
 * @brief Concept that defines arithmetic types suitable for number construction.
 *
 * Accepts all arithmetic types except const char* to avoid ambiguity
 * with string constructors.
 */
template <typename T>
concept number_type = std::is_arithmetic_v<T> && ! std::same_as<T, const char*>;

/**
 * @brief Arbitrary precision number class with string-based storage.
 *
 * The number class provides arbitrary precision arithmetic operations using
 * string-based internal representation. This allows handling numbers of any
 * size without overflow concerns, making it suitable for financial calculations,
 * cryptographic operations, and scientific computing where precision is critical.
 *
 * Key features:
 * - Arbitrary precision arithmetic (no overflow)
 * - String-based storage for maximum precision
 * - Support for integers and floating-point numbers
 * - Full set of arithmetic and comparison operators
 * - Conversion to/from standard arithmetic types
 * - NaN (Not a Number) support for error handling
 * - Value semantics (copyable and assignable)
 * - Thread-safe for read operations
 *
 * Performance characteristics:
 * - Construction: O(n) where n is the number of digits
 * - Arithmetic operations: O(max(m,n)) for addition/subtraction, O(m*n) for multiplication
 * - Comparisons: O(min(m,n)) for most cases
 * - Memory usage: Proportional to the number of digits
 *
 * Error handling:
 * - Invalid operations return NaN instead of throwing exceptions
 * - Division by zero results in NaN
 * - Invalid string representations result in NaN
 *
 * Thread safety:
 * - Const operations are thread-safe
 * - Non-const operations require external synchronization
 *
 * @code
 * // Basic usage
 * number a{"123456789012345678901234567890"};
 * number b{42};
 * number result = a * b;  // No overflow!
 *
 * // Arithmetic operations
 * number sum = number{"999999999999999999"} + number{"1"};
 *
 * // Comparisons
 * if (number{"3.14159"} > number{"3.14"}) {
 *     // Handle greater value
 * }
 *
 * // Conversion
 * double d = static_cast<double>(number{"123.456"});
 * std::string s = static_cast<std::string>(number{42});
 * @endcode
 */
class number {
public:
    /**
     * @brief Default constructor creating a number with value 0.
     */
    number();

    /**
     * @brief Copy constructor.
     * @param other The number to copy from
     */
    number(const number& other);

    /**
     * @brief Construct from C-style string.
     * @param str Null-terminated string representation of the number
     *
     * Accepts decimal numbers in standard notation (e.g., "123", "-45.67", "1.23e-4").
     * Invalid strings result in NaN.
     */
    number(const char* str);

    /**
     * @brief Construct from std::string.
     * @param str String representation of the number
     *
     * Accepts decimal numbers in standard notation (e.g., "123", "-45.67", "1.23e-4").
     * Invalid strings result in NaN.
     */
    number(const std::string& str);

    /**
     * @brief Construct from any arithmetic type.
     * @param n The arithmetic value to convert
     *
     * Converts standard arithmetic types (int, float, double, etc.) to number.
     * The conversion preserves the full precision of the input type.
     */
    template <number_type T>
    number(const T n) : number(std::to_string(n)) {}

    /**
     * @brief Destructor.
     */
    ~number();

    /**
     * @brief Check if this number represents NaN (Not a Number).
     * @return true if the number is NaN, false otherwise
     *
     * NaN results from invalid operations like division by zero,
     * invalid string parsing, or arithmetic errors.
     */
    bool is_nan() const;

    /**
     * @brief Check if this number represents an integer value.
     * @return true if the number has no fractional part, false otherwise
     *
     * Returns false for NaN values.
     */
    bool is_integer() const;

    /**
     * @brief Test equality with another number.
     * @param other The number to compare with
     * @return true if both numbers represent the same value, false otherwise
     *
     * NaN is not equal to any value, including itself.
     */
    bool equals(const number& other) const;

    /**
     * @brief Test equality with an arithmetic value.
     * @param n The arithmetic value to compare with
     * @return true if this number equals the arithmetic value, false otherwise
     */
    template <number_type T>
    bool equals(const T n) const { return equals(number(n)); }

    /**
     * @brief Three-way comparison with another number.
     * @param other The number to compare with
     * @return std::strong_ordering result (less, equal, greater, or unordered)
     *
     * Returns std::strong_ordering::unordered if either number is NaN.
     */
    std::strong_ordering compare(const number& other) const noexcept;

    /**
     * @brief Three-way comparison with an arithmetic value.
     * @param n The arithmetic value to compare with
     * @return std::strong_ordering result (less, equal, greater, or unordered)
     */
    template <number_type T>
    std::strong_ordering compare(const T n) const noexcept { return compare(number(n)); }

    /**
     * @brief Equality comparison operator.
     * @param other The number to compare with
     * @return true if both numbers are equal, false otherwise
     */
    bool operator==(const number& other) const;

    /**
     * @brief Inequality comparison operator.
     * @param other The number to compare with
     * @return true if numbers are not equal, false otherwise
     */
    bool operator!=(const number& other) const;

    /**
     * @brief Three-way comparison operator (spaceship operator).
     * @param other The number to compare with
     * @return std::strong_ordering result for use with comparison operators
     */
    std::strong_ordering operator<=>(const number& other) const noexcept;

    /**
     * @brief Equality comparison with arithmetic types.
     * @param n The arithmetic value to compare with
     * @return true if this number equals the arithmetic value
     */
    template <number_type T>
    bool operator==(const T n) const { return equals(n); }

    /**
     * @brief Inequality comparison with arithmetic types.
     * @param n The arithmetic value to compare with
     * @return true if this number does not equal the arithmetic value
     */
    template <number_type T>
    bool operator!=(const T n) const { return (! equals(n)); }

    /**
     * @brief Three-way comparison with arithmetic types.
     * @param n The arithmetic value to compare with
     * @return std::strong_ordering result for use with comparison operators
     */
    template <number_type T>
    std::strong_ordering operator<=>(const T n) const noexcept { return compare(n); }

    /**
     * @brief Copy assignment operator.
     * @param other The number to assign from
     * @return Reference to this number
     */
    number& operator=(const number& other);

    /**
     * @brief Assignment from C-style string.
     * @param str Null-terminated string representation
     * @return Reference to this number
     */
    number& operator=(const char* str);

    /**
     * @brief Assignment from std::string.
     * @param str String representation of the number
     * @return Reference to this number
     */
    number& operator=(const std::string& str);

    /**
     * @brief Assignment from arithmetic types.
     * @param n The arithmetic value to assign
     * @return Reference to this number
     */
    template <number_type T>
    number& operator=(const T n) { return operator=(number(n)); }

    /**
     * @brief Convert to string representation.
     * @return String representation of the number
     *
     * Returns "NaN" for NaN values. The string format preserves
     * the full precision of the number.
     */
    explicit operator std::string() const;

    /**
     * @brief Convert to int.
     * @return Integer representation, truncated if necessary
     * @throws std::runtime_error if the number is NaN or out of range
     */
    explicit operator int() const;

    /**
     * @brief Convert to double.
     * @return Double-precision floating-point representation
     * @throws std::runtime_error if the number is NaN
     *
     * May lose precision for very large numbers or numbers with
     * many decimal places.
     */
    explicit operator double() const;

    /**
     * @brief Convert to long long.
     * @return Long long integer representation, truncated if necessary
     * @throws std::runtime_error if the number is NaN or out of range
     */
    explicit operator long long() const;

    /**
     * @brief Addition operator.
     * @param other The number to add
     * @return Result of addition
     *
     * Returns NaN if either operand is NaN.
     */
    number operator+(const number& other) const;

    /**
     * @brief Subtraction operator.
     * @param other The number to subtract
     * @return Result of subtraction
     *
     * Returns NaN if either operand is NaN.
     */
    number operator-(const number& other) const;

    /**
     * @brief Multiplication operator.
     * @param other The number to multiply by
     * @return Result of multiplication
     *
     * Returns NaN if either operand is NaN.
     */
    number operator*(const number& other) const;

    /**
     * @brief Division operator.
     * @param other The divisor
     * @return Result of division
     *
     * Returns NaN if either operand is NaN or if dividing by zero.
     */
    number operator/(const number& other) const;

    /**
     * @brief Modulo operator.
     * @param other The divisor for modulo operation
     * @return Remainder after division
     *
     * Returns NaN if either operand is NaN, if the divisor is zero,
     * or if either operand is not an integer.
     */
    number operator%(const number& other) const;

    /**
     * @brief Addition assignment operator.
     * @param other The number to add
     * @return Reference to this number after addition
     */
    number& operator+=(const number& other);

    /**
     * @brief Subtraction assignment operator.
     * @param other The number to subtract
     * @return Reference to this number after subtraction
     */
    number& operator-=(const number& other);

    /**
     * @brief Multiplication assignment operator.
     * @param other The number to multiply by
     * @return Reference to this number after multiplication
     */
    number& operator*=(const number& other);

    /**
     * @brief Division assignment operator.
     * @param other The divisor
     * @return Reference to this number after division
     */
    number& operator/=(const number& other);

    /**
     * @brief Modulo assignment operator.
     * @param other The divisor for modulo operation
     * @return Reference to this number after modulo operation
     */
    number& operator%=(const number& other);

    /**
     * @brief Create a NaN (Not a Number) value.
     * @return A number representing NaN
     *
     * Use this to create NaN values for error conditions or
     * to test against NaN using is_nan().
     */
    static number nan();

private:
    class storage;
    std::unique_ptr<storage> _store;
};

/**
 * @brief Stream output operator for number.
 * @param os The output stream
 * @param n The number to output
 * @return Reference to the output stream
 *
 * Outputs the string representation of the number to the stream.
 * For NaN values, outputs "NaN".
 */
std::ostream& operator<<(std::ostream& os, const number& n);

}
