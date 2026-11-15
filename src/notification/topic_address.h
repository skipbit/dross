#pragma once

#include <string_view>

// A topic as the library tells it apart: its name and the type it carries,
// without copying either.
namespace dross {

struct topic_address final {
    std::string_view name;
    std::string_view type;
};

}  // namespace dross
