#include "notification/topic_address.h"

#include <string_view>
#include <typeinfo>

namespace dross {

namespace {

// How GCC and Clang name an unnamed namespace. An identifier holding two
// underscores in a row is reserved, so no other name contains it.
constexpr std::string_view unnamed_namespace{ "_GLOBAL__N_" };

}  // namespace

topic_address topic_address::of(std::string_view name, const std::type_info& type) noexcept
{
    const std::string_view type_name{ type.name() };
    const bool file_local = (type_name.find(unnamed_namespace) != std::string_view::npos);
    return { name, type_name, file_local ? &type : nullptr };
}

}  // namespace dross
