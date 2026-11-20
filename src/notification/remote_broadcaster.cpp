#include "notification/remote_broadcaster.h"

#include "dross/notification/listener.h"
#include "notification/notification_access.h"
#include "notification/wire.h"

#include <algorithm>
#include <system_error>
#include <utility>
#include <variant>

namespace dross {

std::shared_ptr<remote_broadcaster> remote_broadcaster::connect(const path& where)
{
    std::shared_ptr<remote_broadcaster> made{ new remote_broadcaster{} };
    made->_connector = std::make_unique<connector>(where, made);
    made->_connector->start();
    return made;
}

remote_broadcaster::remote_broadcaster()
    : _subscribers{ std::make_shared<registry>() }
{
}

remote_broadcaster::~remote_broadcaster() = default;

topic_address remote_broadcaster::link_address() noexcept
{
    return topic_address::of(listener::link.name(), typeid(link_event));
}

bool remote_broadcaster::is_link(const topic_address& where) noexcept
{
    const auto link = link_address();
    return (where.name == link.name) && (where.type == link.type);
}

std::weak_ptr<remote_broadcaster::registry> remote_broadcaster::subscribers() const noexcept
{
    return _subscribers;
}

void remote_broadcaster::add(const std::string_view name, const std::type_info& type, std::shared_ptr<receiver> which, const value_reader read)
{
    _subscribers->add(topic_address::of(name, type), std::move(which));
    if (! is_link(topic_address::of(name, type))) {
        want(name, type, read);
    }
}

void remote_broadcaster::remove(const std::string_view name, const std::type_info& type)
{
    const auto where = topic_address::of(name, type);
    _subscribers->prune(where);
    if (! is_link(where)) {
        unwant(name, type);
    }
}

void remote_broadcaster::want(const std::string_view name, const std::type_info& type, const value_reader read)
{
    const auto where = topic_address::of(name, type);
    const std::lock_guard guard{ _lock };
    auto found = _wanted.find(name);
    const bool first = (found == _wanted.end());
    if (first) {
        found = _wanted.try_emplace(std::string{ name }).first;
    }
    auto& readings = found->second;
    const auto same = std::ranges::find_if(readings, [&where](const reading& each) {
        return (each.type == where.type) && (each.identity == where.identity);
    });
    if (same != readings.end()) {
        ++same->count;
    } else {
        readings.push_back(reading{ std::string{ where.type }, where.identity, read, 1 });
    }
    // Sent under the lock, so the broadcaster hears of each name in the order
    // it was wanted and let go. Before connected(), it is sent from there.
    if (first && _linked) {
        _connector->send(wire::encode(wire::subscribe{ std::string{ name } }));
    }
}

void remote_broadcaster::unwant(const std::string_view name, const std::type_info& type)
{
    const auto where = topic_address::of(name, type);
    const std::lock_guard guard{ _lock };
    const auto found = _wanted.find(name);
    if (found == _wanted.end()) {
        return;
    }
    auto& readings = found->second;
    const auto same = std::ranges::find_if(readings, [&where](const reading& each) {
        return (each.type == where.type) && (each.identity == where.identity);
    });
    if (same == readings.end()) {
        return;
    }
    if (--same->count == 0) {
        readings.erase(same);
    }
    if (readings.empty()) {
        _wanted.erase(found);
        if (! _linked) {
            return;
        }
        _connector->send(wire::encode(wire::unsubscribe{ std::string{ name } }));
    }
}

void remote_broadcaster::connected()
{
    {
        const std::lock_guard guard{ _lock };
        _linked = true;
        for (const auto& [name, readings] : _wanted) {
            _connector->send(wire::encode(wire::subscribe{ name }));
        }
    }
    tell(link_event{ link_event::kind::connected, {}, {} });
}

void remote_broadcaster::received(std::vector<std::byte> message)
{
    auto read = wire::decode(message);
    if (! read) {
        _connector->close(std::move(read).error());
        return;
    }
    auto* const sent = std::get_if<wire::notice>(&*read);
    if (sent == nullptr) {
        // A broadcaster sends nothing else, so the peer is not one.
        _connector->close(error{ std::errc::invalid_argument, "a listener was sent a subscription" });
        return;
    }

    std::vector<reading> readings;
    {
        const std::lock_guard guard{ _lock };
        const auto found = _wanted.find(sent->name);
        if (found == _wanted.end()) {
            return;
        }
        readings = found->second;
    }
    for (const auto& each : readings) {
        auto made = each.read(sent->content);
        if (! made) {
            tell(link_event{ link_event::kind::type_mismatch, sent->name, std::move(made).error() });
            continue;
        }
        hand(topic_address{ sent->name, each.type, each.identity }, *made);
    }
}

void remote_broadcaster::disconnected(error reason)
{
    {
        const std::lock_guard guard{ _lock };
        _linked = false;
    }
    tell(link_event{ link_event::kind::disconnected, {}, std::move(reason) });
}

void remote_broadcaster::hand(const topic_address& where, const std::shared_ptr<const void>& value) const
{
    const auto receivers = _subscribers->subscribers(where);
    if (! receivers) {
        return;
    }
    const auto sent = std::make_shared<notification_access::storage_type>();
    for (const auto& each : *receivers) {
        each->deliver(value, sent, false);
    }
}

void remote_broadcaster::tell(link_event what) const
{
    const auto where = link_address();
    if (! _subscribers->subscribers(where)) {
        return;
    }
    hand(where, std::make_shared<const link_event>(std::move(what)));
}

}  // namespace dross
