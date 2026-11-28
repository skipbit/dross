#pragma once

#include "dross/thread/runloop.h"
#include "dross/thread/thread.h"
#include "thread/time_source.h"

#include <expected>
#include <functional>
#include <memory>

// Ways to start a thread that the public interface does not offer. Used by
// the library's create() and its own tests.
namespace dross {

class thread_access final {
public:
    // Threads like the ones create() and create(body) start, except that
    // join() and join_for() on them read the time and wait through source.
    // The thread's own run loop still uses steady().
    static std::expected<thread, error> start(std::shared_ptr<const time_source> source);
    static std::expected<thread, error> start(std::function<void()> body, std::shared_ptr<const time_source> source);

    // The run loop of a thread one of the above started.
    static runloop loop(const thread& which);
};

}  // namespace dross
