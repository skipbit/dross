#include "format/cbor.h"

#include "dross/type/array.h"
#include "dross/type/boolean.h"
#include "dross/type/dictionary.h"
#include "dross/type/number.h"
#include "dross/type/string.h"
#include "dross/type/timestamp.h"

#include <algorithm>
#include <bit>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace dross::cbor {

namespace {

enum class major_type : std::uint8_t {
    unsigned_integer = 0,
    negative_integer = 1,
    byte_string = 2,
    text_string = 3,
    array = 4,
    map = 5,
    tag = 6,
    simple = 7,
};

constexpr std::uint64_t kDateTimeStringTag = 0;
constexpr std::uint64_t kEpochTimeTag = 1;
constexpr std::uint64_t kPositiveBignumTag = 2;
constexpr std::uint64_t kNegativeBignumTag = 3;
constexpr std::uint64_t kDecimalFractionTag = 4;

constexpr std::uint8_t kFalse = 0xf4;
constexpr std::uint8_t kTrue = 0xf5;
constexpr std::uint8_t kNull = 0xf6;
constexpr std::uint8_t kHalfFloat = 0xf9;

// What the reader takes at most.
constexpr int kMaximumDepth = 128;
constexpr std::size_t kMaximumDigits = 4096;
// 256^1701 is below 10^4097, so no more bytes are needed for 4096 digits.
constexpr std::uint64_t kMaximumBignumBytes = 1701;

// Big integers are converted between bytes and decimal nine digits at a time.
constexpr std::uint64_t kLimbBase = 1000000000;
constexpr std::size_t kLimbDigits = 9;

// The first second of the year 0 and of the year 10000, from the epoch.
constexpr std::int64_t kFirstSecond = -62167219200;
constexpr std::int64_t kEndSecond = 253402300800;

// One less than a positive decimal integer, given and returned as its
// digits.
std::string decrement(std::string digits)
{
    auto position = digits.rbegin();
    while (*position == '0') {
        *position = '9';
        ++position;
    }
    --*position;
    if ((digits.size() > 1) && (digits.front() == '0')) {
        digits.erase(0, 1);
    }
    return digits;
}

// The big-endian bytes of a non-negative decimal integer, with no leading
// zero byte.
std::vector<std::uint8_t> decimal_to_bytes(const std::string& digits)
{
    // Nine decimal digits at a time, most significant first.
    std::vector<std::uint32_t> limbs;
    const std::size_t head_length = ((digits.size() - 1) % kLimbDigits) + 1;
    for (std::size_t start = 0; start < digits.size(); start += (start == 0) ? head_length : kLimbDigits) {
        const std::size_t length = (start == 0) ? head_length : kLimbDigits;
        std::uint32_t limb = 0;
        std::from_chars(digits.data() + start, digits.data() + start + length, limb);
        limbs.push_back(limb);
    }
    std::vector<std::uint8_t> result;
    std::size_t first = 0;
    while (first < limbs.size()) {
        std::uint64_t remainder = 0;
        for (std::size_t i = first; i < limbs.size(); ++i) {
            const std::uint64_t current = (remainder * kLimbBase) + limbs[i];
            limbs[i] = static_cast<std::uint32_t>(current / 256);
            remainder = current % 256;
        }
        result.push_back(static_cast<std::uint8_t>(remainder));
        while ((first < limbs.size()) && (limbs[first] == 0)) {
            ++first;
        }
    }
    std::ranges::reverse(result);
    return result;
}

class writer final {
public:
    void write(const value& input)
    {
        if (! input) {
            _bytes.push_back(kNull);
        } else if (const auto* flag = input.get_if<boolean>()) {
            _bytes.push_back(flag->value() ? kTrue : kFalse);
        } else if (const auto* amount = input.get_if<number>()) {
            write_number(*amount);
        } else if (const auto* text = input.get_if<string>()) {
            write_text(static_cast<std::string>(*text));
        } else if (const auto* bytes = input.get_if<data>()) {
            head(major_type::byte_string, bytes->size());
            if (const auto start = bytes->bytes()) {
                _bytes.insert(_bytes.end(), *start, *start + bytes->size());
            }
        } else if (const auto* items = input.get_if<array>()) {
            head(major_type::array, items->length());
            for (const auto& item : *items) {
                write(item);
            }
        } else if (const auto* entries = input.get_if<dictionary>()) {
            head(major_type::map, entries->size());
            for (const auto& [key, entry] : *entries) {
                write_text(key);
                write(entry);
            }
        } else if (const auto* moment = input.get_if<timestamp>()) {
            head(major_type::tag, kDateTimeStringTag);
            write_text(moment->format(timestamp::format_type::rfc3339));
        }
    }

    data finish() const
    {
        return data{ _bytes };
    }

private:
    // The initial byte and the argument that follows it, in the fewest bytes.
    void head(const major_type type, const std::uint64_t argument)
    {
        const auto initial = static_cast<std::uint8_t>(static_cast<std::uint8_t>(type) << 5);
        int length = 0;
        if (argument < 24) {
            _bytes.push_back(initial | static_cast<std::uint8_t>(argument));
            return;
        }
        if (argument <= 0xff) {
            _bytes.push_back(initial | 24);
            length = 1;
        } else if (argument <= 0xffff) {
            _bytes.push_back(initial | 25);
            length = 2;
        } else if (argument <= 0xffffffff) {
            _bytes.push_back(initial | 26);
            length = 4;
        } else {
            _bytes.push_back(initial | 27);
            length = 8;
        }
        for (int shift = (length - 1) * 8; shift >= 0; shift -= 8) {
            _bytes.push_back(static_cast<std::uint8_t>(argument >> shift));
        }
    }

    void write_text(const std::string_view text)
    {
        head(major_type::text_string, text.size());
        _bytes.insert(_bytes.end(), text.begin(), text.end());
    }

    // An integer given as its sign and the digits of its magnitude.
    void write_integer(const bool negative, std::string magnitude)
    {
        // A negative integer n is written as -1 - n.
        const std::string argument = negative ? decrement(std::move(magnitude)) : std::move(magnitude);
        std::uint64_t small = 0;
        const auto [end, code] = std::from_chars(argument.data(), argument.data() + argument.size(), small);
        if ((code == std::errc{}) && (end == argument.data() + argument.size())) {
            head(negative ? major_type::negative_integer : major_type::unsigned_integer, small);
            return;
        }
        const std::vector<std::uint8_t> bytes = decimal_to_bytes(argument);
        head(major_type::tag, negative ? kNegativeBignumTag : kPositiveBignumTag);
        head(major_type::byte_string, bytes.size());
        _bytes.insert(_bytes.end(), bytes.begin(), bytes.end());
    }

    void write_number(const number& amount)
    {
        if (amount.is_nan()) {
            _bytes.insert(_bytes.end(), { kHalfFloat, 0x7e, 0x00 });
            return;
        }
        // The text is an optional minus sign and digits, with a decimal point
        // when there is a fraction; it has no exponent.
        std::string text = static_cast<std::string>(amount);
        const bool negative = text.starts_with('-');
        if (negative) {
            text.erase(0, 1);
        }
        if (! amount.is_floating_point()) {
            write_integer(negative, std::move(text));
            return;
        }
        // A decimal fraction: [exponent, mantissa], the value mantissa * 10^exponent.
        const auto point = text.find('.');
        std::size_t fraction_digits = 0;
        if (point != std::string::npos) {
            fraction_digits = text.size() - point - 1;
            text.erase(point, 1);
        }
        const auto first = text.find_first_not_of('0');
        text = (first == std::string::npos) ? std::string{ "0" } : text.substr(first);
        // A whole number keeps its trailing zeros in the exponent: 1000 is
        // [3, 1]. A fraction has none, as number writes none.
        std::size_t trailing_zeros = 0;
        if ((fraction_digits == 0) && (text != "0")) {
            trailing_zeros = text.size() - text.find_last_not_of('0') - 1;
            text.erase(text.size() - trailing_zeros);
        }
        head(major_type::tag, kDecimalFractionTag);
        head(major_type::array, 2);
        write_integer(fraction_digits != 0, std::to_string((fraction_digits != 0) ? fraction_digits : trailing_zeros));
        write_integer(negative && (text != "0"), std::move(text));
    }

    std::vector<std::uint8_t> _bytes;
};

// One more than a non-negative decimal integer, given and returned as its
// digits.
std::string increment(std::string digits)
{
    auto position = digits.rbegin();
    while ((position != digits.rend()) && (*position == '9')) {
        *position = '0';
        ++position;
    }
    if (position == digits.rend()) {
        digits.insert(digits.begin(), '1');
    } else {
        ++*position;
    }
    return digits;
}

// The decimal digits of a big-endian unsigned integer.
std::string bytes_to_decimal(const std::uint8_t* bytes, const std::size_t size)
{
    // Nine decimal digits at a time, least significant first.
    std::vector<std::uint32_t> limbs;
    for (std::size_t i = 0; i < size; ++i) {
        std::uint64_t carry = bytes[i];
        for (auto& limb : limbs) {
            const std::uint64_t current = (static_cast<std::uint64_t>(limb) * 256) + carry;
            limb = static_cast<std::uint32_t>(current % kLimbBase);
            carry = current / kLimbBase;
        }
        if (carry != 0) {
            limbs.push_back(static_cast<std::uint32_t>(carry));
        }
    }
    if (limbs.empty()) {
        return "0";
    }
    std::string result = std::to_string(limbs.back());
    for (auto limb = std::next(limbs.rbegin()); limb != limbs.rend(); ++limb) {
        const std::string part = std::to_string(*limb);
        result.append(kLimbDigits - part.size(), '0');
        result += part;
    }
    return result;
}

// The value of a half-precision float, given as its 16 bits.
double half_to_double(const std::uint16_t bits)
{
    const int exponent = (bits >> 10) & 0x1f;
    const int fraction = bits & 0x3ff;
    double magnitude = 0.0;
    if (exponent == 0) {
        magnitude = std::ldexp(fraction, -24);
    } else if (exponent == 31) {
        magnitude = (fraction == 0) ? std::numeric_limits<double>::infinity() : std::numeric_limits<double>::quiet_NaN();
    } else {
        magnitude = std::ldexp(fraction + 1024, exponent - 25);
    }
    return ((bits & 0x8000) != 0) ? -magnitude : magnitude;
}

class reader final {
public:
    reader(const std::uint8_t* bytes, const std::size_t size)
        : _bytes{ bytes }
        , _size{ size }
    {
    }

    std::expected<value, error> read_document()
    {
        auto result = read(1);
        if (result && (_position != _size)) {
            _start = _position;
            return fail("bytes follow the data item");
        }
        return result;
    }

private:
    struct item_head final {
        major_type type;
        std::uint8_t additional;
        std::uint64_t argument;
    };

    std::unexpected<error> fail(const std::string& message) const
    {
        return std::unexpected(error{ std::errc::invalid_argument, "byte " + std::to_string(_start) + ": " + message });
    }

    std::size_t remaining() const noexcept
    {
        return _size - _position;
    }

    std::expected<item_head, error> read_head()
    {
        _start = _position;
        if (remaining() == 0) {
            return fail("the input ends inside a data item");
        }
        const std::uint8_t initial = _bytes[_position++];
        item_head head{ static_cast<major_type>(initial >> 5), static_cast<std::uint8_t>(initial & 0x1f), 0 };
        if (head.additional < 24) {
            head.argument = head.additional;
            return head;
        }
        if (head.additional == 31) {
            return fail("an indefinite length is not read");
        }
        if (head.additional > 27) {
            return fail("the additional information is reserved");
        }
        const std::size_t length = std::size_t{ 1 } << (head.additional - 24);
        if (remaining() < length) {
            return fail("the input ends inside a data item");
        }
        for (std::size_t i = 0; i < length; ++i) {
            head.argument = (head.argument << 8) | _bytes[_position++];
        }
        return head;
    }

    // The bytes of a string of the given length, which the input must hold.
    std::expected<const std::uint8_t*, error> take(const std::uint64_t length)
    {
        if (length > remaining()) {
            return fail("a length goes past the end of the input");
        }
        const std::uint8_t* start = _bytes + _position;
        _position += static_cast<std::size_t>(length);
        return start;
    }

    // The text string of a head, whose length the input must hold.
    std::expected<std::string, error> take_text(const item_head& head)
    {
        const auto start = take(head.argument);
        if (! start) {
            return std::unexpected(start.error());
        }
        return std::string(reinterpret_cast<const char*>(*start), static_cast<std::size_t>(head.argument));
    }

    static std::string too_deep_message()
    {
        return "arrays and maps are nested deeper than " + std::to_string(kMaximumDepth);
    }

    static std::string too_many_digits_message(const std::string_view what)
    {
        return std::string(what) + " has more than " + std::to_string(kMaximumDigits) + " digits";
    }

    std::expected<value, error> read(const int depth)
    {
        const auto head = read_head();
        if (! head) {
            return std::unexpected(head.error());
        }
        if (((head->type == major_type::array) || (head->type == major_type::map)) && (depth > kMaximumDepth)) {
            return fail(too_deep_message());
        }
        switch (head->type) {
        case major_type::unsigned_integer:
        case major_type::negative_integer:
            return number{ integer_text(*head) };
        case major_type::byte_string: {
            const auto start = take(head->argument);
            if (! start) {
                return std::unexpected(start.error());
            }
            return data{ *start, static_cast<std::size_t>(head->argument) };
        }
        case major_type::text_string: {
            auto text = take_text(*head);
            if (! text) {
                return std::unexpected(text.error());
            }
            return string{ std::move(*text) };
        }
        case major_type::array:
            return read_array(head->argument, depth);
        case major_type::map:
            return read_map(head->argument, depth);
        case major_type::tag:
            return read_tag(head->argument);
        case major_type::simple:
            return read_simple(*head);
        }
        return fail("unknown major type");
    }

    std::expected<value, error> read_array(const std::uint64_t count, const int depth)
    {
        // Each item takes a byte at least, so a count beyond the bytes left
        // is refused before anything is made for it.
        if (count > remaining()) {
            return fail("an array has more items than bytes left");
        }
        array items;
        for (std::uint64_t i = 0; i < count; ++i) {
            auto item = read(depth + 1);
            if (! item) {
                return item;
            }
            items.append(std::move(*item));
        }
        return items;
    }

    std::expected<value, error> read_map(const std::uint64_t count, const int depth)
    {
        if (count > (remaining() / 2)) {
            return fail("a map has more entries than bytes left");
        }
        dictionary entries;
        for (std::uint64_t i = 0; i < count; ++i) {
            const auto key_head = read_head();
            if (! key_head) {
                return std::unexpected(key_head.error());
            }
            if (key_head->type != major_type::text_string) {
                return fail("a map key is not a text string");
            }
            auto taken = take_text(*key_head);
            if (! taken) {
                return std::unexpected(taken.error());
            }
            const std::string key = std::move(*taken);
            if (entries.contains(key)) {
                return fail("a map key appears twice");
            }
            auto entry = read(depth + 1);
            if (! entry) {
                return entry;
            }
            entries[key] = std::move(*entry);
        }
        return entries;
    }

    std::expected<value, error> read_simple(const item_head& head)
    {
        switch (head.additional) {
        case 20:
            return boolean{ false };
        case 21:
            return boolean{ true };
        case 22:
            return value{};
        case 25:
        case 26:
        case 27:
            return read_float(head);
        default:
            return fail("simple value " + std::to_string(head.argument) + " is not read");
        }
    }

    static bool is_float(const item_head& head)
    {
        return (head.type == major_type::simple) && (head.additional >= 25) && (head.additional <= 27);
    }

    // The value of a half, single or double precision float head.
    static double float_value(const item_head& head)
    {
        if (head.additional == 25) {
            return half_to_double(static_cast<std::uint16_t>(head.argument));
        }
        if (head.additional == 26) {
            return std::bit_cast<float>(static_cast<std::uint32_t>(head.argument));
        }
        return std::bit_cast<double>(head.argument);
    }

    std::expected<value, error> read_float(const item_head& head)
    {
        const double amount = float_value(head);
        if (std::isinf(amount)) {
            return fail("an infinity cannot be held");
        }
        if (std::isnan(amount)) {
            return number{ std::numeric_limits<double>::quiet_NaN() };
        }
        if (head.additional == 26) {
            // Written as the shortest text that gives the same float.
            return number{ static_cast<float>(amount) };
        }
        return number{ amount };
    }

    // The decimal text of an integer head, with a minus sign when negative.
    static std::string integer_text(const item_head& head)
    {
        const std::string magnitude = std::to_string(head.argument);
        if (head.type == major_type::unsigned_integer) {
            return magnitude;
        }
        // A negative integer is written as -1 - n.
        return "-" + increment(magnitude);
    }

    // An integer, or with bignum also a bignum, as decimal text.
    std::expected<std::string, error> read_integer(const bool bignum)
    {
        const auto head = read_head();
        if (! head) {
            return std::unexpected(head.error());
        }
        if ((head->type == major_type::unsigned_integer) || (head->type == major_type::negative_integer)) {
            return integer_text(*head);
        }
        if (bignum && (head->type == major_type::tag) && ((head->argument == kPositiveBignumTag) || (head->argument == kNegativeBignumTag))) {
            return read_bignum(head->argument == kNegativeBignumTag);
        }
        return fail("an integer is expected");
    }

    std::expected<value, error> read_tag(const std::uint64_t tag)
    {
        switch (tag) {
        case kDateTimeStringTag:
            return read_date_time_string();
        case kEpochTimeTag:
            return read_epoch_time();
        case kPositiveBignumTag:
        case kNegativeBignumTag: {
            const auto digits = read_bignum(tag == kNegativeBignumTag);
            if (! digits) {
                return std::unexpected(digits.error());
            }
            return number{ *digits };
        }
        case kDecimalFractionTag:
            return read_decimal_fraction();
        default:
            return fail("tag " + std::to_string(tag) + " is not read");
        }
    }

    std::expected<value, error> read_date_time_string()
    {
        const auto head = read_head();
        if (! head) {
            return std::unexpected(head.error());
        }
        if (head->type != major_type::text_string) {
            return fail("a date and time is not a text string");
        }
        const auto text = take_text(*head);
        if (! text) {
            return std::unexpected(text.error());
        }
        const auto moment = timestamp::from_string(*text);
        if (! moment) {
            return fail("a date and time string is not one a timestamp holds");
        }
        return *moment;
    }

    // The seconds and nanoseconds from the epoch of an integer or float head,
    // or nothing outside the years 0 to 9999.
    static std::optional<std::pair<std::int64_t, std::int64_t>> epoch_instant(const item_head& head)
    {
        if (head.type == major_type::unsigned_integer) {
            if (head.argument >= static_cast<std::uint64_t>(kEndSecond)) {
                return std::nullopt;
            }
            return std::pair{ static_cast<std::int64_t>(head.argument), std::int64_t{ 0 } };
        }
        if (head.type == major_type::negative_integer) {
            if (head.argument >= static_cast<std::uint64_t>(-kFirstSecond)) {
                return std::nullopt;
            }
            return std::pair{ -1 - static_cast<std::int64_t>(head.argument), std::int64_t{ 0 } };
        }
        const double amount = float_value(head);
        // Also false for NaN.
        if (! ((amount >= static_cast<double>(kFirstSecond)) && (amount < static_cast<double>(kEndSecond)))) {
            return std::nullopt;
        }
        const double whole = std::floor(amount);
        std::int64_t seconds = static_cast<std::int64_t>(whole);
        std::int64_t nanoseconds = std::llround((amount - whole) * 1e9);
        if (nanoseconds == 1000000000) {
            ++seconds;
            nanoseconds = 0;
        }
        if (seconds >= kEndSecond) {
            return std::nullopt;
        }
        return std::pair{ seconds, nanoseconds };
    }

    static std::optional<timestamp> timestamp_at(const std::int64_t seconds, const std::int64_t nanoseconds)
    {
        const auto moment = timestamp::from_components(1970, 1, 1)->add(std::chrono::seconds{ seconds });
        if (! moment) {
            return std::nullopt;
        }
        if (nanoseconds == 0) {
            return *moment;
        }
        // A UTC timestamp formats as ...:SSZ; the fraction goes before the Z.
        std::string text = moment->format(timestamp::format_type::rfc3339);
        std::string fraction = std::to_string(nanoseconds);
        fraction.insert(0, 9 - fraction.size(), '0');
        text.insert(text.size() - 1, "." + fraction);
        const auto precise = timestamp::from_string(text);
        if (! precise) {
            return std::nullopt;
        }
        return *precise;
    }

    std::expected<value, error> read_epoch_time()
    {
        const auto head = read_head();
        if (! head) {
            return std::unexpected(head.error());
        }
        const bool integer = (head->type == major_type::unsigned_integer) || (head->type == major_type::negative_integer);
        if (! integer && ! is_float(*head)) {
            return fail("an epoch time is not a number");
        }
        const auto instant = epoch_instant(*head);
        const auto moment = instant ? timestamp_at(instant->first, instant->second) : std::nullopt;
        if (! moment) {
            return fail("an epoch time is outside the years 0 to 9999");
        }
        return *moment;
    }

    // The digits of a bignum, with a minus sign when negative.
    std::expected<std::string, error> read_bignum(const bool negative)
    {
        const auto head = read_head();
        if (! head) {
            return std::unexpected(head.error());
        }
        if (head->type != major_type::byte_string) {
            return fail("a bignum is not a byte string");
        }
        if (head->argument > kMaximumBignumBytes) {
            return fail(too_many_digits_message("a bignum"));
        }
        const auto start = take(head->argument);
        if (! start) {
            return std::unexpected(start.error());
        }
        std::string digits = bytes_to_decimal(*start, static_cast<std::size_t>(head->argument));
        if (negative) {
            // A negative bignum holds -1 - n.
            digits = increment(std::move(digits));
        }
        if (digits.size() > kMaximumDigits) {
            return fail(too_many_digits_message("a bignum"));
        }
        return negative ? "-" + digits : digits;
    }

    std::expected<value, error> read_decimal_fraction()
    {
        const auto head = read_head();
        if (! head) {
            return std::unexpected(head.error());
        }
        if ((head->type != major_type::array) || (head->argument != 2)) {
            return fail("a decimal fraction is not an array of two items");
        }
        const auto exponent_text = read_integer(false);
        if (! exponent_text) {
            return std::unexpected(exponent_text.error());
        }
        auto mantissa = read_integer(true);
        if (! mantissa) {
            return std::unexpected(mantissa.error());
        }
        std::int64_t exponent = 0;
        const auto [end, code] = std::from_chars(exponent_text->data(), exponent_text->data() + exponent_text->size(), exponent);
        const auto limit = static_cast<std::int64_t>(kMaximumDigits);
        if ((code != std::errc{}) || (end != exponent_text->data() + exponent_text->size()) || (exponent > limit) || (exponent < -limit)) {
            return fail(too_many_digits_message("a decimal fraction"));
        }
        const bool negative = mantissa->starts_with('-');
        std::string digits = negative ? mantissa->substr(1) : *mantissa;
        if (digits == "0") {
            return number{ "0.0" };
        }
        // The text of mantissa * 10^exponent, with a decimal point so that the
        // number is floating point.
        std::string text;
        if (exponent >= 0) {
            if ((digits.size() + static_cast<std::size_t>(exponent)) > kMaximumDigits) {
                return fail(too_many_digits_message("a decimal fraction"));
            }
            text = digits + std::string(static_cast<std::size_t>(exponent), '0') + ".0";
        } else {
            const auto fraction_digits = static_cast<std::size_t>(-exponent);
            if (digits.size() > fraction_digits) {
                text = digits.substr(0, digits.size() - fraction_digits) + "." + digits.substr(digits.size() - fraction_digits);
            } else {
                text = "0." + std::string(fraction_digits - digits.size(), '0') + digits;
            }
        }
        if (negative) {
            text.insert(0, 1, '-');
        }
        return number{ text };
    }

    const std::uint8_t* _bytes;
    std::size_t _size;
    std::size_t _position{ 0 };
    // Where the data item being read starts, for the error.
    std::size_t _start{ 0 };
};

}  // namespace

data serialize(const value& input)
{
    writer output;
    output.write(input);
    return output.finish();
}

std::expected<value, error> deserialize(const data& input)
{
    const auto start = input.bytes();
    reader source{ start ? *start : nullptr, input.size() };
    return source.read_document();
}

}  // namespace dross::cbor
