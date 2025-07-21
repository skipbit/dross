/**
 * @file type.h
 * @brief Type system header with core types and utility functions.
 *
 * This header provides access to the complete dross type system including
 * the core polymorphic types (boolean, number, string, array, dictionary, datetime, timezone, value)
 * and utility functions for string manipulation and container operations.
 *
 * The type system is designed around value semantics with no exceptions,
 * using std::optional and std::expected for error handling. All types
 * use the Pimpl idiom for ABI stability.
 *
 * Key features:
 * - Arbitrary precision arithmetic with number
 * - Unicode-aware string handling
 * - Type-safe boolean operations
 * - Dynamic arrays and key-value dictionaries
 * - Date and time handling with timezone support
 * - Polymorphic value type using std::variant
 * - Seamless string conversion for all types
 * - Utility functions for common operations
 *
 * @code
 * #include <dross/type.h>
 * using namespace dross;
 *
 * // Core types
 * boolean flag{true};
 * number precise{"99999999999999999999999999999"};
 * string text{"Hello, 世界!"};
 * datetime meeting{2024, 1, 21, 15, 30, 0, timezone::offset(9)}; // +09:00
 * array list = {value{1}, value{"two"}, value{3.14}};
 * dictionary config = {{"host", value{string{"localhost"}}},
 *                      {"port", value{number{8080}}}};
 *
 * // Seamless string conversion (implicit)
 * std::string b_str = flag;    // "true"
 * std::string n_str = precise; // "99999999999999999999999999999"
 * std::string s_str = text;    // "Hello, 世界!"
 * std::string d_str = meeting; // "2024-01-21T15:30:00+09:00"
 *
 * // STL-style explicit conversion
 * auto b_string = to_string(flag);    // "true"
 * auto n_string = to_string(precise); // "99999999999999999999999999999"
 * auto s_string = to_string(text);    // "Hello, 世界!"
 * auto d_string = to_string(meeting); // "2024-01-21T15:30:00+09:00"
 *
 * // Stream output
 * std::cout << flag << " " << precise << " " << text << " " << meeting << std::endl;
 *
 * // Utility functions
 * auto tokens = split("a,b,c", ",");
 * auto combined = join(tokens, ";");
 * @endcode
 */

#pragma once

#include <dross/type/array.h>
#include <dross/type/boolean.h>
#include <dross/type/datetime.h>
#include <dross/type/dictionary.h>
#include <dross/type/number.h>
#include <dross/type/string.h>
#include <dross/type/timezone.h>
#include <dross/type/value.h>

#include <vector>

/**
 * @brief Main namespace for the dross type system and utilities.
 */
namespace dross {

/**
 * @brief Split a string into tokens using a delimiter.
 * @param s The input string to split
 * @param delimiter The delimiter string to split on
 * @return Vector of string tokens
 *
 * Splits the input string at each occurrence of the delimiter.
 * Empty tokens are preserved in the result. If the delimiter
 * is not found, returns a vector containing the original string.
 *
 * @code
 * auto tokens = split("apple,banana,cherry", ",");
 * // Result: {"apple", "banana", "cherry"}
 *
 * auto parts = split("a::b::c", "::");
 * // Result: {"a", "b", "c"}
 * @endcode
 */
std::vector<std::string> split(const std::string& s, const std::string& delimiter);

/**
 * @brief Join string tokens into a single string using a delimiter.
 * @param tokens The vector of strings to join
 * @param delimiter The delimiter string to insert between tokens
 * @return The joined string
 *
 * Concatenates all tokens with the delimiter inserted between each pair.
 * If the token vector is empty, returns an empty string. If there's only
 * one token, returns that token without any delimiter.
 *
 * @code
 * auto result = join({"apple", "banana", "cherry"}, ", ");
 * // Result: "apple, banana, cherry"
 *
 * auto path = join({"usr", "local", "bin"}, "/");
 * // Result: "usr/local/bin"
 * @endcode
 */
std::string join(const std::vector<std::string>& tokens, const std::string& delimiter);

/**
 * @brief Convert a boolean to string representation (STL-style).
 * @param b The boolean to convert
 * @return "true" or "false"
 *
 * Provides STL-style explicit string conversion similar to std::to_string().
 * Always returns lowercase "true" or "false" for consistency.
 *
 * @code
 * boolean flag{true};
 * auto str = to_string(flag);  // "true"
 * @endcode
 */
std::string to_string(const boolean& b);

/**
 * @brief Convert a number to string representation (STL-style).
 * @param n The number to convert
 * @return String representation of the number
 *
 * Provides STL-style explicit string conversion similar to std::to_string().
 * Returns "NaN" for NaN values. The string format preserves the full
 * precision of the number.
 *
 * @code
 * number n{42.5};
 * auto str = to_string(n);  // "42.5"
 * @endcode
 */
std::string to_string(const number& n);

/**
 * @brief Convert a string to string representation (STL-style).
 * @param s The string to convert
 * @return String representation (UTF-8 encoded)
 *
 * Provides STL-style explicit string conversion for consistency with
 * other types. Returns the UTF-8 encoded string content.
 *
 * @code
 * string text{"Hello, 世界!"};
 * auto str = to_string(text);  // "Hello, 世界!"
 * @endcode
 */
std::string to_string(const string& s);

/**
 * @brief Convert a datetime to string representation (STL-style).
 * @param dt The datetime to convert
 * @return ISO 8601 formatted string representation
 *
 * Provides STL-style explicit string conversion for consistency with
 * other types. Returns the datetime in ISO 8601 format with timezone
 * information if available.
 *
 * @code
 * datetime meeting{2024, 1, 21, 15, 30, 0, timezone::offset(9)}; // +09:00
 * auto str = to_string(meeting);  // "2024-01-21T15:30:00+09:00"
 * @endcode
 */
std::string to_string(const datetime& dt);

/**
 * @brief Convert a timezone to string representation (STL-style).
 * @param tz The timezone to convert
 * @return ISO 8601 formatted timezone string
 *
 * Provides STL-style explicit string conversion for consistency with
 * other types. Returns the timezone in ISO 8601 format (e.g., "+09:00", "Z").
 * Local timezone returns empty string.
 *
 * @code
 * timezone jst = timezone::offset(9);
 * auto str = to_string(jst);  // "+09:00"
 *
 * timezone utc = timezone::utc();
 * auto utc_str = to_string(utc);  // "Z"
 * @endcode
 */
std::string to_string(const timezone& tz);

/**
 * @brief Concept defining requirements for concatenatable container types.
 *
 * A container type must provide begin()/end() iterators and support
 * insertion at the end to be used with the concat() function.
 *
 * Supported containers include std::vector, std::list, std::deque,
 * and other standard library containers that meet these requirements.
 */
template <typename T> concept container_type = requires(T a) {
    { a.begin() } -> std::same_as<typename T::iterator>;
    { a.end() } -> std::same_as<typename T::iterator>;
    requires std::is_same_v<decltype(a.insert(a.end(), *a.begin())), typename T::iterator>;
};

/**
 * @brief Concatenate multiple containers into a single container.
 * @tparam T The primary container type (determines return type)
 * @tparam Ts Additional container types (must be compatible with T)
 * @param first The first container to concatenate
 * @param rest Additional containers to append
 * @return New container of type T containing all elements
 *
 * Creates a new container by concatenating all input containers in order.
 * The result type matches the first parameter type. All containers must
 * be compatible (same element type and satisfy container_type concept).
 *
 * @code
 * std::vector<int> v1 = {1, 2, 3};
 * std::vector<int> v2 = {4, 5};
 * std::vector<int> v3 = {6, 7, 8};
 *
 * auto result = concat(v1, v2, v3);
 * // Result: {1, 2, 3, 4, 5, 6, 7, 8}
 *
 * std::list<std::string> l1 = {"a", "b"};
 * std::list<std::string> l2 = {"c", "d"};
 * auto combined = concat(l1, l2);
 * // Result: {"a", "b", "c", "d"}
 * @endcode
 */
template<container_type T, typename... Ts>
T concat(const T& first, const Ts&... rest) {
    T result = first;
    (result.insert(result.end(), rest.begin(), rest.end()), ...);
    return result;
}

}
