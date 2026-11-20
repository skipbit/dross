/**
 * @file ipc.h
 * @brief Places where processes meet.
 *
 * A place names where one process listens and others connect, and is given
 * to what communicates there:
 * @code
 * #include <dross/ipc.h>
 *
 * int main()
 * {
 *     const dross::ipc::unix_socket place{ dross::path{ std::string{ "/run/user/1000/player.sock" } } };
 *     return place.location().string().empty() ? 1 : 0;
 * }
 * @endcode
 *
 * Places:
 * - unix_socket: a Unix domain socket at a path
 *
 * Platform support:
 * - Linux and macOS only
 */

#pragma once

#include <dross/ipc/unix_socket.h>

/**
 * @brief Places where processes meet.
 *
 * @namespace dross::ipc
 */
