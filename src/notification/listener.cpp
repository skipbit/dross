#include "dross/notification/listener.h"

#include "notification/broadcaster_access.h"
#include "notification/delivery_access.h"
#include "notification/subscription_access.h"
#include "notification/topic_address.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace dross {

// The state behind a listener handle: the broadcaster it hears, held without
// keeping it going.
class listener::storage final {
public:
    explicit storage(std::weak_ptr<broadcaster_access::storage_type> source)
        : source{ std::move(source) }
    {
    }

    const std::weak_ptr<broadcaster_access::storage_type> source;
};

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

bool listener::operator==(const listener& other) const noexcept
{
    return _store == other._store;
}

subscription listener::subscribe_any(std::string_view name,
                                     std::string_view type,
                                     std::function<void(const void*)> function,
                                     const delivery& how)
{
    using receiver = subscription_access::receiver;

    const topic_address where{ name, type };
    receiver::origin from{ _store, _store->source, nullptr };
    from.detach = [source = _store->source, name = std::string{ name }, type = std::string{ type }]() {
        if (auto alive = source.lock()) {
            alive->prune(topic_address{ name, type });
        }
    };

    auto which = std::make_shared<receiver>(delivery_access::resolve(how), std::move(function), std::move(from));
    if (auto source = _store->source.lock()) {
        source->add(where, which);
    }
    return subscription_access::make(std::move(which));
}

}  // namespace dross
