#pragma once

#include "dross/type/data.h"
#include "dross/type/error.h"
#include "dross/type/value.h"

#include <expected>

// CBOR (RFC 8949) for dross::value, kept inside the library: the subset
// dross writes, and the same subset read back.
namespace dross::cbor {

// Hides a declaration from the library's exported symbol table, as
// deadline.h does: this header is never installed, so nothing outside the
// library names it, and this keeps it off nm -D's output as well.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_CBOR_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_CBOR_HIDDEN
#endif

/**
 * @brief Write a value as one CBOR data item.
 * @param input The value to write
 * @return The bytes of the item
 *
 * Each kind is written so that reading it back gives an equal value with
 * the same form:
 * - null, boolean: the simple values null, true and false
 * - number: an integer as an integer, or as a bignum (tags 2 and 3) beyond
 *   64 bits; a floating-point number as a decimal fraction (tag 4), so no
 *   digit is lost; NaN as the half-precision NaN
 * - string: a text string of the bytes it holds, which are not checked to
 *   be UTF-8
 * - data: a byte string
 * - array, dictionary: an array and a map of definite length, the map with
 *   text string keys
 * - timestamp: an RFC 3339 date and time string (tag 0), with its offset
 *   and fractional seconds
 *
 * A number whose integer or fractional part has more than 4096 digits, as
 * arithmetic can make, is written, but deserialize() refuses it.
 */
DROSS_CBOR_HIDDEN data serialize(const value& input);

/**
 * @brief Read one CBOR data item as a value.
 * @param input The bytes of exactly one item
 * @return The value, or an error with std::errc::invalid_argument whose
 * detail names the byte where the last data item read starts: the one
 * refused, or the last one read inside it
 *
 * It reads what serialize() writes, and also:
 * - an integer, bignum or decimal fraction not in the fewest bytes
 * - a half, single or double precision float, as a floating-point number
 * - an epoch date and time (tag 1), as a UTC timestamp; a float is rounded
 *   to the nanosecond
 *
 * It refuses:
 * - input that ends inside an item, or has bytes after it
 * - a length or count that goes past the bytes left, before anything is
 *   made for it
 * - arrays and maps nested deeper than 128
 * - a map key that is not a text string, or appears twice
 * - an infinity; a bignum, or a decimal fraction's integer or fractional
 *   part, of more than 4096 digits; a date and time outside the years 0 to
 *   9999
 * - an indefinite length, a tag other than 0 to 4, and a simple value other
 *   than false, true and null
 *
 * A text string is not checked to be UTF-8, as dross::string does not
 * check it.
 */
DROSS_CBOR_HIDDEN std::expected<value, error> deserialize(const data& input);

#undef DROSS_CBOR_HIDDEN

}  // namespace dross::cbor
