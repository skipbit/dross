#include "dross/notification/broadcaster.h"

#include "notification/broadcaster_access.h"
#include "notification/broadcaster_storage.h"
#include "notification/topic_address.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace dross {

topic_address broadcaster::storage::address::view() const noexcept
{
    return { name, type };
}

broadcaster::storage::address::operator topic_address() const noexcept
{
    return view();
}

std::size_t broadcaster::storage::address_hash::operator()(const topic_address& which) const noexcept
{
    const std::size_t name = std::hash<std::string_view>{}(which.name);
    const std::size_t type = std::hash<std::string_view>{}(which.type);
    return name ^ (type + static_cast<std::size_t>(0x9e3779b97f4a7c15ULL) + (name << 6) + (name >> 2));
}

bool broadcaster::storage::address_equal::operator()(const topic_address& lhs, const topic_address& rhs) const noexcept
{
    return (lhs.name == rhs.name) && (lhs.type == rhs.type);
}

void broadcaster::storage::add(const topic_address& where, std::shared_ptr<receiver> which)
{
    std::shared_ptr<const receivers> replaced;
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        auto found = _topics.find(where);
        if (found == _topics.end()) {
            found = _topics.emplace(address{ std::string{ where.name }, std::string{ where.type } }, std::make_shared<const receivers>()).first;
        }
        replaced = rebuild(found, std::move(which));
    }
}

std::shared_ptr<const broadcaster::storage::receivers> broadcaster::storage::subscribers(const topic_address& where) const
{
    const std::lock_guard<std::mutex> guard{ _mutex };
    const auto found = _topics.find(where);
    if (found == _topics.end()) {
        return nullptr;
    }
    return found->second;
}

void broadcaster::storage::prune(const topic_address& where)
{
    std::shared_ptr<const receivers> replaced;
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        const auto found = _topics.find(where);
        if (found == _topics.end()) {
            return;
        }
        replaced = rebuild(found, nullptr);
    }
}

std::shared_ptr<const broadcaster::storage::receivers> broadcaster::storage::rebuild(topic_map::iterator which,
                                                                                     std::shared_ptr<receiver> added)
{
    std::shared_ptr<const receivers> replaced = which->second;
    receivers kept;
    kept.reserve(replaced->size() + 1);
    for (const auto& each : *replaced) {
        if (! each->ended()) {
            kept.push_back(each);
        }
    }
    if (added) {
        kept.push_back(std::move(added));
    }

    if (kept.empty()) {
        _topics.erase(which);
    } else {
        which->second = std::make_shared<const receivers>(std::move(kept));
    }
    return replaced;
}

broadcaster::broadcaster(std::shared_ptr<storage> store) noexcept
    : _store{ std::move(store) }
{
}

broadcaster::broadcaster(const broadcaster& other) = default;

broadcaster::~broadcaster() = default;

broadcaster& broadcaster::operator=(const broadcaster& other) = default;

broadcaster broadcaster::create()
{
    return broadcaster{ std::make_shared<storage>() };
}

bool broadcaster::operator==(const broadcaster& other) const noexcept
{
    return _store == other._store;
}

notification broadcaster::notify_any(std::string_view name,
                                     std::string_view type,
                                     const std::function<std::shared_ptr<const void>()>& make)
{
    const topic_address where{ name, type };
    const auto subscribers = _store->subscribers(where);
    if (! subscribers) {
        return notification{ nullptr };
    }

    // A subscription removes itself from the list when it ends, so one that
    // ends after this copy was taken is skipped by deliver() itself.
    const auto value = make();
    for (const auto& each : *subscribers) {
        each->deliver(value);
    }
    return notification{ nullptr };
}

std::weak_ptr<broadcaster_access::storage_type> broadcaster_access::weaken(const broadcaster& which)
{
    return which._store;
}

}  // namespace dross
