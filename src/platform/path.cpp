#include "dross/platform/path.h"

#include "dross/platform/environment.h"

#include <optional>
#include <pwd.h>
#include <unistd.h>

namespace dross {

std::expected<path, std::filesystem::filesystem_error> path::mkdir(const std::string& dir_path)
{
    return path::mkdir(std::filesystem::path{ dir_path });
}

std::expected<path, std::filesystem::filesystem_error> path::mkdir(const std::filesystem::path& dir_path)
{
    std::error_code err;
    // create_directories() returns false both when the directory already
    // existed and when it failed to create one, so the return value alone
    // cannot tell success from failure; check err instead.
    std::filesystem::create_directories(dir_path, err);
    if (err) {
        return std::unexpected(std::filesystem::filesystem_error("failed", dir_path, err));
    }

    return path{ dir_path };
}

std::optional<path> path::home()
{
    const auto h = environment::value("HOME").and_then([](const std::string& home) {
        return std::make_optional(path(home));
    }).or_else([]() {
        struct passwd* pw = getpwuid(getuid());
        if (pw && pw->pw_dir) {
            const std::string p = pw->pw_dir;
            return std::make_optional(path{ p });
        }
        return std::optional<path>(std::nullopt);
    });

    return h;
}

std::string path::separator()
{
    return "/";
}

path::path()
    : _path(std::filesystem::absolute(std::filesystem::path{ "." }))
{
}

path::path(const std::string& p)
    : _path(p)
{
}

path::path(const std::filesystem::path& p)
    : _path(p)
{
}

path::path(const path& p)
    : _path(p._path)
{
}

bool path::exists() const
{
    return std::filesystem::exists(_path);
}

path path::append(const std::string& component) const
{
    // Every leading separator is dropped, not just one: a component still
    // starting with one would replace the path instead of joining it.
    const auto first = component.find_first_not_of(path::separator());
    std::filesystem::path p(_path);
    p.append((first == std::string::npos) ? std::string{} : component.substr(first));
    return path{ p };
}

std::string path::string() const
{
    return _path.string();
}

std::expected<path, std::filesystem::filesystem_error> path::expand() const
{
    if (_path.string().substr(0, 1) == "~") {
        const auto expanded = path::home().and_then([&](const path& p) {
            return std::make_optional(p.append(_path.string().replace(0, 1, "")).string());
        });
        if (expanded) {
            try {
                return path{ std::filesystem::canonical(std::filesystem::path{ expanded.value() }) };
            } catch (const std::filesystem::filesystem_error& e) {
                return std::unexpected(e);
            }
        } else {
            return std::unexpected(std::filesystem::filesystem_error("fail to expand tilde", _path, std::make_error_code(std::errc::no_such_file_or_directory)));
        }
    }

    return path{ _path };
}

std::expected<path, std::filesystem::filesystem_error> path::resolve() const
{
    try {
        const auto expanded = expand();
        if (expanded) {
            return path{ std::filesystem::canonical(expanded.value()._path) };
        } else {
            return expanded;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        return std::unexpected(e);
    }
}

path::operator std::string() const
{
    return _path.string();
}

path::operator std::filesystem::path() const
{
    return _path;
}

}  // namespace dross
