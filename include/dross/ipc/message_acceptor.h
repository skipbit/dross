#pragma once

#include "dross/ipc/message_channel.h"
#include "dross/type/error.h"

#include <memory>

namespace dross::ipc {

/**
 * @brief What happens at a place a part listens at.
 *
 * The library implements this, and gives it to the part, which calls it
 * from its own threads, one call at a time, possibly before the function
 * the events were given to has returned. The library checks this as
 * message_channel_events says.
 */
class message_acceptor_events {
public:
    /**
     * @brief Destructor.
     */
    virtual ~message_acceptor_events();

    message_acceptor_events(const message_acceptor_events& other) = delete;
    message_acceptor_events& operator=(const message_acceptor_events& other) = delete;

    /**
     * @brief A peer connected.
     * @param channel The new connection
     * @return What to tell about the connection, which the part holds as
     *         message_channel_events says. It is never null: a part given
     *         null ends the process as a misuse, as one that breaks the
     *         order of the calls does
     */
    virtual std::shared_ptr<message_channel_events> accepted(std::shared_ptr<message_channel> channel) = 0;

    /**
     * @brief The part stopped listening on a failure of its own.
     *
     * Every channel it accepted has had closed() first, and nothing more is
     * called after it. Not called when the acceptor is destroyed.
     *
     * @param reason Why it stopped
     */
    virtual void stopped(error reason) = 0;

protected:
    /**
     * @brief Construct the events.
     */
    message_acceptor_events();
};

/**
 * @brief Listening at a place, for as long as it is held.
 *
 * A part implements this.
 *
 * Destroying it:
 * - From another thread, it stops listening, and returns once nothing more
 *   is called on its events or on the events of the channels it accepted
 * - From inside one of those calls, it stops once that call returns
 * - Either way, the channels it accepted get no closed(), and their send()
 *   fails with std::errc::not_connected
 */
class message_acceptor {
public:
    /**
     * @brief Destructor, which stops listening.
     */
    virtual ~message_acceptor();

    message_acceptor(const message_acceptor& other) = delete;
    message_acceptor& operator=(const message_acceptor& other) = delete;

protected:
    /**
     * @brief Construct the acceptor.
     */
    message_acceptor();
};

}  // namespace dross::ipc
