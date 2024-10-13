#pragma once

#include <optional>
#include <string>

namespace dross {

class environment {
public:
    static std::optional<std::string> value(const std::string&);
};

}
