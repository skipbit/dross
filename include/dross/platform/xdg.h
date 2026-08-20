#pragma once

#include <optional>
#include <string>

namespace dross {

/**
 * @brief XDG Base Directory Specification implementation for proper app data storage.
 * 
 * The xdg class implements the XDG Base Directory Specification, which defines
 * standard locations for application data, configuration, cache, and state files
 * on Unix-like systems. This ensures applications store their data in appropriate
 * locations that integrate well with the desktop environment and user expectations.
 * 
 * Key features:
 * - XDG Base Directory Specification compliance
 * - Automatic fallback to standard directories when XDG variables are unset
 * - Application-specific subdirectory creation
 * - Cross-platform compatibility (Unix-like systems primarily)
 * 
 * XDG directories:
 * - Config: User-specific configuration files
 * - Data: User-specific data files
 * - Cache: User-specific non-essential cached data
 * - State: User-specific state data (logs, history, etc.)
 * 
 * Environment variables used:
 * - XDG_CONFIG_HOME (default: ~/.config)
 * - XDG_DATA_HOME (default: ~/.local/share)
 * - XDG_CACHE_HOME (default: ~/.cache)
 * - XDG_STATE_HOME (default: ~/.local/state)
 * 
 * @code
 * // Application-specific XDG directories
 * xdg app_dirs{"myapp"};
 * 
 * // Get configuration directory
 * if (auto config_dir = app_dirs.config_home()) {
 *     path config_file = path{*config_dir} / "config.toml";
 *     // Store configuration in ~/.config/myapp/config.toml
 * }
 * 
 * // Get data directory
 * if (auto data_dir = app_dirs.data_home()) {
 *     path db_file = path{*data_dir} / "database.sqlite";
 *     // Store data in ~/.local/share/myapp/database.sqlite
 * }
 * 
 * // Get cache directory
 * if (auto cache_dir = app_dirs.cache_home()) {
 *     path cache_file = path{*cache_dir} / "thumbnails";
 *     // Store cache in ~/.cache/myapp/thumbnails
 * }
 * @endcode
 * 
 * Platform compatibility:
 * - Full support on Linux and other Unix-like systems
 * - Limited support on macOS (uses similar directory structure)
 * - Not applicable on Windows (consider using appropriate Windows APIs)
 */
class xdg {
public:
    /**
     * @brief Construct XDG directory helper for a specific application.
     * @param app_name The name of the application for directory creation
     * 
     * Creates an XDG directory helper that will create application-specific
     * subdirectories within the standard XDG base directories. The application
     * name is used as the subdirectory name.
     * 
     * The application name should be:
     * - A valid directory name (no path separators)
     * - Unique to your application
     * - Following naming conventions (lowercase, hyphens for separation)
     * 
     * @code
     * xdg app_dirs{"my-awesome-app"};
     * // Will create directories like ~/.config/my-awesome-app/
     * @endcode
     */
    xdg(const std::string& app_name);

    /**
     * @brief Get the application's configuration directory.
     * @return Optional containing the config directory path, or std::nullopt on error
     * 
     * Returns the application-specific configuration directory according to
     * XDG Base Directory Specification:
     * - Uses $XDG_CONFIG_HOME/app_name if XDG_CONFIG_HOME is set
     * - Falls back to $HOME/.config/app_name
     * - Returns std::nullopt if HOME cannot be determined
     * 
     * The returned directory may not exist yet. Use path::mkdir() to create it.
     * 
     * @code
     * xdg app{"myapp"};
     * if (auto config_dir = app.config_home()) {
     *     // Typically returns something like "/home/user/.config/myapp"
     *     path config_path{*config_dir};
     *     if (auto result = path::mkdir(config_path.string())) {
     *         // Directory ready for config files, whether just created or
     *         // already there
     *     }
     * }
     * @endcode
     */
    std::optional<std::string> config_home() const;
    
    /**
     * @brief Get the application's data directory.
     * @return Optional containing the data directory path, or std::nullopt on error
     * 
     * Returns the application-specific data directory according to
     * XDG Base Directory Specification:
     * - Uses $XDG_DATA_HOME/app_name if XDG_DATA_HOME is set
     * - Falls back to $HOME/.local/share/app_name
     * - Returns std::nullopt if HOME cannot be determined
     * 
     * Use this directory for application data files, databases, etc.
     */
    std::optional<std::string> data_home() const;
    
    /**
     * @brief Get the application's cache directory.
     * @return Optional containing the cache directory path, or std::nullopt on error
     * 
     * Returns the application-specific cache directory according to
     * XDG Base Directory Specification:
     * - Uses $XDG_CACHE_HOME/app_name if XDG_CACHE_HOME is set
     * - Falls back to $HOME/.cache/app_name
     * - Returns std::nullopt if HOME cannot be determined
     * 
     * Use this directory for non-essential cached data that can be regenerated.
     * Cache files may be deleted by system cleanup tools.
     */
    std::optional<std::string> cache_home() const;
    
    /**
     * @brief Get the application's state directory.
     * @return Optional containing the state directory path, or std::nullopt on error
     * 
     * Returns the application-specific state directory according to
     * XDG Base Directory Specification:
     * - Uses $XDG_STATE_HOME/app_name if XDG_STATE_HOME is set
     * - Falls back to $HOME/.local/state/app_name
     * - Returns std::nullopt if HOME cannot be determined
     * 
     * Use this directory for state data like logs, history, recently used files, etc.
     * This data should persist between application runs but is not user configuration.
     */
    std::optional<std::string> state_home() const;

private:
    std::string _name;  ///< Application name for directory creation
};

}
