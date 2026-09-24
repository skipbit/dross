#pragma once

#include <memory>
#include <utility>

// What runloop and thread both hand out once the calling thread's own
// bookkeeping for them has been torn down, so a user's thread-local
// destructor that asks for current_runloop() or current_thread() late stays
// defined instead of reaching into a destroyed record.
namespace dross::teardown {

// Set once this thread's bookkeeping for Storage starts going away.
// Trivially destructible, so it has no destructor of its own and stays
// readable no matter what order thread-locals on this thread are torn down
// in.
template <typename Storage>
bool& torn_down()
{
    thread_local bool flag = false;
    return flag;
}

// An already finished Storage, shared by every call made after torn_down()
// is set. Shared rather than kept thread_local: nothing is ever installed on
// it, so nothing needs it to be distinct per thread. Never destroyed, since a
// thread still running when the program ends would otherwise reach a
// destroyed one. finish is passed in because it may be private to Storage.
template <typename Storage>
std::shared_ptr<Storage> finished_placeholder(void (Storage::*finish)())
{
    static const std::shared_ptr<Storage>* const the_one = [finish]() {
        auto self = std::make_shared<Storage>();
        ((*self).*finish)();
        return new std::shared_ptr<Storage>{ std::move(self) };
    }();
    return *the_one;
}

}  // namespace dross::teardown
