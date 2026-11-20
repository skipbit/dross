#pragma once

#include "dross/ipc/unix_socket.h"
#include "dross/notification/notification.h"
#include "dross/notification/topic.h"
#include "dross/type/error.h"
#include "dross/type/value.h"
#include "dross/type/value_traits.h"

#include <concepts>
#include <expected>
#include <functional>
#include <memory>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>

namespace dross {

/**
 * @brief When notify() runs the functions of subscribers that chose
 * dross::delivery::direct().
 *
 * Subscribers that chose any other delivery are queued the same way either
 * way.
 */
enum class notify_timing {
    immediate,  ///< Inside notify(), on the sending thread, before it returns
    deferred,   ///< On the sending thread's run loop, once notify() has returned
};

/**
 * @brief How broadcaster::notify() sends one notification.
 *
 * Every member has a default, so a notification given no options gets them
 * all, and one given some can name just those:
 * @code
 * struct progress {
 *     int done;
 *     int total;
 * };
 *
 * inline constexpr dross::topic<progress> kProgress{ "com.example.player.progress" };
 *
 * int main()
 * {
 *     auto news = dross::broadcaster::create();
 *     news.notify(kProgress, progress{ 3, 10 }, { .timing = dross::notify_timing::deferred });
 *     dross::current_runloop().run_pending();  // direct subscribers run here
 * }
 * @endcode
 *
 * Deferring:
 * - No subscriber's function runs before notify() returns. A sender uses it
 *   when a subscriber must not run in the middle of what the sender is doing
 * - The calls are queued on the sending thread's run loop in the order of
 *   the subscribers, and run when that thread runs its loop. Until then the
 *   notification can still be cancelled
 * - So a deferred notification reaches a direct subscriber after any sent
 *   later from the same thread without deferring, before the loop runs
 * - A loop that is not running yet holds them until it runs, and drops them
 *   if its thread ends first, as it does any task
 */
struct notify_options final {
    notify_timing timing{ notify_timing::immediate };  ///< When direct subscribers run
};

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
 * - notify() returns a dross::notification, through which the deliveries
 *   that have not started can be cancelled
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
 * Publishing to other processes:
 * - A broadcaster made by create() with a place is published there, and a
 *   dross::listener in another process made with the same place hears it.
 *   Every notification it sends goes out, so its topics carry a type with
 *   dross::value_traits, which turns the value into a dross::value on the
 *   way. Sending a type without them stops the program, as a misuse
 * - A listener in this process can still be made from it, and is handed
 *   each value directly, without turning it into a dross::value
 * - A notification goes to a listener in another process only once that
 *   listener's subscription has reached this one. One sent before then
 *   does not reach it, and is not kept
 * - Between processes a topic is told apart by its name alone. A listener
 *   that cannot turn the value back into its own type says so on
 *   dross::listener::link
 * - A notification is at most 64 KiB once turned into bytes, and at most
 *   1 MiB waits to be sent to one listener. A listener that falls further
 *   behind is disconnected, and connects again
 * - Only processes of the same effective user can connect. The socket file
 *   is made for its owner alone, and removed when the last handle goes
 * - When a connection cannot be accepted, as when the process has no file
 *   descriptor left, accepting pauses for a while. macOS drops the
 *   connection it could not accept, so that listener connects again
 * - Nothing tells the sender that a listener connected, went or fell
 *   behind
 *
 * Exceptions:
 * - An exception thrown by a subscriber's function called with
 *   dross::delivery::direct() propagates out of notify(), and the
 *   subscribers after it are not handed the notification. Unless notify()
 *   was deferred: the exception then propagates out of the running call of
 *   the sending thread's loop
 * - A notify() that leaves by an exception returns no dross::notification,
 *   so what it had already handed on still runs and cannot be cancelled
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
     * @brief Make a broadcaster published at place, which listeners in other
     * processes can hear.
     * @param place Where to listen. A socket file left there by a process
     * that has gone is replaced; one that a live process listens on is not
     * @return A handle to the new broadcaster, or the error that kept it
     * from listening at place, such as std::errc::address_in_use when
     * another process listens there, or std::errc::filename_too_long for a
     * path longer than the system takes
     */
    [[nodiscard]] static std::expected<broadcaster, error> create(const ipc::unix_socket& place);

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
     * @param how How to send it; by default, direct subscribers run before
     * this returns
     * @return The notification sent, through which it can be cancelled
     *
     * Returns once each subscriber has been handed the notification: after
     * the function has run for one given dross::delivery::direct() unless
     * how defers it, and once it is queued for the others. A notification to
     * other processes is queued to be sent before any subscriber here runs.
     *
     * On a published broadcaster, T has dross::value_traits; without them,
     * this stops the program as a misuse, whether or not anyone listens.
     */
    template <std::move_constructible T>
    notification notify(const topic<T>& which, std::type_identity_t<T> value, const notify_options& how = {});

    /**
     * @brief Test whether two handles name the same broadcaster.
     * @param other The handle to compare with
     * @return true when both name one broadcaster
     */
    bool operator==(const broadcaster& other) const noexcept;

private:
    class storage;

    explicit broadcaster(std::shared_ptr<storage> store) noexcept;

    // Turns the value notify() made into a dross::value; null for a type
    // without dross::value_traits.
    using value_writer = dross::value (*)(const void* made);

    // notify() without its type. make is called only when someone is
    // subscribed, so a notification no one receives allocates nothing.
    notification notify_any(std::string_view name,
                            const std::type_info& type,
                            const std::function<std::shared_ptr<const void>()>& make,
                            const notify_options& how,
                            value_writer write);

    std::shared_ptr<storage> _store;

    friend class broadcaster_access;
};

template <std::move_constructible T>
notification broadcaster::notify(const topic<T>& which, std::type_identity_t<T> value, const notify_options& how)
{
    // Made here, not in the library, so the value is put in on the same side
    // of a shared library boundary as the subscriber's function takes it out.
    value_writer write = nullptr;
    if constexpr (value_convertible_type<T>) {
        write = [](const void* made) {
            return value_traits<T>::to_value(*static_cast<const T*>(made));
        };
    }
    return notify_any(which.name(), topic<T>::type(), [&value]() {
        return std::shared_ptr<const void>{ std::make_shared<const T>(std::move(value)) };
    }, how, write);
}

}  // namespace dross
