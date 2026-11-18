#pragma once

#include "dross/type/error.h"

#include <cerrno>
#include <system_error>

namespace dross {

// Hides a declaration from the library's exported symbol table, as
// poller.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_LAST_ERROR_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_LAST_ERROR_HIDDEN
#endif

// The error for errno after the system call named call failed.
DROSS_LAST_ERROR_HIDDEN inline error last_error(const char* call)
{
    return error{ static_cast<std::errc>(errno), call };
}

#undef DROSS_LAST_ERROR_HIDDEN

}  // namespace dross
