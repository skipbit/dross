#pragma once

#include "dross/notification/broadcaster.h"
#include "dross/notification/delivery.h"
#include "dross/notification/error.h"
#include "dross/notification/subscription.h"
#include "dross/notification/topic.h"
#include "dross/type/error.h"

#include <concepts>
#include <expected>
#include <functional>
#include <memory>
#include <string_view>
#include <type_traits>
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
     * empty std::function
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

    // subscribe() without its type: function is given a pointer to the value.
    subscription subscribe_any(std::string_view name,
                               std::string_view type,
                               std::function<void(const void*)> function,
                               const delivery& how);

    std::shared_ptr<storage> _store;
};

template <typename T, subscriber_type<T> F>
std::expected<subscription, error> listener::subscribe(const topic<T>& which, F&& function, const delivery& how)
{
    if (is_empty<std::decay_t<F>>(function)) {
        return std::unexpected(error{ notification_errc::empty_function });
    }
    // Wrapped here, not in the library, so the value is taken out on the same
    // side of a shared library boundary as notify() puts it in.
    return subscribe_any(which.name(), topic<T>::type_name(), [function = std::forward<F>(function)](const void* value) mutable {
        std::invoke(function, *static_cast<const T*>(value));
    }, how);
}

}  // namespace dross
