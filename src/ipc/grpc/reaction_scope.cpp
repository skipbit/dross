#include "ipc/grpc/reaction_scope.h"

namespace dross::ipc {

namespace {

thread_local bool in_reaction = false;

}  // namespace

reaction_scope::reaction_scope() noexcept
    : _outer{ in_reaction }
{
    in_reaction = true;
}

reaction_scope::~reaction_scope()
{
    in_reaction = _outer;
}

bool reaction_scope::inside() noexcept
{
    return in_reaction;
}

}  // namespace dross::ipc
