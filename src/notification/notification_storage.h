#pragma once

#include "dross/notification/notification.h"

#include <atomic>

// The state behind a notification handle: whether the sending has been
// cancelled. Each delivery of the sending holds it and reads it just before
// calling the subscriber's function.
namespace dross {

class notification::storage final {
public:
    void cancel() noexcept;

    bool cancelled() const noexcept;

private:
    std::atomic<bool> _cancelled{ false };
};

}  // namespace dross
