#include "dross/notification/subscription.h"

#include "dross/thread/runloop.h"
#include "notification/subscription_access.h"
#include "notification/subscription_storage.h"
#include "thread/operation_queue_access.h"
#include "thread/runloop_access.h"

#include <memory>
#include <optional>
#include <utility>
#include <variant>

namespace dross {

subscription::storage::storage(delivery_target target, function_type function, std::optional<origin> from)
    : _target{ std::move(target) }
    , _function{ std::move(function) }
    , _from{ std::move(from) }
{
}

void subscription::storage::deliver(const std::shared_ptr<const void>& value, const std::shared_ptr<const sending>& sent, bool deferred)
{
    if (_ended.load()) {
        return;
    }
    const auto on_direct = [this, &value, &sent, deferred](const direct_target&) {
        if (deferred) {
            runloop::current().perform(make_task(value, sent));
        } else {
            call(value.get(), *sent);
        }
    };
    const auto on_loop = [this, &value, &sent](const runloop& loop) {
        runloop target_loop = loop;
        target_loop.perform(make_task(value, sent));
    };
    const auto on_queue = [this, &value, &sent](const operation_queue_access::weak_queue& weak) {
        if (auto queue = operation_queue_access::lock(weak)) {
            queue->submit(make_task(value, sent));
        }
    };
    std::visit(overloaded{ on_direct, on_loop, on_queue }, _target);
}

void subscription::storage::end() noexcept
{
    if (_ended.exchange(true)) {
        return;
    }
    if (_from && _from->detach) {
        // Detaching allocates a new list; running out of memory here ends the
        // program, as noexcept does.
        _from->detach();
    }
}

bool subscription::storage::ended() const noexcept
{
    return _ended.load();
}

bool subscription::storage::active() const
{
    if (_ended.load()) {
        return false;
    }
    if (_from && _from->source.expired()) {
        return false;
    }
    const auto on_direct = [](const direct_target&) {
        return true;
    };
    const auto on_loop = [](const runloop& loop) {
        return (! runloop_access::finished(loop));
    };
    const auto on_queue = [](const operation_queue_access::weak_queue& weak) {
        const auto queue = operation_queue_access::lock(weak);
        return (queue && operation_queue_access::accepting(*queue));
    };
    return std::visit(overloaded{ on_direct, on_loop, on_queue }, _target);
}

std::function<void()> subscription::storage::make_task(const std::shared_ptr<const void>& value, const std::shared_ptr<const sending>& sent)
{
    return [self = shared_from_this(), value, sent]() {
        self->call(value.get(), *sent);
    };
}

void subscription::storage::call(const void* value, const sending& sent) const
{
    if (_ended.load() || sent.cancelled()) {
        return;
    }
    _function(value);
}

subscription::subscription() noexcept = default;

subscription::subscription(std::shared_ptr<storage> store) noexcept
    : _store{ std::move(store) }
{
}

subscription::subscription(subscription&& other) noexcept
    : _store{ std::move(other._store) }
{
}

subscription::~subscription()
{
    reset();
}

subscription& subscription::operator=(subscription&& other) noexcept
{
    if (this != &other) {
        reset();
        _store = std::move(other._store);
    }
    return *this;
}

void subscription::reset() noexcept
{
    if (_store) {
        _store->end();
        _store.reset();
    }
}

bool subscription::active() const
{
    return (_store && _store->active());
}

subscription subscription_access::make(std::shared_ptr<receiver> which)
{
    return subscription{ std::move(which) };
}

}  // namespace dross
