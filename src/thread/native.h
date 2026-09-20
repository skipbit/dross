#pragma once

#include <cstdint>

namespace dross::native {

// The two places this library asks the operating system about a thread.
// Every platform difference in the thread module lives in native.cpp; nothing
// else branches on the platform.

// The kernel's own identifier for the calling thread, which is what a
// debugger, a profiler and the system tools show. std::thread::id is a
// library value and does not match any of them.
std::uint64_t thread_id();

// Whether the caller is the thread the process started on. Asked of the
// system rather than recorded at load time, so it stays right for a library
// loaded by a worker and for a thread that reuses an ended thread's identity.
bool is_main_thread();

}
