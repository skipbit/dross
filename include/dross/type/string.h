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
 * @brief UTF-8 string class with value semantics.
 * 
 * The string class holds text as UTF-8 bytes and hands those bytes back
 * unchanged. Its operations work on the bytes rather than on characters:
 * length() counts bytes, and the prefix and equality comparisons compare
 * bytes, so they can split or match across a multi-byte character. Nothing
 * validates the content, so the class neither repairs nor rejects malformed
 * UTF-8.
 * 
 * Key features:
 * - Byte-oriented operations over UTF-8 content
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
 * size_t len = greeting.length();  // 14: bytes, not the 10 characters
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
     * The bytes are stored as given. Nothing validates them, so content
     * that is not valid UTF-8 is kept and returned unchanged, and the
     * byte-oriented operations simply operate on it.
     */
    string(const std::string& str);
    
    /**
     * @brief Construct from C-style string.
     * @param str Null-terminated C-style string
     * 
     * The bytes are stored as given, with no UTF-8 validation.
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
     * The comparison is byte-wise over the UTF-8 content and
     * case-sensitive, so a prefix that ends mid-character still matches.
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
     * @return true if both strings hold the same bytes
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
