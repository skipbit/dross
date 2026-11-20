#include "thread/native.h"

#include <sys/types.h>
#include <unistd.h>

namespace dross::native {

std::uint64_t thread_id()
{
    // The thread's kernel id. On the main thread this equals the process id.
    return static_cast<std::uint64_t>(gettid());
}

bool is_main_thread()
{
    return (getpid() == gettid());
}

std::optional<std::uint64_t> main_thread_id()
{
    // The process id is the kernel id of its thread group leader, which is
    // the thread the process started on; unlike gettid(), this holds no
    // matter which thread asks.
    return static_cast<std::uint64_t>(getpid());
}

}  // namespace dross::native
