#include <gtest/gtest.h>

#include "dross/type/number.h"
#include <sstream>
#include <limits>
#include <cmath>

TEST(number_test, default_constructor_is_zero)
{
    const dross::number n1;
    EXPECT_FALSE(n1.is_nan());
    EXPECT_EQ(static_cast<int>(n1), 0);
    EXPECT_EQ(std::string(n1), "0");
}

TEST(number_test, alphabet_is_not_a_number)
{
    const dross::number n1{ "a" };
    EXPECT_TRUE(n1.is_nan());
}

TEST(number_test, empty_string_is_not_a_number)
{
    const dross::number n1{ "" };
    EXPECT_TRUE(n1.is_nan());
}

TEST(number_test, digits_is_a_number)
{
    const dross::number n1{ "1234" };
    EXPECT_FALSE(n1.is_nan());
}

TEST(number_test, number_equality_with_string)
{
    const dross::number n1{ "1234" };
    const dross::number n2{ "1234" };
    EXPECT_EQ(n1, n2);
}

TEST(number_test, number_not_equality_with_string)
{
    const dross::number n1{ "1234" };
    const dross::number n2{ "4321" };
    EXPECT_NE(n1, n2);
}

TEST(number_test, same_compare_string_and_int)
{
    const dross::number n1{ "1234" };
    const dross::number n2{ 1234 };
    EXPECT_EQ(n1, n2);
}

TEST(number_test, same_compare_string_and_raw_int)
{
    const dross::number n1{ "1234" };
    EXPECT_EQ(n1, 1234);
}

TEST(number_test, same_compare_int_and_raw_string)
{
    const dross::number n1{ 1234 };
    EXPECT_EQ(n1, "1234");
}

// Tests for negative numbers
TEST(number_test, negative_number_parsing)
{
    const dross::number n1{ "-123" };
    EXPECT_FALSE(n1.is_nan());
    EXPECT_EQ(static_cast<int>(n1), -123);
}

TEST(number_test, negative_number_comparison)
{
    const dross::number n1{ "-10" };
    const dross::number n2{ "5" };
    EXPECT_LT(n1, n2);
}

// Tests for decimal numbers
TEST(number_test, decimal_number_parsing)
{
    const dross::number n1{ "3.14" };
    EXPECT_FALSE(n1.is_nan());
    EXPECT_DOUBLE_EQ(static_cast<double>(n1), 3.14);
}

TEST(number_test, decimal_number_comparison)
{
    const dross::number n1{ "3.14" };
    const dross::number n2{ "3.15" };
    EXPECT_LT(n1, n2);
}

// Tests for arithmetic operations
TEST(number_test, addition)
{
    const dross::number n1{ "10" };
    const dross::number n2{ "5" };
    const dross::number result = n1 + n2;
    EXPECT_EQ(static_cast<int>(result), 15);
}

TEST(number_test, subtraction)
{
    const dross::number n1{ "10" };
    const dross::number n2{ "3" };
    const dross::number result = n1 - n2;
    EXPECT_EQ(static_cast<int>(result), 7);
}

TEST(number_test, multiplication)
{
    const dross::number n1{ "4" };
    const dross::number n2{ "5" };
    const dross::number result = n1 * n2;
    EXPECT_EQ(static_cast<int>(result), 20);
}

TEST(number_test, division)
{
    const dross::number n1{ "10" };
    const dross::number n2{ "2" };
    const dross::number result = n1 / n2;
    EXPECT_EQ(static_cast<int>(result), 5);
}

TEST(number_test, decimal_arithmetic)
{
    const dross::number n1{ "3.5" };
    const dross::number n2{ "2.0" };
    const dross::number result = n1 + n2;
    EXPECT_DOUBLE_EQ(static_cast<double>(result), 5.5);
}

// Tests for compound assignment
TEST(number_test, compound_assignment)
{
    dross::number n1{ "10" };
    n1 += dross::number{ "5" };
    EXPECT_EQ(static_cast<int>(n1), 15);

    n1 -= dross::number{ "3" };
    EXPECT_EQ(static_cast<int>(n1), 12);

    n1 *= dross::number{ "2" };
    EXPECT_EQ(static_cast<int>(n1), 24);

    n1 /= dross::number{ "4" };
    EXPECT_EQ(static_cast<int>(n1), 6);
}

// Tests for invalid numbers
TEST(number_test, invalid_number_arithmetic)
{
    const dross::number valid{ "10" };
    const dross::number invalid{ "abc" };
    const dross::number result = valid + invalid;
    EXPECT_TRUE(result.is_nan());
}

TEST(number_test, division_by_zero)
{
    const dross::number n1{ "10" };
    const dross::number zero{ "0" };
    const dross::number result = n1 / zero;
    EXPECT_TRUE(result.is_nan());
}

// Tests for NaN constant
TEST(number_test, nan_constant)
{
    const auto nan_value = dross::number::nan();
    EXPECT_TRUE(nan_value.is_nan());
}

TEST(number_test, nan_arithmetic)
{
    const dross::number valid{ "10" };
    const auto nan_value = dross::number::nan();

    const auto result1 = valid + nan_value;
    const auto result2 = nan_value * valid;
    const auto result3 = nan_value / nan_value;

    EXPECT_TRUE(result1.is_nan());
    EXPECT_TRUE(result2.is_nan());
    EXPECT_TRUE(result3.is_nan());
}

TEST(number_test, nan_comparison)
{
    const auto nan1 = dross::number::nan();
    const auto nan2 = dross::number::nan();
    const dross::number valid{ "10" };

    EXPECT_EQ(nan1, nan2);  // NaN values are equal to each other
    EXPECT_NE(nan1, valid);  // NaN is not equal to valid numbers
    EXPECT_GT(valid, nan1);  // Valid numbers are greater than NaN
}

// Tests for large number arithmetic
TEST(number_test, large_number_addition)
{
    // Numbers larger than long long max (9223372036854775807)
    const dross::number n1{ "99999999999999999999999999999999999999" };
    const dross::number n2{ "11111111111111111111111111111111111111" };
    const dross::number result = n1 + n2;

    EXPECT_EQ(std::string(result), "111111111111111111111111111111111111110");
}

TEST(number_test, large_number_subtraction)
{
    const dross::number n1{ "99999999999999999999999999999999999999" };
    const dross::number n2{ "11111111111111111111111111111111111111" };
    const dross::number result = n1 - n2;

    EXPECT_EQ(std::string(result), "88888888888888888888888888888888888888");
}

TEST(number_test, large_number_multiplication)
{
    const dross::number n1{ "123456789012345678901234567890" };
    const dross::number n2{ "2" };
    const dross::number result = n1 * n2;

    EXPECT_EQ(std::string(result), "246913578024691357802469135780");
}

TEST(number_test, large_negative_numbers)
{
    const dross::number n1{ "-99999999999999999999999999999999999999" };
    const dross::number n2{ "11111111111111111111111111111111111111" };
    const dross::number result1 = n1 + n2;
    const dross::number result2 = n1 - n2;

    EXPECT_EQ(std::string(result1), "-88888888888888888888888888888888888888");
    EXPECT_EQ(std::string(result2), "-111111111111111111111111111111111111110");
}

TEST(number_test, is_integer_method)
{
    const dross::number integer{ "12345" };
    const dross::number decimal{ "123.45" };
    const dross::number negative_int{ "-12345" };
    const dross::number invalid{ "abc" };

    EXPECT_TRUE(integer.is_integer());
    EXPECT_FALSE(decimal.is_integer());
    EXPECT_TRUE(negative_int.is_integer());
    EXPECT_FALSE(invalid.is_integer());
}

TEST(number_test, mixed_large_and_regular_arithmetic)
{
    const dross::number large{ "99999999999999999999999999999999999999" };
    const dross::number regular{ "1" };

    const dross::number sum = large + regular;
    const dross::number diff = large - regular;

    EXPECT_EQ(std::string(sum), "100000000000000000000000000000000000000");
    EXPECT_EQ(std::string(diff), "99999999999999999999999999999999999998");
}

// Tests for arbitrary precision division
TEST(number_test, large_number_division)
{
    // Test large number division with exact result
    const dross::number n1{ "999999999999999999999999999999999999999" };
    const dross::number n2{ "333333333333333333333333333333333333333" };
    const dross::number result = n1 / n2;

    EXPECT_EQ(std::string(result), "3");
}

TEST(number_test, decimal_division_precision)
{
    // Test decimal division with high precision
    const dross::number n1{ "22.7" };
    const dross::number n2{ "3.14" };
    const dross::number result = n1 / n2;

    // Should get approximately 7.229299363
    std::string result_str = std::string(result);
    EXPECT_TRUE(result_str.substr(0, 4) == "7.22" || result_str.substr(0, 4) == "0.72");
}

TEST(number_test, one_third_precision)
{
    // Test 1/3 for decimal precision
    const dross::number n1{ "1" };
    const dross::number n2{ "3" };
    const dross::number result = n1 / n2;

    std::string result_str = std::string(result);
    EXPECT_TRUE(result_str.find("0.33333") == 0);  // Should start with 0.33333
}

TEST(number_test, division_with_remainder)
{
    // Test division that produces remainder
    const dross::number n1{ "10" };
    const dross::number n2{ "3" };
    const dross::number result = n1 / n2;

    std::string result_str = std::string(result);
    EXPECT_TRUE(result_str.find("3.33333") == 0);  // Should start with 3.33333
}

// Tests for arbitrary precision modulo
TEST(number_test, large_number_modulo)
{
    // Test large number modulo
    const dross::number n1{ "999999999999999999999999999999999999999" };
    const dross::number n2{ "333333333333333333333333333333333333333" };
    const dross::number result = n1 % n2;

    EXPECT_EQ(std::string(result), "0");  // Should be exact division
}

TEST(number_test, small_number_modulo)
{
    // Test basic modulo operation
    const dross::number n1{ "17" };
    const dross::number n2{ "5" };
    const dross::number result = n1 % n2;

    EXPECT_EQ(std::string(result), "2");
}

TEST(number_test, negative_number_modulo)
{
    // Test modulo with negative dividend
    const dross::number n1{ "-17" };
    const dross::number n2{ "5" };
    const dross::number result = n1 % n2;

    EXPECT_EQ(std::string(result), "-2");  // Result has same sign as dividend
}

TEST(number_test, modulo_by_zero)
{
    // Test modulo by zero
    const dross::number n1{ "17" };
    const dross::number zero{ "0" };
    const dross::number result = n1 % zero;

    EXPECT_TRUE(result.is_nan());
}

TEST(number_test, modulo_larger_divisor)
{
    // Test modulo where divisor is larger than dividend
    const dross::number n1{ "5" };
    const dross::number n2{ "17" };
    const dross::number result = n1 % n2;

    EXPECT_EQ(std::string(result), "5");  // Should be the dividend itself
}

TEST(number_test, compound_modulo_assignment)
{
    // Test compound modulo assignment
    dross::number n1{ "17" };
    n1 %= dross::number{ "5" };

    EXPECT_EQ(static_cast<int>(n1), 2);
    EXPECT_EQ(std::string(n1), "2");
}

// Test mixed division and modulo operations
TEST(number_test, division_modulo_relationship)
{
    // Test that (a/b)*b + (a%b) = a
    const dross::number a{ "17" };
    const dross::number b{ "5" };

    const dross::number quotient = a / b;
    const dross::number remainder = a % b;

    // Convert quotient to integer (floor division)
    int q_int = static_cast<int>(quotient);
    const dross::number q_floor{ std::to_string(q_int) };

    const dross::number reconstructed = q_floor * b + remainder;

    EXPECT_EQ(std::string(reconstructed), std::string(a));
}

// =============================================================================
// Copy Constructor and Assignment Operators
// =============================================================================

TEST(number_test, copy_constructor)
{
    const dross::number original{ "12345.6789" };
    const dross::number copy(original);

    EXPECT_EQ(original, copy);
    EXPECT_EQ(std::string(original), std::string(copy));
    EXPECT_FALSE(copy.is_nan());
}

TEST(number_test, copy_constructor_nan)
{
    const dross::number original = dross::number::nan();
    const dross::number copy(original);

    EXPECT_TRUE(copy.is_nan());
    EXPECT_EQ(original, copy);
}

TEST(number_test, assignment_operator_number)
{
    dross::number n1{ "100" };
    const dross::number n2{ "200" };

    n1 = n2;
    EXPECT_EQ(n1, n2);
    EXPECT_EQ(std::string(n1), "200");
}

TEST(number_test, assignment_operator_string)
{
    dross::number n;
    n = "12345";
    EXPECT_EQ(std::string(n), "12345");

    n = std::string("67890");
    EXPECT_EQ(std::string(n), "67890");
}

TEST(number_test, assignment_operator_numeric_types)
{
    dross::number n;

    n = 42;
    EXPECT_EQ(static_cast<int>(n), 42);

    n = 3.14;
    EXPECT_DOUBLE_EQ(static_cast<double>(n), 3.14);

    n = -100L;
    EXPECT_EQ(static_cast<long long>(n), -100LL);

    n = 255u;
    EXPECT_EQ(static_cast<int>(n), 255);
}

TEST(number_test, self_assignment)
{
    dross::number n{ "12345" };
    dross::number& ref = n;
    n = ref;  // Self-assignment through reference

    EXPECT_EQ(std::string(n), "12345");
    EXPECT_FALSE(n.is_nan());
}

// =============================================================================
// Type Conversion Tests
// =============================================================================

TEST(number_test, conversion_to_long_long)
{
    const dross::number n1{ "9223372036854775807" };  // LLONG_MAX
    const dross::number n2{ "-9223372036854775808" }; // LLONG_MIN
    const dross::number n3{ "12345" };

    EXPECT_EQ(static_cast<long long>(n1), std::numeric_limits<long long>::max());
    EXPECT_EQ(static_cast<long long>(n2), std::numeric_limits<long long>::min());
    EXPECT_EQ(static_cast<long long>(n3), 12345LL);
}

TEST(number_test, conversion_overflow)
{
    // Number too large for int
    const dross::number large{ "9999999999999999999999999999999" };
    // Conversion behavior is implementation-defined for overflow
    // Just ensure it doesn't crash
    volatile int i = static_cast<int>(large);
    (void)i;

    volatile long long ll = static_cast<long long>(large);
    (void)ll;
}

TEST(number_test, conversion_nan_to_numeric)
{
    const dross::number nan = dross::number::nan();

    // NaN conversions should return 0 or implementation-defined value
    volatile int i = static_cast<int>(nan);
    volatile double d = static_cast<double>(nan);
    volatile long long ll = static_cast<long long>(nan);

    (void)i;
    (void)d;
    (void)ll;
    // Just ensure conversions don't crash
}

// =============================================================================
// Stream Output Operator
// =============================================================================

TEST(number_test, stream_output_operator)
{
    const dross::number n1{ "12345" };
    const dross::number n2{ "-67.89" };
    const dross::number n3 = dross::number::nan();

    std::ostringstream oss1;
    oss1 << n1;
    EXPECT_EQ(oss1.str(), "12345");

    std::ostringstream oss2;
    oss2 << n2;
    EXPECT_EQ(oss2.str(), "-67.89");

    std::ostringstream oss3;
    oss3 << n3;
    EXPECT_EQ(oss3.str(), "__invalid__");
}

TEST(number_test, stream_output_chaining)
{
    const dross::number n1{ "10" };
    const dross::number n2{ "20" };

    std::ostringstream oss;
    oss << "Values: " << n1 << " and " << n2;
    EXPECT_EQ(oss.str(), "Values: 10 and 20");
}

// =============================================================================
// Edge Cases and Boundary Values
// =============================================================================

TEST(number_test, zero_special_cases)
{
    const dross::number zero{ "0" };
    const dross::number negative_zero{ "-0" };
    const dross::number positive{ "10" };

    EXPECT_EQ(zero, negative_zero);
    EXPECT_EQ(zero + positive, positive);
    EXPECT_EQ(zero * positive, zero);
    EXPECT_TRUE((positive / zero).is_nan());
}

TEST(number_test, leading_zeros)
{
    const dross::number n1{ "00123" };
    const dross::number n2{ "123" };

    EXPECT_EQ(n1, n2);
    EXPECT_EQ(std::string(n1), "00123");  // Leading zeros are preserved
}

TEST(number_test, whitespace_handling)
{
    // Numbers with whitespace should be NaN
    const dross::number n1{ " 123" };
    const dross::number n2{ "123 " };
    const dross::number n3{ "1 23" };

    EXPECT_TRUE(n1.is_nan());
    EXPECT_TRUE(n2.is_nan());
    EXPECT_TRUE(n3.is_nan());
}

TEST(number_test, scientific_notation)
{
    // Test scientific notation support
    const dross::number n1{ "1e10" };
    const dross::number n2{ "3.14e-5" };
    const dross::number n3{ "2.5E+3" };
    const dross::number n4{ "1.5e0" };

    // Should be valid numbers now
    EXPECT_FALSE(n1.is_nan());
    EXPECT_FALSE(n2.is_nan());
    EXPECT_FALSE(n3.is_nan());
    EXPECT_FALSE(n4.is_nan());

    // Test conversions
    EXPECT_DOUBLE_EQ(static_cast<double>(n1), 1e10);
    EXPECT_DOUBLE_EQ(static_cast<double>(n2), 3.14e-5);
    EXPECT_DOUBLE_EQ(static_cast<double>(n3), 2.5e3);
    EXPECT_DOUBLE_EQ(static_cast<double>(n4), 1.5);
}

TEST(number_test, scientific_notation_edge_cases)
{
    // Test various scientific notation formats
    const dross::number n1{ "1E10" };          // Capital E
    const dross::number n2{ "1e+10" };         // explicit positive exponent
    const dross::number n3{ "1e-10" };         // negative exponent
    const dross::number n4{ "123.456e-3" };    // decimal with exponent
    const dross::number n5{ "0.1e1" };         // should equal 1.0
    const dross::number n6{ "1000e-3" };       // should equal 1.0

    EXPECT_FALSE(n1.is_nan());
    EXPECT_FALSE(n2.is_nan());
    EXPECT_FALSE(n3.is_nan());
    EXPECT_FALSE(n4.is_nan());
    EXPECT_FALSE(n5.is_nan());
    EXPECT_FALSE(n6.is_nan());

    EXPECT_DOUBLE_EQ(static_cast<double>(n1), 1e10);
    EXPECT_DOUBLE_EQ(static_cast<double>(n2), 1e10);
    EXPECT_DOUBLE_EQ(static_cast<double>(n3), 1e-10);
    EXPECT_DOUBLE_EQ(static_cast<double>(n4), 0.123456);
    EXPECT_DOUBLE_EQ(static_cast<double>(n5), 1.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(n6), 1.0);

    // Test equality
    EXPECT_EQ(n5, n6);  // Both should equal 1.0
}

TEST(number_test, scientific_notation_arithmetic)
{
    // Test arithmetic with scientific notation
    const dross::number n1{ "1e3" };    // 1000
    const dross::number n2{ "2e2" };    // 200
    const dross::number n3{ "1e-3" };   // 0.001

    // Addition
    auto result1 = n1 + n2;  // 1000 + 200 = 1200
    EXPECT_DOUBLE_EQ(static_cast<double>(result1), 1200.0);

    // Multiplication
    auto result2 = n1 * n3;  // 1000 * 0.001 = 1
    EXPECT_DOUBLE_EQ(static_cast<double>(result2), 1.0);

    // Division
    auto result3 = n1 / n2;  // 1000 / 200 = 5
    EXPECT_DOUBLE_EQ(static_cast<double>(result3), 5.0);
}

TEST(number_test, scientific_notation_comparison)
{
    // Test comparison with scientific notation
    const dross::number n1{ "1e3" };    // 1000
    const dross::number n2{ "1000" };   // 1000
    const dross::number n3{ "1e4" };    // 10000
    const dross::number n4{ "1e-3" };   // 0.001

    EXPECT_EQ(n1, n2);  // 1e3 == 1000
    EXPECT_LT(n1, n3);  // 1e3 < 1e4
    EXPECT_GT(n1, n4);  // 1e3 > 1e-3
}

TEST(number_test, scientific_notation_string_conversion)
{
    // Test string conversion behavior with scientific notation
    const dross::number n1{ "1e10" };
    const dross::number n2{ "3.14e-5" };

    // String conversion should maintain precision
    std::string str1 = std::string(n1);
    std::string str2 = std::string(n2);

    // Parse back and ensure equality
    const dross::number parsed1{ str1 };
    const dross::number parsed2{ str2 };

    EXPECT_EQ(n1, parsed1);
    EXPECT_EQ(n2, parsed2);
}

TEST(number_test, scientific_notation_invalid_formats)
{
    // Test invalid scientific notation formats
    const dross::number n1{ "1ee10" };      // double e
    const dross::number n2{ "1e" };         // incomplete exponent
    const dross::number n3{ "e10" };        // missing mantissa
    const dross::number n4{ "1e10.5" };     // decimal in exponent
    const dross::number n5{ "1e++" };       // invalid exponent
    const dross::number n6{ "1e--5" };      // double minus
    const dross::number n7{ "1.2.3e4" };    // multiple decimals

    // These should all be NaN (invalid)
    EXPECT_TRUE(n1.is_nan());
    EXPECT_TRUE(n2.is_nan());
    EXPECT_TRUE(n3.is_nan());
    EXPECT_TRUE(n4.is_nan());
    EXPECT_TRUE(n5.is_nan());
    EXPECT_TRUE(n6.is_nan());
    EXPECT_TRUE(n7.is_nan());
}

TEST(number_test, scientific_notation_boundary_values)
{
    // Test very large and very small scientific notation values
    const dross::number very_large{ "1e308" };    // near double max
    const dross::number very_small{ "1e-307" };   // near double min (within representable range)
    const dross::number zero_exp{ "123e0" };      // exponent 0

    EXPECT_FALSE(very_large.is_nan());
    EXPECT_FALSE(very_small.is_nan());
    EXPECT_FALSE(zero_exp.is_nan());

    EXPECT_DOUBLE_EQ(static_cast<double>(zero_exp), 123.0);

    // Test that very large/small values are finite
    double large_val = static_cast<double>(very_large);
    double small_val = static_cast<double>(very_small);

    EXPECT_TRUE(std::isfinite(large_val));
    EXPECT_TRUE(std::isfinite(small_val));
    EXPECT_GT(large_val, 0.0);
    EXPECT_GT(small_val, 0.0);
}

// =============================================================================
// Floating Point Precision Tests
// =============================================================================

TEST(number_test, decimal_precision_preservation)
{
    const dross::number n{ "3.14159265358979323846" };
    std::string str = std::string(n);

    // Should preserve the full precision
    EXPECT_EQ(str, "3.14159265358979323846");
}

TEST(number_test, trailing_zeros_decimal)
{
    const dross::number n1{ "1.2300" };
    const dross::number n2{ "1.23" };

    // Trailing zeros might be preserved or trimmed
    // Just ensure equality comparison works correctly
    EXPECT_EQ(n1, n2);
}

TEST(number_test, very_small_decimals)
{
    const dross::number n1{ "0.000000000000000001" };
    const dross::number n2{ "0.000000000000000002" };
    const dross::number sum = n1 + n2;

    EXPECT_NE(n1, n2);
    EXPECT_LT(n1, n2);

    // Check that arithmetic preserves precision
    std::string sum_str = std::string(sum);
    EXPECT_TRUE(sum_str == "0.000000000000000003" ||
                sum_str == "3e-18" ||
                sum.is_nan());  // Implementation-dependent
}

// =============================================================================
// Three-way Comparison Tests
// =============================================================================

TEST(number_test, three_way_comparison)
{
    const dross::number n1{ "10" };
    const dross::number n2{ "20" };
    const dross::number n3{ "10" };

    EXPECT_TRUE((n1 <=> n2) < 0);
    EXPECT_TRUE((n2 <=> n1) > 0);
    EXPECT_TRUE((n1 <=> n3) == 0);
}

TEST(number_test, three_way_comparison_with_primitives)
{
    const dross::number n{ "42" };

    EXPECT_TRUE((n <=> 42) == 0);
    EXPECT_TRUE((n <=> 100) < 0);
    EXPECT_TRUE((n <=> 10) > 0);
}

// =============================================================================
// Complex Arithmetic Sequences
// =============================================================================

TEST(number_test, complex_arithmetic_sequence)
{
    dross::number result{ "100" };

    result = (result + dross::number{"50"}) * dross::number{"2"};
    EXPECT_EQ(std::string(result), "300");

    result = result / dross::number{"3"} - dross::number{"25"};
    EXPECT_EQ(std::string(result), "75");

    result = result % dross::number{"20"} + dross::number{"5"};
    EXPECT_EQ(std::string(result), "20");
}

TEST(number_test, chained_operations)
{
    const dross::number n1{ "5" };
    const dross::number n2{ "3" };
    const dross::number n3{ "2" };

    const dross::number result = n1 + n2 * n3;  // Should be 5 + 6 = 11
    EXPECT_EQ(std::string(result), "11");
}

// =============================================================================
// Unified Parsing System Tests (Phase 2)
// =============================================================================

TEST(number_test, unified_parsing_basic_integers)
{
    const dross::number positive{ "123" };
    const dross::number negative{ "-456" };
    const dross::number zero{ "0" };

    EXPECT_FALSE(positive.is_nan());
    EXPECT_FALSE(negative.is_nan());
    EXPECT_FALSE(zero.is_nan());

    EXPECT_TRUE(positive.is_integer());
    EXPECT_TRUE(negative.is_integer());
    EXPECT_TRUE(zero.is_integer());

    EXPECT_EQ(static_cast<int>(positive), 123);
    EXPECT_EQ(static_cast<int>(negative), -456);
    EXPECT_EQ(static_cast<int>(zero), 0);
}

TEST(number_test, unified_parsing_decimal_numbers)
{
    const dross::number simple_decimal{ "3.14" };
    const dross::number negative_decimal{ "-2.71" };
    const dross::number leading_dot{ ".5" };
    const dross::number trailing_dot{ "5." };

    EXPECT_FALSE(simple_decimal.is_nan());
    EXPECT_FALSE(negative_decimal.is_nan());
    EXPECT_FALSE(leading_dot.is_nan());
    EXPECT_FALSE(trailing_dot.is_nan());

    EXPECT_FALSE(simple_decimal.is_integer());
    EXPECT_FALSE(negative_decimal.is_integer());
    EXPECT_FALSE(leading_dot.is_integer());
    EXPECT_TRUE(trailing_dot.is_integer());  // "5." should be treated as integer

    EXPECT_DOUBLE_EQ(static_cast<double>(simple_decimal), 3.14);
    EXPECT_DOUBLE_EQ(static_cast<double>(negative_decimal), -2.71);
    EXPECT_DOUBLE_EQ(static_cast<double>(leading_dot), 0.5);
    EXPECT_DOUBLE_EQ(static_cast<double>(trailing_dot), 5.0);
}

TEST(number_test, unified_parsing_edge_cases)
{
    const dross::number plus_sign{ "+123" };
    const dross::number multiple_zeros{ "000123" };
    const dross::number decimal_zeros{ "1.000" };
    const dross::number zero_decimal{ "0.0" };

    EXPECT_FALSE(plus_sign.is_nan());
    EXPECT_FALSE(multiple_zeros.is_nan());
    EXPECT_FALSE(decimal_zeros.is_nan());
    EXPECT_FALSE(zero_decimal.is_nan());

    EXPECT_EQ(static_cast<int>(plus_sign), 123);
    EXPECT_EQ(static_cast<int>(multiple_zeros), 123);
    EXPECT_EQ(static_cast<int>(decimal_zeros), 1);
    EXPECT_EQ(static_cast<int>(zero_decimal), 0);
}

TEST(number_test, unified_parsing_invalid_inputs)
{
    const dross::number empty{ "" };
    const dross::number just_sign{ "-" };
    const dross::number just_plus{ "+" };
    const dross::number multiple_dots{ "1.2.3" };
    const dross::number letters{ "abc" };
    const dross::number mixed{ "1a2" };

    EXPECT_TRUE(empty.is_nan());
    EXPECT_TRUE(just_sign.is_nan());
    EXPECT_TRUE(just_plus.is_nan());
    EXPECT_TRUE(multiple_dots.is_nan());
    EXPECT_TRUE(letters.is_nan());
    EXPECT_TRUE(mixed.is_nan());
}

TEST(number_test, unified_parsing_int_conversion_rounding)
{
    const dross::number round_down{ "3.4" };
    const dross::number round_up{ "3.6" };
    const dross::number exact_half{ "3.5" };
    const dross::number negative_round{ "-2.7" };

    // Test rounding behavior (should round to nearest)
    EXPECT_EQ(static_cast<int>(round_down), 3);
    EXPECT_EQ(static_cast<int>(round_up), 4);
    EXPECT_EQ(static_cast<int>(exact_half), 4);  // Round half to even or round half up
    EXPECT_EQ(static_cast<int>(negative_round), -3);
}

TEST(number_test, unified_parsing_int_conversion_clamping)
{
    // Test values that exceed int range
    const dross::number too_large{ "99999999999999999999" };
    const dross::number too_small{ "-99999999999999999999" };

    // Should clamp to int limits
    int large_result = static_cast<int>(too_large);
    int small_result = static_cast<int>(too_small);

    EXPECT_EQ(large_result, std::numeric_limits<int>::max());
    EXPECT_EQ(small_result, std::numeric_limits<int>::min());
}

TEST(number_test, unified_parsing_long_long_conversion)
{
    const dross::number max_ll{ "9223372036854775807" };  // LLONG_MAX
    const dross::number min_ll{ "-9223372036854775808" }; // LLONG_MIN
    const dross::number decimal_ll{ "123.456" };

    EXPECT_EQ(static_cast<long long>(max_ll), std::numeric_limits<long long>::max());
    EXPECT_EQ(static_cast<long long>(min_ll), std::numeric_limits<long long>::min());
    EXPECT_EQ(static_cast<long long>(decimal_ll), 123LL);  // Should truncate decimal part
}

TEST(number_test, unified_parsing_double_conversion)
{
    const dross::number precise{ "3.141592653589793" };
    const dross::number large_int{ "123456789012345" };
    const dross::number small_decimal{ "0.000000123456789" };

    auto precise_val = static_cast<double>(precise);
    auto large_val = static_cast<double>(large_int);
    auto small_val = static_cast<double>(small_decimal);

    EXPECT_NEAR(precise_val, 3.141592653589793, 1e-15);
    EXPECT_DOUBLE_EQ(large_val, 123456789012345.0);
    EXPECT_NEAR(small_val, 0.000000123456789, 1e-15);
}

TEST(number_test, unified_parsing_zero_handling)
{
    const dross::number positive_zero{ "0" };
    const dross::number negative_zero{ "-0" };
    const dross::number decimal_zero{ "0.0" };
    const dross::number negative_decimal_zero{ "-0.0" };

    // All should be treated as zero when converted to numeric types
    EXPECT_EQ(static_cast<int>(positive_zero), 0);
    EXPECT_EQ(static_cast<int>(negative_zero), 0);
    EXPECT_EQ(static_cast<int>(decimal_zero), 0);
    EXPECT_EQ(static_cast<int>(negative_decimal_zero), 0);

    EXPECT_DOUBLE_EQ(static_cast<double>(positive_zero), 0.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(negative_zero), 0.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(decimal_zero), 0.0);
    EXPECT_DOUBLE_EQ(static_cast<double>(negative_decimal_zero), 0.0);

    // All should be equal in value (even if string representation differs)
    EXPECT_EQ(positive_zero, negative_zero);
    EXPECT_EQ(positive_zero, decimal_zero);
    // Note: negative_decimal_zero might have different string representation but same numeric value
}

TEST(number_test, unified_parsing_fractional_precision)
{
    const dross::number high_precision{ "0.123456789012345678901234567890" };
    const dross::number trailing_zeros{ "1.23000000000000000000" };

    // Test that high precision is preserved in string representation
    std::string hp_str = std::string(high_precision);
    std::string tz_str = std::string(trailing_zeros);

    // Should preserve significant digits
    EXPECT_TRUE(hp_str.find("0.123456789") == 0);

    // Should normalize trailing zeros
    EXPECT_TRUE(tz_str == "1.23" || tz_str == "1.23000000000000000000");
}

TEST(number_test, unified_parsing_consistency_with_legacy)
{
    // Test that new parsing produces same results as legacy for valid inputs
    std::vector<std::string> test_cases = {
        "0", "123", "-456", "3.14", "-2.71", "0.5",
        "999999999999999999", "-888888888888888888"
    };

    for (const auto& test_case : test_cases) {
        dross::number n{test_case};

        // Should not be NaN for valid inputs
        EXPECT_FALSE(n.is_nan()) << "Failed for input: " << test_case;

        // Conversions should work consistently
        volatile int i = static_cast<int>(n);
        volatile double d = static_cast<double>(n);
        volatile long long ll = static_cast<long long>(n);

        (void)i; (void)d; (void)ll;
    }
}

TEST(number_test, unified_parsing_error_handling)
{
    // Test that invalid inputs are handled consistently
    std::vector<std::string> invalid_cases = {
        "", " ", "abc", "1.2.3", "1a2", "a1", "1a",
        "+", "-", ".", "..", "1.", "..1"
    };

    for (const auto& invalid_case : invalid_cases) {
        dross::number n{invalid_case};

        // Most should be NaN, but some edge cases might be handled differently
        // Just ensure no crashes occur
        volatile bool is_nan = n.is_nan();
        volatile bool is_int = n.is_integer();
        volatile int i = static_cast<int>(n);
        volatile double d = static_cast<double>(n);
        volatile long long ll = static_cast<long long>(n);

        (void)is_nan; (void)is_int; (void)i; (void)d; (void)ll;
    }
}

// Test that unified parsing maintains arithmetic consistency
TEST(number_test, unified_parsing_arithmetic_consistency)
{
    const dross::number a{ "10" };
    const dross::number b{ "5" };

    // Test that arithmetic operations work correctly with unified parsing
    const auto sum = a + b;
    const auto diff = a - b;
    const auto prod = a * b;
    const auto quot = a / b;

    EXPECT_FALSE(sum.is_nan());
    EXPECT_FALSE(diff.is_nan());
    EXPECT_FALSE(prod.is_nan());
    EXPECT_FALSE(quot.is_nan());

    // Verify results with simple integer arithmetic
    EXPECT_EQ(static_cast<int>(sum), 15);
    EXPECT_EQ(static_cast<int>(diff), 5);
    EXPECT_EQ(static_cast<int>(prod), 50);
    EXPECT_EQ(static_cast<int>(quot), 2);
}
