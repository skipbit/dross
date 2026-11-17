#pragma once

#include "dross/type/array.h"
#include "dross/type/boolean.h"
#include "dross/type/data.h"
#include "dross/type/dictionary.h"
#include "dross/type/error.h"
#include "dross/type/number.h"
#include "dross/type/string.h"
#include "dross/type/timestamp.h"
#include "dross/type/value.h"

#include <charconv>
#include <concepts>
#include <expected>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dross {

/**
 * @brief Why value_traits<T>::from_value() could not make a T.
 */
enum class value_errc {
    kind_mismatch = 1,  ///< The value holds another kind, such as a string where a number is wanted
    out_of_range,       ///< The number does not fit the type, or has a fraction where an integer is wanted
};

/**
 * @brief Get the category of value_errc, named "dross.value".
 * @return The one category object, the same on every call
 */
const std::error_category& value_category() noexcept;

/**
 * @brief Make an error code from a value_errc.
 * @param e The value
 * @return An error code of value_category()
 */
std::error_code make_error_code(value_errc e) noexcept;

}  // namespace dross

template <>
struct std::is_error_code_enum<dross::value_errc> : std::true_type { };

namespace dross {

/**
 * @brief How a T becomes a dross::value and comes back from one.
 *
 * It is not defined for any type by default. A type is given a conversion by
 * specializing it in namespace dross with two static functions:
 *
 * @code
 * struct point {
 *     int x;
 *     int y;
 * };
 *
 * template <>
 * struct dross::value_traits<point> {
 *     static dross::value to_value(const point& p)
 *     {
 *         dross::dictionary d;
 *         d["x"] = p.x;
 *         d["y"] = p.y;
 *         return d;
 *     }
 *
 *     static std::expected<point, dross::error> from_value(const dross::value& v)
 *     {
 *         const auto* d = v.get_if<dross::dictionary>();
 *         if (d == nullptr || ! d->contains("x") || ! d->contains("y")) {
 *             return std::unexpected(dross::value_errc::kind_mismatch);
 *         }
 *         auto x = dross::value_traits<int>::from_value((*d)["x"]);
 *         auto y = dross::value_traits<int>::from_value((*d)["y"]);
 *         if (! x || ! y) {
 *             return std::unexpected(! x ? x.error() : y.error());
 *         }
 *         return point{ *x, *y };
 *     }
 * };
 *
 * int main()
 * {
 *     const dross::value held = dross::value_traits<point>::to_value(point{ 1, 2 });
 *     const auto back = dross::value_traits<point>::from_value(held);
 *     return back ? 0 : 1;
 * }
 * @endcode
 *
 * to_value() always succeeds. from_value() returns an error when the value
 * does not have the shape the type needs, since a value received from
 * elsewhere can be anything; value_errc names the reasons dross itself
 * reports.
 *
 * dross gives a conversion to:
 * - dross::value itself, and the kinds it holds: boolean, number, string,
 *   array, dictionary, timestamp and data
 * - bool, integer and floating-point types; character types have none, as
 *   dross::value does not take them as numbers
 * - std::string
 * - std::vector, std::optional, and std::map and std::unordered_map keyed by
 *   std::string, of any type that has a conversion
 *
 * @tparam T The type to convert
 */
template <typename T>
struct value_traits;

/**
 * @brief A type that value_traits converts to and from a dross::value.
 */
template <typename T>
concept value_convertible_type = requires(const T& object, const value& held) {
    { value_traits<T>::to_value(object) } -> std::same_as<value>;
    { value_traits<T>::from_value(held) } -> std::same_as<std::expected<T, error>>;
};

/**
 * @brief The conversion of dross::value itself, which keeps the value as it is.
 */
template <>
struct value_traits<value> {
    static value to_value(const value& object)
    {
        return object;
    }

    static std::expected<value, error> from_value(const value& held)
    {
        return held;
    }
};

/**
 * @brief The conversion of a kind dross::value holds: boolean, number,
 * string, array, dictionary, timestamp or data.
 *
 * from_value() fails with value_errc::kind_mismatch when the value holds
 * another kind, or is null.
 */
template <typename T>
    requires std::same_as<T, boolean> || std::same_as<T, number> || std::same_as<T, string> || std::same_as<T, array>
             || std::same_as<T, dictionary> || std::same_as<T, timestamp> || std::same_as<T, data>
struct value_traits<T> {
    static value to_value(const T& object)
    {
        return object;
    }

    static std::expected<T, error> from_value(const value& held)
    {
        if (const T* found = held.get_if<T>()) {
            return *found;
        }
        return std::unexpected(value_errc::kind_mismatch);
    }
};

/**
 * @brief The conversion of bool, held as a boolean.
 */
template <>
struct value_traits<bool> {
    static value to_value(const bool object)
    {
        return boolean{ object };
    }

    static std::expected<bool, error> from_value(const value& held)
    {
        if (const boolean* found = held.get_if<boolean>()) {
            return found->value();
        }
        return std::unexpected(value_errc::kind_mismatch);
    }
};

/**
 * @brief The conversion of an integer type, held as a number.
 *
 * from_value() fails with value_errc::kind_mismatch when the value is not a
 * number, and with value_errc::out_of_range when the number is NaN, has a
 * fraction, or does not fit T. A number made as floating point with no
 * fraction, such as 2.0, gives the integer.
 */
template <value_number_type T>
    requires std::integral<T>
struct value_traits<T> {
    static value to_value(const T object)
    {
        return number{ object };
    }

    static std::expected<T, error> from_value(const value& held)
    {
        const number* found = held.get_if<number>();
        if (found == nullptr) {
            return std::unexpected(value_errc::kind_mismatch);
        }
        if (! found->is_integer()) {
            return std::unexpected(value_errc::out_of_range);
        }
        // An integer number's text is its digits alone, with a sign when negative.
        const std::string text = static_cast<std::string>(*found);
        T result{};
        const auto [end, code] = std::from_chars(text.data(), text.data() + text.size(), result);
        if ((code != std::errc{}) || (end != text.data() + text.size())) {
            return std::unexpected(value_errc::out_of_range);
        }
        return result;
    }
};

/**
 * @brief The conversion of a floating-point type, held as a number.
 *
 * to_value() makes an infinity NaN, as number does. from_value() rounds the
 * number to the nearest value of double, and then of T; a long double gets
 * no more precision than double. It fails with value_errc::kind_mismatch
 * when the value is not a number, and with value_errc::out_of_range when the
 * number is beyond the largest finite value of T.
 */
template <std::floating_point T>
struct value_traits<T> {
    static value to_value(const T object)
    {
        return number{ object };
    }

    static std::expected<T, error> from_value(const value& held)
    {
        const number* found = held.get_if<number>();
        if (found == nullptr) {
            return std::unexpected(value_errc::kind_mismatch);
        }
        if (found->is_nan()) {
            return std::numeric_limits<T>::quiet_NaN();
        }
        // The number goes through double, so a wider T is bounded as double is.
        using bound_type = std::conditional_t<(sizeof(T) > sizeof(double)), double, T>;
        const number largest{ std::numeric_limits<bound_type>::max() };
        const number lowest{ std::numeric_limits<bound_type>::lowest() };
        if ((*found > largest) || (*found < lowest)) {
            return std::unexpected(value_errc::out_of_range);
        }
        return static_cast<T>(static_cast<double>(*found));
    }
};

/**
 * @brief The conversion of std::string, held as a string.
 *
 * from_value() fails with value_errc::kind_mismatch when the value holds
 * another kind, or is null.
 */
template <>
struct value_traits<std::string> {
    static value to_value(const std::string& object)
    {
        return string{ object };
    }

    static std::expected<std::string, error> from_value(const value& held)
    {
        if (const string* found = held.get_if<string>()) {
            return static_cast<std::string>(*found);
        }
        return std::unexpected(value_errc::kind_mismatch);
    }
};

/**
 * @brief The conversion of std::vector, held as an array.
 *
 * from_value() fails with value_errc::kind_mismatch when the value is not an
 * array, and with the error of the first element that does not convert.
 */
template <value_convertible_type T>
struct value_traits<std::vector<T>> {
    static value to_value(const std::vector<T>& object)
    {
        array result;
        for (const auto& element : object) {
            result.append(value_traits<T>::to_value(element));
        }
        return result;
    }

    static std::expected<std::vector<T>, error> from_value(const value& held)
    {
        const array* found = held.get_if<array>();
        if (found == nullptr) {
            return std::unexpected(value_errc::kind_mismatch);
        }
        std::vector<T> result;
        result.reserve(found->length());
        for (const auto& element : *found) {
            auto converted = value_traits<T>::from_value(element);
            if (! converted) {
                return std::unexpected(std::move(converted).error());
            }
            result.push_back(std::move(*converted));
        }
        return result;
    }
};

/**
 * @brief The conversion of std::optional, held as null when it is empty.
 *
 * A null value comes back as an empty optional, so a T that is itself held
 * as null, such as an empty optional inside an optional, comes back as an
 * empty optional too.
 */
template <value_convertible_type T>
struct value_traits<std::optional<T>> {
    static value to_value(const std::optional<T>& object)
    {
        if (! object) {
            return value{};
        }
        return value_traits<T>::to_value(*object);
    }

    static std::expected<std::optional<T>, error> from_value(const value& held)
    {
        if (! held) {
            return std::optional<T>{};
        }
        auto converted = value_traits<T>::from_value(held);
        if (! converted) {
            return std::unexpected(std::move(converted).error());
        }
        return std::optional<T>{ std::move(*converted) };
    }
};

/**
 * @brief The conversion of std::map and std::unordered_map keyed by
 * std::string, held as a dictionary.
 *
 * from_value() fails with value_errc::kind_mismatch when the value is not a
 * dictionary, and with the error of the first entry that does not convert.
 */
template <typename M>
    requires(std::same_as<M, std::map<std::string, typename M::mapped_type>>
             || std::same_as<M, std::unordered_map<std::string, typename M::mapped_type>>)
            && value_convertible_type<typename M::mapped_type>
struct value_traits<M> {
    static value to_value(const M& object)
    {
        dictionary result;
        for (const auto& [key, element] : object) {
            result[key] = value_traits<typename M::mapped_type>::to_value(element);
        }
        return result;
    }

    static std::expected<M, error> from_value(const value& held)
    {
        const dictionary* found = held.get_if<dictionary>();
        if (found == nullptr) {
            return std::unexpected(value_errc::kind_mismatch);
        }
        M result;
        for (const auto& [key, element] : *found) {
            auto converted = value_traits<typename M::mapped_type>::from_value(element);
            if (! converted) {
                return std::unexpected(std::move(converted).error());
            }
            result.emplace(key, std::move(*converted));
        }
        return result;
    }
};

}  // namespace dross
