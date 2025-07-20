/**
 * @file dross.h
 * @brief Main header file for the dross library.
 * 
 * This is the primary include file for the dross library. Including this
 * header provides access to all core functionality including the type system,
 * platform utilities, and configuration support.
 * 
 * Usage:
 * @code
 * #include <dross/dross.h>
 * using namespace dross;
 * 
 * // Use any dross functionality
 * number big_num{"12345678901234567890"};
 * string text{"Hello, world!"};
 * array data = {value{1}, value{"two"}, value{3.0}};
 * @endcode
 * 
 * Modules included:
 * - Type System: number, string, array, dictionary, value
 * - Platform Layer: environment, path, xdg utilities
 * - Configuration: TOML and other format support (when available)
 * 
 * @author Yuma Endo
 * @version 0.0.1
 * @date 2025
 * @copyright MIT License
 */

#pragma once

#include <dross/type.h>
#include <dross/platform.h>

/**
 * @brief Main namespace for all dross library functionality.
 * 
 * The dross namespace contains all public API classes, functions, and types
 * provided by the library. This includes the core type system, platform
 * utilities, and configuration support.
 * 
 * Key components:
 * - Type system: Polymorphic value types with arbitrary precision
 * - Platform layer: Cross-platform environment and filesystem utilities
 * - Modern C++23: Concepts, ranges, and error handling without exceptions
 * 
 * @namespace dross
 */
