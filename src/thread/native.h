#pragma once

#include <cstdint>
#include <optional>

namespace dross::native {

// Hides a declaration from the library's exported symbol table. These are
// implementation details of a header under src/, which is never installed,
// so nothing outside this library can name them; this keeps them off
// nm -D's output as well.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_NATIVE_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_NATIVE_HIDDEN
#endif

// The four places this library asks the operating system about a thread.
// Every platform difference in the thread module lives in native.cpp; nothing
// else branches on the platform.

// The kernel's own identifier for the calling thread, which is what a
// debugger, a profiler and the system tools show. std::thread::id is a
// library value and does not match any of them.
DROSS_NATIVE_HIDDEN std::uint64_t thread_id();

// Whether the caller is the thread the process started on. Asked of the
// system rather than recorded at load time, so it stays right for a library
// loaded by a worker and for a thread that reuses an ended thread's identity.
DROSS_NATIVE_HIDDEN bool is_main_thread();

// The kernel id of the thread the process started on, asked from any thread.
// Empty when the platform cannot answer this from a thread other than the
// main one.
DROSS_NATIVE_HIDDEN std::optional<std::uint64_t> main_thread_id();

// is_main_thread(), cached per thread: the answer cannot change once a
// thread is running, and asking costs a system call.
DROSS_NATIVE_HIDDEN bool on_main_thread();

#undef DROSS_NATIVE_HIDDEN

}
