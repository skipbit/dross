#pragma once

#include <system_error>
#include <type_traits>

namespace dross {

/**
 * @brief Why carrying a message between processes failed.
 *
 * A connection that is not open fails with std::errc::not_connected.
 *
 * Like the error codes of the other modules, it is in namespace dross, and
 * its name says its module.
 */
enum class ipc_errc {
    message_too_large = 1,   ///< A message was larger than the connection carries
    queue_overflow = 2,      ///< More waited to be sent on a connection than it holds
    peer_not_same_user = 3,  ///< The process at the other end ran as another user
    message_cut_short = 4,   ///< The connection ended in the middle of a message
};

/**
 * @brief Get the category of ipc_errc, named "dross.ipc".
 * @return The one category object, the same on every call
 */
const std::error_category& ipc_category() noexcept;

/**
 * @brief Make an error code from an ipc_errc.
 * @param e The value
 * @return An error code of ipc_category()
 */
std::error_code make_error_code(ipc_errc e) noexcept;

}  // namespace dross

template <>
struct std::is_error_code_enum<dross::ipc_errc> : std::true_type { };
