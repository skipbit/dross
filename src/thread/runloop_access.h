#pragma once

#include "dross/thread/runloop.h"
#include "thread/time_source.h"

#include <memory>

// Ways to make a run loop that the public interface does not offer. Used by
// the library's own tests.
namespace dross {

class runloop_access final {
public:
    // A loop that belongs to no thread and reads the time from source.
    // Nothing marks it finished: its tasks and timers go when its last
    // handle does, and a timer installed on it stays valid() until then.
    static runloop standalone(std::shared_ptr<const time_source> source);
};

}  // namespace dross
