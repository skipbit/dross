#pragma once

#include "dross/type/error.h"

#include <expected>
#include <new>
#include <system_error>
#include <thread>
#include <utility>

namespace dross {

// Whether a test has planned the next start on the calling thread to fail,
// taking that failure when it has.
bool take_planned_start_failure();

// Starts a std::thread that runs body. When the system will not start it,
// returns std::errc::resource_unavailable_try_again, and when there is no
// memory to start it, std::errc::not_enough_memory, so the threads started
// through it report a failure the same way. body is handed to std::thread
// as it is, so nothing is allocated for it outside this function.
template <typename Body>
std::expected<std::thread, error> start_thread(Body body)
{
    if (take_planned_start_failure()) {
        return std::unexpected(error{ std::errc::resource_unavailable_try_again });
    }
    // std::thread reports the failure only by throwing. It starts the thread
    // with the default attributes, for which the system fails only for want
    // of resources.
    try {
        return std::thread{ std::move(body) };
    } catch (const std::system_error&) {
        return std::unexpected(error{ std::errc::resource_unavailable_try_again });
    } catch (const std::bad_alloc&) {
        return std::unexpected(error{ std::errc::not_enough_memory });
    }
}

}  // namespace dross
