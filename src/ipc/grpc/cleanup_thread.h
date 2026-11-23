#pragma once

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
// the first listen or connection that needs it, and is never stopped.

// Starts the thread unless it already runs. Fails as dross::thread::create()
// does.
DROSS_CLEANUP_THREAD_HIDDEN std::expected<void, error> start_cleanup_thread();

// Has the thread do work, once start_cleanup_thread() has succeeded.
DROSS_CLEANUP_THREAD_HIDDEN void clean_up(std::function<void()> work);

// A way for the library's own tests to start from no thread.
class DROSS_CLEANUP_THREAD_HIDDEN cleanup_thread_access final {
public:
    // Lets go of the thread, so the next start_cleanup_thread() starts
    // another. The thread let go keeps running the work it was given.
    static void forget();
};

#undef DROSS_CLEANUP_THREAD_HIDDEN

}  // namespace dross::ipc
