#pragma once

#include "dross/type/boolean.h"
#include "dross/type/number.h"  
#include "dross/type/string.h"
#include "dross/type/timestamp.h"
#include "dross/type/data.h"

#include <initializer_list>
#include <memory>

namespace dross {

class boolean;
class number;
class string;
class array;
class dictionary;
class timestamp;
class data;

/**
 * @brief Polymorphic value type that can hold any supported dross type.
 *
 * The value class provides a type-safe polymorphic container that can hold
 * any of the core dross types (boolean, number, string, array, dictionary, timestamp, data). It uses
 * std::variant internally for type safety and performance, and provides
 * convenient construction and conversion methods.
 *
 * Key features:
 * - Type-safe polymorphic storage using std::variant
 * - Value semantics (copyable and assignable)
 * - Convenient construction from any supported type
 * - Template-based type checking and casting
 * - Support for initializer list construction
 * - Thread-safe for read operations
 *
 * Supported types:
 * - boolean: True/false values with logical operations
 * - number: Arbitrary precision arithmetic
 * - string: UTF-8 text held as bytes
 * - array: Dynamic arrays of values
 * - dictionary: Key-value mappings
 * - timestamp: Date and time values with timezone support
 * - data: Binary data with encoding and I/O capabilities
 * - Arithmetic types (automatically converted to number)
 * - String types (automatically converted to string)
 *
 * Performance characteristics:
 * - Construction: O(1) for simple types, O(n) for complex types
 * - Type checking: O(1) compile-time and runtime checks
 * - Casting: O(1) with type safety validation
 * - Memory usage: Size of largest possible type plus small overhead
 *
 * Thread safety:
 * - Const operations are thread-safe
 * - Non-const operations require external synchronization
 *
 * @code
 * // Basic usage with different types
 * value bool_val = boolean{true};
 * value num = number{42};
 * value str = string{"hello"};
 * value arr = array{value{1}, value{2}, value{3}};
 *
 * // Automatic type conversion
 * value int_val = 123;      // Creates number
 * value str_val = "text";   // Creates string
 *
 * // Type checking and casting
 * if (num.is<number>()) {
 *     number n = value_cast<number>(num);
 * }
 *
 * // Initializer list construction
 * value list_val = {value{1}, value{"two"}, value{3.0}};
 * @endcode
 */
class value final {
public:
    /**
     * @brief Default constructor creating a null value.
     */
    value();

    /**
     * @brief Copy constructor.
     * @param other The value to copy from
     */
    value(const value& other);

    /**
     * @brief Construct from a boolean.
     * @param bool_val The boolean to store
     */
    value(const boolean& bool_val);

    /**
     * @brief Construct from a number.
     * @param num The number to store
     */
    value(const number& num);

    /**
     * @brief Construct from a string.
     * @param str The string to store
     */
    value(const string& str);

    /**
     * @brief Construct from an array.
     * @param arr The array to store
     */
    value(const array& arr);

    /**
     * @brief Construct from a dictionary.
     * @param dict The dictionary to store
     */
    value(const dictionary& dict);

    /**
     * @brief Construct from a timestamp.
     * @param ts The timestamp to store
     */
    value(const timestamp& ts);

    /**
     * @brief Construct from data.
     * @param d The data to store
     */
    value(const data& d);

    /**
     * @brief Construct an array from initializer list.
     * @param values Initializer list of values to create an array
     *
     * Creates an array value from the provided initializer list.
     */
    value(const std::initializer_list<value>& values);

    /**
     * @brief Destructor.
     */
    ~value();

    /**
     * @brief Construct from any arithmetic type.
     * @param n The arithmetic value to convert to number
     *
     * Automatically converts arithmetic types to number for convenient usage.
     */
    template <number_type T>
    value(const T n) : value(number(n)) {}

    /**
     * @brief Construct from any string-like type.
     * @param s The string value to convert to string
     *
     * Automatically converts string types to string for convenient usage.
     */
    template <string_type T>
    value(const T s) : value(string(s)) {}

    /**
     * @brief Test equality with another value.
     * @param other The value to compare with
     * @return true if both values contain the same type and data
     *
     * Performs deep comparison considering both type and content.
     */
    bool equals(const value& other) const;

    /**
     * @brief Equality comparison operator.
     * @param other The value to compare with
     * @return true if values are equal
     */
    bool operator==(const value& other) const;

    /**
     * @brief Inequality comparison operator.
     * @param other The value to compare with
     * @return true if values are not equal
     */
    bool operator!=(const value& other) const;

    /**
     * @brief Convert to boolean for truthiness testing.
     * @return true if the value is not null/empty, false otherwise
     *
     * Allows using value in boolean contexts like if statements.
     * Returns false for null values, empty strings, arrays, and dictionaries.
     */
    explicit operator bool() const;

    /**
     * @brief Assignment from nullptr (creates null value).
     * @param nullptr value to assign
     * @return Reference to this value
     */
    value& operator=(const std::nullptr_t);

    /**
     * @brief Copy assignment operator.
     * @param other The value to assign from
     * @return Reference to this value
     */
    value& operator=(const value& other);

    /**
     * @brief Assignment from boolean.
     * @param bool_val The boolean to assign
     * @return Reference to this value
     */
    value& operator=(const boolean& bool_val);

    /**
     * @brief Assignment from number.
     * @param num The number to assign
     * @return Reference to this value
     */
    value& operator=(const number& num);

    /**
     * @brief Assignment from string.
     * @param str The string to assign
     * @return Reference to this value
     */
    value& operator=(const string& str);

    /**
     * @brief Assignment from array.
     * @param arr The array to assign
     * @return Reference to this value
     */
    value& operator=(const array& arr);

    /**
     * @brief Assignment from dictionary.
     * @param dict The dictionary to assign
     * @return Reference to this value
     */
    value& operator=(const dictionary& dict);

    /**
     * @brief Assign a timestamp to this value.
     * @param ts The timestamp to assign
     * @return Reference to this value
     */
    value& operator=(const timestamp& ts);

    /**
     * @brief Assign data to this value.
     * @param d The data to assign
     * @return Reference to this value
     */
    value& operator=(const data& d);

    /**
     * @brief Check if the value contains a specific type.
     * @tparam T The type to check for
     * @return true if the value contains type T, false otherwise
     *
     * Use this before casting: as<T>() hands back a default-constructed T
     * when the value holds something else.
     * Supports checking for boolean, number, string, array, dictionary, timestamp, and data types.
     */
    template <class T>
    bool is() const noexcept;

    /**
     * @brief Cast the value to a specific type.
     * @tparam T The type to cast to
     * @return The value as the specified type
     *
     * This is a convenience method that delegates to value_cast.
     * When the value holds another type this returns a default-constructed
     * T rather than the contained object, so call is<T>() first. It does
     * not throw.
     */
    template <class T>
    T as() const noexcept;

private:
    template <class T>
    friend T value_cast(const value&) noexcept;

    class storage;
    std::unique_ptr<storage> _store;
};

/**
 * @brief Cast a value to a specific type.
 * @tparam T The target type to cast to
 * @param val The value to cast from
 * @return The contained object of type T
 *
 * When the value holds another type this returns a default-constructed T
 * rather than the contained object, so call value.is<T>() first. It does
 * not throw.
 * Supported types: boolean, number, string, array, dictionary, timestamp, data.
 *
 * @code
 * value val = number{42};
 * if (val.is<number>()) {
 *     number n = value_cast<number>(val);
 * }
 * @endcode
 */
template <class T>
T value_cast(const value& val) noexcept;

}
