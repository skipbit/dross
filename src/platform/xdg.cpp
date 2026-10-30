#include "dross/platform/xdg.h"

#include "dross/platform/environment.h"
#include "dross/platform/path.h"
#include "platform/absolute_directory.h"

#include <filesystem>

namespace dross {

namespace {

// The XDG Base Directory specification treats an empty variable as unset
// and asks for a relative path in one to be ignored.
std::optional<std::string> variable(const std::string& name)
{
    return environment::value(name).and_then(absolute_directory).transform([](const std::filesystem::path& v) {
        return v.string();
    });
}

}  // namespace

xdg::xdg(const std::string& name)
    : _name(name)
{
}

std::optional<std::string> xdg::config_home() const
{
    const auto directory = variable("XDG_CONFIG_HOME").or_else([]() {
        return path::home().and_then([](const path& home) {
            return std::make_optional<std::string>(home.append(".config").string());
        });
    }).and_then([this](const std::string& p) {
        return std::make_optional<std::string>(path(p).append(_name).string());
    });

    return directory;
}

std::optional<std::string> xdg::data_home() const
{
    const auto directory = variable("XDG_DATA_HOME").or_else([]() {
        return path::home().and_then([](const path& home) {
            return std::make_optional<std::string>(home.append(".local").append("share").string());
        });
    }).and_then([this](const std::string& p) {
        return std::make_optional<std::string>(path(p).append(_name).string());
    });

    return directory;
}

std::optional<std::string> xdg::cache_home() const
{
    const auto directory = variable("XDG_CACHE_HOME").or_else([]() {
        return path::home().and_then([](const path& home) {
            return std::make_optional<std::string>(home.append(".cache").string());
        });
    }).and_then([this](const std::string& p) {
        return std::make_optional<std::string>(path(p).append(_name).string());
    });

    return directory;
}

std::optional<std::string> xdg::state_home() const
{
    const auto directory = variable("XDG_STATE_HOME").or_else([]() {
        return path::home().and_then([](const path& home) {
            return std::make_optional<std::string>(home.append(".local").append("state").string());
        });
    }).and_then([this](const std::string& p) {
        return std::make_optional<std::string>(path(p).append(_name).string());
    });

    return directory;
}

}  // namespace dross
