#pragma once

#include <system_error>
#include <type_traits>

namespace dross {

/**
 * @brief Why an operation's result holds no value.
 */
enum class operation_errc {
    cancelled = 1,      ///< The operation was taken off its queue before it ran
    not_finished = 2,   ///< The operation has not finished yet
    type_mismatch = 3,  ///< The operation returned a different type from the one asked for
};

/**
 * @brief Get the category of operation_errc, named "dross.operation".
 * @return The one category object, the same on every call
 */
const std::error_category& operation_category() noexcept;

/**
 * @brief Make an error code from an operation_errc.
 * @param e The value
 * @return An error code of operation_category()
 */
std::error_code make_error_code(operation_errc e) noexcept;

}  // namespace dross

template <>
struct std::is_error_code_enum<dross::operation_errc> : std::true_type { };
