#include "thread/native.h"

#include <pthread.h>

namespace dross::native {

std::uint64_t thread_id()
{
    std::uint64_t id = 0;
    if (pthread_threadid_np(nullptr, &id) == 0) {
        return id;
    }
    // pthread_threadid_np() documents no cause for failure, but its output
    // is left at 0 on one; the Mach port name is the fallback rather than
    // returning that, which would otherwise look like a valid, if wrong, id.
    return static_cast<std::uint64_t>(pthread_mach_thread_np(pthread_self()));
}

bool is_main_thread()
{
    return (pthread_main_np() != 0);
}

std::optional<std::uint64_t> main_thread_id()
{
    // No API answers this for a thread other than the one asking, so this
    // is only ever known once the main thread has used the module itself.
    return is_main_thread() ? std::optional{ thread_id() } : std::nullopt;
}

}  // namespace dross::native
