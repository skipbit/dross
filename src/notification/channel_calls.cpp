#include "notification/channel_calls.h"

#include <cstdio>
#include <cstdlib>

namespace dross {

namespace {

[[noreturn]] void misused(const char* name, const char* how)
{
    std::fprintf(stderr, "dross: message_channel_events::%s: called %s\n", name, how);
    std::abort();
}

}  // namespace

channel_calls::call::call(std::atomic<bool>& calling) noexcept
    : _calling{ calling }
{
}

channel_calls::call::~call()
{
    _calling.store(false);
}

channel_calls::call channel_calls::received()
{
    return enter("received", false);
}

channel_calls::call channel_calls::closed()
{
    return enter("closed", true);
}

channel_calls::call channel_calls::enter(const char* name, const bool closing)
{
    if (_calling.exchange(true)) {
        misused(name, "while another call was under way");
    }
    if (_closed.load()) {
        misused(name, "after closed()");
    }
    if (closing) {
        _closed.store(true);
    }
    return call{ _calling };
}

}  // namespace dross
