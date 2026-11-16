#pragma once

#include "dross/notification/delivery.h"
#include "dross/notification/subscription.h"

#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace dross {

template <std::equality_comparable T>
class observable;

/**
 * @brief A function observable::when_changed() takes for a value of T:
 * copyable, and callable either with the value before the change and the
 * value after it, both as const T&, or with the value after it alone. What
 * it returns is ignored.
 */
template <typename F, typename T>
concept observer_type = std::copy_constructible<std::decay_t<F>>
                        && (std::invocable<std::decay_t<F>&, const T&, const T&> || std::invocable<std::decay_t<F>&, const T&>);

/**
 * @brief The part of dross::observable that does not depend on its type.
 *
 * Only observable uses it.
 */
class observable_core final {
private:
    template <std::equality_comparable T>
    friend class observable;

    class storage;

    // Takes the previous value and the new one.
    using function_type = std::function<void(const void* previous, const void* value)>;

    // Whether two values are equal.
    using equal_type = bool (*)(const void* lhs, const void* rhs);

    // Owned by the calling thread.
    observable_core(std::shared_ptr<const void> initial, equal_type equal);
    observable_core(observable_core&& other) noexcept;
    observable_core& operator=(observable_core&& other) noexcept;
    ~observable_core();

    observable_core(const observable_core& other) = delete;
    observable_core& operator=(const observable_core& other) = delete;

    // The value, which only the owner may read; stops the program otherwise.
    const void* get() const;

    // Reflects value here on the owner's thread, and posts it to the owner's
    // run loop from any other thread.
    bool set(std::shared_ptr<const void> value) const;

    subscription when_changed(function_type function, const delivery& how) const;

    // set() for a proxy, which reaches the value through which.
    static bool set(const std::weak_ptr<storage>& which, std::shared_ptr<const void> value);

    std::weak_ptr<storage> weaken() const noexcept;

    std::shared_ptr<storage> _store;
};

/**
 * @brief A value that its owner thread changes, and that others can be told
 * of when it changes.
 *
 * The thread that constructs an observable owns it. The value is read and
 * changed on that thread only: get() there, and set() there takes effect at
 * once. A set() from any other thread is posted to the owner's run loop and
 * takes effect when that loop runs it, so the value needs no lock.
 *
 * @code
 * class player {
 * public:
 *     const dross::observable<int>& volume() const { return _volume; }
 *
 *     void fade_out_on(dross::operation_queue& workers)
 *     {
 *         auto volume = _volume.make_proxy();
 *         workers.enqueue([volume] { volume.set(0); });
 *     }
 *
 * private:
 *     dross::observable<int> _volume{ 50 };
 * };
 *
 * class volume_view {
 * public:
 *     explicit volume_view(const player& source)
 *         : _watch{ source.volume().when_changed([this](int now) { show(now); }, dross::delivery::queued()) }
 *     {
 *         show(source.volume().get());
 *     }
 *
 * private:
 *     void show(int volume) { std::cout << "Volume: " << volume << std::endl; }
 *
 *     dross::subscription _watch;
 * };
 * @endcode
 *
 * Handing out a const reference, as player does above, lets others watch the
 * value without changing it.
 *
 * Changing:
 * - set() changes the value only when the new one is not equal to the
 *   current one, so a function given to when_changed() is called for changes
 *   alone
 * - From another thread, set() posts the value to the owner's run loop and
 *   returns. Several posted from one thread take effect in the order they
 *   were posted
 * - set() returns false when the value can no longer be reached: the
 *   observable has gone, or the owner's thread has ended. true does not
 *   promise that a posted value takes effect, since the owner's thread may
 *   end before its loop runs it
 * - make_proxy() gives a handle that can set() and do nothing else, for
 *   handing to other threads
 *
 * Watching:
 * - when_changed() calls a function on each change, the way its
 *   dross::delivery says, until the subscription it returns ends. It is not
 *   called for the value there already was
 * - A function that takes two values is given the value just before the
 *   change and the value after it; one that takes one value is given the
 *   value after it
 * - A function run by dross::delivery::direct() is called on the owner's
 *   thread as each change takes effect, so it sees every change
 * - A function run anywhere else is called with the latest change at the
 *   time it runs. Changes that come while one is waiting replace it, so
 *   values in between can be skipped. The value before is then the one just
 *   before the latest change, not the one the function was last given
 * - An empty function, a null function pointer or an empty std::function,
 *   gives a subscription that is never called and is not active
 * - A change made from inside a function takes effect at once, and is handed
 *   on once the change being handed on has reached every function, so each
 *   function is given the changes in the order they took effect
 *
 * Exceptions:
 * - An exception thrown by a function is not caught. It propagates out of
 *   the call that ran the function: the set() that made the change for
 *   dross::delivery::direct(), and the running call of the run loop or the
 *   worker otherwise
 * - When it leaves a set(), the functions after it are not given that
 *   change, nor any change made inside it that had yet to be handed on. The
 *   value keeps the latest change
 *
 * Misuse:
 * - get() from a thread that does not own the observable writes a line to
 *   standard error and calls std::abort(). The value is not guarded by a
 *   lock, so reading it there would be a data race
 *
 * Lifetime:
 * - The observable is not copied. Moving it keeps its owner and the
 *   subscriptions made on it. A moved-from observable may only be assigned
 *   to or destroyed
 * - Once the observable is destroyed, a value posted to it is dropped, its
 *   subscriptions are no longer active, and a proxy's set() returns false
 *
 * Thread safety:
 * - when_changed(), make_proxy() and set() may be called from any thread,
 *   including from inside a function given to when_changed()
 * - Ending a subscription may happen on any thread
 * - get() is for the owner's thread alone
 *
 * @tparam T The value's type, compared with == to tell a change
 */
template <std::equality_comparable T>
class observable final {
public:
    /**
     * @brief A handle that sets the value of an observable from anywhere,
     * and does nothing else.
     *
     * It is copied freely and does not keep the observable going.
     */
    class proxy final {
    public:
        /**
         * @brief Set the value, as observable::set() does.
         * @param value The new value
         * @return false when the observable has gone or its owner's thread
         * has ended
         */
        bool set(T value) const
        {
            return observable_core::set(_target, std::make_shared<const T>(std::move(value)));
        }

    private:
        friend class observable;

        explicit proxy(std::weak_ptr<observable_core::storage> target) noexcept
            : _target{ std::move(target) }
        {
        }

        std::weak_ptr<observable_core::storage> _target;
    };

    /**
     * @brief Construct an observable owned by the calling thread.
     * @param initial The value it starts with
     */
    explicit observable(T initial)
        : _core{ std::make_shared<const T>(std::move(initial)), &equal }
    {
    }

    /**
     * @brief Move constructor. The owner stays the thread that constructed
     * other.
     * @param other The observable to move from
     */
    observable(observable&& other) noexcept = default;

    /**
     * @brief Move assignment.
     * @param other The observable to move from
     * @return Reference to this observable
     */
    observable& operator=(observable&& other) noexcept = default;

    ~observable() = default;

    observable(const observable& other) = delete;
    observable& operator=(const observable& other) = delete;

    /**
     * @brief Read the value, on the owner's thread.
     * @return The value, valid until the next change takes effect
     *
     * Called from any other thread, it stops the program; see Misuse.
     */
    const T& get() const
    {
        return *static_cast<const T*>(_core.get());
    }

    /**
     * @brief Set the value: at once on the owner's thread, and by posting it
     * to the owner's run loop from any other.
     * @param value The new value
     * @return false when the owner's thread has ended
     */
    bool set(T value) const
    {
        return _core.set(std::make_shared<const T>(std::move(value)));
    }

    /**
     * @brief Have function called on each change of the value.
     * @param function The function, copied, and called with the value before
     * and after the change, or with the value after it alone
     * @param how Where the function runs; there is no default
     * @return The subscription, which receives for as long as it is held
     *
     * Dropping what this returns ends the subscription at once, so the result
     * is not to be discarded.
     */
    template <observer_type<T> F>
    [[nodiscard]] subscription when_changed(F&& function, const delivery& how) const
    {
        if (is_empty<std::decay_t<F>>(function)) {
            return subscription{};
        }
        return _core.when_changed([function = std::forward<F>(function)](const void* previous, const void* value) mutable {
            if constexpr (std::invocable<std::decay_t<F>&, const T&, const T&>) {
                std::invoke(function, *static_cast<const T*>(previous), *static_cast<const T*>(value));
            } else {
                std::invoke(function, *static_cast<const T*>(value));
            }
        }, how);
    }

    /**
     * @brief Make a handle that sets the value from any thread.
     * @return A proxy for this observable
     */
    proxy make_proxy() const
    {
        return proxy{ _core.weaken() };
    }

private:
    static bool equal(const void* lhs, const void* rhs)
    {
        return (*static_cast<const T*>(lhs) == *static_cast<const T*>(rhs));
    }

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

    observable_core _core;
};

}  // namespace dross
