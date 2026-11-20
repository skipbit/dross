#include "dross/notification/listener.h"

#include "notification/broadcaster_access.h"
#include "notification/delivery_access.h"
#include "notification/subscription_access.h"
#include "notification/topic_address.h"

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

// The state behind a listener handle: the broadcaster it hears, held without
// keeping it going.
class DROSS_LISTENER_STORAGE_HIDDEN listener::storage final {
public:
    explicit storage(std::weak_ptr<broadcaster_access::storage_type> source)
        : source{ std::move(source) }
    {
    }

    const std::weak_ptr<broadcaster_access::storage_type> source;
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

bool listener::operator==(const listener& other) const noexcept
{
    return _store == other._store;
}

subscription listener::subscribe_any(std::string_view name,
                                     const std::type_info& type,
                                     std::function<void(const void*)> function,
                                     const delivery& how)
{
    using receiver = subscription_access::receiver;

    const auto where = topic_address::of(name, type);
    receiver::origin from{ _store, _store->source, nullptr };
    from.detach = [source = _store->source, name = std::string{ name }, type = &type]() {
        if (auto alive = source.lock()) {
            alive->prune(topic_address::of(name, *type));
        }
    };

    auto which = std::make_shared<receiver>(delivery_access::resolve(how), std::move(function), std::move(from));
    if (auto source = _store->source.lock()) {
        source->add(where, which);
    }
    return subscription_access::make(std::move(which));
}

}  // namespace dross
