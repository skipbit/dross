#pragma once

#include "dross/notification/notification.h"
#include "dross/notification/topic.h"

#include <concepts>
#include <functional>
#include <memory>
#include <string_view>
#include <utility>

namespace dross {

/**
 * @brief Sends notifications to whoever subscribes to them through a
 * dross::listener.
 *
 * A broadcaster only sends. Receiving goes through a listener made from it,
 * so the two sides are told apart by their type.
 *
 * @code
 * struct progress {
 *     int done;
 *     int total;
 * };
 *
 * inline constexpr dross::topic<progress> kProgress{ "com.example.player.progress" };
 *
 * void play(dross::broadcaster news)
 * {
 *     for (int done = 1; done <= 10; ++done) {
 *         news.notify(kProgress, progress{ done, 10 });
 *     }
 * }
 *
 * int main()
 * {
 *     play(dross::broadcaster::create());
 * }
 * @endcode
 *
 * Sending:
 * - notify() hands the value to each subscriber of the topic, the way each
 *   one's dross::delivery says. Every subscriber is handed the same value,
 *   copied or moved once when notify() is called
 * - The subscribers are those subscribed when notify() is called. One that
 *   subscribes from inside a subscriber's function receives the next
 *   notification, not this one
 * - A notification sent while no one is subscribed reaches no one, and is
 *   not kept for later subscribers
 * - Notifications are not merged: each one sent is handed on
 *
 * Handle semantics:
 * - A broadcaster is a handle, made by create(). Copying gives another handle
 *   to the same broadcaster, and operator== asks whether two handles name
 *   the same one
 * - There is no empty handle and no null state, so moving copies
 *
 * Lifetime:
 * - A listener does not keep its broadcaster going. Once the last handle
 *   goes, nothing more is sent, and every subscription made through its
 *   listeners is no longer active
 *
 * Thread safety:
 * - Every operation is free of data races when called from any thread,
 *   including from inside a subscriber's function
 *
 * Exceptions:
 * - An exception thrown by a subscriber's function called with
 *   dross::delivery::direct() propagates out of notify(), and the
 *   subscribers after it are not handed the notification
 *
 * Type identity:
 * - Topic types are told apart through typeid, so code that sends or
 *   subscribes is compiled with run-time type information
 */
class broadcaster final {
public:
    /**
     * @brief Make a broadcaster that sends within this process.
     * @return A handle to the new broadcaster
     */
    [[nodiscard]] static broadcaster create();

    /**
     * @brief Copy constructor, giving another handle to the same
     * broadcaster.
     * @param other The handle to copy
     */
    broadcaster(const broadcaster& other);

    /**
     * @brief Destructor.
     */
    ~broadcaster();

    /**
     * @brief Copy assignment, naming the same broadcaster as the source.
     * @param other The handle to copy
     * @return Reference to this handle
     */
    broadcaster& operator=(const broadcaster& other);

    /**
     * @brief Send value on which to its subscribers.
     * @param which The topic
     * @param value The value, moved into the one copy every subscriber is
     * handed
     * @return The notification sent
     *
     * Returns once each subscriber has been handed the notification: after
     * the function has run for one given dross::delivery::direct(), and
     * once it is queued for the others.
     */
    template <std::move_constructible T>
    notification notify(const topic<T>& which, T value);

    /**
     * @brief Test whether two handles name the same broadcaster.
     * @param other The handle to compare with
     * @return true when both name one broadcaster
     */
    bool operator==(const broadcaster& other) const noexcept;

private:
    class storage;

    explicit broadcaster(std::shared_ptr<storage> store) noexcept;

    // notify() without its type. make is called only when someone is
    // subscribed, so a notification no one receives allocates nothing.
    notification notify_any(std::string_view name,
                            std::string_view type,
                            const std::function<std::shared_ptr<const void>()>& make);

    std::shared_ptr<storage> _store;

    friend class broadcaster_access;
};

template <std::move_constructible T>
notification broadcaster::notify(const topic<T>& which, T value)
{
    // Made here, not in the library, so the value is put in on the same side
    // of a shared library boundary as the subscriber's function takes it out.
    return notify_any(which.name(), topic<T>::type_name(), [&value]() {
        return std::shared_ptr<const void>{ std::make_shared<const T>(std::move(value)) };
    });
}

}  // namespace dross
