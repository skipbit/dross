#pragma once

#include "dross/platform/path.h"
#include <optional>
#include <string>

namespace dross {

class xdg {
public:
    xdg(const std::string& name);

    std::optional<path> config_home() const;
    std::optional<path> data_home() const;
    std::optional<path> cache_home() const;
    std::optional<path> state_home() const;

private:
    std::string _name;
};

}
