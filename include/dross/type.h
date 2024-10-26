#pragma once

#include <dross/type/array.h>
#include <dross/type/dictionary.h>
#include <dross/type/number.h>
#include <dross/type/string.h>

#include <vector>

namespace dross {

/**
 * @brief Split a string into tokens.
 *
 * @param s The string to split.
 * @param delimiter The delimiter to split on.
 * @return std::vector<std::string> A vector of tokens.
 */
std::vector<std::string> split(const std::string& s, const std::string& d);

/**
 * @brief Join tokens into a string.
 *
 * @param c The tokens to join.
 * @param delimiter The delimiter to join with.
 * @return std::string The joined string.
 */
std::string join(const std::vector<std::string>& c, const std::string& d);

/**
 * @brief Concatenate containers.
 *
 * @tparam T The container type.
 * @param a The container to concatenate.
 * @return T The concatenated container.
 */
template <typename T> concept container_type = requires(T a) {
    { a.begin() } -> std::same_as<typename T::iterator>;
    { a.end() } -> std::same_as<typename T::iterator>;
    requires std::is_same_v<decltype(a.insert(a.end(), *a.begin())), typename T::iterator>;
};

/**
 * @brief Concatenate containers.
 *
 * @tparam T The container type.
 * @tparam Ts The container types.
 * @param a The container to concatenate.
 * @param b The container to concatenate.
 * @return T The concatenated container.
 */
template<container_type T, typename... Ts>
T concat(const T& a, const Ts&... b) {
    T c = a;
    (c.insert(c.end(), b.begin(), b.end()), ...);
    return c;
}

}
