#pragma once

#include "dross/platform/path.h"

#include <utility>

namespace dross::ipc {

/**
 * @brief A place where processes meet: a Unix domain socket at a path.
 *
 * It only names the place. What listens there and what connects to it is
 * decided by what the place is given to:
 * @code
 * int main()
 * {
 *     const dross::ipc::unix_socket place{ dross::path{ std::string{ "/run/user/1000/player.sock" } } };
 *     return place.location().string().empty() ? 1 : 0;
 * }
 * @endcode
 *
 * The path is not checked here. A path the system cannot listen on or
 * connect to fails where the place is used.
 */
class unix_socket final {
public:
    /**
     * @brief Construct the place at location.
     * @param location The path of the socket file
     */
    explicit unix_socket(path location)
        : _location{ std::move(location) }
    {
    }

    /**
     * @brief Get the path of the socket file.
     * @return The path the place was made with
     */
    const path& location() const noexcept
    {
        return _location;
    }

private:
    path _location;
};

}  // namespace dross::ipc
