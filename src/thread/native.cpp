#include "thread/native.h"

#if defined(__linux__)
#include <sys/types.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <pthread.h>
#else
#error "the thread module supports Linux and macOS"
#endif

namespace dross::native {

std::uint64_t thread_id()
{
#if defined(__linux__)
    // The thread's kernel id. On the main thread this equals the process id.
    return static_cast<std::uint64_t>(gettid());
#else
    std::uint64_t id = 0;
    if (pthread_threadid_np(nullptr, &id) == 0) {
        return id;
    }
    // pthread_threadid_np() documents no cause for failure, but its output
    // is left at 0 on one; the Mach port name is the fallback rather than
    // returning that, which would otherwise look like a valid, if wrong, id.
    return static_cast<std::uint64_t>(pthread_mach_thread_np(pthread_self()));
#endif
}

bool is_main_thread()
{
#if defined(__linux__)
    return getpid() == gettid();
#else
    return pthread_main_np() != 0;
#endif
}

std::optional<std::uint64_t> main_thread_id()
{
#if defined(__linux__)
    // The process id is the kernel id of its thread group leader, which is
    // the thread the process started on; unlike gettid(), this holds no
    // matter which thread asks.
    return static_cast<std::uint64_t>(getpid());
#else
    // No API answers this for a thread other than the one asking, so this
    // is only ever known once the main thread has used the module itself.
    return is_main_thread() ? std::optional{thread_id()} : std::nullopt;
#endif
}

bool on_main_thread()
{
    static const thread_local bool is_main = is_main_thread();
    return is_main;
}

}
