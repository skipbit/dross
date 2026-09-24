#pragma once

#include "dross/thread/thread.h"
#include "thread/time_source.h"

#include <functional>
#include <memory>

// Ways to start a thread that the public interface does not offer. Used by
// the library's own tests.
namespace dross {

class thread_access final {
public:
    // Threads like the ones thread() and thread(body) start, except that
    // join() and join_for() on them read the time and wait through source.
    // The thread's own run loop still uses steady().
    static thread start(std::shared_ptr<const time_source> source);
    static thread start(std::function<void()> body, std::shared_ptr<const time_source> source);
};

}  // namespace dross
