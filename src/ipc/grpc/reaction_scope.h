#pragma once

namespace dross::ipc {

// Hides a declaration from the library's exported symbol table, as
// poller.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_REACTION_SCOPE_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_REACTION_SCOPE_HIDDEN
#endif

// Marks the thread as inside a call the gRPC part makes from one of gRPC's
// reactions, for as long as it is held. gRPC cannot shut a server down from
// such a thread.
class DROSS_REACTION_SCOPE_HIDDEN reaction_scope final {
public:
    reaction_scope() noexcept;
    ~reaction_scope();

    reaction_scope(const reaction_scope& other) = delete;
    reaction_scope& operator=(const reaction_scope& other) = delete;

    // Whether the thread is inside such a call.
    static bool inside() noexcept;

private:
    const bool _outer;
};

#undef DROSS_REACTION_SCOPE_HIDDEN

}  // namespace dross::ipc
