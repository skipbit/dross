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
    const auto on_subscribing_thread = [](const delivery::storage::subscribing_thread&) -> delivery_target {
        return current_runloop();
    };
    const auto on_direct = [](const direct_target& target) -> delivery_target {
        return target;
    };
    const auto on_loop = [](const runloop& loop) -> delivery_target {
        return loop;
    };
    const auto on_queue = [](const operation_queue_access::weak_queue& queue) -> delivery_target {
        return queue;
    };
    return std::visit(overloaded{ on_subscribing_thread, on_direct, on_loop, on_queue }, which._store->place);
}

}  // namespace dross
