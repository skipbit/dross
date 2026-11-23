#pragma once

#include "dross/ipc/message_connector.h"
#include "dross/ipc/message_place.h"
#include "dross/ipc/unix_socket.h"
#include "dross/notification/broadcaster.h"
#include "dross/notification/delivery.h"
#include "dross/notification/error.h"
#include "dross/notification/link_event.h"
#include "dross/notification/subscription.h"
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
 * @brief A function listener::subscribe() takes for a topic of T: callable
 * with the value as const T&, and copyable. What it returns is ignored.
 */
template <typename F, typename T>
concept subscriber_type = std::copy_constructible<std::decay_t<F>> && std::invocable<std::decay_t<F>&, const T&>;

/**
 * @brief Subscribes to the notifications of one dross::broadcaster.
 *
 * Subscribing always goes through a listener, which listens and does not
 * send. A listener hears one broadcaster.
 *
 * @code
 * struct progress {
 *     int done;
 *     int total;
 * };
 *
 * inline constexpr dross::topic<progress> kProgress{ "com.example.player.progress" };
 *
 * void show(const progress& now);
 *
 * int main()
 * {
 *     auto news = dross::broadcaster::create();
 *     auto ears = dross::listener::create(news);
 *
 *     auto subscribed = ears.subscribe(kProgress, show, dross::delivery::queued());
 *     if (! subscribed) {
 *         return 1;
 *     }
 *     dross::subscription keep = std::move(*subscribed);
 *     news.notify(kProgress, progress{ 3, 10 });  // queued on this thread's loop
 * }
 * @endcode
 *
 * Handle semantics:
 * - A listener is a handle, made by create(). Copying gives another handle
 *   to the same listener, and operator== asks whether two handles name the
 *   same one
 * - There is no empty handle and no null state, so moving copies
 *
 * Lifetime:
 * - A subscription keeps its listener going, so dropping every handle to the
 *   listener does not end the subscriptions made through it
 * - A listener does not keep its broadcaster going; see broadcaster's own
 *   doc comment
 *
 * Thread safety:
 * - Every operation is free of data races when called from any thread,
 *   including from inside a subscriber's function
 *
 * Listening to another process:
 * - A listener made by create() with a place hears the broadcaster
 *   published there by another process. It can be made before that
 *   broadcaster is, and keeps connecting until it reaches it, and again
 *   whenever the connection ends, after a wait that doubles from 100
 *   milliseconds up to 5 seconds while the tries fail, starting over once
 *   one succeeds
 * - It tells the broadcaster each topic name it subscribes to, and each it
 *   stops subscribing to. A notification sent before a subscription has
 *   reached the broadcaster does not reach it, and is not kept; nor is one
 *   sent while the listener is not connected
 * - Each value comes as a dross::value and is turned back into the topic's
 *   type with dross::value_traits, so subscribe() takes only a type that has
 *   them. A topic is told apart by its name alone, so a value that the type
 *   cannot be made from is dropped and reported on link
 * - link carries what happens to the connection: connected, disconnected,
 *   and type_mismatch. Nothing is reported when no one subscribes to it, and
 *   a try to connect that fails is never reported
 * - The listener has a thread of its own that connects, and what the place
 *   carries comes on threads of its own; at a dross::ipc::unix_socket, the
 *   connection has one that receives. A subscriber that chose
 *   dross::delivery::direct() runs on the receiving thread, so an exception
 *   it throws leaves that thread and ends the program, as one leaving a
 *   worker of an operation_queue does
 * - A listener made from a broadcaster in this process never reports on
 *   link
 *
 * @code
 * inline constexpr dross::topic<int> kTrack{ "com.example.player.track" };
 *
 * void show(const int& track);
 * void report(const dross::link_event& what);
 *
 * int main()
 * {
 *     const dross::ipc::unix_socket place{ dross::path{ std::string{ "/run/user/1000/player.sock" } } };
 *     auto ears = dross::listener::create(place);
 *     if (! ears) {
 *         return 1;
 *     }
 *
 *     auto watched = ears->subscribe(dross::listener::link, report, dross::delivery::queued());
 *     auto subscribed = ears->subscribe(kTrack, show, dross::delivery::queued());
 *     if (! watched || ! subscribed) {
 *         return 1;
 *     }
 *     dross::current_runloop().run();
 * }
 * @endcode
 */
class listener final {
public:
    /**
     * @brief Make a listener that hears source, in this process.
     * @param source The broadcaster to hear, which the listener does not keep
     * going
     * @return A handle to the new listener
     */
    [[nodiscard]] static listener create(const broadcaster& source);

    /**
     * @brief Make a listener that hears the broadcaster another process
     * published at place.
     * @param place Where that broadcaster listens, now or later. The listener
     * keeps what its connector() gives, not place itself
     * @return A handle to the new listener, which starts connecting at once,
     * or std::errc::resource_unavailable_try_again when the system will not
     * start its thread, or std::errc::not_enough_memory when there is no
     * memory to start it
     */
    template <ipc::message_connectable_place_type P>
    [[nodiscard]] static std::expected<listener, error> create(const P& place);

    /**
     * @brief The topic on which a listener made with a place reports what
     * happens to its connection. A subscriber hears only what happens after
     * it subscribes, so a connection made before is not reported to it.
     */
    static constexpr topic<link_event> link{ "dross.listener.link" };

    /**
     * @brief Copy constructor, giving another handle to the same listener.
     * @param other The handle to copy
     */
    listener(const listener& other);

    /**
     * @brief Destructor.
     */
    ~listener();

    /**
     * @brief Copy assignment, naming the same listener as the source.
     * @param other The handle to copy
     * @return Reference to this handle
     */
    listener& operator=(const listener& other);

    /**
     * @brief Have function called with each notification sent on which.
     * @param which The topic
     * @param function The function, copied, and called with the value as
     * const T&
     * @param how Where the function runs; there is no default
     * @return The subscription, which receives for as long as it is held, or
     * notification_errc::empty_function for a null function pointer or an
     * empty std::function, or notification_errc::type_not_convertible on a
     * listener made with a place, for a type without dross::value_traits
     * on any topic but link
     *
     * Dropping what this returns ends the subscription at once, so the result
     * is not to be discarded. When the broadcaster has already gone, the
     * subscription is made but is not active.
     */
    template <typename T, subscriber_type<T> F>
    [[nodiscard]] std::expected<subscription, error> subscribe(const topic<T>& which, F&& function, const delivery& how);

    /**
     * @brief Test whether two handles name the same listener.
     * @param other The handle to compare with
     * @return true when both name one listener
     */
    bool operator==(const listener& other) const noexcept;

private:
    class storage;

    explicit listener(std::shared_ptr<storage> store) noexcept;

    // create() with a place, past the type of the place.
    static std::expected<listener, error> connect(std::shared_ptr<ipc::message_connector> part);

    // A std::function of any signature. A class derived from one is not,
    // since std::function never takes such a class as empty either.
    template <typename F>
    struct is_std_function : std::false_type { };

    template <typename R, typename... Args>
    struct is_std_function<std::function<R(Args...)>> : std::true_type { };

    // Empty the way std::function takes a function as empty; a function of
    // any other type is never empty.
    template <typename F>
    static bool is_empty(const F& function) noexcept
    {
        if constexpr (std::is_function_v<std::remove_pointer_t<F>> || std::is_member_pointer_v<F> || is_std_function<F>::value) {
            return (function == nullptr);
        } else {
            return false;
        }
    }

    // Makes the value a dross::value holds, or says why it could not; null
    // for a type without dross::value_traits.
    using value_reader = std::expected<std::shared_ptr<const void>, error> (*)(const value& held);

    // subscribe() without its type: function is given a pointer to the value.
    std::expected<subscription, error> subscribe_any(std::string_view name,
                                                     const std::type_info& type,
                                                     std::function<void(const void*)> function,
                                                     const delivery& how,
                                                     value_reader read);

    std::shared_ptr<storage> _store;
};

template <ipc::message_connectable_place_type P>
std::expected<listener, error> listener::create(const P& place)
{
    return connect(place.connector());
}

template <typename T, subscriber_type<T> F>
std::expected<subscription, error> listener::subscribe(const topic<T>& which, F&& function, const delivery& how)
{
    if (is_empty<std::decay_t<F>>(function)) {
        return std::unexpected(error{ notification_errc::empty_function });
    }
    // Wrapped here, not in the library, so the value is taken out on the same
    // side of a shared library boundary as notify() puts it in.
    value_reader read = nullptr;
    if constexpr (value_convertible_type<T>) {
        read = [](const value& held) -> std::expected<std::shared_ptr<const void>, error> {
            auto made = value_traits<T>::from_value(held);
            if (! made) {
                return std::unexpected(std::move(made).error());
            }
            return std::shared_ptr<const void>{ std::make_shared<const T>(std::move(*made)) };
        };
    }
    return subscribe_any(which.name(), topic<T>::type(), [function = std::forward<F>(function)](const void* value) mutable {
        std::invoke(function, *static_cast<const T*>(value));
    }, how, read);
}

}  // namespace dross
