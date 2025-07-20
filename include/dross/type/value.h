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

/**
 * @brief Polymorphic value type that can hold any supported dross type.
 * 
 * The value class provides a type-safe polymorphic container that can hold
 * any of the core dross types (number, string, array, dictionary). It uses
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
 * - number: Arbitrary precision arithmetic
 * - string: Unicode-aware strings
 * - array: Dynamic arrays of values
 * - dictionary: Key-value mappings
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
     * @brief Assignment from number.
     * @param num The number to assign
     * @return Reference to this value
     */
    value& operator=(const number& num);
    
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
     * @brief Check if the value contains a specific type.
     * @tparam T The type to check for
     * @return true if the value contains type T, false otherwise
     * 
     * Use this for type checking before casting to avoid exceptions.
     * Supports checking for number, string, array, and dictionary types.
     */
    template <class T>
    bool is() const noexcept;

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
 * @throws std::bad_cast if the value doesn't contain type T
 * 
 * Use value.is<T>() to check the type before casting to avoid exceptions.
 * Supported types: number, string, array, dictionary.
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
