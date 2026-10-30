#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace dross {

// A directory taken from the environment counts only as an absolute path,
// so an empty or relative one is treated as missing.
inline std::optional<std::filesystem::path> absolute_directory(const std::string& value)
{
    std::filesystem::path directory{ value };
    return directory.is_absolute() ? std::make_optional(directory) : std::nullopt;
}

}  // namespace dross
