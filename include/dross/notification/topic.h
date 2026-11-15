#pragma once

#include <string_view>
#include <type_traits>
#include <typeinfo>

namespace dross {

/**
 * @brief The name a notification is sent under, and the type of value it
 * carries.
 *
 * A topic is defined once, as a constant, and used both to send and to
 * subscribe:
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
 *     news.notify(kProgress, progress{ 3, 10 });
 * }
 * @endcode
 *
 * Naming:
 * - Within a process, a broadcaster tells topics apart by name and type
 *   together, so two topics that share a name but carry different types
 *   never reach each other's subscribers
 * - A name is written in reverse domain name order, starting from a domain
 *   the writer controls, so that topics from different libraries do not
 *   share one
 *
 * Types defined in an unnamed namespace:
 * - Such a type exists only in its own source file, and two of them in two
 *   files can share a name. They are still told apart, so a topic carrying
 *   one never reaches the subscribers of a topic carrying the other
 * - Defined in a header, it is a different type in each source file that
 *   includes the header, so a notification sent from one of those files
 *   does not reach a subscriber in another
 *
 * A type defined inside a function with internal linkage, such as a static
 * function, is not told apart from a type of the same name inside a function
 * of the same name in another source file. Such a type is not used for a
 * topic.
 *
 * Lifetime:
 * - The topic holds the name without copying it, so the characters it is
 *   made from outlive it; a string literal always does
 *
 * @tparam T The type of value notifications on this topic carry: an object
 * type, not const, which notify() moves or copies once
 */
template <typename T>
class topic final {
    static_assert(std::is_object_v<T> && (! std::is_const_v<T>), "a topic carries a non-const object type");

public:
    /**
     * @brief Construct a topic named name.
     * @param name The name, whose characters outlive the topic
     */
    constexpr explicit topic(std::string_view name) noexcept
        : _name{ name }
    {
    }

    /**
     * @brief Get the name.
     * @return The name the topic was made with
     */
    constexpr std::string_view name() const noexcept
    {
        return _name;
    }

private:
    // Topics are told apart by name and this together. Evaluated in the
    // caller's code, so the same type matches across shared libraries.
    static const std::type_info& type() noexcept
    {
        return typeid(T);
    }

    friend class broadcaster;
    friend class listener;

    std::string_view _name;
};

}  // namespace dross
