/**
 * @file format.h
 * @brief Format layer header for the dross library.
 * 
 * This header provides access to all format parsing and serialization
 * functionality in the dross library. It includes support for various
 * structured data formats like TOML, with JSON and YAML planned for
 * future releases.
 * 
 * Usage:
 * @code
 * #include <dross/format.h>
 * using namespace dross;
 * 
 * // TOML parsing
 * auto toml_result = toml::deserialize(toml_data);
 * if (toml_result) {
 *     auto& config = toml_result.value();
 *     // Use dictionary directly
 * }
 * 
 * // TOML serialization
 * dictionary config;
 * config["key"] = value(string{"value"});
 * auto toml_data = toml::serialize(config);
 * @endcode
 * 
 * Supported formats:
 * - TOML: Type-safe parsing with dictionary API (dross::toml)
 * - JSON: Planned for future release (dross::json)
 * - YAML: Planned for future release (dross::yaml)
 * 
 * Design principles:
 * - Type-safe APIs reflecting format specifications
 * - Zero-dependency parsing implementations
 * - Modern C++23 error handling with std::expected
 * - Consistent namespace organization per format
 * 
 * @author Yuma Endo
 * @version 0.0.1
 * @date 2025
 * @copyright MIT License
 */

#pragma once

#include <dross/format/toml.h>

/**
 * @brief Format layer namespace containing all parsing and serialization functionality.
 * 
 * The format namespace provides sub-namespaces for different structured data formats.
 * Each format namespace contains deserialize/serialize functions appropriate for
 * that format's specification.
 * 
 * Available formats:
 * - dross::toml: TOML v1.0.0 with dictionary root type
 * 
 * Planned formats:
 * - dross::json: JSON with value root type (any type allowed)
 * - dross::yaml: YAML with value root type (any type allowed)
 * 
 * @namespace dross::format
 */