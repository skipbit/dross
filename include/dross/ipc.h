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
 * Carrying messages:
 * - A part that carries messages between processes implements
 *   message_acceptor, message_connector and message_channel, and the
 *   library implements the events they call
 * - The part's place has a listen() that message_listenable_place_type
 *   describes, a connector() that message_connectable_place_type describes,
 *   or both
 *
 * Platform support:
 * - Linux and macOS only
 */

#pragma once

#include <dross/ipc/error.h>
#include <dross/ipc/message_acceptor.h>
#include <dross/ipc/message_channel.h>
#include <dross/ipc/message_connector.h>
#include <dross/ipc/message_place.h>
#include <dross/ipc/unix_socket.h>

/**
 * @brief Places where processes meet.
 *
 * @namespace dross::ipc
 */
