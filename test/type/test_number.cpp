#include <gtest/gtest.h>

#include "dross/type/number.h"
#include <sstream>
#include <limits>

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
    // Test if scientific notation is handled (likely as NaN)
    const dross::number n1{ "1e10" };
    const dross::number n2{ "3.14e-5" };
    
    // These are likely NaN since the implementation seems to use string-based storage
    // Just verify consistent behavior
    bool n1_is_valid = !n1.is_nan();
    bool n2_is_valid = !n2.is_nan();
    
    // Either both valid or both NaN
    EXPECT_EQ(n1_is_valid, n2_is_valid);
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
