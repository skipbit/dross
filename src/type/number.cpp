#include "dross/type/number.h"

#include <algorithm>
#include <cmath>
#include <compare>
#include <limits>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace dross {

namespace {
// Internal NaN representation (implementation detail)
constexpr const char* NAN_VALUE = "__invalid__";

/**
 * @brief Comprehensive number representation after parsing.
 *
 * This struct holds all parsed components of a number string, providing
 * a unified interface for all conversion operations. It avoids the need
 * for multiple parsing functions and ensures consistency across all
 * type conversions.
 */
struct parsed_number {
    bool is_negative = false;                    // Sign of the number
    std::string integer_part = "0";              // Integer portion (without sign)
    std::optional<std::string> fractional_part;  // Fractional portion (if any)
    bool is_valid = true;                        // Whether parsing was successful

    /**
     * @brief Convert to double with proper error handling.
     * @return std::optional<double> containing the value or nullopt on error
     */
    std::optional<double> to_double() const
    {
        if (! is_valid) {
            return std::nullopt;
        }

        std::string full_str = (is_negative ? "-" : "") + integer_part;
        if (fractional_part) {
            full_str += "." + *fractional_part;
        }

        try {
            size_t processed = 0;
            double value = std::stod(full_str, &processed);
            if (processed == full_str.length()) {
                return value;
            }
        } catch (const std::exception&) {
            // Invalid conversion
        }
        return std::nullopt;
    }

    /**
     * @brief Convert to long long with proper overflow handling.
     * @return std::optional<long long> containing the value or nullopt on error
     */
    std::optional<long long> to_long_long() const
    {
        if (! is_valid) {
            return std::nullopt;
        }

        // For long long conversion, we only use the integer part
        std::string int_str = (is_negative ? "-" : "") + integer_part;

        try {
            size_t processed = 0;
            long long value = std::stoll(int_str, &processed);
            if (processed == int_str.length()) {
                return value;
            }
        } catch (const std::exception&) {
            // Invalid conversion or overflow
        }
        return std::nullopt;
    }

    /**
     * @brief Convert to int with rounding and clamping.
     * @return int value with proper overflow protection
     */
    int to_int() const
    {
        auto double_val = to_double();
        if (! double_val) {
            return 0;
        }

        // Apply rounding for decimal numbers
        double rounded = std::round(*double_val);

        // Clamp to int range to prevent overflow
        return static_cast<int>(std::clamp(rounded, static_cast<double>(std::numeric_limits<int>::min()), static_cast<double>(std::numeric_limits<int>::max())));
    }
};

/**
 * @brief Split canonical number text into its components.
 *
 * @param str Canonical text or NAN_VALUE
 * @return parsed_number, invalid for NAN_VALUE
 */
parsed_number parse_number_unified(const std::string& str)
{
    parsed_number result;

    if (str == NAN_VALUE) {
        result.is_valid = false;
        return result;
    }

    const size_t start = (str[0] == '-') ? 1 : 0;
    result.is_negative = (start == 1);

    const size_t dot = str.find('.', start);
    if (dot == std::string::npos) {
        result.integer_part = str.substr(start);
    } else {
        result.integer_part = str.substr(start, dot - start);
        result.fractional_part = str.substr(dot + 1);
    }

    return result;
}

// Check if string represents a valid number (integer, decimal, or scientific notation)
bool is_valid_number(const std::string& str)
{
    if (str.empty()) {
        return false;
    }

    size_t start = 0;
    bool has_dot = false;

    // Handle optional sign
    if ((str[0] == '-') || (str[0] == '+')) {
        if (str.length() == 1) {
            return false;
        }
        start = 1;
    }

    // Check for exponent position
    size_t exp_pos = str.find_first_of("eE", start);
    bool has_exp = (exp_pos != std::string::npos);

    // Validate mantissa (part before exponent)
    size_t mantissa_end = has_exp ? exp_pos : str.length();

    // For scientific notation, mantissa cannot be empty
    if (has_exp && (mantissa_end <= start)) {
        return false;
    }

    for (size_t i = start; i < mantissa_end; ++i) {
        if (str[i] == '.') {
            if (has_dot) {
                return false;  // Multiple dots
            }
            has_dot = true;
        } else if (! std::isdigit(str[i])) {
            return false;
        }
    }

    // Don't allow trailing or leading dot in mantissa
    if (has_dot && ((str[start] == '.') || ((mantissa_end > start) && (str[mantissa_end - 1] == '.')))) {
        // Allow ".5" or "5." patterns
        if (mantissa_end <= (start + 1)) {
            return false;
        }
    }

    // Validate exponent part if present
    if (has_exp) {
        if ((exp_pos + 1) >= str.length()) {
            return false;  // Nothing after 'e'
        }

        size_t exp_start = (exp_pos + 1);

        // Handle optional sign in exponent
        if ((str[exp_start] == '-') || (str[exp_start] == '+')) {
            exp_start++;
            if (exp_start >= str.length()) {
                return false;  // Nothing after sign
            }
        }

        // Exponent must have at least one digit
        if (exp_start >= str.length()) {
            return false;
        }

        // Check that exponent contains only digits
        for (size_t i = exp_start; i < str.length(); ++i) {
            if (! std::isdigit(str[i])) {
                return false;
            }
        }
    }

    return true;
}

// The most digits a number written with an exponent may expand to. Beyond it
// the value is NaN, so a short input cannot ask for an arbitrarily long string.
constexpr std::size_t max_expanded_digits = 4096;

/**
 * @brief Expand the exponent of a valid number string into decimal notation.
 *
 * Moves the decimal point in the text, so every digit that was written
 * survives. A string without an exponent is returned as it is. An expansion
 * longer than max_expanded_digits gives NAN_VALUE, the only failure signal.
 *
 * @param str A string for which is_valid_number is true
 */
std::string expand_exponent(const std::string& str)
{
    const size_t exp_pos = str.find_first_of("eE");
    if (exp_pos == std::string::npos) {
        return str;
    }

    const bool negative = (str[0] == '-');
    const size_t start = ((str[0] == '-') || (str[0] == '+')) ? 1 : 0;
    const std::string mantissa = str.substr(start, exp_pos - start);

    std::string digits;
    size_t point = mantissa.length();
    for (size_t i = 0; i < mantissa.length(); ++i) {
        if (mantissa[i] == '.') {
            point = i;
        } else {
            digits += mantissa[i];
        }
    }

    const size_t first = digits.find_first_not_of('0');
    if (first == std::string::npos) {
        return "0";
    }
    digits.erase(0, first);
    const long long leading = static_cast<long long>(point) - static_cast<long long>(first);

    // An exponent this long cannot stay within max_expanded_digits.
    std::string exponent = str.substr(exp_pos + 1);
    const bool exponent_negative = (exponent[0] == '-');
    if ((exponent[0] == '-') || (exponent[0] == '+')) {
        exponent.erase(0, 1);
    }
    exponent.erase(0, std::min(exponent.find_first_not_of('0'), exponent.length()));
    if (exponent.length() > 9) {
        return NAN_VALUE;
    }
    const long long shift = exponent.empty() ? 0 : std::stoll(exponent);

    // Digits before the decimal point once the exponent is applied.
    const long long integer_digits = leading + (exponent_negative ? -shift : shift);
    const long long length = static_cast<long long>(digits.length());
    const long long total = (integer_digits <= 0) ? (1 - integer_digits + length) : std::max(integer_digits, length);
    if (total > static_cast<long long>(max_expanded_digits)) {
        return NAN_VALUE;
    }

    std::string result;
    if (integer_digits <= 0) {
        result = "0." + std::string(static_cast<size_t>(-integer_digits), '0') + digits;
    } else if (integer_digits >= length) {
        result = digits + std::string(static_cast<size_t>(integer_digits - length), '0');
    } else {
        result = digits.substr(0, static_cast<size_t>(integer_digits)) + "." + digits.substr(static_cast<size_t>(integer_digits));
    }

    return negative ? ("-" + result) : result;
}

/**
 * @brief The canonical form of a number string.
 *
 * Expands an exponent, drops a leading '+', leading zeros of the integer part
 * and trailing zeros of the fraction, supplies "0" for an empty integer part,
 * and writes zero without a sign. Equal values therefore have equal text.
 * Every string that is not a number, or whose exponent expands beyond
 * max_expanded_digits, becomes the one NaN.
 */
std::string canonical_text(const std::string& str)
{
    if (! is_valid_number(str)) {
        return NAN_VALUE;
    }

    const std::string expanded = expand_exponent(str);
    if (expanded == NAN_VALUE) {
        return NAN_VALUE;
    }

    const bool negative = (expanded[0] == '-');
    const size_t start = ((expanded[0] == '-') || (expanded[0] == '+')) ? 1 : 0;
    const std::string unsigned_part = expanded.substr(start);

    const size_t dot = unsigned_part.find('.');
    std::string integer = (dot == std::string::npos) ? unsigned_part : unsigned_part.substr(0, dot);
    std::string fraction = (dot == std::string::npos) ? "" : unsigned_part.substr(dot + 1);

    integer.erase(0, std::min(integer.find_first_not_of('0'), integer.length()));
    if (integer.empty()) {
        integer = "0";
    }
    const size_t last = fraction.find_last_not_of('0');
    fraction.erase((last == std::string::npos) ? 0 : (last + 1));

    const std::string result = fraction.empty() ? integer : (integer + "." + fraction);
    return (negative && (result != "0")) ? ("-" + result) : result;
}

// Compare two number strings (handles both integers and decimals).
// Both arguments are canonical, non-NaN text. Allocation-free.
int compare_numbers(std::string_view a, std::string_view b)
{
    const bool a_neg = (a[0] == '-');
    const bool b_neg = (b[0] == '-');

    if (a_neg != b_neg) {
        return a_neg ? -1 : 1;
    }

    if (a_neg) {
        a.remove_prefix(1);
        b.remove_prefix(1);
    }

    const size_t a_dot = std::min(a.find('.'), a.size());
    const size_t b_dot = std::min(b.find('.'), b.size());
    const std::string_view a_int = a.substr(0, a_dot);
    const std::string_view b_int = b.substr(0, b_dot);

    // More integer digits means a larger magnitude; the fraction has no trailing zeros,
    // so a fraction that is a prefix of the other is the smaller one.
    int cmp = 0;
    if (a_int.size() != b_int.size()) {
        cmp = (a_int.size() < b_int.size()) ? -1 : 1;
    } else if (int c = a_int.compare(b_int); c != 0) {
        cmp = (c < 0) ? -1 : 1;
    } else if (int c = a.substr(a_dot).compare(b.substr(b_dot)); c != 0) {
        cmp = (c < 0) ? -1 : 1;
    }

    return a_neg ? -cmp : cmp;
}

// Parse canonical number text into integer and fractional parts
struct NumberParts {
    bool negative;
    std::string integer;
    std::string fractional;

    NumberParts(const std::string& num)
    {
        negative = (num[0] == '-');
        std::string abs_num = negative ? num.substr(1) : num;

        size_t dot_pos = abs_num.find('.');
        if (dot_pos == std::string::npos) {
            integer = abs_num;
            fractional = "";
        } else {
            integer = abs_num.substr(0, dot_pos);
            fractional = abs_num.substr(dot_pos + 1);
        }
    }
};

// Add two positive number strings (supports decimals)
std::string add_positive_numbers(const std::string& a, const std::string& b)
{
    NumberParts pa(a);
    NumberParts pb(b);

    // Make fractional parts same length
    size_t max_frac = std::max(pa.fractional.length(), pb.fractional.length());
    pa.fractional.resize(max_frac, '0');
    pb.fractional.resize(max_frac, '0');

    // Add fractional parts
    std::string result_frac;
    int carry = 0;
    for (int i = (max_frac - 1); i >= 0; i--) {
        int sum = (carry + (pa.fractional[i] - '0') + (pb.fractional[i] - '0'));
        carry = sum / 10;
        result_frac = char('0' + sum % 10) + result_frac;
    }

    // Add integer parts
    std::string result_int;
    int i = (pa.integer.length() - 1);
    int j = (pb.integer.length() - 1);

    while ((i >= 0) || (j >= 0) || (carry > 0)) {
        int sum = carry;
        if (i >= 0) {
            sum += pa.integer[i--] - '0';
        }
        if (j >= 0) {
            sum += pb.integer[j--] - '0';
        }

        carry = sum / 10;
        result_int = char('0' + sum % 10) + result_int;
    }

    // Combine result
    std::string result = result_int;
    if (! result_frac.empty()) {
        // Remove trailing zeros from fractional part
        while ((! result_frac.empty()) && (result_frac.back() == '0')) {
            result_frac.pop_back();
        }
        if (! result_frac.empty()) {
            result += "." + result_frac;
        }
    }

    return canonical_text(result);
}

// Subtract two positive number strings (a >= b, supports decimals)
std::string subtract_positive_numbers(const std::string& a, const std::string& b)
{
    NumberParts pa(a);
    NumberParts pb(b);

    // Make fractional parts same length
    size_t max_frac = std::max(pa.fractional.length(), pb.fractional.length());
    pa.fractional.resize(max_frac, '0');
    pb.fractional.resize(max_frac, '0');

    // Subtract fractional parts
    std::string result_frac;
    int borrow = 0;
    for (int i = (max_frac - 1); i >= 0; i--) {
        int diff = ((pa.fractional[i] - '0') - (pb.fractional[i] - '0') - borrow);
        if (diff < 0) {
            diff += 10;
            borrow = 1;
        } else {
            borrow = 0;
        }
        result_frac = char('0' + diff) + result_frac;
    }

    // Subtract integer parts
    std::string result_int;
    int i = (pa.integer.length() - 1);
    int j = (pb.integer.length() - 1);

    while (i >= 0) {
        int diff = ((pa.integer[i] - '0') - borrow);
        if (j >= 0) {
            diff -= (pb.integer[j--] - '0');
        }

        if (diff < 0) {
            diff += 10;
            borrow = 1;
        } else {
            borrow = 0;
        }

        result_int = char('0' + diff) + result_int;
        i--;
    }

    // Combine result
    std::string result = result_int;
    if (! result_frac.empty()) {
        // Remove trailing zeros from fractional part
        while ((! result_frac.empty()) && (result_frac.back() == '0')) {
            result_frac.pop_back();
        }
        if (! result_frac.empty()) {
            result += "." + result_frac;
        }
    }

    return canonical_text(result);
}

// Multiply two positive number strings (supports decimals)
std::string multiply_positive_numbers(const std::string& a, const std::string& b)
{
    NumberParts pa(a);
    NumberParts pb(b);

    // Convert to pure integers by combining integer and fractional parts
    std::string num_a = pa.integer + pa.fractional;
    std::string num_b = pb.integer + pb.fractional;
    int total_decimal_places = (pa.fractional.length() + pb.fractional.length());

    // Multiply as integers
    if (num_a == "0" || num_b == "0") {
        return "0";
    }

    std::string result(num_a.length() + num_b.length(), '0');

    for (int i = (num_a.length() - 1); i >= 0; i--) {
        for (int j = (num_b.length() - 1); j >= 0; j--) {
            int mul = ((num_a[i] - '0') * (num_b[j] - '0'));
            int p1 = (i + j), p2 = (i + j + 1);
            int sum = (mul + (result[p2] - '0'));

            result[p2] = char('0' + sum % 10);
            result[p1] += sum / 10;
        }
    }

    // Remove leading zeros
    size_t start = 0;
    while ((start < (result.length() - 1)) && (result[start] == '0')) {
        start++;
    }
    result = result.substr(start);

    // Insert decimal point
    if (total_decimal_places > 0) {
        if (result.length() <= static_cast<size_t>(total_decimal_places)) {
            // Need to pad with leading zeros
            result = std::string(static_cast<size_t>(total_decimal_places) - result.length() + 1, '0') + result;
        }

        size_t decimal_pos = (result.length() - static_cast<size_t>(total_decimal_places));
        if (decimal_pos == 0) {
            result = "0." + result;
        } else {
            result = result.substr(0, decimal_pos) + "." + result.substr(decimal_pos);
        }
    }

    return canonical_text(result);
}

// Significant digits a quotient keeps, as IEEE decimal128 does; the digits
// after them are truncated.
constexpr std::size_t quotient_significant_digits = 34;

// Divide two positive number strings (supports decimals). The integer part of
// the quotient is exact; the fraction stops once the quotient holds
// significant_digits significant digits, so 0 gives integer division.
std::string divide_positive_numbers(const std::string& a, const std::string& b, std::size_t significant_digits = quotient_significant_digits)
{
    if (b == "0") {
        return NAN_VALUE;
    }

    NumberParts pa(a);
    NumberParts pb(b);

    // Scaling both operands by the same power of ten leaves the quotient
    // unchanged, so padding the shorter fraction removes the need to move
    // the point afterwards.
    const size_t scale = std::max(pa.fractional.length(), pb.fractional.length());
    pa.fractional.resize(scale, '0');
    pb.fractional.resize(scale, '0');

    // Convert to pure integers for division algorithm
    std::string dividend = pa.integer + pa.fractional;
    std::string divisor = pb.integer + pb.fractional;

    // Remove leading zeros from divisor
    while ((divisor.length() > 1) && (divisor[0] == '0')) {
        divisor = divisor.substr(1);
    }

    if (divisor == "0") {
        return NAN_VALUE;
    }

    // Perform long division
    std::string quotient = "0";
    std::string remainder = "0";

    // Integer division first
    for (char digit : dividend) {
        remainder = remainder + digit;

        // Remove leading zeros from remainder
        while ((remainder.length() > 1) && (remainder[0] == '0')) {
            remainder = remainder.substr(1);
        }

        // Find how many times divisor goes into current remainder
        int count = 0;
        std::string temp_remainder = remainder;

        while (compare_numbers(temp_remainder, divisor) >= 0) {
            temp_remainder = subtract_positive_numbers(temp_remainder, divisor);
            count++;
        }

        quotient = quotient + std::to_string(count);
        remainder = temp_remainder;
    }

    // Remove leading zeros from quotient
    while ((quotient.length() > 1) && (quotient[0] == '0')) {
        quotient = quotient.substr(1);
    }

    // Fraction digits until the quotient holds enough significant digits.
    // Zeros before the first non-zero digit are not significant.
    std::string decimal_part = "";
    std::size_t significant = (quotient == "0") ? 0 : quotient.length();
    while ((remainder != "0") && (significant < significant_digits)) {
        remainder = remainder + "0";  // Add a zero for next decimal place

        int count = 0;
        std::string temp_remainder = remainder;

        while (compare_numbers(temp_remainder, divisor) >= 0) {
            temp_remainder = subtract_positive_numbers(temp_remainder, divisor);
            count++;
        }

        decimal_part = decimal_part + std::to_string(count);
        remainder = temp_remainder;
        if ((significant > 0) || (count != 0)) {
            significant++;
        }
    }

    // Combine integer and decimal parts
    std::string result = quotient;
    if (! decimal_part.empty()) {
        // Remove trailing zeros from decimal part
        while ((! decimal_part.empty()) && (decimal_part.back() == '0')) {
            decimal_part.pop_back();
        }
        if (! decimal_part.empty()) {
            result = result + "." + decimal_part;
        }
    }

    return canonical_text(result);
}

// Modulo operation for positive numbers (a % b = a - floor(a/b) * b)
std::string modulo_positive_numbers(const std::string& a, const std::string& b)
{
    if (b == "0") {
        return NAN_VALUE;
    }

    // For modulo, we need integer division (floor division)
    std::string quotient = divide_positive_numbers(a, b, 0);  // Integer division

    // Handle case where quotient might have decimal (shouldn't happen with precision 0, but safety check)
    size_t dot_pos = quotient.find('.');
    if (dot_pos != std::string::npos) {
        quotient = quotient.substr(0, dot_pos);  // Take only integer part
    }

    // Calculate b * quotient
    std::string product = multiply_positive_numbers(b, quotient);

    // Calculate a - (b * quotient)
    std::string result = subtract_positive_numbers(a, product);

    return canonical_text(result);
}

// Perform string-based arithmetic on stored text (canonical or NAN_VALUE)
std::string perform_arithmetic(const std::string& na, const std::string& nb, char operation)
{
    if ((na == NAN_VALUE) || (nb == NAN_VALUE)) {
        return NAN_VALUE;
    }

    bool a_neg = (na[0] == '-');
    bool b_neg = (nb[0] == '-');

    std::string a_abs = a_neg ? na.substr(1) : na;
    std::string b_abs = b_neg ? nb.substr(1) : nb;

    switch (operation) {
    case '+': {
        if (a_neg == b_neg) {
            // Same sign: add absolute values
            std::string result = add_positive_numbers(a_abs, b_abs);
            return a_neg ? "-" + result : result;
        } else {
            // Different signs: subtract smaller from larger
            int cmp = compare_numbers(a_abs, b_abs);
            if (cmp == 0) {
                return "0";
            }

            if (cmp > 0) {
                std::string result = subtract_positive_numbers(a_abs, b_abs);
                return a_neg ? "-" + result : result;
            } else {
                std::string result = subtract_positive_numbers(b_abs, a_abs);
                return b_neg ? "-" + result : result;
            }
        }
    }
    case '-': {
        if (a_neg != b_neg) {
            // Different signs: add absolute values
            std::string result = add_positive_numbers(a_abs, b_abs);
            return a_neg ? "-" + result : result;
        } else {
            // Same signs: subtract
            int cmp = compare_numbers(a_abs, b_abs);
            if (cmp == 0) {
                return "0";
            }

            if (cmp > 0) {
                std::string result = subtract_positive_numbers(a_abs, b_abs);
                return a_neg ? "-" + result : result;
            } else {
                std::string result = subtract_positive_numbers(b_abs, a_abs);
                return a_neg ? result : "-" + result;
            }
        }
    }
    case '*': {
        std::string result = multiply_positive_numbers(a_abs, b_abs);
        if (result == "0") {
            return "0";
        }
        return (a_neg != b_neg) ? "-" + result : result;
    }
    case '/': {
        if (nb == "0") {
            return NAN_VALUE;
        }
        std::string result = divide_positive_numbers(a_abs, b_abs);
        if (result == NAN_VALUE) {
            return NAN_VALUE;
        }
        return (a_neg != b_neg) ? "-" + result : result;
    }
    case '%': {
        // Both are in canonical form, so a decimal point means a fraction.
        if ((nb == "0") || (na.find('.') != std::string::npos) || (nb.find('.') != std::string::npos)) {
            return NAN_VALUE;
        }
        std::string result = modulo_positive_numbers(a_abs, b_abs);
        if (result == NAN_VALUE) {
            return NAN_VALUE;
        }
        // For modulo, result has same sign as dividend (a)
        return a_neg ? "-" + result : result;
    }
    default:
        return NAN_VALUE;
    }
}
}  // namespace

class number::storage {
public:
    // Holds only the canonical form or NAN_VALUE.
    std::string number{ "0" };

    storage() = default;
    storage(const char* s)
        : number(canonical_text(s))
    {
    }
    storage(const std::string& s)
        : number(canonical_text(s))
    {
    }
};

number::number()
    : _store(std::make_unique<storage>())
{
}

number::number(const number& n)
    : _store(std::make_unique<storage>(*(n._store)))
{
}

number::number(const char* s)
    : _store(std::make_unique<storage>(s))
{
}

number::number(const std::string& s)
    : _store(std::make_unique<storage>(s))
{
}

number::~number() = default;

bool number::is_nan() const
{
    return (_store->number == NAN_VALUE);
}

bool number::is_integer() const
{
    return (! is_nan()) && (_store->number.find('.') == std::string::npos);
}

bool number::equals(const number& n) const
{
    return (compare(n) == std::strong_ordering::equal);
}

std::strong_ordering number::compare(const number& n) const noexcept
{
    // Handle invalid numbers
    const bool valid1 = is_valid_number(_store->number);
    const bool valid2 = is_valid_number(n._store->number);

    // NaN is one value, below every number.
    if ((! valid1) || (! valid2)) {
        if ((! valid1) && (! valid2)) {
            return std::strong_ordering::equal;
        }
        return valid1 ? std::strong_ordering::greater : std::strong_ordering::less;
    }

    // Both are valid numbers
    int cmp = compare_numbers(_store->number, n._store->number);
    if (cmp < 0) {
        return std::strong_ordering::less;
    }
    if (cmp > 0) {
        return std::strong_ordering::greater;
    }
    return std::strong_ordering::equal;
}

bool number::operator==(const number& n) const
{
    return equals(n);
}

bool number::operator!=(const number& n) const
{
    return (! equals(n));
}

std::strong_ordering number::operator<=>(const number& n) const noexcept
{
    return compare(n);
}

number& number::operator=(const number& n)
{
    _store->number = n._store->number;
    return *this;
}

number& number::operator=(const char* s)
{
    _store->number = canonical_text(s);
    return *this;
}

number& number::operator=(const std::string& s)
{
    _store->number = canonical_text(s);
    return *this;
}

number::operator std::string() const
{
    return is_nan() ? "NaN" : _store->number;
}

number::operator int() const
{
    auto parsed = parse_number_unified(_store->number);
    return parsed.to_int();
}

number::operator double() const
{
    auto parsed = parse_number_unified(_store->number);
    auto val = parsed.to_double();
    return val ? *val : 0.0;
}

number::operator long long() const
{
    auto parsed = parse_number_unified(_store->number);
    auto val = parsed.to_long_long();
    return val ? *val : 0LL;
}

// Arithmetic operators
number number::operator+(const number& other) const
{
    std::string result = perform_arithmetic(_store->number, other._store->number, '+');
    return number{ result };
}

number number::operator-(const number& other) const
{
    std::string result = perform_arithmetic(_store->number, other._store->number, '-');
    return number{ result };
}

number number::operator*(const number& other) const
{
    std::string result = perform_arithmetic(_store->number, other._store->number, '*');
    return number{ result };
}

number number::operator/(const number& other) const
{
    std::string result = perform_arithmetic(_store->number, other._store->number, '/');
    return number{ result };
}

number number::operator%(const number& other) const
{
    std::string result = perform_arithmetic(_store->number, other._store->number, '%');
    return number{ result };
}

// Compound assignment operators
number& number::operator+=(const number& other)
{
    *this = *this + other;
    return *this;
}

number& number::operator-=(const number& other)
{
    *this = *this - other;
    return *this;
}

number& number::operator*=(const number& other)
{
    *this = *this * other;
    return *this;
}

number& number::operator/=(const number& other)
{
    *this = *this / other;
    return *this;
}

number& number::operator%=(const number& other)
{
    *this = *this % other;
    return *this;
}

// Static NaN accessor
number number::nan()
{
    return number{ NAN_VALUE };
}

// Stream output operator
std::ostream& operator<<(std::ostream& os, const number& n)
{
    return os << std::string(n);
}

}  // namespace dross
