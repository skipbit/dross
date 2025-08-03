#pragma once

#include <expected>
#include "../type/value.h"
#include "../type/data.h"
#include "../type/error.h"

namespace dross::toml {

/**
 * @brief TOML format parser and serializer.
 *
 * This module provides functionality to convert between TOML format binary data
 * and dross structured data types. It follows the TOML v1.0.0 specification.
 *
 * Key features:
 * - Complete TOML v1.0.0 specification support
 * - Bidirectional conversion (deserialize/serialize)
 * - Integration with dross::value type system
 * - Comprehensive error handling with detailed diagnostics
 * - Unicode support for strings and keys
 * - Precise datetime handling with timezone support
 * - Comments preservation during roundtrip operations
 *
 * Supported TOML types mapping to dross types:
 * - TOML String → dross::string
 * - TOML Integer → dross::number
 * - TOML Float → dross::number
 * - TOML Boolean → dross::boolean
 * - TOML Datetime → dross::timestamp
 * - TOML Array → dross::array
 * - TOML Table → dross::dictionary
 *
 * @example
 * @code
 * // Deserialize TOML data to structured format
 * data toml_content{"title = \"Example\"\nversion = 1.0"};
 * auto result = dross::toml::deserialize(toml_content);
 * if (result) {
 *     auto config = result.value();
 *     // Access parsed data through value interface
 * }
 *
 * // Serialize structured data to TOML format
 * value config = dictionary{
 *     {"title", string{"My App"}},
 *     {"version", number{"1.0"}}
 * };
 * auto toml_data = dross::toml::serialize(config);
 * if (toml_data) {
 *     std::string toml_string = toml_data.value();
 * }
 * @endcode
 */

/**
 * @brief Deserialize TOML format binary data to structured data.
 * 
 * Converts TOML format text data to dross structured value type.
 * The input data is expected to be valid UTF-8 encoded TOML content.
 *
 * @param input TOML format binary data (UTF-8 encoded)
 * @return Parsed structured data on success, error on failure
 *
 * Error conditions:
 * - Invalid TOML syntax
 * - Encoding errors (non-UTF-8 content)
 * - Duplicate keys in tables
 * - Invalid datetime formats
 * - Array type inconsistencies
 * - Table redefinitions
 *
 * @note The parser follows TOML v1.0.0 specification strictly.
 *       Comments are preserved in internal representation but not
 *       accessible through standard value interface.
 */
std::expected<value, error> deserialize(const data& input);

/**
 * @brief Serialize structured data to TOML format binary data.
 *
 * Converts dross structured value type to TOML format text data.
 * The output will be valid UTF-8 encoded TOML content.
 *
 * @param input Structured data to serialize
 * @return TOML format binary data on success, error on failure
 *
 * Error conditions:
 * - Unsupported value types (only basic types and containers supported)
 * - Circular references in nested structures
 * - Invalid characters in table keys
 * - Values that cannot be represented in TOML format
 *
 * Serialization rules:
 * - Dictionary keys are sorted alphabetically for consistent output
 * - Arrays are formatted with proper spacing
 * - Nested tables use dotted notation when appropriate
 * - Strings are properly escaped according to TOML rules
 * - Numbers maintain precision without unnecessary trailing zeros
 *
 * @note The serializer produces clean, readable TOML output
 *       suitable for human editing and version control.
 */
std::expected<data, error> serialize(const value& input);

}