#include "ipc/part_calls.h"

#include <cstdio>
#include <cstdlib>

namespace dross {

part_calls::call::call(std::atomic<bool>& calling) noexcept
    : _calling{ calling }
{
}

part_calls::call::~call()
{
    _calling.store(false);
}

part_calls::part_calls(const char* events) noexcept
    : _events{ events }
{
}

part_calls part_calls::for_channel_events() noexcept
{
    return part_calls{ "message_channel_events" };
}

part_calls part_calls::for_acceptor_events() noexcept
{
    return part_calls{ "message_acceptor_events" };
}

part_calls::call part_calls::any(const char* name)
{
    return enter(name, false);
}

part_calls::call part_calls::last(const char* name)
{
    return enter(name, true);
}

part_calls::call part_calls::enter(const char* name, const bool last)
{
    if (_calling.exchange(true)) {
        std::fprintf(stderr, "dross: %s::%s: called while another call was under way\n", _events, name);
        std::abort();
    }
    if (const char* ended = _last.load(); ended != nullptr) {
        std::fprintf(stderr, "dross: %s::%s: called after %s()\n", _events, name, ended);
        std::abort();
    }
    if (last) {
        _last.store(name);
    }
    return call{ _calling };
}

}  // namespace dross
