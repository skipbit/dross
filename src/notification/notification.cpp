#include "dross/notification/notification.h"

#include <memory>
#include <utility>

namespace dross {

// Nothing about one sending is kept yet, so every handle holds none.
class notification::storage final { };

notification::notification(std::shared_ptr<storage> store) noexcept
    : _store{ std::move(store) }
{
}

notification::notification(const notification& other) = default;

notification::~notification() = default;

notification& notification::operator=(const notification& other) = default;

}  // namespace dross
