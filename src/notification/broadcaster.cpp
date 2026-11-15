#include "dross/notification/broadcaster.h"

#include "notification/broadcaster_access.h"
#include "notification/broadcaster_storage.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace dross {

std::size_t broadcaster::storage::address_hash::operator()(const address_view& which) const noexcept
{
    const std::size_t name = std::hash<std::string_view>{}(which.name);
    const std::size_t type = std::hash<std::string_view>{}(which.type);
    return name ^ (type + 0x9e3779b97f4a7c15ULL + (name << 6) + (name >> 2));
}

std::size_t broadcaster::storage::address_hash::operator()(const address& which) const noexcept
{
    return (*this)(address_view{ which.name, which.type });
}

bool broadcaster::storage::address_equal::operator()(const address_view& lhs, const address_view& rhs) const noexcept
{
    return (lhs.name == rhs.name) && (lhs.type == rhs.type);
}

bool broadcaster::storage::address_equal::operator()(const address& lhs, const address& rhs) const noexcept
{
    return (*this)(address_view{ lhs.name, lhs.type }, address_view{ rhs.name, rhs.type });
}

bool broadcaster::storage::address_equal::operator()(const address& lhs, const address_view& rhs) const noexcept
{
    return (*this)(address_view{ lhs.name, lhs.type }, rhs);
}

bool broadcaster::storage::address_equal::operator()(const address_view& lhs, const address& rhs) const noexcept
{
    return (*this)(lhs, address_view{ rhs.name, rhs.type });
}

void broadcaster::storage::add(std::string_view name, std::string_view type, std::shared_ptr<receiver> which)
{
    std::shared_ptr<const receivers> replaced;
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        auto found = _topics.find(address_view{ name, type });
        if (found == _topics.end()) {
            found = _topics.emplace(address{ std::string{ name }, std::string{ type } }, std::make_shared<const receivers>()).first;
        }
        replaced = found->second;
        rebuild(found, std::move(which));
    }
    // The list replaced goes here, outside the lock: dropping it can destroy
    // an ended subscriber's function, which runs its captures' own code.
}

std::shared_ptr<const broadcaster::storage::receivers> broadcaster::storage::subscribers(std::string_view name,
                                                                                         std::string_view type) const
{
    const std::lock_guard<std::mutex> guard{ _mutex };
    const auto found = _topics.find(address_view{ name, type });
    if (found == _topics.end()) {
        return nullptr;
    }
    return found->second;
}

void broadcaster::storage::prune(std::string_view name, std::string_view type)
{
    std::shared_ptr<const receivers> replaced;
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        const auto found = _topics.find(address_view{ name, type });
        if (found == _topics.end()) {
            return;
        }
        replaced = found->second;
        rebuild(found, nullptr);
    }
}

void broadcaster::storage::rebuild(topic_map::iterator which, std::shared_ptr<receiver> added)
{
    receivers kept;
    kept.reserve(which->second->size() + 1);
    for (const auto& each : *which->second) {
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
    const auto subscribers = _store->subscribers(name, type);
    if (! subscribers) {
        return notification{ nullptr };
    }

    const auto value = make();
    bool saw_ended = false;
    for (const auto& each : *subscribers) {
        if (each->ended()) {
            saw_ended = true;
            continue;
        }
        each->deliver(value);
    }
    if (saw_ended) {
        _store->prune(name, type);
    }
    return notification{ nullptr };
}

std::weak_ptr<broadcaster_access::storage_type> broadcaster_access::weaken(const broadcaster& which)
{
    return which._store;
}

}  // namespace dross
