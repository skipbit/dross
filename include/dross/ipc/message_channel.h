#pragma once

#include "dross/type/error.h"

#include <cstddef>
#include <expected>
#include <span>
#include <vector>

namespace dross::ipc {

/**
 * @brief What happens on one connection of a message_channel.
 *
 * The library implements this, and gives it to the part that carries the
 * messages, which calls it.
 *
 * Calls:
 * - received() any number of times, then closed() exactly once, last. A
 *   channel whose message_acceptor is destroyed first gets no closed()
 * - They come from the part's own threads, any of them, one at a time on
 *   the same events. Calls on the events of different channels may run at
 *   once
 * - They may start before the function the events were given to has
 *   returned
 * - The library checks the order. A part that breaks it ends the process:
 *   a line naming the rule goes to standard error, then std::abort() is
 *   called
 *
 * Lifetime:
 * - The part holds the events by the std::shared_ptr it was given, until
 *   closed() has returned or the part is destroyed, and then lets them go
 */
class message_channel_events {
public:
    /**
     * @brief Destructor.
     */
    virtual ~message_channel_events();

    message_channel_events(const message_channel_events& other) = delete;
    message_channel_events& operator=(const message_channel_events& other) = delete;

    /**
     * @brief A message arrived, whole and in the order it was sent.
     * @param message The message
     */
    virtual void received(std::vector<std::byte> message) = 0;

    /**
     * @brief The connection ended. Nothing more is called.
     * @param reason An empty error when either side ended it in order
     */
    virtual void closed(error reason) = 0;

protected:
    /**
     * @brief Construct the events.
     */
    message_channel_events();
};

/**
 * @brief One connection between two processes, carrying whole messages.
 *
 * A part that carries messages implements this. Every function is safe
 * from any thread.
 *
 * Lifetime:
 * - Letting a channel go does not close it; close() does
 * - Once it is closed, by either side, send() fails with
 *   std::errc::not_connected
 */
class message_channel {
public:
    /**
     * @brief Destructor.
     */
    virtual ~message_channel();

    message_channel(const message_channel& other) = delete;
    message_channel& operator=(const message_channel& other) = delete;

    /**
     * @brief Queue a message to be sent.
     * @param message The message, which the part copies before returning
     * @return Nothing, or why it was not queued: std::errc::not_connected,
     *         ipc_errc::message_too_large for one larger than the part
     *         carries, or ipc_errc::queue_overflow, which also closes the
     *         connection
     */
    virtual std::expected<void, error> send(std::span<const std::byte> message) = 0;

    /**
     * @brief End the connection, dropping what is still waiting to be sent.
     *
     * closed() still follows, from the part's threads. Calling it again does
     * nothing.
     */
    virtual void close() = 0;

protected:
    /**
     * @brief Construct the channel.
     */
    message_channel();
};

}  // namespace dross::ipc
