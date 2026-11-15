#pragma once

#include "dross/notification/notification.h"
#include "notification/notification_storage.h"

// Ways into a notification that the public interface does not offer. Used by
// subscription and by the library's own tests.
namespace dross {

class notification_access final {
public:
    using storage_type = notification::storage;
};

}  // namespace dross
