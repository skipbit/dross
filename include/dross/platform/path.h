#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace dross {

/**
 * @brief Cross-platform filesystem path operations with error handling.
 * 
 * The path class provides a safe, cross-platform wrapper around filesystem
 * operations using std::filesystem as the underlying implementation. It uses
 * std::expected for operations that may fail and std::optional for operations
 * that may not return a value.
 * 
 * Key features:
 * - Cross-platform path handling (Windows, Unix-like systems)
 * - Safe error handling with std::expected and std::optional
 * - Path expansion and resolution
 * - Directory creation with proper error reporting
 * - Integration with std::filesystem
 * 
 * Error handling:
 * - Uses std::expected<path, std::filesystem::filesystem_error> for fallible operations
 * - Uses std::optional<path> for operations that may not return a value
 * - No exceptions thrown directly, but std::filesystem ones propagate:
 *   exists(), expand() on a ~ path, and the default constructor all call
 *   throwing std::filesystem functions
 * 
 * Performance characteristics:
 * - Thin wrapper over std::filesystem with minimal overhead
 * - Path operations are typically O(1) or O(path_length)
 * - Filesystem I/O operations depend on underlying system performance
 * 
 * Thread safety:
 * - Path objects are not thread-safe for modification
 * - Const operations are thread-safe
 * - Filesystem operations may have race conditions inherent to the filesystem
 * 
 * @code
 * // Basic path operations
 * path config_path{"~/.config/myapp"};
 * if (auto expanded = config_path.expand()) {
 *     std::cout << "Expanded path: " << expanded->string() << std::endl;
 * }
 * 
 * // Directory creation
 * path new_dir{"/tmp/myapp/data"};
 * if (auto result = path::mkdir(new_dir.string())) {
 *     std::cout << "Directory created: " << result->string() << std::endl;
 * } else {
 *     std::cerr << "Failed to create directory: " << result.error().what() << std::endl;
 * }
 * 
 * // Path building
 * if (auto home = path::home()) {
 *     path config_file = home->append("myapp").append("config.toml");
 *     if (config_file.exists()) {
 *         // Process config file
 *     }
 * }
 * @endcode
 */
class path {
public:
    /**
     * @brief Create a directory from a string path.
     * @param dir_path The directory path to create as a string
     * @return Expected containing the created path on success, or filesystem_error on failure
     * 
     * Creates the specified directory and any necessary parent directories.
     * Succeeds only when a directory is actually created: if dir_path is
     * already present the call reports failure. That case is still
     * recognisable — the reported error's code() is zero, whereas a real
     * filesystem failure carries a nonzero code.
     *
     * @code
     * if (auto result = path::mkdir("/tmp/myapp/data")) {
     *     std::cout << "Created: " << result->string() << std::endl;
     * } else {
     *     std::cerr << "Error: " << result.error().what() << std::endl;
     * }
     * @endcode
     */
    static std::expected<path, std::filesystem::filesystem_error> mkdir(const std::string& dir_path);
    
    /**
     * @brief Create a directory from a filesystem::path.
     * @param dir_path The directory path to create as a filesystem::path
     * @return Expected containing the created path on success, or filesystem_error on failure
     * 
     * Creates the specified directory and any necessary parent directories.
     * Succeeds only when a directory is actually created: if dir_path is
     * already present the call reports failure, with an error whose code()
     * is zero; a real filesystem failure carries a nonzero code. This
     * overload holds the logic; the std::string one forwards to it.
     */
    static std::expected<path, std::filesystem::filesystem_error> mkdir(const std::filesystem::path& dir_path);
    
    /**
     * @brief Get the user's home directory.
     * @return Optional containing the home directory path, or std::nullopt if not determinable
     * 
     * Attempts to determine the user's home directory using platform-appropriate methods:
     * - Unix-like systems: $HOME environment variable
     * - Windows: %USERPROFILE% or %HOMEDRIVE%%HOMEPATH%
     * 
     * @code
     * if (auto home = path::home()) {
     *     path config = home->append(".config");
     * } else {
     *     // Handle case where home directory cannot be determined
     * }
     * @endcode
     */
    static std::optional<path> home();
    
    /**
     * @brief Get the platform-appropriate path separator.
     * @return String containing the path separator ("/" on Unix, "\\" on Windows)
     * 
     * Returns the native path separator for the current platform.
     * Useful for building paths manually or for display purposes.
     */
    static std::string separator();

    /**
     * @brief Default constructor holding the current working directory.
     *
     * Resolves "." to an absolute path, so the result is the working
     * directory at the time of construction, not an empty path. Uses the
     * throwing form of std::filesystem::absolute.
     */
    path();
    
    /**
     * @brief Construct a path from a string.
     * @param path_str The path string to construct from
     * 
     * Creates a path object from the given string. The string is interpreted
     * using the native path format for the current platform.
     */
    path(const std::string& path_str);
    
    /**
     * @brief Construct a path from a std::filesystem::path.
     * @param fs_path The filesystem path to construct from
     * 
     * Creates a path object wrapping the given std::filesystem::path.
     */
    path(const std::filesystem::path& fs_path);
    
    /**
     * @brief Copy constructor.
     * @param other The path to copy from
     */
    path(const path& other);

    /**
     * @brief Check if the path exists in the filesystem.
     * @return true if the path exists (file or directory), false otherwise
     * 
     * Checks whether the path refers to an existing filesystem entity.
     * This includes files, directories, symbolic links, and other filesystem objects.
     *
     * Uses the throwing form of std::filesystem::exists. A path that is
     * merely absent yields false, but an error while querying it — an
     * over-long name, or a directory the process may not traverse — escapes
     * as a std::filesystem::filesystem_error.
     */
    bool exists() const;
    
    /**
     * @brief Append a path component to this path.
     * @param component The path component to append
     * @return New path object with the component appended
     * 
     * Creates a new path by appending the given component using the
     * platform-appropriate path separator. Does not modify this path object.
     * 
     * @code
     * path base{"/usr/local"};
     * path full = base.append("bin").append("myapp");
     * // Result: "/usr/local/bin/myapp"
     * @endcode
     */
    path append(const std::string& component) const;
    
    /**
     * @brief Get the string representation of the path.
     * @return String representation using native path format
     * 
     * Returns the path as a string using the native format for the current platform.
     * On Unix-like systems, uses forward slashes. On Windows, uses backslashes.
     */
    std::string string() const;

    /**
     * @brief Expand user home directory (~) in the path.
     * @return Expected containing the expanded path on success, or filesystem_error on failure
     * 
     * Expands tilde (~) notation to the actual home directory path.
     * Only processes paths that start with "~" or "~/".
     * 
     * @code
     * path user_config{"~/.config/myapp"};
     * if (auto expanded = user_config.expand()) {
     *     // expanded contains something like "/home/user/.config/myapp"
     * }
     * @endcode
     */
    std::expected<path, std::filesystem::filesystem_error> expand() const;
    
    /**
     * @brief Resolve the path to an absolute, canonical form.
     * @return Expected containing the resolved path on success, or filesystem_error on failure
     * 
     * Converts the path to an absolute path and resolves any symbolic links,
     * "." and ".." components. The resulting path is in canonical form.
     * 
     * @code
     * path relative{"../config/../data/file.txt"};
     * if (auto resolved = relative.resolve()) {
     *     // resolved contains the canonical absolute path
     * }
     * @endcode
     */
    std::expected<path, std::filesystem::filesystem_error> resolve() const;

    /**
     * @brief Convert to string representation.
     * @return String representation of the path
     * 
     * Implicit conversion to string for convenient usage with APIs
     * that expect string paths.
     */
    operator std::string() const;
    
    /**
     * @brief Convert to std::filesystem::path.
     * @return The underlying std::filesystem::path object
     * 
     * Provides access to the underlying std::filesystem::path for
     * interoperability with standard library filesystem operations.
     */
    operator std::filesystem::path() const;
private:
    std::filesystem::path _path;
};

}
