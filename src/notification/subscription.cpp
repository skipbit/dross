#include "dross/notification/subscription.h"

#include "notification/subscription_access.h"
#include "notification/subscription_storage.h"
#include "thread/operation_queue_access.h"
#include "thread/runloop_access.h"

#include <memory>
#include <utility>
#include <variant>

namespace dross {

subscription::storage::storage(delivery_target target, function_type function)
    : _target{ std::move(target) }
    , _function{ std::move(function) }
{
}

void subscription::storage::deliver(const std::shared_ptr<const void>& value)
{
    if (_ended.load()) {
        return;
    }
    if (std::holds_alternative<direct_target>(_target)) {
        call(value.get());
        return;
    }

    auto task = [self = shared_from_this(), value]() {
        self->call(value.get());
    };
    if (const auto* loop = std::get_if<runloop>(&_target)) {
        runloop target_loop = *loop;
        target_loop.perform(std::move(task));
    } else if (auto queue = operation_queue_access::lock(std::get<operation_queue_access::weak_queue>(_target))) {
        queue->submit(std::move(task));
    }
}

void subscription::storage::end() noexcept
{
    _ended.store(true);
}

bool subscription::storage::active() const
{
    if (_ended.load()) {
        return false;
    }
    if (const auto* loop = std::get_if<runloop>(&_target)) {
        return (! runloop_access::finished(*loop));
    }
    if (const auto* weak = std::get_if<operation_queue_access::weak_queue>(&_target)) {
        const auto queue = operation_queue_access::lock(*weak);
        return (queue && operation_queue_access::accepting(*queue));
    }
    return true;
}

void subscription::storage::call(const void* value) const
{
    if (_ended.load()) {
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
