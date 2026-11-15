#pragma once

#include "dross/notification/broadcaster.h"
#include "notification/broadcaster_storage.h"

#include <memory>

// Ways into a broadcaster that the public interface does not offer. Used by
// listener and by the library's own tests.
namespace dross {

class broadcaster_access final {
public:
    using storage_type = broadcaster::storage;

    // The state behind which, for a listener to hold without keeping it
    // going.
    static std::weak_ptr<storage_type> weaken(const broadcaster& which);
};

}  // namespace dross
