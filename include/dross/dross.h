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
 * - Type System: boolean, number, string, array, dictionary, value
 * - Platform Layer: environment, path, xdg utilities
 * - Format Layer: TOML parsing with type-safe dictionary API (dross::toml)
 * - Configuration: Structured data format support
 * 
 * @author Yuma Endo
 * @date 2025
 * @copyright MIT License
 */

#pragma once

#include <dross/type.h>
#include <dross/platform.h>
#include <dross/format.h>

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
 * - Format layer: Type-safe TOML parsing returning dictionary directly
 * - Modern C++23: Concepts, ranges, and errors reported in the return type
 * 
 * @namespace dross
 */
