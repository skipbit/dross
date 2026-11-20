#pragma once

#include "dross/type/error.h"

#include <string>

namespace dross {

/**
 * @brief What happened to a listener's link to a broadcaster in another
 * process, as dross::listener::link carries it.
 */
struct link_event final {
    /**
     * @brief The kinds of event.
     */
    enum class kind {
        connected,      ///< The listener reached the broadcaster, and has asked for what it subscribes to
        disconnected,   ///< The connection ended; the listener connects again
        type_mismatch,  ///< A value came that the subscriber's type could not be made from
    };

    kind what;               ///< Which event it is
    std::string topic_name;  ///< For type_mismatch, the name of the topic; empty otherwise
    error reason;            ///< Why it disconnected or the value could not be read; empty on connected and on an orderly end
};

}  // namespace dross
