/**
 * @file type.h
 * @brief Type system header with core types and utility functions.
 * 
 * This header provides access to the complete dross type system including
 * the core polymorphic types (number, string, array, dictionary, value)
 * and utility functions for string manipulation and container operations.
 * 
 * The type system is designed around value semantics with no exceptions,
 * using std::optional and std::expected for error handling. All types
 * use the Pimpl idiom for ABI stability.
 * 
 * Key features:
 * - Arbitrary precision arithmetic with number
 * - Unicode-aware string handling
 * - Dynamic arrays and key-value dictionaries
 * - Polymorphic value type using std::variant
 * - Utility functions for common operations
 * 
 * @code
 * #include <dross/type.h>
 * using namespace dross;
 * 
 * // Core types
 * number precise{"99999999999999999999999999999"};
 * string text{"Hello, 世界!"};
 * array list = {value{1}, value{"two"}, value{3.14}};
 * dictionary config = {{"host", value{string{"localhost"}}},
 *                      {"port", value{number{8080}}}};
 * 
 * // Utility functions
 * auto tokens = split("a,b,c", ",");
 * auto combined = join(tokens, ";");
 * @endcode
 */

#pragma once

#include <dross/type/array.h>
#include <dross/type/dictionary.h>
#include <dross/type/number.h>
#include <dross/type/string.h>
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
