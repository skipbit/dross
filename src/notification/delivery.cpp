#include "dross/notification/delivery.h"

#include "notification/delivery_access.h"

#include <memory>
#include <utility>
#include <variant>

namespace dross {

class delivery::storage final {
public:
    // queued() without a loop, which names a loop only once it is known
    // which thread subscribes.
    struct subscribing_thread final { };

    using place_type = std::variant<subscribing_thread, direct_target, runloop, operation_queue_access::weak_queue>;

    explicit storage(place_type where)
        : place{ std::move(where) }
    {
    }

    const place_type place;
};

delivery::delivery(std::shared_ptr<const storage> store) noexcept
    : _store{ std::move(store) }
{
}

delivery::delivery(const delivery& other) = default;

delivery::~delivery() = default;

delivery& delivery::operator=(const delivery& other) = default;

delivery delivery::queued()
{
    return delivery{ std::make_shared<const storage>(storage::subscribing_thread{}) };
}

delivery delivery::queued(runloop loop)
{
    return delivery{ std::make_shared<const storage>(std::move(loop)) };
}

delivery delivery::direct()
{
    return delivery{ std::make_shared<const storage>(direct_target{}) };
}

delivery delivery::on(const operation_queue& queue)
{
    return delivery{ std::make_shared<const storage>(operation_queue_access::weaken(queue)) };
}

delivery_target delivery_access::resolve(const delivery& which)
{
    const auto& place = which._store->place;
    if (std::holds_alternative<delivery::storage::subscribing_thread>(place)) {
        return current_runloop();
    }
    if (const auto* loop = std::get_if<runloop>(&place)) {
        return *loop;
    }
    if (const auto* queue = std::get_if<operation_queue_access::weak_queue>(&place)) {
        return *queue;
    }
    return direct_target{};
}

}  // namespace dross
