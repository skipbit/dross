#include "dross/notification/notification.h"

#include "notification/notification_storage.h"

#include <memory>
#include <utility>

namespace dross {

void notification::storage::cancel() noexcept
{
    _cancelled.store(true);
}

bool notification::storage::cancelled() const noexcept
{
    return _cancelled.load();
}

notification::notification(std::shared_ptr<storage> store) noexcept
    : _store{ std::move(store) }
{
}

notification::notification(const notification& other) = default;

notification::~notification() = default;

notification& notification::operator=(const notification& other) = default;

void notification::cancel() noexcept
{
    if (_store) {
        _store->cancel();
    }
}

}  // namespace dross
