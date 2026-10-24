#pragma once

#include "../type/data.h"
#include "../type/dictionary.h"
#include "../type/error.h"
#include "../type/value.h"

#include <expected>

namespace dross::toml {

/**
 * @brief TOML format parser and serializer.
 *
 * This module provides functionality to convert between TOML format binary data
 * and dross structured data types. It reads and writes a subset of TOML v1.0.0;
 * deserialize() lists the forms it does not read.
 *
 * Key features:
 * - Bidirectional conversion (deserialize/serialize)
 * - Integration with dross::value type system
 * - Unicode support for strings and keys
 *
 * Supported TOML types mapping to dross types:
 * - TOML String → dross::string
 * - TOML Integer → dross::number
 * - TOML Float → dross::number
 * - TOML Boolean → dross::boolean
 * - TOML Array → dross::array
 * - TOML Table → dross::dictionary
 *
 * TOML Datetime values are not supported in either direction.
 *
 * @example
 * @code
 * // Deserialize TOML data to dictionary structure
 * data toml_content{"title = \"Example\"\nversion = 1.0"};
 * auto result = dross::toml::deserialize(toml_content);
 * if (result) {
 *     auto& config = result.value();
 *     // Access parsed data through dictionary interface
 *     auto title = config["title"].as<string>();
 *     auto version = config["version"].as<number>();
 * }
 *
 * // Serialize dictionary structure to TOML format
 * dictionary config;
 * config["title"] = value(string{"My App"});
 * config["version"] = value(number{"1.0"});
 * auto toml_data = dross::toml::serialize(config);
 * if (toml_data) {
 *     std::string toml_string = toml_data.value();
 * }
 * @endcode
 */

/**
 * @brief Deserialize TOML format binary data to dictionary structure.
 *
 * Converts TOML format text data to dross dictionary type.
 * The input data is expected to be valid UTF-8 encoded TOML content.
 * TOML documents always have a table (dictionary) as their root structure.
 *
 * @param input TOML format binary data (UTF-8 encoded)
 * @return Parsed dictionary structure on success, error on failure
 *
 * Error conditions:
 * - Invalid TOML syntax
 * - Encoding errors (non-UTF-8 content)
 * - Control characters other than tab, except a CR that ends a line as CRLF
 * - Unicode escapes that do not name a Unicode scalar value
 *
 * Valid TOML that fails to read:
 * - Literal strings and multi-line strings
 * - Datetimes of every kind
 * - Dotted keys such as a.b = 1 (a [a.b] table header is read)
 * - Bare keys that start with a digit or a hyphen
 * - Integers in hexadecimal, octal or binary, or with underscores
 * - Floats with an exponent, inf and nan
 * - Arrays with a comma after the last value
 *
 * Invalid TOML that is read without an error:
 * - A key or table defined twice keeps only the last definition
 * - A key followed by a table of the same name keeps only the table
 * - A table header and key/value pairs on one line are all read
 * - Numbers with a leading zero or without a digit on one side of the
 *   decimal point, such as 01, 1. and -.5
 * - A newline before a comma in an inline table
 *
 * @note Each [[name]] array of tables header is read as a [name] table,
 *       so only the last table of the array is kept. A [a] table header
 *       that follows a [a.b] header replaces table a, so a.b is lost.
 *       Comments are skipped and are not kept in the result.
 */
std::expected<dictionary, error> deserialize(const data& input);

/**
 * @brief Serialize dictionary structure to TOML format binary data.
 *
 * Converts dross dictionary type to TOML format text data.
 * The output will be valid UTF-8 encoded TOML content.
 * TOML documents always have a table (dictionary) as their root structure.
 *
 * @param input Dictionary structure to serialize
 * @return TOML format binary data on success, error on failure
 *
 * Error conditions:
 * - Unsupported value types, such as a timestamp
 * - Values that cannot be represented in TOML format, such as a NaN number
 * - Strings or keys that are not valid UTF-8
 *
 * An empty key is not an error: it is written as an empty bare key, which
 * deserialize() does not read back.
 *
 * Serialization rules:
 * - Dictionary keys are sorted alphabetically for consistent output
 * - Arrays are formatted with proper spacing
 * - Nested tables use dotted notation when appropriate
 * - Strings are properly escaped according to TOML rules
 * - Numbers are written in number's canonical form, without trailing zeros.
 *   number does not record whether it was a TOML integer or float, so a
 *   float whose fraction is all zeros (1.0) is written as an integer (1).
 *   An integral value outside the 64-bit range of TOML integers is written
 *   as a float (9223372036854775808.0). dross reads it back exactly, but a
 *   reader that holds floats as 64-bit doubles may round it.
 *
 * @note The serializer produces clean, readable TOML output
 *       suitable for human editing and version control.
 */
std::expected<data, error> serialize(const dictionary& input);

}  // namespace dross::toml