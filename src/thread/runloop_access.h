#pragma once

#include "dross/thread/runloop.h"
#include "thread/time_source.h"

#include <memory>

// Ways to make and inspect a run loop that the public interface does not
// offer. Used by the library's own tests and by the notification layer.
namespace dross {

class runloop_access final {
public:
    // A loop that belongs to no thread and reads the time from source.
    // Nothing marks it finished: its tasks and timers go when its last
    // handle does, and a timer installed on it stays valid() until then.
    static runloop standalone(std::shared_ptr<const time_source> source);

    // Whether loop's thread has ended, so a task given to it never runs.
    static bool finished(const runloop& loop);
};

}  // namespace dross
