#pragma once

// A way to make start_thread() fail that the system does not offer. Used by
// the library's own tests.
namespace dross {

class start_thread_access final {
public:
    // Has count calls to start_thread() on the calling thread fail as when
    // the system will not start a thread, once the next after calls on it
    // have started theirs. Both are zero or more.
    static void fail_next(int count, int after = 0);
};

}  // namespace dross
