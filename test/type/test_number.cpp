#include <gtest/gtest.h>

#include "dross/type/number.h"

TEST(number_test, empty_is_not_a_number)
{
    const dross::number n1;
    EXPECT_TRUE(n1.is_nan());
}

TEST(number_test, alphabet_is_not_a_number)
{
    const dross::number n1{ "a" };
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
