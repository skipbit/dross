#include "dross/notification/observable.h"

#include "dross/thread/runloop.h"
#include "notification/delivery_access.h"
#include "notification/notification_access.h"
#include "notification/subscription_access.h"

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace dross {

namespace {

using value_pointer = std::shared_ptr<const void>;

// One change: the value before it and the value after it.
struct change final {
    value_pointer previous;
    value_pointer value;
};

// The change one subscriber has yet to be given. A newer change replaces
// one that is still waiting, which is what lets values in between be
// skipped.
class waiting_change final {
public:
    // Returns true when nothing was waiting, so the caller hands the
    // subscriber a delivery; otherwise the one already handed takes this
    // change instead.
    bool offer(change next)
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        const bool was_empty = (! _change.has_value());
        _change = std::move(next);
        return was_empty;
    }

    std::optional<change> take()
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        return std::exchange(_change, std::nullopt);
    }

private:
    std::mutex _mutex;
    std::optional<change> _change;
};

}  // namespace

// The state behind an observable: its owner, its value, and who watches it.
// The value and the changes in flight are touched on the owner's thread
// alone; the list of watchers is guarded by a lock.
class observable_core::storage final : public std::enable_shared_from_this<storage> {
public:
    using receiver = subscription_access::receiver;

    struct watcher final {
        std::shared_ptr<receiver> subscriber;
        std::shared_ptr<waiting_change> waiting;
    };

    using watchers = std::vector<watcher>;

    storage(value_pointer initial, equal_type equal)
        : _owner{ current_runloop() }
        , _value{ std::move(initial) }
        , _equal{ equal }
    {
    }

    bool owned_here() const
    {
        return (current_runloop() == _owner);
    }

    const runloop& owner() const noexcept
    {
        return _owner;
    }

    // Owner's thread only.
    const void* value() const noexcept
    {
        return _value.get();
    }

    // Owner's thread only. A change made from inside a watcher's function
    // takes effect at once, and is handed on once the change being handed on
    // has reached every watcher, so each watcher sees the changes in order.
    void apply(value_pointer next)
    {
        if (_equal(_value.get(), next.get())) {
            return;
        }
        value_pointer previous = std::exchange(_value, std::move(next));
        _changes.push_back(change{ std::move(previous), _value });
        if (_handing_on) {
            return;
        }

        _handing_on = true;
        // An exception from a watcher's function leaves the changes not yet
        // handed on behind, as notify() leaves the subscribers after it.
        struct finish final {
            storage& self;
            ~finish()
            {
                self._changes.clear();
                self._handing_on = false;
            }
        } const guard{ *this };

        while (! _changes.empty()) {
            const change current = std::move(_changes.front());
            _changes.pop_front();
            for (const auto& each : *snapshot()) {
                if (each.waiting->offer(current)) {
                    each.subscriber->deliver(nullptr, _sent, false);
                }
            }
        }
    }

    void add(watcher which)
    {
        std::shared_ptr<const watchers> replaced;
        const std::lock_guard<std::mutex> guard{ _mutex };
        replaced = rebuild(std::move(which));
    }

    void prune()
    {
        std::shared_ptr<const watchers> replaced;
        const std::lock_guard<std::mutex> guard{ _mutex };
        replaced = rebuild(std::nullopt);
    }

private:
    std::shared_ptr<const watchers> snapshot() const
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        return _watchers;
    }

    // Replaces the list with one holding the watchers that have not ended,
    // and added when given. Returns the list it replaced, which the caller
    // lets go only after _mutex is released, because dropping it can destroy
    // an ended watcher's function and run its captures' code. Must hold
    // _mutex.
    [[nodiscard]] std::shared_ptr<const watchers> rebuild(std::optional<watcher> added)
    {
        std::shared_ptr<const watchers> replaced = _watchers;
        watchers kept;
        kept.reserve(replaced->size() + 1);
        for (const auto& each : *replaced) {
            if (! each.subscriber->ended()) {
                kept.push_back(each);
            }
        }
        if (added) {
            kept.push_back(std::move(*added));
        }
        _watchers = std::make_shared<const watchers>(std::move(kept));
        return replaced;
    }

    const runloop _owner;
    value_pointer _value;
    const equal_type _equal;

    // Changes waiting to be handed on, and whether they are being handed on
    // now.
    std::deque<change> _changes;
    bool _handing_on{ false };

    // What deliver() checks for cancelling; an observable's change is never
    // cancelled.
    const std::shared_ptr<const notification_access::storage_type> _sent{ std::make_shared<notification_access::storage_type>() };

    mutable std::mutex _mutex;
    std::shared_ptr<const watchers> _watchers{ std::make_shared<const watchers>() };
};

observable_core::observable_core(std::shared_ptr<const void> initial, equal_type equal)
    : _store{ std::make_shared<storage>(std::move(initial), equal) }
{
}

observable_core::observable_core(observable_core&& other) noexcept = default;

observable_core& observable_core::operator=(observable_core&& other) noexcept = default;

observable_core::~observable_core() = default;

const void* observable_core::get() const
{
    if (! _store->owned_here()) {
        std::fputs("dross: observable::get: called from a thread that does not own the observable\n", stderr);
        std::abort();
    }
    return _store->value();
}

bool observable_core::set(std::shared_ptr<const void> value) const
{
    return set(weaken(), std::move(value));
}

bool observable_core::set(const std::weak_ptr<storage>& which, std::shared_ptr<const void> value)
{
    const auto target = which.lock();
    if (! target) {
        return false;
    }
    if (target->owned_here()) {
        target->apply(std::move(value));
        return true;
    }
    runloop owner = target->owner();
    return owner.perform([which, value = std::move(value)]() mutable {
        if (const auto alive = which.lock()) {
            alive->apply(std::move(value));
        }
    });
}

subscription observable_core::when_changed(function_type function, const delivery& how) const
{
    using receiver = storage::receiver;

    auto waiting = std::make_shared<waiting_change>();
    auto call = [waiting, function = std::move(function)](const void*) {
        if (const auto next = waiting->take()) {
            function(next->previous.get(), next->value.get());
        }
    };

    const std::weak_ptr<storage> source = _store;
    receiver::origin from{ nullptr, source, [source]() {
        if (const auto alive = source.lock()) {
            alive->prune();
        }
    } };

    auto which = std::make_shared<receiver>(delivery_access::resolve(how), std::move(call), std::move(from));
    _store->add(storage::watcher{ which, std::move(waiting) });
    return subscription_access::make(std::move(which));
}

std::weak_ptr<observable_core::storage> observable_core::weaken() const noexcept
{
    return _store;
}

}  // namespace dross
