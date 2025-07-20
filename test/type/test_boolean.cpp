#include <gtest/gtest.h>

#include "dross/type.h"
#include "dross/type/boolean.h"
#include <string>
#include <sstream>

// =============================================================================
// Basic Construction and Properties
// =============================================================================

TEST(boolean_test, default_constructor)
{
    dross::boolean b;
    EXPECT_FALSE(b.value());
    EXPECT_EQ(b, false);
    EXPECT_EQ(static_cast<std::string>(b), "false");
}

TEST(boolean_test, bool_constructor)
{
    dross::boolean b_true{true};
    dross::boolean b_false{false};

    EXPECT_TRUE(b_true.value());
    EXPECT_FALSE(b_false.value());
    EXPECT_EQ(b_true, true);
    EXPECT_EQ(b_false, false);
}

TEST(boolean_test, int_constructor)
{
    dross::boolean b_zero{0};
    dross::boolean b_one{1};
    dross::boolean b_negative{-42};
    dross::boolean b_large{999999};

    EXPECT_FALSE(b_zero.value());
    EXPECT_TRUE(b_one.value());
    EXPECT_TRUE(b_negative.value());  // Non-zero is true
    EXPECT_TRUE(b_large.value());      // Non-zero is true
}

TEST(boolean_test, string_constructor)
{
    // Case variations
    dross::boolean b_true1{"true"};
    dross::boolean b_true2{"TRUE"};
    dross::boolean b_true3{"True"};
    dross::boolean b_true4{"1"};

    dross::boolean b_false1{"false"};
    dross::boolean b_false2{"FALSE"};
    dross::boolean b_false3{"False"};
    dross::boolean b_false4{"0"};

    // All true variations
    EXPECT_TRUE(b_true1.value());
    EXPECT_TRUE(b_true2.value());
    EXPECT_TRUE(b_true3.value());
    EXPECT_TRUE(b_true4.value());

    // All false variations
    EXPECT_FALSE(b_false1.value());
    EXPECT_FALSE(b_false2.value());
    EXPECT_FALSE(b_false3.value());
    EXPECT_FALSE(b_false4.value());
}

TEST(boolean_test, string_constructor_invalid)
{
    // Invalid strings default to false
    dross::boolean b1{"yes"};
    dross::boolean b2{"no"};
    dross::boolean b3{"invalid"};
    dross::boolean b4{""};
    dross::boolean b5{"2"};

    EXPECT_FALSE(b1.value());
    EXPECT_FALSE(b2.value());
    EXPECT_FALSE(b3.value());
    EXPECT_FALSE(b4.value());
    EXPECT_FALSE(b5.value());
}

TEST(boolean_test, const_char_constructor)
{
    dross::boolean b_true{"true"};
    dross::boolean b_false{"false"};

    EXPECT_TRUE(b_true.value());
    EXPECT_FALSE(b_false.value());
}

TEST(boolean_test, copy_constructor)
{
    dross::boolean original{true};
    dross::boolean copy(original);

    EXPECT_EQ(original, copy);
    EXPECT_TRUE(copy.value());

    // Verify deep copy
    copy = false;
    EXPECT_NE(original, copy);
    EXPECT_TRUE(original.value());
    EXPECT_FALSE(copy.value());
}

// =============================================================================
// Factory Methods
// =============================================================================

TEST(boolean_test, factory_methods)
{
    dross::boolean t = dross::boolean::T();
    dross::boolean f = dross::boolean::F();

    EXPECT_TRUE(t.value());
    EXPECT_FALSE(f.value());
    EXPECT_EQ(t, true);
    EXPECT_EQ(f, false);
}

// =============================================================================
// String Conversion
// =============================================================================

TEST(boolean_test, string_conversion)
{
    dross::boolean b_true{true};
    dross::boolean b_false{false};

    // Implicit string conversion
    std::string s_true = b_true;
    std::string s_false = b_false;
    EXPECT_EQ(s_true, "true");
    EXPECT_EQ(s_false, "false");

    // Explicit string conversion
    EXPECT_EQ(static_cast<std::string>(b_true), "true");
    EXPECT_EQ(static_cast<std::string>(b_false), "false");
}

// =============================================================================
// Equality and Comparison
// =============================================================================

TEST(boolean_test, equals_method)
{
    dross::boolean b1{true};
    dross::boolean b2{true};
    dross::boolean b3{false};

    EXPECT_TRUE(b1.equals(b2));
    EXPECT_FALSE(b1.equals(b3));
    EXPECT_TRUE(b1.equals(true));
    EXPECT_FALSE(b1.equals(false));
    EXPECT_TRUE(b3.equals(false));
    EXPECT_FALSE(b3.equals(true));
}

TEST(boolean_test, equality_operators)
{
    dross::boolean b_true{true};
    dross::boolean b_false{false};

    EXPECT_TRUE(b_true == b_true);
    EXPECT_TRUE(b_false == b_false);
    EXPECT_FALSE(b_true == b_false);
    EXPECT_FALSE(b_false == b_true);

    EXPECT_FALSE(b_true != b_true);
    EXPECT_FALSE(b_false != b_false);
    EXPECT_TRUE(b_true != b_false);
    EXPECT_TRUE(b_false != b_true);
}

TEST(boolean_test, equality_with_bool)
{
    dross::boolean b_true{true};
    dross::boolean b_false{false};

    EXPECT_TRUE(b_true == true);
    EXPECT_FALSE(b_true == false);
    EXPECT_TRUE(b_false == false);
    EXPECT_FALSE(b_false == true);

    EXPECT_FALSE(b_true != true);
    EXPECT_TRUE(b_true != false);
    EXPECT_FALSE(b_false != false);
    EXPECT_TRUE(b_false != true);
}

TEST(boolean_test, three_way_comparison)
{
    dross::boolean b_true{true};
    dross::boolean b_false{false};

    // false < true
    EXPECT_LT(b_false, b_true);
    EXPECT_GT(b_true, b_false);
    EXPECT_LE(b_false, b_true);
    EXPECT_GE(b_true, b_false);

    // Equal values
    EXPECT_GE(b_true, b_true);
    EXPECT_LE(b_true, b_true);
    EXPECT_GE(b_false, b_false);
    EXPECT_LE(b_false, b_false);

    // Spaceship operator
    EXPECT_EQ(b_false <=> b_true, std::strong_ordering::less);
    EXPECT_EQ(b_true <=> b_false, std::strong_ordering::greater);
    EXPECT_EQ(b_true <=> b_true, std::strong_ordering::equal);
    EXPECT_EQ(b_false <=> b_false, std::strong_ordering::equal);
}

// =============================================================================
// Assignment Operations
// =============================================================================

TEST(boolean_test, assignment_operators)
{
    dross::boolean b;

    // Assign from boolean
    dross::boolean b_true{true};
    b = b_true;
    EXPECT_TRUE(b.value());
    EXPECT_EQ(b, b_true);

    // Assign from bool
    b = false;
    EXPECT_FALSE(b.value());
    EXPECT_EQ(b, false);

    b = true;
    EXPECT_TRUE(b.value());
    EXPECT_EQ(b, true);
}

// =============================================================================
// Logical Operations
// =============================================================================

TEST(boolean_test, logical_not)
{
    dross::boolean b_true{true};
    dross::boolean b_false{false};

    EXPECT_EQ(!b_true, false);
    EXPECT_EQ(!b_false, true);
    EXPECT_EQ(!!b_true, true);
    EXPECT_EQ(!!b_false, false);
}

TEST(boolean_test, logical_and)
{
    dross::boolean t{true};
    dross::boolean f{false};

    EXPECT_EQ(t && t, true);
    EXPECT_EQ(t && f, false);
    EXPECT_EQ(f && t, false);
    EXPECT_EQ(f && f, false);
}

TEST(boolean_test, logical_or)
{
    dross::boolean t{true};
    dross::boolean f{false};

    EXPECT_EQ(t || t, true);
    EXPECT_EQ(t || f, true);
    EXPECT_EQ(f || t, true);
    EXPECT_EQ(f || f, false);
}

TEST(boolean_test, complex_logical_expressions)
{
    dross::boolean a{true};
    dross::boolean b{false};
    dross::boolean c{true};

    // (a && b) || c
    EXPECT_EQ((a && b) || c, true);

    // a && (b || c)
    EXPECT_EQ(a && (b || c), true);

    // !(a && b)
    EXPECT_EQ(!(a && b), true);

    // !a || !b
    EXPECT_EQ(!a || !b, true);
}

// =============================================================================
// Type Conversions
// =============================================================================

TEST(boolean_test, bool_conversion)
{
    dross::boolean b_true{true};
    dross::boolean b_false{false};

    // Implicit conversion in conditionals
    if (b_true) {
        SUCCEED();
    } else {
        FAIL() << "True boolean should evaluate to true in conditional";
    }

    if (!b_false) {
        SUCCEED();
    } else {
        FAIL() << "False boolean should evaluate to false in conditional";
    }

    // Explicit conversion
    bool native_true = static_cast<bool>(b_true);
    bool native_false = static_cast<bool>(b_false);

    EXPECT_TRUE(native_true);
    EXPECT_FALSE(native_false);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST(boolean_test, self_assignment)
{
    dross::boolean b{true};
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#endif
    b = b;  // Self-assignment
#ifdef __clang__
#pragma clang diagnostic pop
#endif
    EXPECT_TRUE(b.value());
}

TEST(boolean_test, chain_operations)
{
    dross::boolean a{true};
    dross::boolean b{false};
    dross::boolean c{true};

    // Chain assignments
    a = b = c;
    EXPECT_TRUE(a.value());
    EXPECT_TRUE(b.value());
    EXPECT_TRUE(c.value());
}

// =============================================================================
// Stream Output
// =============================================================================

TEST(boolean_test, stream_output)
{
    dross::boolean b_true{true};
    dross::boolean b_false{false};

    std::ostringstream oss_true;
    oss_true << b_true;
    EXPECT_EQ(oss_true.str(), "true");

    std::ostringstream oss_false;
    oss_false << b_false;
    EXPECT_EQ(oss_false.str(), "false");

    // Multiple values
    std::ostringstream oss_multiple;
    oss_multiple << b_true << " and " << b_false;
    EXPECT_EQ(oss_multiple.str(), "true and false");
}

// =============================================================================
// STL-style to_string Function
// =============================================================================

TEST(boolean_test, stl_style_to_string)
{
    using dross::to_string;  // ADL demonstration

    dross::boolean b_true{true};
    dross::boolean b_false{false};

    // STL-style conversion
    EXPECT_EQ(to_string(b_true), "true");
    EXPECT_EQ(to_string(b_false), "false");

    // With explicit namespace
    EXPECT_EQ(dross::to_string(b_true), "true");
    EXPECT_EQ(dross::to_string(b_false), "false");
}
