#pragma once

#include <optional>
#include <string>

namespace dross {

class xdg {
public:
    xdg(const std::string& name);

    std::optional<std::string> config_home() const;
    std::optional<std::string> data_home() const;
    std::optional<std::string> cache_home() const;
    std::optional<std::string> state_home() const;

private:
    std::string _name;
};

}
