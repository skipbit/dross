#include "dross/notification/broadcaster.h"

#include "notification/broadcaster_access.h"
#include "notification/broadcaster_storage.h"
#include "notification/notification_storage.h"
#include "notification/publisher.h"
#include "notification/topic_address.h"

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>

namespace dross {

topic_address broadcaster::storage::address::view() const noexcept
{
    return { name, type, identity };
}

broadcaster::storage::address::operator topic_address() const noexcept
{
    return view();
}

std::size_t broadcaster::storage::address_hash::operator()(const topic_address& which) const noexcept
{
    const std::size_t name = std::hash<std::string_view>{}(which.name);
    const std::size_t type = std::hash<std::string_view>{}(which.type) ^ std::hash<const std::type_info*>{}(which.identity);
    return name ^ (type + static_cast<std::size_t>(0x9e3779b97f4a7c15ULL) + (name << 6) + (name >> 2));
}

bool broadcaster::storage::address_equal::operator()(const topic_address& lhs, const topic_address& rhs) const noexcept
{
    return (lhs.name == rhs.name) && (lhs.type == rhs.type) && (lhs.identity == rhs.identity);
}

broadcaster::storage::storage() = default;

broadcaster::storage::storage(std::unique_ptr<publisher> outside) noexcept
    : _outside{ std::move(outside) }
{
}

broadcaster::storage::~storage() = default;

publisher* broadcaster::storage::outside() const noexcept
{
    return _outside.get();
}

void broadcaster::storage::add(const topic_address& where, std::shared_ptr<receiver> which)
{
    std::shared_ptr<const receivers> replaced;
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        auto found = _topics.find(where);
        if (found == _topics.end()) {
            found = _topics.emplace(address{ std::string{ where.name }, std::string{ where.type }, where.identity }, std::make_shared<const receivers>()).first;
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

std::expected<broadcaster, error> broadcaster::create(const ipc::unix_socket& place)
{
    auto outside = publisher::listen([place](std::shared_ptr<ipc::message_acceptor_events> events) {
        return place.listen(std::move(events));
    });
    if (! outside) {
        return std::unexpected(std::move(outside).error());
    }
    return broadcaster{ std::make_shared<storage>(std::move(*outside)) };
}

bool broadcaster::operator==(const broadcaster& other) const noexcept
{
    return _store == other._store;
}

notification broadcaster::notify_any(std::string_view name,
                                     const std::type_info& type,
                                     const std::function<std::shared_ptr<const void>()>& make,
                                     const notify_options& how,
                                     const value_writer write)
{
    publisher* const outside = _store->outside();
    if ((outside != nullptr) && (write == nullptr)) {
        std::fputs("dross: broadcaster::notify: a published broadcaster sends a type without dross::value_traits\n", stderr);
        std::abort();
    }
    const auto where = topic_address::of(name, type);
    const auto subscribers = _store->subscribers(where);
    const bool sent_out = (outside != nullptr) && outside->wanted(name);
    if (! subscribers && ! sent_out) {
        return notification{ nullptr };
    }

    const auto value = make();
    if (sent_out) {
        outside->send(name, write(value.get()));
    }
    if (! subscribers) {
        return notification{ nullptr };
    }

    // A subscription removes itself from the list when it ends, so one that
    // ends after this copy was taken is skipped by deliver() itself.
    auto sent = std::make_shared<notification::storage>();
    const bool deferred = (how.timing == notify_timing::deferred);
    for (const auto& each : *subscribers) {
        each->deliver(value, sent, deferred);
    }
    return notification{ std::move(sent) };
}

std::weak_ptr<broadcaster_access::storage_type> broadcaster_access::weaken(const broadcaster& which)
{
    return which._store;
}

}  // namespace dross
