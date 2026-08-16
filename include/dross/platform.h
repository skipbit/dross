/**
 * @file platform.h
 * @brief Platform abstraction layer providing cross-platform utilities.
 * 
 * This header provides access to platform-specific functionality in a
 * cross-platform manner. It includes utilities for environment variable
 * access, filesystem operations, and XDG Base Directory specification
 * support for proper application data storage.
 * 
 * Key features:
 * - Environment variable access with safe error handling
 * - Cross-platform filesystem path operations
 * - XDG Base Directory specification compliance on Unix-like systems
 * - Modern C++23 error handling with std::optional and std::expected
 * 
 * Usage:
 * @code
 * #include <dross/platform.h>
 * using namespace dross;
 * 
 * // Environment variables
 * if (auto home = environment::value("HOME")) {
 *     std::cout << "Home: " << *home << std::endl;
 * }
 * 
 * // Filesystem operations
 * path config_dir = path::home().value_or(path{"/tmp"}) / "myapp";
 * if (auto result = path::mkdir(config_dir.string()); result) {
 *     // Directory created successfully
 * }
 * 
 * // XDG directories
 * xdg app_dirs{"myapp"};
 * if (auto config = app_dirs.config_home()) {
 *     path config_file = path{*config} / "config.toml";
 * }
 * @endcode
 * 
 * Error handling:
 * - Uses std::optional for operations that may not return a value
 * - Uses std::expected for operations that may fail with detailed error info
 * - The platform layer throws nothing of its own, but std::filesystem
 * exceptions do propagate: see path::exists(), path::expand() and the
 * default path constructor
 * 
 * Platform support:
 * - Unix-like systems (Linux, macOS, BSD)
 * - Windows (with appropriate path separator handling)
 * - XDG specification support on Unix-like systems
 */

#pragma once

#include <dross/platform/environment.h>
#include <dross/platform/path.h>
#include <dross/platform/xdg.h>

/**
 * @brief Platform abstraction namespace containing cross-platform utilities.
 * 
 * The platform layer provides safe, cross-platform access to system
 * functionality including environment variables, filesystem operations,
 * and standard directory locations.
 */