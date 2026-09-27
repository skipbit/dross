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
#include <optional>
#include <set>
#include <stdexcept>
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
// the ones running, and which workers already have a sweep on their loop.
// The workers hold it, not the handles' storage, so the handles going does
// not take it from a worker still sweeping. It holds handles to the workers
// in turn until it stops, which is what lets it hand them sweeps.
class backlog final : public std::enable_shared_from_this<backlog> {
public:
    backlog(std::vector<thread> workers, std::shared_ptr<const time_source> source)
        : _source{ std::move(source) }
        , _number{ next_queue_number() }
        , _workers{ std::move(workers) }
        , _sweeping(_workers.size(), false)
    {
    }

    // Queues task for the workers; false for a priority operation_priority
    // does not name, when it is to run after a result from another queue, or
    // once the queue has stopped. The task is wrapped before _mutex is
    // taken, and a task not accepted, or cancelled as it is accepted, is
    // destroyed after it is released, as is what options holds.
    bool submit(std::function<void()> task, operation_options options)
    {
        if (! names_a_priority(options.priority)) {
            return false;
        }
        if (! all_from_here(options.after)) {
            return false;
        }
        auto held = std::make_unique<std::function<void()>>(std::move(task));
        const std::lock_guard<std::mutex> guard{ _mutex };
        if (! _accepting) {
            return false;
        }
        if (! any_cancelled(options.after)) {
            queue(options, std::nullopt, std::move(held), nullptr);
        }
        return true;
    }

    // As submit(), for a task whose return value fills in a result. The
    // result is made under the same lock, so within one queue ids follow
    // the order tasks are accepted in, and a task not accepted takes none.
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
        operation_result result = operation_access::make(_source, _number);
        if (any_cancelled(options.after)) {
            operation_access::cancel(result);
        } else {
            queue(options, result, nullptr, std::move(held));
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
                if (removed[i].result) {
                    operation_access::cancel(*removed[i].result);
                    for (entry& after : take_dependents(removed[i].result->id())) {
                        removed.push_back(std::move(after));
                    }
                }
            }
        }
        _settled.notify_all();
        return true;
    }

    // Stops accepting and hands every worker a task that runs what is left
    // and then ends the worker. Under the same lock as submit(), so every
    // sweep submit() handed out is ahead of it on its worker's loop. It lets
    // go of its handles to the workers, since their loops hold it and would
    // otherwise keep it alive, and drops them once _mutex is released.
    void stop()
    {
        std::vector<thread> workers;
        {
            const std::lock_guard<std::mutex> guard{ _mutex };
            if (! _accepting) {
                return;
            }
            _accepting = false;

            for (std::size_t i = 0; i < _workers.size(); ++i) {
                _workers[i].perform(finish(i));
            }
            workers = std::move(_workers);
            _workers.clear();
        }
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
    // A task in the list, with its place in the order tasks were accepted.
    // A task from submit() is in task; one from enqueue() is in call, and
    // fills in result. Both are behind a pointer, so moving an entry under
    // _mutex runs none of the task's own code, and what the move leaves
    // behind holds nothing: a std::function may copy a small task as it
    // moves.
    struct entry final {
        std::uint64_t order;
        std::optional<operation_result> result;
        std::unique_ptr<std::function<void()>> task;
        std::unique_ptr<std::function<std::any()>> call;

        void run() const
        {
            if (call) {
                operation_access::finish(*result, (*call)());
            } else {
                (*task)();
            }
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
    void queue(const operation_options& options,
               std::optional<operation_result> result,
               std::unique_ptr<std::function<void()>> task,
               std::unique_ptr<std::function<std::any()>> call)
    {
        const std::uint64_t order = _accepted++;
        _unfinished.insert(order);
        if (result) {
            _dependents.try_emplace(result->id());
        }
        entry accepted{ order, std::move(result), std::move(task), std::move(call) };

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

    // Adds a task to the list of its priority and hands a sweep to every
    // worker that has none; a sweep keeps taking until every list is empty,
    // so an existing one reaches the task too. Every idle worker gets one,
    // not just the first, since a worker with no sweep may still be busy
    // running something else on its loop. Whichever gets there first takes
    // the task, and the rest find the lists empty. Called with _mutex held.
    // After stop() there is no worker to hand one to, so a task made ready
    // then is taken by the worker whose done() made it ready, as it sweeps.
    void make_ready(entry task, operation_priority priority)
    {
        _tasks[static_cast<std::size_t>(priority)].push_back(std::move(task));

        for (std::size_t i = 0; i < _workers.size(); ++i) {
            if (_sweeping[i]) {
                continue;
            }
            // Marked even when perform() fails: a worker whose loop has
            // ended is never offered a sweep again.
            _sweeping[i] = true;
            _workers[i].perform(sweep(i));
        }
    }

    std::function<void()> sweep(std::size_t worker)
    {
        return [self = shared_from_this(), worker]() {
            self->run_until_empty(worker);
        };
    }

    std::function<void()> finish(std::size_t worker)
    {
        return [self = shared_from_this(), worker]() {
            self->run_until_empty(worker);

            // A task may have queued more on this worker's loop, say with
            // current_thread().perform(); they run before the worker ends.
            runloop loop = current_runloop();
            if (loop.pending_count() > 0) {
                loop.perform(self->finish(worker));
            } else {
                loop.quit();
            }
        };
    }

    void run_until_empty(std::size_t worker)
    {
        while (auto next = take(worker)) {
            next->run();
            done(*next);
        }
    }

    // The first task of the highest priority waiting, or none once every
    // list is empty, which also clears this worker's sweep so the next
    // submit() hands it another.
    std::optional<entry> take(std::size_t worker)
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        for (auto tasks = _tasks.rbegin(); tasks != _tasks.rend(); ++tasks) {
            if (! tasks->empty()) {
                entry next = std::move(tasks->front());
                tasks->pop_front();
                return next;
            }
        }
        _sweeping[worker] = false;
        return std::nullopt;
    }

    // Marks a task finished and, for one that returned a result, counts it
    // off every task that runs after it; those it was the last for become
    // ready.
    void done(const entry& finished)
    {
        {
            const std::lock_guard<std::mutex> guard{ _mutex };
            _unfinished.erase(finished.order);
            if (finished.result) {
                for (const std::uint64_t order : take_dependent_orders(finished.result->id())) {
                    const auto found = _waiting.find(order);
                    // Gone once cancelled.
                    if ((found != _waiting.end()) && (--found->second.remaining == 0)) {
                        waiting ready = std::move(found->second);
                        _waiting.erase(found);
                        make_ready(std::move(ready.task), ready.priority);
                    }
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
                return ready.result && (ready.result->id() == id);
            });
            if (found != tasks.end()) {
                entry task = std::move(*found);
                tasks.erase(found);
                return task;
            }
        }
        const auto found = std::find_if(_waiting.begin(), _waiting.end(), [&id](const auto& aside) {
            return aside.second.task.result && (aside.second.task.result->id() == id);
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
    // Every task from enqueue() that has not finished, by the id of its
    // result, with the order of each task waiting to run after it.
    std::unordered_map<operation_id, std::vector<std::uint64_t>> _dependents;
    // Every task waiting for results to return before it runs, by order.
    std::map<std::uint64_t, waiting> _waiting;
    // Empty once the queue has stopped.
    std::vector<thread> _workers;
    std::vector<bool> _sweeping;
    bool _accepting{ true };
};

}  // namespace

class operation_queue::storage final {
public:
    storage(std::size_t thread_count, std::shared_ptr<const time_source> source);
    ~storage();

    bool submit(std::function<void()> task, operation_options options);
    std::expected<operation_result, error> enqueue_any(std::function<std::any()> task, operation_options options);
    bool cancel(const operation_id& id);
    bool wait_for(std::chrono::milliseconds timeout);
    bool shutdown(std::chrono::milliseconds timeout);

    std::size_t thread_count() const noexcept;

private:
    bool on_a_worker() const;

    const std::shared_ptr<const time_source> _source;
    std::shared_ptr<backlog> _backlog;
    std::vector<thread> _workers;
};

operation_queue::storage::storage(std::size_t thread_count, std::shared_ptr<const time_source> source)
    : _source{ source }
{
    if (thread_count == 0) {
        throw std::invalid_argument("operation_queue needs at least one worker");
    }

    _workers.reserve(thread_count);
    try {
        for (std::size_t i = 0; i < thread_count; ++i) {
            _workers.push_back(thread_access::start(source));
        }
        _backlog = std::make_shared<backlog>(_workers, source);
    } catch (...) {
        for (auto& worker : _workers) {
            worker.quit();
        }
        throw;
    }
}

operation_queue::storage::~storage()
{
    _backlog->stop();
}

bool operation_queue::storage::submit(std::function<void()> task, operation_options options)
{
    if (! task) {
        return false;
    }
    return _backlog->submit(std::move(task), std::move(options));
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
    return _backlog->wait_for(timeout, (! on_a_worker()));
}

bool operation_queue::storage::shutdown(std::chrono::milliseconds timeout)
{
    _backlog->stop();
    if (on_a_worker()) {
        timeout = std::chrono::milliseconds::zero();
    }

    // One deadline for every worker, rather than timeout for each in turn.
    const auto deadline = deadline::after(_source->now(), timeout);
    for (auto& worker : _workers) {
        const auto left = std::chrono::ceil<std::chrono::milliseconds>(deadline - _source->now());
        if (! worker.join_for(left)) {
            return false;
        }
    }
    return true;
}

std::size_t operation_queue::storage::thread_count() const noexcept
{
    return _workers.size();
}

bool operation_queue::storage::on_a_worker() const
{
    return std::any_of(_workers.begin(), _workers.end(), [](const thread& worker) {
        return worker.is_current_thread();
    });
}

operation_queue::operation_queue(std::shared_ptr<storage> store) noexcept
    : _store{ std::move(store) }
{
}

operation_queue::operation_queue(std::size_t thread_count)
    : operation_queue{ std::make_shared<storage>(thread_count, time_source::steady()) }
{
}

operation_queue::operation_queue(const operation_queue& other) = default;

operation_queue::~operation_queue() = default;

operation_queue& operation_queue::operator=(const operation_queue& other) = default;

bool operation_queue::submit(std::function<void()> task, operation_options options)
{
    return _store->submit(std::move(task), std::move(options));
}

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

bool operation_queue::shutdown(std::chrono::milliseconds timeout)
{
    return _store->shutdown(timeout);
}

std::size_t operation_queue::thread_count() const noexcept
{
    return _store->thread_count();
}

bool operation_queue::operator==(const operation_queue& other) const noexcept
{
    return _store == other._store;
}

operation_queue operation_queue_access::make(std::size_t thread_count, std::shared_ptr<const time_source> source)
{
    return operation_queue{ std::make_shared<operation_queue::storage>(thread_count, std::move(source)) };
}

}  // namespace dross
