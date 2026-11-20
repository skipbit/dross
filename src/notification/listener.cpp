#include "dross/notification/listener.h"

#include "notification/broadcaster_access.h"
#include "notification/delivery_access.h"
#include "notification/remote_broadcaster.h"
#include "notification/subscription_access.h"
#include "notification/topic_address.h"

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>

namespace dross {

// Hidden from the library's exported symbol table, as the broadcaster's
// state it holds is.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_LISTENER_STORAGE_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_LISTENER_STORAGE_HIDDEN
#endif

// The state behind a listener handle: the subscribers of the broadcaster it
// hears, held without keeping them going. For a broadcaster in another
// process, they are kept here, by remote, which the listener keeps going.
class DROSS_LISTENER_STORAGE_HIDDEN listener::storage final {
public:
    explicit storage(std::weak_ptr<broadcaster_access::storage_type> source)
        : source{ std::move(source) }
    {
    }

    explicit storage(std::shared_ptr<remote_broadcaster> remote)
        : source{ remote->subscribers() }
        , remote{ std::move(remote) }
    {
    }

    const std::weak_ptr<broadcaster_access::storage_type> source;
    const std::shared_ptr<remote_broadcaster> remote;
};

#undef DROSS_LISTENER_STORAGE_HIDDEN

listener::listener(std::shared_ptr<storage> store) noexcept
    : _store{ std::move(store) }
{
}

listener::listener(const listener& other) = default;

listener::~listener() = default;

listener& listener::operator=(const listener& other) = default;

listener listener::create(const broadcaster& source)
{
    return listener{ std::make_shared<storage>(broadcaster_access::weaken(source)) };
}

listener listener::create(const ipc::unix_socket& place)
{
    return listener{ std::make_shared<storage>(remote_broadcaster::connect(place.location())) };
}

bool listener::operator==(const listener& other) const noexcept
{
    return _store == other._store;
}

std::expected<subscription, error> listener::subscribe_any(std::string_view name,
                                                           const std::type_info& type,
                                                           std::function<void(const void*)> function,
                                                           const delivery& how,
                                                           const value_reader read)
{
    using receiver = subscription_access::receiver;

    const auto where = topic_address::of(name, type);
    // link is reported here, not asked of the broadcaster.
    const auto link_address = topic_address::of(link.name(), typeid(link_event));
    const bool on_link = (where.name == link_address.name) && (where.type == link_address.type);
    std::weak_ptr<remote_broadcaster> remote;
    if (_store->remote && ! on_link) {
        if (read == nullptr) {
            return std::unexpected(error{ notification_errc::type_not_convertible });
        }
        remote = _store->remote;
    }

    receiver::origin from{ _store, _store->source, nullptr };
    from.detach = [source = _store->source, remote, name = std::string{ name }, type = &type]() {
        if (auto alive = source.lock()) {
            alive->prune(topic_address::of(name, *type));
        }
        if (auto alive = remote.lock()) {
            alive->unwant(name, *type);
        }
    };

    auto which = std::make_shared<receiver>(delivery_access::resolve(how), std::move(function), std::move(from));
    if (auto source = _store->source.lock()) {
        source->add(where, which);
    }
    if (auto alive = remote.lock()) {
        alive->want(name, type, read);
    }
    return subscription_access::make(std::move(which));
}

}  // namespace dross
