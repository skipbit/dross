#pragma once

#include "dross/notification/broadcaster.h"
#include "notification/subscription_access.h"
#include "notification/topic_address.h"

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>

// The state behind a broadcaster handle: who is subscribed to each topic. A
// listener adds to it, and notify() reads it.
namespace dross {

class broadcaster::storage final {
public:
    using receiver = subscription_access::receiver;
    using receivers = std::vector<std::shared_ptr<receiver>>;

    // Adds which to the subscribers of the topic at where, and drops those of
    // that topic that have ended.
    void add(const topic_address& where, std::shared_ptr<receiver> which);

    // The subscribers of that topic as they are now, which later changes do
    // not touch. Empty when there are none.
    std::shared_ptr<const receivers> subscribers(const topic_address& where) const;

    // Drops the subscribers of that topic that have ended.
    void prune(const topic_address& where);

private:
    // A topic is named by its name and the type it carries together, so two
    // topics that share a name but not a type are told apart.
    struct address final {
        std::string name;
        std::string type;
        const std::type_info* identity;

        topic_address view() const noexcept;
        operator topic_address() const noexcept;
    };

    struct address_hash final {
        using is_transparent = void;

        std::size_t operator()(const topic_address& which) const noexcept;
    };

    struct address_equal final {
        using is_transparent = void;

        bool operator()(const topic_address& lhs, const topic_address& rhs) const noexcept;
    };

    // Replaced whole, never changed in place, so a list subscribers() handed
    // out stays as it was.
    using topic_map = std::unordered_map<address, std::shared_ptr<const receivers>, address_hash, address_equal>;

    // Replaces the list of which with one holding its subscribers that have
    // not ended, and added when given, or removes the topic when that leaves
    // none. Returns the list it replaced, which the caller lets go only after
    // _mutex is released, because dropping it can destroy an ended
    // subscriber's function and run its captures' code. Must hold _mutex.
    [[nodiscard]] std::shared_ptr<const receivers> rebuild(topic_map::iterator which, std::shared_ptr<receiver> added);

    mutable std::mutex _mutex;
    topic_map _topics;
};

}  // namespace dross
