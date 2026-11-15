#include "dross/notification/listener.h"

#include "notification/broadcaster_access.h"
#include "notification/delivery_access.h"
#include "notification/subscription_access.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace dross {

namespace {

class notification_category_type final : public std::error_category {
public:
    const char* name() const noexcept override
    {
        return "dross.notification";
    }

    std::string message(int value) const override
    {
        switch (static_cast<notification_errc>(value)) {
        case notification_errc::empty_function:
            return "subscriber function is empty";
        }
        return "unknown notification error";
    }
};

}  // namespace

const std::error_category& notification_category() noexcept
{
    static const notification_category_type the_category;
    return the_category;
}

std::error_code make_error_code(notification_errc e) noexcept
{
    return { static_cast<int>(e), notification_category() };
}

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

    auto which = std::make_shared<receiver>(delivery_access::resolve(how),
                                            std::move(function),
                                            receiver::origin{ _store, _store->source });
    if (auto source = _store->source.lock()) {
        source->add(name, type, which);
    }
    return subscription_access::make(std::move(which));
}

}  // namespace dross
