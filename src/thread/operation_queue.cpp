#include "dross/thread/operation_queue.h"

#include "dross/thread/runloop.h"
#include "dross/thread/thread.h"
#include "thread/deadline.h"
#include "thread/operation_access.h"
#include "thread/operation_queue_access.h"
#include "thread/thread_access.h"
#include "thread/time_source.h"

#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <set>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dross {

namespace {

// Whether priority is one of the values operation_priority names, which a
// cast from an integer need not be.
bool names_a_priority(operation_priority priority)
{
    return (static_cast<std::size_t>(priority) <= static_cast<std::size_t>(operation_priority::high));
}

// A number no other queue in this process has, for its results to carry.
std::uint64_t next_queue_number() noexcept
{
    static std::atomic<std::uint64_t> last{ 0 };
    return last.fetch_add(1, std::memory_order_relaxed) + 1;
}

// What the workers share with the queue's handles: the tasks waiting to run,
// the ones running, and the workers themselves. The workers hold it, not the
// handles' storage, so the handles going does not take it from a worker
// still running. It starts a worker when a task is ready and every worker
// it has is busy, up to max_concurrency, and holds a handle to each until
// that worker leaves.
class backlog final : public std::enable_shared_from_this<backlog> {
public:
    backlog(std::size_t max_concurrency, std::shared_ptr<const time_source> source, std::chrono::milliseconds idle)
        : _source{ std::move(source) }
        , _number{ next_queue_number() }
        , _max_concurrency{ max_concurrency }
        , _idle{ idle }
    {
    }

    // Queues task for the workers, with a result its return value fills
    // in. The task is wrapped before _mutex is taken, and a task not
    // accepted, or cancelled as it is accepted, is destroyed after it is
    // released, as is what options holds. The result is made under _mutex,
    // so within one queue ids follow the order tasks are accepted in, and a
    // task not accepted takes none.
    std::expected<operation_result, error> enqueue(std::function<std::any()> task, operation_options options)
    {
        if (! names_a_priority(options.priority)) {
            return std::unexpected(error(operation_errc::invalid_priority));
        }
        if (! all_from_here(options.after)) {
            return std::unexpected(error(operation_errc::foreign_dependency));
        }
        auto held = std::make_unique<std::function<std::any()>>(std::move(task));
        const std::lock_guard<std::mutex> guard{ _mutex };
        if (! _accepting) {
            return std::unexpected(error(operation_errc::queue_stopped));
        }
        // With no worker, a task accepted could never run.
        if (_workers.empty() && (! start_worker())) {
            return std::unexpected(error(operation_errc::queue_stopped));
        }
        operation_result result = operation_access::make(_source, _number);
        if (any_cancelled(options.after)) {
            operation_access::cancel(result);
        } else {
            queue(options, result, std::move(held));
        }
        return result;
    }

    // Takes the task whose result has id off the queue, whether it is ready
    // to run or still waiting for what it runs after, and marks its result
    // cancelled; then does the same for every task that runs after it, and
    // after those in turn. The results are marked under _mutex, so a
    // wait_for() that sees the tasks gone also sees their results finished.
    // The tasks themselves are destroyed only once _mutex is released, as
    // take() leaves a task it hands out, since what they captured may use
    // the queue as they go.
    bool cancel(const operation_id& id)
    {
        std::vector<entry> removed;
        {
            const std::lock_guard<std::mutex> guard{ _mutex };
            std::optional<entry> found = take_out(id);
            if (! found) {
                return false;
            }
            removed.push_back(std::move(*found));
            for (std::size_t i = 0; i < removed.size(); ++i) {
                _unfinished.erase(removed[i].order);
                operation_access::cancel(removed[i].result);
                for (entry& after : take_dependents(removed[i].result.id())) {
                    removed.push_back(std::move(after));
                }
            }
        }
        _settled.notify_all();
        return true;
    }

    // Stops accepting and wakes every worker, so one with nothing left on
    // its loop leaves at once rather than at the end of its idle wait. The
    // wake is queued under _mutex, so a worker still here has it ahead of
    // the check that lets it leave.
    void stop()
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        _accepting = false;
        for (auto& [id, worker] : _workers) {
            worker.handle.perform([]() {
                runloop::current().quit();
            });
        }
    }

    // Whether the calling thread is one of the workers.
    bool on_a_worker()
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        return std::any_of(_workers.begin(), _workers.end(), [](const auto& worker) {
            return worker.second.handle.is_current();
        });
    }

    std::size_t max_concurrency() const noexcept
    {
        return _max_concurrency;
    }

    // Waits until every task accepted before the call has finished, or
    // until timeout has passed; with wait false, only reports whether they
    // have.
    bool wait_for(std::chrono::milliseconds timeout, bool wait)
    {
        std::unique_lock<std::mutex> lock{ _mutex };
        const std::uint64_t target = _accepted;
        const auto settled = [this, target]() {
            return (_unfinished.empty() || (*_unfinished.begin() >= target));
        };
        if ((! wait) || (timeout <= std::chrono::milliseconds::zero())) {
            return settled();
        }

        const auto deadline = deadline::after(_source->now(), timeout);
        while ((! settled()) && (_source->now() < deadline)) {
            _source->wait_until(lock, _settled, deadline);
        }
        return settled();
    }

private:
    // A task in the list, with its place in the order tasks were accepted
    // and the result it fills in. The task is behind a pointer, so moving an
    // entry under _mutex runs none of the task's own code, and what the move
    // leaves behind holds nothing: a std::function may copy a small task as
    // it moves.
    struct entry final {
        std::uint64_t order;
        operation_result result;
        std::unique_ptr<std::function<std::any()>> call;

        void run() const
        {
            operation_access::finish(result, (*call)());
        }
    };

    // A task that runs after others, with how many of them have not
    // returned yet.
    struct waiting final {
        entry task;
        operation_priority priority;
        std::size_t remaining;
    };

    // Whether every result in after was made by this queue.
    bool all_from_here(const std::vector<operation_result>& after) const
    {
        return std::all_of(after.begin(), after.end(), [this](const operation_result& before) {
            return (operation_access::queue_of(before) == _number);
        });
    }

    // Whether a result in after was cancelled, so a task to run after it
    // never runs. Called with _mutex held, under which cancel() marks a
    // result.
    static bool any_cancelled(const std::vector<operation_result>& after)
    {
        return std::any_of(after.begin(), after.end(), [](const operation_result& before) {
            return operation_access::is_cancelled(before);
        });
    }

    // Accepts a task. One that runs after results not yet finished waits
    // aside until they have all returned; any other goes straight to the
    // list of its priority. Takes options by reference, so the results in
    // it, which may be the last handles to them, go only once the caller
    // has released _mutex. Called with _mutex held.
    void queue(const operation_options& options, operation_result result, std::unique_ptr<std::function<std::any()>> call)
    {
        const std::uint64_t order = _accepted++;
        _unfinished.insert(order);
        _dependents.try_emplace(result.id());
        entry accepted{ order, std::move(result), std::move(call) };

        // A result still in _dependents has not finished; any other that
        // got past any_cancelled() has returned, and is already met. A
        // result named more than once in options.after is counted each
        // time, and done() counts it off each time too.
        std::size_t remaining = 0;
        for (const operation_result& before : options.after) {
            const auto found = _dependents.find(before.id());
            if (found != _dependents.end()) {
                found->second.push_back(order);
                ++remaining;
            }
        }
        if (remaining > 0) {
            _waiting.emplace(order, waiting{ std::move(accepted), options.priority, remaining });
        } else {
            make_ready(std::move(accepted), options.priority);
        }
    }

    // A worker, and whether it has a sweep on its loop.
    struct worker final {
        thread handle;
        bool sweeping;
    };

    // Adds a task to the list of its priority and hands a sweep to every
    // worker that has none, starting one first when every worker has one
    // and the queue is still below max_concurrency. A sweep keeps taking
    // until every list is empty, so an existing one reaches the task too.
    // Every idle worker gets one, not just the first, since a worker with
    // no sweep may still be busy running something else on its loop.
    // Whichever gets there first takes the task, and the rest find the
    // lists empty. Called with _mutex held. Once the queue has stopped no
    // worker is started, as the worker whose done() made the task ready
    // takes it as it sweeps.
    void make_ready(entry task, operation_priority priority)
    {
        _tasks[static_cast<std::size_t>(priority)].push_back(std::move(task));

        const bool all_busy = std::all_of(_workers.begin(), _workers.end(), [](const auto& each) {
            return each.second.sweeping;
        });
        if (all_busy && _accepting && (_workers.size() < _max_concurrency)) {
            // A worker that does not start leaves the task to the others.
            start_worker();
        }
        for (auto& [id, each] : _workers) {
            if (! each.sweeping) {
                each.sweeping = true;
                each.handle.perform(sweep(id));
            }
        }
    }

    // Starts a worker with no sweep yet; false when the system will not
    // start one. Called with _mutex held, which the new thread does not
    // take before it is started.
    bool start_worker()
    {
        const std::uint64_t id = _next_worker++;
        auto started = thread_access::start([self = shared_from_this(), id]() {
            self->serve(id);
        }, _source);
        if (! started) {
            return false;
        }
        _workers.emplace(id, worker{ std::move(*started), false });
        return true;
    }

    // The body of a worker: runs its loop, which takes its sweeps, until it
    // leaves.
    void serve(std::uint64_t id)
    {
        runloop loop = runloop::current();
        for (;;) {
            const std::size_t ran = loop.run_for(_idle);
            if (leave(id, ran, loop)) {
                return;
            }
        }
    }

    // Lets the worker go once it has been idle for a whole wait, or at once
    // after the queue has stopped, as long as nothing is queued on its loop
    // and no timer is installed there. Its handle is dropped once _mutex is
    // released. Called between waits, when no sweep is running on it.
    bool leave(std::uint64_t id, std::size_t ran, const runloop& loop)
    {
        std::optional<thread> gone;
        {
            const std::lock_guard<std::mutex> guard{ _mutex };
            if (_accepting && (ran > 0)) {
                return false;
            }
            if ((loop.pending_count() > 0) || (loop.timer_count() > 0)) {
                return false;
            }
            const auto found = _workers.find(id);
            if (found != _workers.end()) {
                gone = std::move(found->second.handle);
                _workers.erase(found);
            }
        }
        return true;
    }

    std::function<void()> sweep(std::uint64_t id)
    {
        return [self = shared_from_this(), id]() {
            self->run_until_empty(id);
        };
    }

    void run_until_empty(std::uint64_t id)
    {
        while (auto next = take(id)) {
            next->run();
            done(*next);
        }
    }

    // The first task of the highest priority waiting, or none once every
    // list is empty, which also clears this worker's sweep so the next
    // enqueue() hands it another.
    std::optional<entry> take(std::uint64_t id)
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        for (auto tasks = _tasks.rbegin(); tasks != _tasks.rend(); ++tasks) {
            if (! tasks->empty()) {
                entry next = std::move(tasks->front());
                tasks->pop_front();
                return next;
            }
        }
        _workers.at(id).sweeping = false;
        return std::nullopt;
    }

    // Marks a task finished and counts it off every task that runs after
    // it; those it was the last for become ready.
    void done(const entry& finished)
    {
        {
            const std::lock_guard<std::mutex> guard{ _mutex };
            _unfinished.erase(finished.order);
            for (const std::uint64_t order : take_dependent_orders(finished.result.id())) {
                const auto found = _waiting.find(order);
                // Gone once cancelled.
                if ((found != _waiting.end()) && (--found->second.remaining == 0)) {
                    waiting ready = std::move(found->second);
                    _waiting.erase(found);
                    make_ready(std::move(ready.task), ready.priority);
                }
            }
        }
        _settled.notify_all();
    }

    // The task whose result has id, taken off its list or out of those
    // waiting, or none when neither has it. Called with _mutex held.
    std::optional<entry> take_out(const operation_id& id)
    {
        for (auto& tasks : _tasks) {
            const auto found = std::find_if(tasks.begin(), tasks.end(), [&id](const entry& ready) {
                return (ready.result.id() == id);
            });
            if (found != tasks.end()) {
                entry task = std::move(*found);
                tasks.erase(found);
                return task;
            }
        }
        const auto found = std::find_if(_waiting.begin(), _waiting.end(), [&id](const auto& aside) {
            return (aside.second.task.result.id() == id);
        });
        if (found == _waiting.end()) {
            return std::nullopt;
        }
        entry task = std::move(found->second.task);
        _waiting.erase(found);
        return task;
    }

    // Takes the order of every task waiting to run after the one whose
    // result has id out of _dependents; none once it has gone. Called with
    // _mutex held.
    std::vector<std::uint64_t> take_dependent_orders(const operation_id& id)
    {
        auto node = _dependents.extract(id);
        return node ? std::move(node.mapped()) : std::vector<std::uint64_t>{};
    }

    // Takes every task still waiting to run after the one whose result has
    // id out of those waiting. Called with _mutex held.
    std::vector<entry> take_dependents(const operation_id& id)
    {
        std::vector<entry> taken;
        for (const std::uint64_t order : take_dependent_orders(id)) {
            const auto found = _waiting.find(order);
            if (found != _waiting.end()) {
                taken.push_back(std::move(found->second.task));
                _waiting.erase(found);
            }
        }
        return taken;
    }

    // Set once, at construction, and never reassigned, so they are read
    // without _mutex.
    const std::shared_ptr<const time_source> _source;
    const std::uint64_t _number;
    const std::size_t _max_concurrency;
    // How long a worker waits with nothing to do before it leaves.
    const std::chrono::milliseconds _idle;

    std::mutex _mutex;
    std::condition_variable _settled;
    // One list per operation_priority, indexed by its value so the lowest
    // comes first, each in the order its tasks were accepted.
    static_assert(static_cast<std::size_t>(operation_priority::low) == 0);
    static_assert(static_cast<std::size_t>(operation_priority::normal) == 1);
    static_assert(static_cast<std::size_t>(operation_priority::high) == 2);
    std::array<std::deque<entry>, static_cast<std::size_t>(operation_priority::high) + 1> _tasks;
    std::uint64_t _accepted{ 0 };
    // The order of every task accepted and not yet finished or cancelled,
    // waiting or running, so wait_for() needs only the first.
    std::set<std::uint64_t> _unfinished;
    // Every task that has not finished, by the id of its result, with the
    // order of each task waiting to run after it.
    std::unordered_map<operation_id, std::vector<std::uint64_t>> _dependents;
    // Every task waiting for results to return before it runs, by order.
    std::map<std::uint64_t, waiting> _waiting;
    // Every worker that has not left, by a number of its own.
    std::map<std::uint64_t, worker> _workers;
    std::uint64_t _next_worker{ 0 };
    bool _accepting{ true };
};

}  // namespace

class operation_queue::storage final {
public:
    // A new queue, which starts its workers as tasks need them.
    static std::expected<std::shared_ptr<storage>, error> make(std::size_t max_concurrency,
                                                               std::shared_ptr<const time_source> source,
                                                               std::chrono::milliseconds idle);

    explicit storage(std::shared_ptr<backlog> work) noexcept;
    ~storage();

    std::expected<operation_result, error> enqueue_any(std::function<std::any()> task, operation_options options);
    bool cancel(const operation_id& id);
    bool wait_for(std::chrono::milliseconds timeout);

    std::size_t max_concurrency() const noexcept;

private:
    std::shared_ptr<backlog> _backlog;
};

std::expected<std::shared_ptr<operation_queue::storage>, error> operation_queue::storage::make(std::size_t max_concurrency,
                                                                                               std::shared_ptr<const time_source> source,
                                                                                               std::chrono::milliseconds idle)
{
    if (max_concurrency == 0) {
        return std::unexpected(error{ std::errc::invalid_argument });
    }
    try {
        return std::make_shared<storage>(std::make_shared<backlog>(max_concurrency, std::move(source), idle));
    } catch (const std::bad_alloc&) {
        return std::unexpected(error{ std::errc::not_enough_memory });
    }
}

operation_queue::storage::storage(std::shared_ptr<backlog> work) noexcept
    : _backlog{ std::move(work) }
{
}

operation_queue::storage::~storage()
{
    _backlog->stop();
}

std::expected<operation_result, error> operation_queue::storage::enqueue_any(std::function<std::any()> task,
                                                                             operation_options options)
{
    return _backlog->enqueue(std::move(task), std::move(options));
}

bool operation_queue::storage::cancel(const operation_id& id)
{
    return _backlog->cancel(id);
}

bool operation_queue::storage::wait_for(std::chrono::milliseconds timeout)
{
    return _backlog->wait_for(timeout, (! _backlog->on_a_worker()));
}

std::size_t operation_queue::storage::max_concurrency() const noexcept
{
    return _backlog->max_concurrency();
}

operation_queue::operation_queue(std::shared_ptr<storage> store) noexcept
    : _store{ std::move(store) }
{
}

std::expected<operation_queue, error> operation_queue::create(std::size_t max_concurrency)
{
    return operation_queue_access::make(max_concurrency, time_source::steady());
}

operation_queue::operation_queue(const operation_queue& other) = default;

operation_queue::~operation_queue() = default;

operation_queue& operation_queue::operator=(const operation_queue& other) = default;

std::expected<operation_result, error> operation_queue::enqueue_any(std::function<std::any()> task,
                                                                    operation_options options)
{
    return _store->enqueue_any(std::move(task), std::move(options));
}

bool operation_queue::cancel(const operation_id& id)
{
    return _store->cancel(id);
}

bool operation_queue::wait_for(std::chrono::milliseconds timeout)
{
    return _store->wait_for(timeout);
}

std::size_t operation_queue::max_concurrency() const noexcept
{
    return _store->max_concurrency();
}

bool operation_queue::operator==(const operation_queue& other) const noexcept
{
    return _store == other._store;
}

std::expected<operation_queue, error> operation_queue_access::make(std::size_t max_concurrency, std::shared_ptr<const time_source> source)
{
    return make(max_concurrency, std::move(source), std::chrono::milliseconds{ 500 });
}

std::expected<operation_queue, error> operation_queue_access::make(std::size_t max_concurrency,
                                                                   std::shared_ptr<const time_source> source,
                                                                   std::chrono::milliseconds idle)
{
    return operation_queue::storage::make(max_concurrency, std::move(source), idle).transform([](std::shared_ptr<operation_queue::storage> store) {
        return operation_queue{ std::move(store) };
    });
}

operation_queue_access::weak_queue operation_queue_access::weaken(const operation_queue& queue)
{
    return queue._store;
}

std::optional<operation_queue> operation_queue_access::lock(const weak_queue& queue)
{
    if (auto store = queue.lock()) {
        return operation_queue{ std::move(store) };
    }
    return std::nullopt;
}

}  // namespace dross
