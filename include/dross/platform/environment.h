#pragma once

#include <optional>
#include <string>

namespace dross {

/**
 * @brief Utility class for safe environment variable access.
 * 
 * The environment class provides safe, cross-platform access to environment
 * variables using std::optional to handle cases where variables may not exist.
 * This approach avoids the security risks and undefined behavior associated
 * with direct getenv() usage.
 * 
 * Key features:
 * - Safe environment variable access with std::optional
 * - Cross-platform compatibility (Unix, Windows)
 * - No undefined behavior for missing variables
 * - Thread-safe read operations
 * 
 * Security considerations:
 * - Always check if a variable exists before using it
 * - Be aware that environment variables are visible to all processes
 * - Avoid storing sensitive information in environment variables
 * 
 * Performance characteristics:
 * - O(1) typical case for environment variable lookup
 * - Thread-safe for concurrent read access
 * - No dynamic memory allocation for the API itself
 * 
 * @code
 * // Basic usage
 * if (auto home = environment::value("HOME")) {
 *     std::cout << "Home directory: " << *home << std::endl;
 * } else {
 *     std::cout << "HOME environment variable not set" << std::endl;
 * }
 * 
 * // Providing defaults
 * std::string shell = environment::value("SHELL").value_or("/bin/sh");
 * 
 * // Common environment variables
 * auto path = environment::value("PATH");
 * auto user = environment::value("USER");
 * auto temp = environment::value("TMPDIR").value_or("/tmp");
 * @endcode
 */
class environment {
public:
    /**
     * @brief Get the value of an environment variable.
     * @param name The name of the environment variable to retrieve
     * @return std::optional containing the value if the variable exists, std::nullopt otherwise
     * 
     * Safely retrieves the value of the specified environment variable.
     * Returns std::nullopt if the variable is not set or is empty.
     * 
     * Thread safety:
     * - Safe for concurrent read access from multiple threads
     * - Environment modifications during execution may not be visible
     * 
     * Platform notes:
     * - On Unix-like systems, uses getenv() internally
     * - On Windows, uses GetEnvironmentVariable() internally
     * - Variable names are case-sensitive on Unix, case-insensitive on Windows
     * 
     * @code
     * // Check if variable exists
     * if (auto value = environment::value("MY_VAR")) {
     *     std::cout << "MY_VAR = " << *value << std::endl;
     * }
     * 
     * // Use with default value
     * std::string config_dir = environment::value("CONFIG_DIR")
     *                           .value_or("/etc/myapp");
     * @endcode
     */
    static std::optional<std::string> value(const std::string& name);
    
private:
    // Prevent instantiation - this is a utility class with static methods only
    environment() = delete;
    ~environment() = delete;
    environment(const environment&) = delete;
    environment& operator=(const environment&) = delete;
};

}
