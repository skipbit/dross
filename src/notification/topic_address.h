#pragma once

#include <string_view>
#include <typeinfo>

// A topic as the library tells it apart: its name and the type it carries,
// without copying either.
namespace dross {

struct topic_address final {
    std::string_view name;
    std::string_view type;

    // The type itself, for a type in an unnamed namespace, whose name a type
    // in another source file can share. Null for any other type.
    const std::type_info* identity;

    static topic_address of(std::string_view name, const std::type_info& type) noexcept;
};

}  // namespace dross
