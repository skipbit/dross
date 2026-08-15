#pragma once

#include <memory>
#include <string>

namespace dross {

/**
 * @brief Concept that defines string-like types for string construction.
 * 
 * Accepts const char* and std::string for convenient string creation.
 */
template <typename T>
concept string_type = std::same_as<T, const char*> || std::same_as<T, std::string>;

/**
 * @brief Unicode-aware string class with value semantics.
 * 
 * The string class provides Unicode-aware string handling with a focus on
 * correctness and safety. It uses internal UTF-8 encoding and provides
 * methods for common string operations while maintaining encoding integrity.
 * 
 * Key features:
 * - Unicode-aware string operations
 * - UTF-8 internal encoding
 * - Value semantics (copyable and assignable)
 * - Safe string manipulation methods
 * - Efficient string concatenation
 * - Thread-safe for read operations
 * 
 * Performance characteristics:
 * - Construction: O(n) where n is the string length
 * - Comparison: O(min(m,n)) for most cases
 * - Concatenation: O(m+n) where m and n are string lengths
 * - Memory usage: Proportional to UTF-8 encoded length
 * 
 * Thread safety:
 * - Const operations are thread-safe
 * - Non-const operations require external synchronization
 * 
 * @code
 * // Basic usage
 * string greeting{"Hello, 世界!"};
 * string name{"Alice"};
 * 
 * // String operations
 * if (greeting.starts_with("Hello")) {
 *     greeting += " " + name;
 * }
 * 
 * // Length and comparison
 * size_t len = greeting.length();  // Unicode-aware length
 * if (greeting == "Hello, 世界! Alice") {
 *     // Handle match
 * }
 * 
 * // Conversion
 * std::string std_str = greeting;
 * @endcode
 */
class string final {
public:
    /**
     * @brief Default constructor creating an empty string.
     */
    string();
    
    /**
     * @brief Copy constructor.
     * @param other The string to copy from
     */
    string(const string& other);
    
    /**
     * @brief Construct from std::string.
     * @param str The std::string to copy from
     * 
     * The input string is assumed to be valid UTF-8. Invalid UTF-8
     * sequences may result in undefined behavior.
     */
    string(const std::string& str);
    
    /**
     * @brief Construct from C-style string.
     * @param str Null-terminated C-style string
     * 
     * The input string is assumed to be valid UTF-8. Invalid UTF-8
     * sequences may result in undefined behavior.
     */
    string(const char* str);
    
    /**
     * @brief Destructor.
     */
    ~string();

    /**
     * @brief Check if the string starts with a given prefix.
     * @param prefix The prefix to check for
     * @return true if the string starts with the prefix, false otherwise
     * 
     * The comparison is Unicode-aware and case-sensitive.
     */
    bool starts_with(const std::string& prefix) const;

    /**
     * @brief Get the length of the string in bytes.
     * @return The number of bytes in the UTF-8 encoded content
     * 
     * Counts UTF-8 code units, not code points: a string holding non-ASCII
     * characters reports more than the number of characters it contains.
     */
    size_t length() const;
    
    /**
     * @brief Test equality with another string.
     * @param other The string to compare with
     * @return true if both strings contain the same Unicode sequence
     */
    bool equals(const string& other) const;
    
    /**
     * @brief Test equality with a std::string.
     * @param str The std::string to compare with
     * @return true if the strings are equal
     */
    bool equals(const std::string& str) const;
    
    /**
     * @brief Test equality with a C-style string.
     * @param str The C-style string to compare with
     * @return true if the strings are equal
     */
    bool equals(const char* str) const;

    /**
     * @brief Equality comparison operator.
     * @param other The string to compare with
     * @return true if strings are equal
     */
    bool operator==(const string& other) const;
    
    /**
     * @brief Inequality comparison operator.
     * @param other The string to compare with
     * @return true if strings are not equal
     */
    bool operator!=(const string& other) const;

    /**
     * @brief Equality comparison with std::string.
     * @param str The std::string to compare with
     * @return true if strings are equal
     */
    bool operator==(const std::string& str) const;
    
    /**
     * @brief Inequality comparison with std::string.
     * @param str The std::string to compare with
     * @return true if strings are not equal
     */
    bool operator!=(const std::string& str) const;

    /**
     * @brief Equality comparison with C-style string.
     * @param str The C-style string to compare with
     * @return true if strings are equal
     */
    bool operator==(const char* str) const;
    
    /**
     * @brief Inequality comparison with C-style string.
     * @param str The C-style string to compare with
     * @return true if strings are not equal
     */
    bool operator!=(const char* str) const;

    /**
     * @brief Copy assignment operator.
     * @param other The string to assign from
     * @return Reference to this string
     */
    string& operator=(const string& other);
    
    /**
     * @brief Assignment from std::string.
     * @param str The std::string to assign from
     * @return Reference to this string
     */
    string& operator=(const std::string& str);
    
    /**
     * @brief Assignment from C-style string.
     * @param str The C-style string to assign from
     * @return Reference to this string
     */
    string& operator=(const char* str);

    /**
     * @brief Append another string to this string.
     * @param other The string to append
     * @return Reference to this string after concatenation
     */
    string& operator+=(const string& other);
    
    /**
     * @brief Append a std::string to this string.
     * @param str The std::string to append
     * @return Reference to this string after concatenation
     */
    string& operator+=(const std::string& str);
    
    /**
     * @brief Append a C-style string to this string.
     * @param str The C-style string to append
     * @return Reference to this string after concatenation
     */
    string& operator+=(const char* str);

    /**
     * @brief Convert to std::string.
     * @return std::string representation with UTF-8 encoding
     * 
     * The returned string maintains the UTF-8 encoding and can be
     * used with standard library string operations.
     */
    operator std::string() const;

private:
    class storage;
    std::unique_ptr<storage> _store;
};

}
