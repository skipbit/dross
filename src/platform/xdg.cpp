#include "dross/platform/xdg.h"
#include "dross/platform/environment.h"
#include "dross/platform/path.h"

namespace dross {

xdg::xdg(const std::string& name)
    : _name(name)
{
}

std::optional<path> xdg::config_home() const
{
    const auto directory = environment::value("XDG_CONFIG_HOME")
        .or_else([]() {
            return path::home().and_then([](const path& home) {
                return std::make_optional<std::string>(home.append(".config").string());
            });
        })
        .and_then([this](const std::string& p) {
            return std::make_optional<std::string>(path(p).append(_name).string());
        });

    return directory;
}

std::optional<path> xdg::data_home() const
{
    const auto directory = environment::value("XDG_DATA_HOME")
        .or_else([]() {
            return path::home().and_then([](const path& home) {
                 return std::make_optional<std::string>(home.append(".local").append("share").string());
            });
        })
        .and_then([this](const std::string& p) {
            return std::make_optional<std::string>(path(p).append(_name).string());
        });

    return directory;
}

std::optional<path> xdg::cache_home() const
{
    const auto directory = environment::value("XDG_CACHE_HOME")
        .or_else([]() {
            return path::home().and_then([](const path& home) {
                return std::make_optional<std::string>(home.append(".cache").string());
            });
        })
        .and_then([this](const std::string& p) {
            return std::make_optional<std::string>(path(p).append(_name).string());
        });

    return directory;
}

std::optional<path> xdg::state_home() const
{
    const auto directory = environment::value("XDG_STATE_HOME")
        .or_else([]() {
            return path::home().and_then([](const path& home) {
                return std::make_optional<std::string>(home.append(".local").append("state").string());
            });
        })
        .and_then([this](const std::string& p) {
            return std::make_optional<std::string>(path(p).append(_name).string());
        });

    return directory;
}

}
