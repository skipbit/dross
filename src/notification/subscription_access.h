#pragma once

#include "dross/notification/subscription.h"
#include "notification/subscription_storage.h"

#include <memory>

// Ways to make a subscription that the public interface does not offer. Used
// by the rest of the notification layer and by the library's own tests.
namespace dross {

class subscription_access final {
public:
    using receiver = subscription::storage;

    // A subscription that ends which when it is reset or destroyed.
    static subscription make(std::shared_ptr<receiver> which);
};

}  // namespace dross
