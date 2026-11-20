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
 *
 * // Use any dross functionality
 * dross::number big_num{"12345678901234567890"};
 * dross::string text{"Hello, world!"};
 * dross::array list = {dross::value{1}, dross::value{"two"}, dross::value{3.0}};
 * @endcode
 *
 * Modules included:
 * - Type System: boolean, number, string, timestamp, timezone, array, dictionary, data, value, error
 * - Platform Layer: environment, path, xdg utilities
 * - Format Layer: TOML parsing with type-safe dictionary API (dross::toml)
 * - IPC Layer: places where processes meet, such as a Unix domain socket (dross::ipc)
 * - Thread Layer: per-thread run loops that other threads can hand work to, and thread, a handle to an OS thread
 * - Notification Layer: typed notifications from a broadcaster to subscribers through a listener, each called where its delivery says
 *
 * @author Yuma Endo
 * @date 2025
 * @copyright MIT License
 */

#pragma once

#include <dross/format.h>
#include <dross/ipc.h>
#include <dross/notification.h>
#include <dross/platform.h>
#include <dross/thread.h>
#include <dross/type.h>

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
 * - Thread layer: Run loops a thread owns and other threads can post to, and thread, a handle to an OS thread
 * - Notification layer: Typed notifications from a broadcaster to subscribers, each on the thread it chose
 * - Modern C++23: Concepts, ranges, and errors reported in the return type
 *
 * @namespace dross
 */
