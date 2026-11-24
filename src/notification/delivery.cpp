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

    using place_type = std::variant<subscribing_thread, delivery_target>;

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
    return delivery{ std::make_shared<const storage>(storage::place_type{ storage::subscribing_thread{} }) };
}

delivery delivery::queued(runloop loop)
{
    return delivery{ std::make_shared<const storage>(storage::place_type{ delivery_target{ std::move(loop) } }) };
}

delivery delivery::direct()
{
    return delivery{ std::make_shared<const storage>(storage::place_type{ delivery_target{ direct_target{} } }) };
}

delivery delivery::on(const operation_queue& queue)
{
    return delivery{ std::make_shared<const storage>(storage::place_type{ delivery_target{ operation_queue_access::weaken(queue) } }) };
}

delivery_target delivery_access::resolve(const delivery& which)
{
    const auto on_subscribing_thread = [](const delivery::storage::subscribing_thread&) -> delivery_target {
        return runloop::current();
    };
    const auto on_resolved = [](const delivery_target& target) {
        return target;
    };
    return std::visit(overloaded{ on_subscribing_thread, on_resolved }, which._store->place);
}

}  // namespace dross
