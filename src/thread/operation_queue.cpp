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
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dross {

namespace {

// What the workers share with the queue's handles: the tasks waiting to run,
// the ones running, and which workers already have a sweep on their loop.
// The workers hold it, not the handles' storage, so the handles going does
// not take it from a worker still sweeping.
class backlog final : public std::enable_shared_from_this<backlog> {
public:
    backlog(std::size_t thread_count, std::shared_ptr<const time_source> source)
        : _source{ std::move(source) }
        , _sweeping(thread_count, false)
    {
    }

    // Queues task for the workers; false once the queue has stopped. The
    // task is wrapped before _mutex is taken, and a task not accepted is
    // destroyed after it is released.
    bool submit(std::function<void()> task, std::vector<thread>& workers)
    {
        auto held = std::make_unique<std::function<void()>>(std::move(task));
        const std::lock_guard<std::mutex> guard{ _mutex };
        if (! _accepting) {
            return false;
        }
        queue(std::nullopt, std::move(held), nullptr, workers);
        return true;
    }

    // As submit(), for a task whose return value fills in a result. The
    // result is made under the same lock, so within one queue ids follow
    // the order tasks are accepted in, and a task not accepted takes none.
    std::optional<operation_result> enqueue(std::function<std::any()> task, std::vector<thread>& workers)
    {
        auto held = std::make_unique<std::function<std::any()>>(std::move(task));
        const std::lock_guard<std::mutex> guard{ _mutex };
        if (! _accepting) {
            return std::nullopt;
        }
        operation_result result = operation_access::make(_source);
        queue(result, nullptr, std::move(held), workers);
        return result;
    }

    // Takes the task whose result has id off the list and marks the result
    // cancelled. wait_for() looks only at the front of the list and at what
    // is running, so a task taken out of the middle leaves it right. The
    // result is marked under _mutex, so a wait_for() that sees the task gone
    // also sees its result finished. The task itself is destroyed only once
    // _mutex is released, as take() leaves a task it hands out, since what
    // it captured may use the queue as it goes.
    bool cancel(const operation_id& id)
    {
        std::optional<entry> removed;
        {
            const std::lock_guard<std::mutex> guard{ _mutex };
            const auto found = std::find_if(_tasks.begin(), _tasks.end(), [&id](const entry& waiting) {
                return waiting.result && (waiting.result->id() == id);
            });
            if (found == _tasks.end()) {
                return false;
            }
            operation_access::cancel(*found->result);
            removed = std::move(*found);
            _tasks.erase(found);
        }
        _settled.notify_all();
        return true;
    }

    // Stops accepting and hands every worker a task that runs what is left
    // and then ends the worker. Under the same lock as submit(), so every
    // sweep submit() handed out is ahead of it on its worker's loop.
    void stop(std::vector<thread>& workers)
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        if (! _accepting) {
            return;
        }
        _accepting = false;

        for (std::size_t i = 0; i < workers.size(); ++i) {
            workers[i].perform(finish(i));
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
            return (_tasks.empty() || (_tasks.front().order >= target))
                   && std::none_of(_running.begin(), _running.end(), [target](std::uint64_t order) {
                return (order < target);
            });
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
    // _mutex runs none of the task's own code: a std::function may copy a
    // small task as it moves.
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

    // Adds a task to the list and hands a sweep to every worker that has
    // none; a sweep keeps taking until the list is empty, so an existing one
    // reaches the task too. Every idle worker gets one, not just the first,
    // since a worker with no sweep may still be busy running something else
    // on its loop. Whichever gets there first takes the task, and the rest
    // find the list empty. Called with _mutex held.
    void queue(std::optional<operation_result> result,
               std::unique_ptr<std::function<void()>> task,
               std::unique_ptr<std::function<std::any()>> call,
               std::vector<thread>& workers)
    {
        _tasks.push_back(entry{ _accepted++, std::move(result), std::move(task), std::move(call) });

        for (std::size_t i = 0; i < workers.size(); ++i) {
            if (_sweeping[i]) {
                continue;
            }
            // Marked even when perform() fails: a worker whose loop has
            // ended is never offered a sweep again.
            _sweeping[i] = true;
            workers[i].perform(sweep(i));
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
            done(next->order);
        }
    }

    // The next task, or none once the list is empty, which also clears this
    // worker's sweep so the next submit() hands it another.
    std::optional<entry> take(std::size_t worker)
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        if (_tasks.empty()) {
            _sweeping[worker] = false;
            return std::nullopt;
        }
        entry next = std::move(_tasks.front());
        _tasks.pop_front();
        _running.push_back(next.order);
        return next;
    }

    void done(std::uint64_t order)
    {
        {
            const std::lock_guard<std::mutex> guard{ _mutex };
            std::erase(_running, order);
        }
        _settled.notify_all();
    }

    // Set once, at construction, and never reassigned, so it is read
    // without _mutex.
    const std::shared_ptr<const time_source> _source;

    std::mutex _mutex;
    std::condition_variable _settled;
    // In the order tasks were accepted, so every task accepted before the
    // front one has left the list; those still running are in _running,
    // which holds one per worker unless a task runs its worker's loop
    // itself.
    std::deque<entry> _tasks;
    std::uint64_t _accepted{ 0 };
    std::vector<std::uint64_t> _running;
    std::vector<bool> _sweeping;
    bool _accepting{ true };
};

}  // namespace

class operation_queue::storage final {
public:
    storage(std::size_t thread_count, std::shared_ptr<const time_source> source);
    ~storage();

    bool submit(std::function<void()> task);
    std::optional<operation_result> enqueue_any(std::function<std::any()> task);
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
    , _backlog{ std::make_shared<backlog>(thread_count, source) }
{
    if (thread_count == 0) {
        throw std::invalid_argument("operation_queue needs at least one worker");
    }

    _workers.reserve(thread_count);
    try {
        for (std::size_t i = 0; i < thread_count; ++i) {
            _workers.push_back(thread_access::start(source));
        }
    } catch (...) {
        for (auto& worker : _workers) {
            worker.quit();
        }
        throw;
    }
}

operation_queue::storage::~storage()
{
    _backlog->stop(_workers);
}

bool operation_queue::storage::submit(std::function<void()> task)
{
    if (! task) {
        return false;
    }
    return _backlog->submit(std::move(task), _workers);
}

std::optional<operation_result> operation_queue::storage::enqueue_any(std::function<std::any()> task)
{
    return _backlog->enqueue(std::move(task), _workers);
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
    _backlog->stop(_workers);
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

bool operation_queue::submit(std::function<void()> task)
{
    return _store->submit(std::move(task));
}

std::optional<operation_result> operation_queue::enqueue_any(std::function<std::any()> task)
{
    return _store->enqueue_any(std::move(task));
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
