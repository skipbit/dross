#include "dross/platform/environment.h"

#include <cstdlib>

namespace dross {

std::optional<std::string> environment::value(const std::string& key)
{
    const char* value = std::getenv(key.c_str());
    return (value ? std::make_optional<std::string>(value) : std::nullopt);
}

}
