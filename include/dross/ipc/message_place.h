#pragma once

#include "dross/ipc/message_acceptor.h"
#include "dross/ipc/message_connector.h"
#include "dross/type/error.h"

#include <concepts>
#include <expected>
#include <memory>

namespace dross::ipc {

/**
 * @brief A place something can listen at, such as where a broadcaster is
 *        published.
 *
 * Its listen() starts listening and calls the events given as
 * message_acceptor_events says. What listens there, such as a published
 * broadcaster, keeps a copy of the place to listen again later.
 */
template <typename T>
concept message_listenable_place_type = std::copy_constructible<T> && requires(const T& place, std::shared_ptr<message_acceptor_events> events) {
    { place.listen(events) } -> std::same_as<std::expected<std::unique_ptr<message_acceptor>, error>>;
};

/**
 * @brief A place something can connect to, such as where a listener hears a
 *        broadcaster.
 *
 * Its connector() gives what connects to the place. What connects there,
 * such as a listener, keeps it for as long as it connects again.
 */
template <typename T>
concept message_connectable_place_type = requires(const T& place) {
    { place.connector() } -> std::same_as<std::shared_ptr<message_connector>>;
};

}  // namespace dross::ipc
