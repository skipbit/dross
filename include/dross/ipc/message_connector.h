#pragma once

#include "dross/ipc/message_channel.h"
#include "dross/type/error.h"

#include <expected>
#include <memory>

namespace dross::ipc {

/**
 * @brief Makes connections to a place, as often as asked.
 *
 * A part implements this. connect() is safe from any thread.
 */
class message_connector {
public:
    /**
     * @brief Destructor.
     */
    virtual ~message_connector();

    message_connector(const message_connector& other) = delete;
    message_connector& operator=(const message_connector& other) = delete;

    /**
     * @brief Try once to connect.
     *
     * It blocks for as long as one try takes, which the part keeps short:
     * the library calls it from a thread of its own and waits between
     * tries, and what holds the connector waits for a try under way before
     * it is gone.
     *
     * @param events What to tell about the connection, which the part holds
     *        as message_channel_events says
     * @return The connection, or why the try failed
     */
    virtual std::expected<std::shared_ptr<message_channel>, error> connect(std::shared_ptr<message_channel_events> events) = 0;

protected:
    /**
     * @brief Construct the connector.
     */
    message_connector();
};

}  // namespace dross::ipc
