#include <gtest/gtest.h>

#include "dross/type.h"

// =============================================================================
// STL-style to_string Function Tests
// =============================================================================

TEST(to_string_test, boolean_conversion)
{
    using dross::to_string;

    dross::boolean b_true{true};
    dross::boolean b_false{false};

    EXPECT_EQ(to_string(b_true), "true");
    EXPECT_EQ(to_string(b_false), "false");
}

TEST(to_string_test, number_conversion)
{
    using dross::to_string;

    dross::number n1{42};
    dross::number n2{3.14159};
    dross::number n3{"999999999999999999999999999999"};

    EXPECT_EQ(to_string(n1), "42");
    EXPECT_EQ(to_string(n2), "3.141590");
    EXPECT_EQ(to_string(n3), "999999999999999999999999999999");
}

TEST(to_string_test, string_conversion)
{
    using dross::to_string;

    dross::string s1{"Hello"};
    dross::string s2{"世界"};
    dross::string s3{""};

    EXPECT_EQ(to_string(s1), "Hello");
    EXPECT_EQ(to_string(s2), "世界");
    EXPECT_EQ(to_string(s3), "");
}

TEST(to_string_test, datetime_conversion)
{
    using dross::to_string;

    dross::datetime dt1{2024, 1, 21, 15, 30, 45, dross::timezone::offset(9)}; // +09:00
    dross::datetime dt2{2024, 12, 31, 23, 59, 59}; // No timezone
    dross::datetime dt3; // Epoch

    EXPECT_EQ(to_string(dt1), "2024-01-21T15:30:45+09:00");
    EXPECT_EQ(to_string(dt2), "2024-12-31T23:59:59");
    EXPECT_EQ(to_string(dt3), "1970-01-01T00:00:00");
}

TEST(to_string_test, consistency_with_operator)
{
    dross::boolean b{true};
    dross::number n{42.5};
    dross::string s{"test"};
    dross::datetime dt{2024, 6, 15, 12, 30, 0, dross::timezone::offset(2)}; // +02:00

    // to_string() should match operator std::string()
    EXPECT_EQ(dross::to_string(b), static_cast<std::string>(b));
    EXPECT_EQ(dross::to_string(n), static_cast<std::string>(n));
    EXPECT_EQ(dross::to_string(s), static_cast<std::string>(s));
    EXPECT_EQ(dross::to_string(dt), static_cast<std::string>(dt));
}

TEST(to_string_test, adl_lookup)
{
    // Test Argument Dependent Lookup (ADL)
    dross::boolean flag{true};

    // Should work without explicit namespace qualification
    {
        using dross::to_string;
        auto result = to_string(flag);
        EXPECT_EQ(result, "true");
    }

    // Should work with explicit namespace
    auto result = dross::to_string(flag);
    EXPECT_EQ(result, "true");
}

TEST(to_string_test, template_usage)
{
    // Test usage in generic contexts
    auto convert_to_string = [](const auto& value) {
        return dross::to_string(value);
    };

    dross::boolean b{false};
    dross::number n{123};
    dross::string s{"generic"};
    dross::datetime dt{2024, 3, 15, 14, 45, 30};

    EXPECT_EQ(convert_to_string(b), "false");
    EXPECT_EQ(convert_to_string(n), "123");
    EXPECT_EQ(convert_to_string(s), "generic");
    EXPECT_EQ(convert_to_string(dt), "2024-03-15T14:45:30");
}
