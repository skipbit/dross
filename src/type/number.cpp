#include "dross/type/number.h"

#include <compare>
#include <string>
#include <optional>
#include <ostream>

namespace dross {

namespace {
    // Internal NaN representation (implementation detail)
    constexpr const char* NAN_VALUE = "__invalid__";

    // Helper function to parse number string to double
    std::optional<double> parse_number(const std::string& str) {
        if (str.empty()) return std::nullopt;

        try {
            size_t processed = 0;
            double value = std::stod(str, &processed);

            // Check if entire string was processed
            if (processed == str.length()) {
                return value;
            }
        } catch (const std::exception&) {
            // Invalid number format
        }
        return std::nullopt;
    }

    // Check if string represents an integer (no decimal point)
    bool is_integer_string(const std::string& str) {
        if (str.empty()) return false;

        size_t start = 0;
        if (str[0] == '-' || str[0] == '+') {
            if (str.length() == 1) return false;
            start = 1;
        }

        for (size_t i = start; i < str.length(); ++i) {
            if (!std::isdigit(str[i])) {
                return false;
            }
        }
        return true;
    }

    // Check if string represents a valid number (integer or decimal)
    bool is_valid_number(const std::string& str) {
        if (str.empty()) return false;

        size_t start = 0;
        bool has_dot = false;

        if (str[0] == '-' || str[0] == '+') {
            if (str.length() == 1) return false;
            start = 1;
        }

        for (size_t i = start; i < str.length(); ++i) {
            if (str[i] == '.') {
                if (has_dot) return false;  // Multiple dots
                has_dot = true;
            } else if (!std::isdigit(str[i])) {
                return false;
            }
        }

        // Don't allow trailing or leading dot
        if (has_dot && (str[start] == '.' || str.back() == '.')) {
            return str.length() > start + 1;  // Allow ".5" or "5."
        }

        return true;
    }




    // Normalize number string (remove unnecessary zeros, handle decimal point)
    std::string normalize_number(const std::string& str) {
        if (!is_valid_number(str)) return str;

        bool negative = (str[0] == '-');
        std::string s = negative ? str.substr(1) : str;

        // Find decimal point
        size_t dot_pos = s.find('.');
        bool has_decimal = (dot_pos != std::string::npos);

        if (has_decimal) {
            // Remove trailing zeros after decimal point
            size_t last_nonzero = s.find_last_not_of('0');
            if (last_nonzero != std::string::npos && last_nonzero > dot_pos) {
                s = s.substr(0, last_nonzero + 1);
            }
            // Remove decimal point if no fractional part
            if (s.back() == '.') {
                s.pop_back();
                has_decimal = false;
            }
        }

        // Remove leading zeros
        size_t first_nonzero = 0;
        while (first_nonzero < s.length() - 1 && s[first_nonzero] == '0' && s[first_nonzero + 1] != '.') {
            first_nonzero++;
        }
        s = s.substr(first_nonzero);

        // Handle zero
        if (s.empty() || s == "." || s == "0") return "0";

        return negative && s != "0" ? "-" + s : s;
    }

    // Compare two number strings (handles both integers and decimals)
    int compare_numbers(const std::string& a, const std::string& b) {
        std::string na = normalize_number(a);
        std::string nb = normalize_number(b);

        if (na == nb) return 0;

        bool a_neg = (na[0] == '-');
        bool b_neg = (nb[0] == '-');

        // Different signs
        if (a_neg != b_neg) {
            return a_neg ? -1 : 1;
        }

        // Both positive or both negative
        std::string a_abs = a_neg ? na.substr(1) : na;
        std::string b_abs = b_neg ? nb.substr(1) : nb;

        // Split into integer and fractional parts
        size_t a_dot = a_abs.find('.');
        size_t b_dot = b_abs.find('.');

        std::string a_int = (a_dot == std::string::npos) ? a_abs : a_abs.substr(0, a_dot);
        std::string b_int = (b_dot == std::string::npos) ? b_abs : b_abs.substr(0, b_dot);

        std::string a_frac = (a_dot == std::string::npos) ? "" : a_abs.substr(a_dot + 1);
        std::string b_frac = (b_dot == std::string::npos) ? "" : b_abs.substr(b_dot + 1);

        // Compare integer parts (inline comparison)
        int int_cmp = (a_int.length() != b_int.length()) ?
                      (a_int.length() < b_int.length() ? -1 : 1) :
                      a_int.compare(b_int);
        if (int_cmp != 0) {
            return a_neg ? -int_cmp : int_cmp;
        }

        // Integer parts are equal, compare fractional parts
        // Pad with zeros to make same length
        size_t max_frac = std::max(a_frac.length(), b_frac.length());
        a_frac.resize(max_frac, '0');
        b_frac.resize(max_frac, '0');

        int frac_cmp = a_frac.compare(b_frac);
        if (frac_cmp < 0) return a_neg ? 1 : -1;
        if (frac_cmp > 0) return a_neg ? -1 : 1;

        return 0;
    }

    // Parse number into integer and fractional parts
    struct NumberParts {
        bool negative;
        std::string integer;
        std::string fractional;

        NumberParts(const std::string& num) {
            std::string normalized = normalize_number(num);
            negative = (normalized[0] == '-');
            std::string abs_num = negative ? normalized.substr(1) : normalized;

            size_t dot_pos = abs_num.find('.');
            if (dot_pos == std::string::npos) {
                integer = abs_num;
                fractional = "";
            } else {
                integer = abs_num.substr(0, dot_pos);
                fractional = abs_num.substr(dot_pos + 1);
            }

            // Ensure integer part is not empty
            if (integer.empty()) integer = "0";
        }
    };

    // Add two positive number strings (supports decimals)
    std::string add_positive_numbers(const std::string& a, const std::string& b) {
        NumberParts pa(a);
        NumberParts pb(b);

        // Make fractional parts same length
        size_t max_frac = std::max(pa.fractional.length(), pb.fractional.length());
        pa.fractional.resize(max_frac, '0');
        pb.fractional.resize(max_frac, '0');

        // Add fractional parts
        std::string result_frac;
        int carry = 0;
        for (int i = max_frac - 1; i >= 0; i--) {
            int sum = carry + (pa.fractional[i] - '0') + (pb.fractional[i] - '0');
            carry = sum / 10;
            result_frac = char('0' + sum % 10) + result_frac;
        }

        // Add integer parts
        std::string result_int;
        int i = pa.integer.length() - 1;
        int j = pb.integer.length() - 1;

        while (i >= 0 || j >= 0 || carry > 0) {
            int sum = carry;
            if (i >= 0) sum += pa.integer[i--] - '0';
            if (j >= 0) sum += pb.integer[j--] - '0';

            carry = sum / 10;
            result_int = char('0' + sum % 10) + result_int;
        }

        // Combine result
        std::string result = result_int;
        if (!result_frac.empty()) {
            // Remove trailing zeros from fractional part
            while (!result_frac.empty() && result_frac.back() == '0') {
                result_frac.pop_back();
            }
            if (!result_frac.empty()) {
                result += "." + result_frac;
            }
        }

        return normalize_number(result);
    }

    // Subtract two positive number strings (a >= b, supports decimals)
    std::string subtract_positive_numbers(const std::string& a, const std::string& b) {
        NumberParts pa(a);
        NumberParts pb(b);

        // Make fractional parts same length
        size_t max_frac = std::max(pa.fractional.length(), pb.fractional.length());
        pa.fractional.resize(max_frac, '0');
        pb.fractional.resize(max_frac, '0');

        // Subtract fractional parts
        std::string result_frac;
        int borrow = 0;
        for (int i = max_frac - 1; i >= 0; i--) {
            int diff = (pa.fractional[i] - '0') - (pb.fractional[i] - '0') - borrow;
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
        int i = pa.integer.length() - 1;
        int j = pb.integer.length() - 1;

        while (i >= 0) {
            int diff = (pa.integer[i] - '0') - borrow;
            if (j >= 0) diff -= (pb.integer[j--] - '0');

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
        if (!result_frac.empty()) {
            // Remove trailing zeros from fractional part
            while (!result_frac.empty() && result_frac.back() == '0') {
                result_frac.pop_back();
            }
            if (!result_frac.empty()) {
                result += "." + result_frac;
            }
        }

        return normalize_number(result);
    }

    // Multiply two positive number strings (supports decimals)
    std::string multiply_positive_numbers(const std::string& a, const std::string& b) {
        NumberParts pa(a);
        NumberParts pb(b);

        // Convert to pure integers by combining integer and fractional parts
        std::string num_a = pa.integer + pa.fractional;
        std::string num_b = pb.integer + pb.fractional;
        int total_decimal_places = pa.fractional.length() + pb.fractional.length();

        // Multiply as integers
        if (num_a == "0" || num_b == "0") return "0";

        std::string result(num_a.length() + num_b.length(), '0');

        for (int i = num_a.length() - 1; i >= 0; i--) {
            for (int j = num_b.length() - 1; j >= 0; j--) {
                int mul = (num_a[i] - '0') * (num_b[j] - '0');
                int p1 = i + j, p2 = i + j + 1;
                int sum = mul + (result[p2] - '0');

                result[p2] = char('0' + sum % 10);
                result[p1] += sum / 10;
            }
        }

        // Remove leading zeros
        size_t start = 0;
        while (start < result.length() - 1 && result[start] == '0') {
            start++;
        }
        result = result.substr(start);

        // Insert decimal point
        if (total_decimal_places > 0) {
            if (result.length() <= static_cast<size_t>(total_decimal_places)) {
                // Need to pad with leading zeros
                result = std::string(static_cast<size_t>(total_decimal_places) - result.length() + 1, '0') + result;
            }

            size_t decimal_pos = result.length() - static_cast<size_t>(total_decimal_places);
            if (decimal_pos == 0) {
                result = "0." + result;
            } else {
                result = result.substr(0, decimal_pos) + "." + result.substr(decimal_pos);
            }
        }

        return normalize_number(result);
    }

    // Divide two positive number strings (supports decimals) with precision control
    std::string divide_positive_numbers(const std::string& a, const std::string& b, int max_decimal_places = 10) {
        if (b == "0") return NAN_VALUE;

        NumberParts pa(a);
        NumberParts pb(b);

        // Convert to pure integers for division algorithm
        std::string dividend = pa.integer + pa.fractional;
        std::string divisor = pb.integer + pb.fractional;

        // Calculate decimal place adjustment
        int decimal_adjustment = pb.fractional.length() - pa.fractional.length();

        // Remove leading zeros from divisor
        while (divisor.length() > 1 && divisor[0] == '0') {
            divisor = divisor.substr(1);
        }

        if (divisor == "0") return NAN_VALUE;

        // Perform long division
        std::string quotient = "0";
        std::string remainder = "0";

        // Integer division first
        for (char digit : dividend) {
            remainder = remainder + digit;

            // Remove leading zeros from remainder
            while (remainder.length() > 1 && remainder[0] == '0') {
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
        while (quotient.length() > 1 && quotient[0] == '0') {
            quotient = quotient.substr(1);
        }

        // Handle decimal places if needed
        std::string decimal_part = "";
        if (remainder != "0" && max_decimal_places > 0) {
            for (int i = 0; i < max_decimal_places && remainder != "0"; i++) {
                remainder = remainder + "0"; // Add a zero for next decimal place

                int count = 0;
                std::string temp_remainder = remainder;

                while (compare_numbers(temp_remainder, divisor) >= 0) {
                    temp_remainder = subtract_positive_numbers(temp_remainder, divisor);
                    count++;
                }

                decimal_part = decimal_part + std::to_string(count);
                remainder = temp_remainder;
            }
        }

        // Combine integer and decimal parts
        std::string result = quotient;
        if (!decimal_part.empty()) {
            // Remove trailing zeros from decimal part
            while (!decimal_part.empty() && decimal_part.back() == '0') {
                decimal_part.pop_back();
            }
            if (!decimal_part.empty()) {
                result = result + "." + decimal_part;
            }
        }

        // Apply decimal adjustment
        if (decimal_adjustment != 0) {
            NumberParts result_parts(result);
            int new_decimal_places = static_cast<int>(result_parts.fractional.length()) + decimal_adjustment;

            if (new_decimal_places < 0) {
                // Move decimal point left (multiply by power of 10)
                std::string zeros(-new_decimal_places, '0');
                result = result_parts.integer + result_parts.fractional + zeros;
            } else if (new_decimal_places > static_cast<int>(result_parts.fractional.length())) {
                // Add zeros to the right
                std::string additional_zeros(new_decimal_places - result_parts.fractional.length(), '0');
                result = result_parts.integer + "." + result_parts.fractional + additional_zeros;
            } else if (new_decimal_places > 0) {
                // Insert decimal point
                std::string all_digits = result_parts.integer + result_parts.fractional;
                size_t decimal_pos = all_digits.length() - new_decimal_places;
                if (decimal_pos == 0) {
                    result = "0." + all_digits;
                } else {
                    result = all_digits.substr(0, decimal_pos) + "." + all_digits.substr(decimal_pos);
                }
            } else {
                // new_decimal_places == 0, no decimal point needed
                result = result_parts.integer + result_parts.fractional;
            }
        }

        return normalize_number(result);
    }

    // Modulo operation for positive numbers (a % b = a - floor(a/b) * b)
    std::string modulo_positive_numbers(const std::string& a, const std::string& b) {
        if (b == "0") return NAN_VALUE;

        // For modulo, we need integer division (floor division)
        std::string quotient = divide_positive_numbers(a, b, 0); // No decimal places for integer division

        // Handle case where quotient might have decimal (shouldn't happen with precision 0, but safety check)
        size_t dot_pos = quotient.find('.');
        if (dot_pos != std::string::npos) {
            quotient = quotient.substr(0, dot_pos); // Take only integer part
        }

        // Calculate b * quotient
        std::string product = multiply_positive_numbers(b, quotient);

        // Calculate a - (b * quotient)
        std::string result = subtract_positive_numbers(a, product);

        return normalize_number(result);
    }

    // Perform string-based arithmetic
    std::string perform_arithmetic(const std::string& a, const std::string& b, char operation) {
        if (!is_valid_number(a) || !is_valid_number(b)) {
            return NAN_VALUE;
        }

        std::string na = normalize_number(a);
        std::string nb = normalize_number(b);

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
                    if (cmp == 0) return "0";

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
                    if (cmp == 0) return "0";

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
                if (result == "0") return "0";
                return (a_neg != b_neg) ? "-" + result : result;
            }
            case '/': {
                if (nb == "0") return NAN_VALUE;
                std::string result = divide_positive_numbers(a_abs, b_abs);
                if (result == NAN_VALUE) return NAN_VALUE;
                return (a_neg != b_neg) ? "-" + result : result;
            }
            case '%': {
                if (nb == "0") return NAN_VALUE;
                std::string result = modulo_positive_numbers(a_abs, b_abs);
                if (result == NAN_VALUE) return NAN_VALUE;
                // For modulo, result has same sign as dividend (a)
                return a_neg ? "-" + result : result;
            }
            default:
                return NAN_VALUE;
        }
    }
}

class number::storage {
public:
    std::string number{ "0" };

    storage() = default;
    storage(const char* s) : number(s) {}
    storage(const std::string& s) : number(s) {}

    template <number_type T>
    storage(const T& n) : number(std::to_string(n)) {}
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
    return !is_valid_number(_store->number);
}

bool number::is_integer() const
{
    return is_integer_string(_store->number);
}

bool number::equals(const number& n) const
{
    // Both must be valid numbers
    if (!is_valid_number(_store->number) || !is_valid_number(n._store->number)) {
        // If both are invalid, they're equal if strings match
        if (!is_valid_number(_store->number) && !is_valid_number(n._store->number)) {
            return _store->number == n._store->number;
        }
        return false;
    }

    return compare_numbers(_store->number, n._store->number) == 0;
}

std::strong_ordering number::compare(const number& n) const noexcept
{
    // Handle invalid numbers
    const bool valid1 = is_valid_number(_store->number);
    const bool valid2 = is_valid_number(n._store->number);

    if (!valid1 || !valid2) {
        if (!valid1 && !valid2) {
            // Both invalid - compare as strings
            return _store->number <=> n._store->number;
        }
        // Valid numbers are always greater than invalid ones
        return valid1 ? std::strong_ordering::greater : std::strong_ordering::less;
    }

    // Both are valid numbers
    int cmp = compare_numbers(_store->number, n._store->number);
    if (cmp < 0) return std::strong_ordering::less;
    if (cmp > 0) return std::strong_ordering::greater;
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
    _store->number = s;
    return *this;
}

number& number::operator=(const std::string& s)
{
    _store->number = s;
    return *this;
}

number::operator std::string() const
{
    return _store->number;
}

number::operator int() const
{
    auto val = parse_number(_store->number);
    return val ? static_cast<int>(*val) : 0;
}

number::operator double() const
{
    auto val = parse_number(_store->number);
    return val ? *val : 0.0;
}

number::operator long long() const
{
    auto val = parse_number(_store->number);
    return val ? static_cast<long long>(*val) : 0LL;
}

// Arithmetic operators
number number::operator+(const number& other) const
{
    std::string result = perform_arithmetic(_store->number, other._store->number, '+');
    return number{result};
}

number number::operator-(const number& other) const
{
    std::string result = perform_arithmetic(_store->number, other._store->number, '-');
    return number{result};
}

number number::operator*(const number& other) const
{
    std::string result = perform_arithmetic(_store->number, other._store->number, '*');
    return number{result};
}

number number::operator/(const number& other) const
{
    std::string result = perform_arithmetic(_store->number, other._store->number, '/');
    return number{result};
}

number number::operator%(const number& other) const
{
    std::string result = perform_arithmetic(_store->number, other._store->number, '%');
    return number{result};
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
    return number{NAN_VALUE};
}

// Stream output operator
std::ostream& operator<<(std::ostream& os, const number& n)
{
    return os << std::string(n);
}

}
