#pragma once

#include "dross/notification/broadcaster.h"
#include "notification/subscription_access.h"

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// The state behind a broadcaster handle: who is subscribed to each topic. A
// listener adds to it, and notify() reads it.
namespace dross {

class broadcaster::storage final {
public:
    using receiver = subscription_access::receiver;
    using receivers = std::vector<std::shared_ptr<receiver>>;

    // Adds which to the subscribers of the topic named name that carries
    // type, and drops those of that topic that have ended.
    void add(std::string_view name, std::string_view type, std::shared_ptr<receiver> which);

    // The subscribers of that topic as they are now, which later changes do
    // not touch. Empty when there are none.
    std::shared_ptr<const receivers> subscribers(std::string_view name, std::string_view type) const;

    // Drops the subscribers of that topic that have ended.
    void prune(std::string_view name, std::string_view type);

private:
    // A topic is named by its name and the type it carries together, so two
    // topics that share a name but not a type are told apart.
    struct address final {
        std::string name;
        std::string type;
    };

    // An address looked up without copying its strings.
    struct address_view final {
        std::string_view name;
        std::string_view type;
    };

    struct address_hash final {
        using is_transparent = void;

        std::size_t operator()(const address_view& which) const noexcept;
        std::size_t operator()(const address& which) const noexcept;
    };

    struct address_equal final {
        using is_transparent = void;

        bool operator()(const address_view& lhs, const address_view& rhs) const noexcept;
        bool operator()(const address& lhs, const address& rhs) const noexcept;
        bool operator()(const address& lhs, const address_view& rhs) const noexcept;
        bool operator()(const address_view& lhs, const address& rhs) const noexcept;
    };

    // Replaced whole, never changed in place, so a list subscribers() handed
    // out stays as it was.
    using topic_map = std::unordered_map<address, std::shared_ptr<const receivers>, address_hash, address_equal>;

    // Replaces the list of which with one holding its subscribers that have
    // not ended, and added when given, or removes the topic when that leaves
    // none. Must hold _mutex.
    void rebuild(topic_map::iterator which, std::shared_ptr<receiver> added);

    mutable std::mutex _mutex;
    topic_map _topics;
};

}  // namespace dross
