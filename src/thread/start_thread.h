#pragma once

#include "dross/type/error.h"

#include <expected>
#include <functional>
#include <thread>

namespace dross {

// Starts a std::thread that runs body. When the system will not start it,
// returns std::errc::resource_unavailable_try_again, as every part of the
// library that starts a thread reports it.
std::expected<std::thread, error> start_thread(std::function<void()> body);

}  // namespace dross
