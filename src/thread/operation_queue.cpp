#include "dross/thread/operation_queue.h"

#include "dross/thread/runloop.h"
#include "dross/thread/thread.h"

#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dross {

namespace {

// What the workers share with the queue's handles: the tasks waiting to run,
// and which workers already have a sweep on their loop. The workers hold it,
// not the handles' storage, so the handles going does not take it from a
// worker still sweeping.
class backlog final : public std::enable_shared_from_this<backlog> {
public:
    explicit backlog(std::size_t thread_count)
        : _sweeping(thread_count, false)
    {
    }

    // Queues task and hands a sweep to a worker that has none, unless every
    // worker already has one; a sweep keeps taking until the list is empty,
    // so an existing one reaches task too.
    bool submit(std::function<void()> task, std::vector<thread>& workers)
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        if (! _accepting) {
            return false;
        }
        _tasks.push_back(std::move(task));

        for (std::size_t i = 0; i < workers.size(); ++i) {
            if (_sweeping[i]) {
                continue;
            }
            // Marked even when perform() fails: a worker whose loop has
            // ended is never offered a sweep again.
            _sweeping[i] = true;
            if (workers[i].perform(sweep(i))) {
                break;
            }
        }
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

private:
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
        while (auto task = take(worker)) {
            task();
        }
    }

    // The next task, or empty once there is none, which also clears this
    // worker's sweep so the next submit() hands it another.
    std::function<void()> take(std::size_t worker)
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        if (_tasks.empty()) {
            _sweeping[worker] = false;
            return nullptr;
        }
        auto task = std::move(_tasks.front());
        _tasks.pop_front();
        return task;
    }

    std::mutex _mutex;
    std::deque<std::function<void()>> _tasks;
    std::vector<bool> _sweeping;
    bool _accepting{ true };
};

}  // namespace

class operation_queue::storage final {
public:
    explicit storage(std::size_t thread_count);
    ~storage();

    bool submit(std::function<void()> task);

    std::size_t thread_count() const noexcept;

private:
    std::shared_ptr<backlog> _backlog;
    std::vector<thread> _workers;
};

operation_queue::storage::storage(std::size_t thread_count)
    : _backlog{ std::make_shared<backlog>(thread_count) }
{
    if (thread_count == 0) {
        throw std::invalid_argument("operation_queue needs at least one worker");
    }

    _workers.reserve(thread_count);
    try {
        for (std::size_t i = 0; i < thread_count; ++i) {
            _workers.emplace_back();
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

std::size_t operation_queue::storage::thread_count() const noexcept
{
    return _workers.size();
}

operation_queue::operation_queue(std::size_t thread_count)
    : _store{ std::make_shared<storage>(thread_count) }
{
}

operation_queue::operation_queue(const operation_queue& other) = default;

operation_queue::~operation_queue() = default;

operation_queue& operation_queue::operator=(const operation_queue& other) = default;

bool operation_queue::submit(std::function<void()> task)
{
    return _store->submit(std::move(task));
}

std::size_t operation_queue::thread_count() const noexcept
{
    return _store->thread_count();
}

bool operation_queue::operator==(const operation_queue& other) const noexcept
{
    return _store == other._store;
}

}  // namespace dross
