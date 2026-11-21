#pragma once

#include <system_error>
#include <type_traits>

namespace dross {

/**
 * @brief Why an operation of the notification module failed.
 */
enum class notification_errc {
    empty_function = 1,        ///< The function given was empty, so there is nothing to call
    type_not_convertible = 2,  ///< The type has no dross::value_traits, so it cannot cross between processes
};

/**
 * @brief Get the category of notification_errc, named "dross.notification".
 * @return The one category object, the same on every call
 */
const std::error_category& notification_category() noexcept;

/**
 * @brief Make an error code from a notification_errc.
 * @param e The value
 * @return An error code of notification_category()
 */
std::error_code make_error_code(notification_errc e) noexcept;

}  // namespace dross

template <>
struct std::is_error_code_enum<dross::notification_errc> : std::true_type { };
