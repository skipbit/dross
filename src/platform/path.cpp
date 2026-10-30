#include "dross/platform/path.h"

#include "dross/platform/environment.h"
#include "platform/absolute_directory.h"

#include <cerrno>
#include <cstddef>
#include <optional>
#include <pwd.h>
#include <unistd.h>
#include <vector>

namespace dross {

namespace {

std::optional<path> absolute_home(const std::string& home)
{
    return absolute_directory(home).transform([](const std::filesystem::path& p) {
        return path{ p };
    });
}

// getpwuid() returns storage shared by the whole process, which another
// thread may be using, so the entry is read into a buffer of this call.
std::optional<path> password_database_home()
{
    constexpr std::size_t largest_buffer = 1024 * 1024;
    const long suggested = sysconf(_SC_GETPW_R_SIZE_MAX);
    std::vector<char> buffer(suggested > 0 ? static_cast<std::size_t>(suggested) : 1024);
    const uid_t uid = getuid();
    struct passwd entry { };
    struct passwd* found = nullptr;
    int err = 0;
    while (true) {
        err = getpwuid_r(uid, &entry, buffer.data(), buffer.size(), &found);
        if (err != ERANGE || buffer.size() >= largest_buffer) {
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    if (err != 0 || found == nullptr || found->pw_dir == nullptr) {
        return std::nullopt;
    }
    return absolute_home(found->pw_dir);
}

}  // namespace

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
    const auto h = environment::value("HOME").and_then(absolute_home).or_else(password_database_home);

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
    const auto text = _path.string();
    if (text == "~" || text.starts_with("~/")) {
        const auto expanded = path::home().and_then([&](const path& p) {
            return std::make_optional(p.append(text.substr(1)).string());
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
    const auto expanded = expand();
    if (! expanded) {
        return expanded;
    }

    try {
        return path{ std::filesystem::canonical(expanded.value()._path) };
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
