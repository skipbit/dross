#pragma once

#include "dross/thread/thread.h"
#include "dross/type/error.h"

#include <expected>
#include <functional>

namespace dross::ipc {

// Hides a declaration from the library's exported symbol table, as
// poller.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_CLEANUP_THREAD_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_CLEANUP_THREAD_HIDDEN
#endif

// The one thread of the gRPC part that does the work gRPC does not allow
// where it arises, such as inside one of gRPC's reactions. It is started by
// the first listen or connection that needs it, and is never stopped. What
// is made with a successful start() holds the thread.
class DROSS_CLEANUP_THREAD_HIDDEN cleanup_thread final {
public:
    // Starts the thread unless it already runs, and gives it. Fails as
    // dross::thread::create() does.
    static std::expected<cleanup_thread, error> start();

    // Has the thread do work.
    void clean_up(std::function<void()> work);

private:
    explicit cleanup_thread(thread running);

    thread _running;
};

// A way for the library's own tests to start from no thread.
class DROSS_CLEANUP_THREAD_HIDDEN cleanup_thread_access final {
public:
    // Lets go of the thread, so the next start() starts another. What was
    // made with the one let go keeps handing its work to that one.
    static void forget();
};

#undef DROSS_CLEANUP_THREAD_HIDDEN

}  // namespace dross::ipc
