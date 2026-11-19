#include "thread/native.h"

namespace dross::native {

bool on_main_thread()
{
    static const thread_local bool is_main = is_main_thread();
    return is_main;
}

}  // namespace dross::native
