#include "ipc/grpc/cleanup_thread.h"

#include "thread/start_thread.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <utility>

namespace dross::ipc {

// The work handed to the cleanup thread, which it runs in the order given.
class cleanup_queue final {
public:
    void add(std::function<void()> work)
    {
        {
            const std::lock_guard guard{ _lock };
            _work.push_back(std::move(work));
        }
        _added.notify_one();
    }

    // Runs the work as it comes, for ever.
    void run()
    {
        while (true) {
            std::function<void()> next;
            {
                std::unique_lock guard{ _lock };
                _added.wait(guard, [this] {
                    return ! _work.empty();
                });
                next = std::move(_work.front());
                _work.pop_front();
            }
            next();
        }
    }

private:
    std::mutex _lock;
    std::condition_variable _added;
    std::deque<std::function<void()>> _work;
};

namespace {

struct held_thread final {
    std::mutex lock;
    std::shared_ptr<cleanup_queue> work;
};

held_thread& held()
{
    // Never destroyed, as gRPC's threads may hand work over while the
    // program ends.
    static held_thread* const the_thread = new held_thread();
    return *the_thread;
}

}  // namespace

cleanup_thread::cleanup_thread(std::shared_ptr<cleanup_queue> work)
    : _work{ std::move(work) }
{
}

std::expected<cleanup_thread, error> cleanup_thread::start()
{
    auto& cleanup = held();
    const std::lock_guard guard{ cleanup.lock };
    if (! cleanup.work) {
        auto work = std::make_shared<cleanup_queue>();
        auto started = start_thread([work] {
            work->run();
        });
        if (! started) {
            return std::unexpected(std::move(started).error());
        }
        started->detach();
        cleanup.work = std::move(work);
    }
    return cleanup_thread{ cleanup.work };
}

void cleanup_thread::clean_up(std::function<void()> work)
{
    _work->add(std::move(work));
}

void cleanup_thread_access::forget()
{
    auto& cleanup = held();
    const std::lock_guard guard{ cleanup.lock };
    cleanup.work.reset();
}

}  // namespace dross::ipc
