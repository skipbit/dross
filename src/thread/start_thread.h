#pragma once

#include "dross/type/error.h"

#include <expected>
#include <functional>
#include <thread>

namespace dross {

// Starts a std::thread that runs body. When the system will not start it,
// returns std::errc::resource_unavailable_try_again, as every part of the
// library that starts a thread reports it.
std::expected<std::thread, error> start_thread(std::function<void()> body);

// A way to make start_thread() fail that the system does not offer. Used by
// the library's own tests.
class start_thread_access final {
public:
    // Has count calls to start_thread(), from any thread, fail as when the
    // system will not start a thread, once the next after calls have
    // started theirs.
    static void fail_next(int count, int after = 0);
};

}  // namespace dross
