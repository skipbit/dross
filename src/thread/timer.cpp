#include "dross/thread/timer.h"

#include "thread/deadline.h"
#include "thread/runloop_storage.h"
#include "thread/timer_storage.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

namespace dross {

namespace {

std::uint64_t next_timer_id()
{
    static std::atomic<std::uint64_t> next{ 0 };
    return next.fetch_add(1);
}

}  // namespace

timer::storage::storage(std::chrono::milliseconds interval,
                        bool repeats,
                        std::function<void(timer)> callback,
                        std::weak_ptr<runloop::storage> loop)
    : _interval{ interval }
    , _repeats{ repeats }
    , _callback{ std::move(callback) }
    , _loop{ std::move(loop) }
    , _id{ next_timer_id() }
{
}

void timer::storage::invalidate()
{
    mark_invalid();
    if (auto loop = _loop.lock()) {
        loop->remove_timer(this);
    }
}

bool timer::storage::valid() const
{
    return _valid.load();
}

bool timer::storage::repeats() const
{
    return _repeats;
}

std::chrono::milliseconds timer::storage::interval() const
{
    return _interval;
}

std::uint64_t timer::storage::id() const noexcept
{
    return _id;
}

void timer::storage::fire()
{
    if (_callback) {
        _callback(timer{ shared_from_this() });
    }
}

void timer::storage::mark_invalid()
{
    _valid.store(false);
}

timer::timer(std::shared_ptr<storage> store) noexcept
    : _store{ std::move(store) }
{
}

timer::timer(const timer& other) = default;

timer::~timer() = default;

timer& timer::operator=(const timer& other) = default;

void timer::invalidate()
{
    _store->invalidate();
}

bool timer::valid() const
{
    return _store->valid();
}

bool timer::repeats() const
{
    return _store->repeats();
}

std::chrono::milliseconds timer::interval() const
{
    return _store->interval();
}

bool timer::operator==(const timer& other) const noexcept
{
    return _store == other._store;
}

timer timer::make(std::chrono::milliseconds interval, bool repeats, std::function<void(timer)> callback, runloop loop)
{
    // Checked before the move below, the same as runloop::perform() checks
    // its own task: an empty callback is never installed, the same as one
    // given to a loop that has already finished.
    const bool has_callback = static_cast<bool>(callback);

    auto store = std::make_shared<storage>(interval, repeats, std::move(callback), loop._store);

    if (! has_callback) {
        store->mark_invalid();
    } else {
        const auto first_deadline = deadline::after(loop._store->now(), interval);
        if (! loop._store->install_timer(store, first_deadline)) {
            // The loop is already finished; it will never fire.
            store->mark_invalid();
        }
    }

    return timer{ std::move(store) };
}

timer timer::repeating(std::chrono::milliseconds interval, std::function<void(timer)> callback)
{
    return repeating(interval, std::move(callback), current_runloop());
}

timer timer::repeating(std::chrono::milliseconds interval, std::function<void(timer)> callback, runloop loop)
{
    return make(interval, true, std::move(callback), std::move(loop));
}

timer timer::once(std::chrono::milliseconds delay, std::function<void(timer)> callback)
{
    return once(delay, std::move(callback), current_runloop());
}

timer timer::once(std::chrono::milliseconds delay, std::function<void(timer)> callback, runloop loop)
{
    return make(delay, false, std::move(callback), std::move(loop));
}

}  // namespace dross
