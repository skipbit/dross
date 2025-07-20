#pragma once

#include <memory>
#include <string>
#include <iostream>

namespace dross {

/**
 * @brief Boolean type with value semantics and type safety.
 * 
 * The boolean class provides a type-safe boolean value with explicit conversions
 * and full integration with the dross type system. Unlike native bool, this class
 * provides consistent behavior across the type system and enables features like
 * null/undefined distinction when used with std::optional.
 * 
 * Key features:
 * - Type-safe boolean operations
 * - Seamless string conversion ("true"/"false")
 * - Integration with dross type system
 * - Value semantics (copyable and assignable)
 * - Three-way comparison support
 * - Safe conversions from various types
 * - Thread-safe for read operations
 * 
 * Performance characteristics:
 * - Construction: O(1)
 * - Comparison: O(1)
 * - String conversion: O(1) (returns constant strings)
 * - Memory usage: Minimal (single boolean value + Pimpl overhead)
 * 
 * Thread safety:
 * - Const operations are thread-safe
 * - Non-const operations require external synchronization
 * 
 * @code
 * // Basic usage
 * boolean flag{true};
 * boolean enabled{"true"};  // From string
 * boolean disabled{0};      // From integer (0 = false, non-zero = true)
 * 
 * // Boolean operations
 * if (flag) {
 *     // Handle true case
 * }
 * 
 * // Logical operations
 * boolean a{true};
 * boolean b{false};
 * boolean result = a && b;  // false
 * 
 * // String conversion (multiple approaches)
 * std::string str1 = flag;               // Implicit conversion
 * std::string str2 = to_string(flag);    // STL-style explicit conversion
 * 
 * // Comparison
 * if (flag == true) {
 *     // Direct comparison with bool
 * }
 * @endcode
 */
class boolean final {
public:
    /**
     * @brief Default constructor creating a boolean with value false.
     */
    boolean();
    
    /**
     * @brief Copy constructor.
     * @param other The boolean to copy from
     */
    boolean(const boolean& other);
    
    /**
     * @brief Construct from native bool.
     * @param value The boolean value
     */
    boolean(bool value);
    
    /**
     * @brief Construct from integer.
     * @param value The integer value (0 = false, non-zero = true)
     * 
     * Follows C++ convention where 0 is false and any non-zero value is true.
     */
    boolean(int value);
    
    /**
     * @brief Construct from string.
     * @param str String representation ("true" or "false", case-insensitive)
     * 
     * Accepts "true", "false", "TRUE", "FALSE", "True", "False".
     * Also accepts "1" for true and "0" for false.
     * Any other string value results in false.
     */
    boolean(const std::string& str);
    
    /**
     * @brief Construct from C-style string.
     * @param str C-style string representation
     * 
     * Same parsing rules as std::string constructor.
     */
    boolean(const char* str);
    
    /**
     * @brief Destructor.
     */
    ~boolean();

    /**
     * @brief Get the boolean value.
     * @return The underlying boolean value
     */
    bool value() const;
    
    
    /**
     * @brief Test equality with another boolean.
     * @param other The boolean to compare with
     * @return true if both booleans have the same value
     */
    bool equals(const boolean& other) const;
    
    /**
     * @brief Test equality with a native bool.
     * @param value The bool to compare with
     * @return true if this boolean equals the bool value
     */
    bool equals(bool value) const;

    /**
     * @brief Equality comparison operator.
     * @param other The boolean to compare with
     * @return true if both booleans are equal
     */
    bool operator==(const boolean& other) const;
    
    /**
     * @brief Inequality comparison operator.
     * @param other The boolean to compare with
     * @return true if booleans are not equal
     */
    bool operator!=(const boolean& other) const;

    /**
     * @brief Equality comparison with native bool.
     * @param value The bool to compare with
     * @return true if this boolean equals the bool value
     */
    bool operator==(bool value) const;
    
    /**
     * @brief Inequality comparison with native bool.
     * @param value The bool to compare with
     * @return true if this boolean does not equal the bool value
     */
    bool operator!=(bool value) const;

    /**
     * @brief Three-way comparison operator (spaceship operator).
     * @param other The boolean to compare with
     * @return std::strong_ordering result (false < true)
     * 
     * Defines a total ordering where false < true, consistent with
     * the standard boolean ordering in C++.
     */
    std::strong_ordering operator<=>(const boolean& other) const noexcept;

    /**
     * @brief Copy assignment operator.
     * @param other The boolean to assign from
     * @return Reference to this boolean
     */
    boolean& operator=(const boolean& other);
    
    /**
     * @brief Assignment from native bool.
     * @param value The bool value to assign
     * @return Reference to this boolean
     */
    boolean& operator=(bool value);

    /**
     * @brief Logical NOT operator.
     * @return New boolean with inverted value
     */
    boolean operator!() const;
    
    /**
     * @brief Logical AND operator.
     * @param other The boolean to AND with
     * @return New boolean with result of AND operation
     */
    boolean operator&&(const boolean& other) const;
    
    /**
     * @brief Logical OR operator.
     * @param other The boolean to OR with
     * @return New boolean with result of OR operation
     */
    boolean operator||(const boolean& other) const;

    /**
     * @brief Convert to native bool.
     * @return The underlying boolean value
     * 
     * Allows boolean to be used directly in conditional contexts.
     */
    operator bool() const;
    
    /**
     * @brief Convert to string.
     * @return "true" or "false"
     * 
     * Implicit conversion to string for seamless integration.
     */
    operator std::string() const;

    /**
     * @brief Create a true boolean value.
     * @return A boolean representing true
     * 
     * Factory method for creating true values.
     */
    static boolean T();
    
    /**
     * @brief Create a false boolean value.
     * @return A boolean representing false
     * 
     * Factory method for creating false values.
     */
    static boolean F();

private:
    class storage;
    std::unique_ptr<storage> _store;
};

/**
 * @brief Stream output operator for boolean.
 * @param os The output stream
 * @param b The boolean to output
 * @return Reference to the output stream
 *
 * Outputs the string representation of the boolean to the stream.
 * Outputs "true" for true values and "false" for false values.
 */
std::ostream& operator<<(std::ostream& os, const boolean& b);

}