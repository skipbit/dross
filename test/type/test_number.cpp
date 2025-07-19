#include <gtest/gtest.h>

#include "dross/type/number.h"

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
