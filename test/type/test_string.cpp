#include <gtest/gtest.h>

#include "dross/type/string.h"
#include <algorithm>
#include <limits>

// =============================================================================
// Basic Construction and Properties
// =============================================================================

TEST(string_test, default_constructor)
{
    dross::string s;
    EXPECT_EQ(s.length(), 0);
    EXPECT_EQ(s, "");
    EXPECT_EQ(s, std::string());
}

TEST(string_test, const_char_constructor)
{
    dross::string s{ "abc" };
    EXPECT_EQ(s, "abc");
    EXPECT_EQ(s.length(), 3);
}

TEST(string_test, std_string_constructor)
{
    std::string stdstr = "hello world";
    dross::string s{ stdstr };
    EXPECT_EQ(s, stdstr);
    EXPECT_EQ(s.length(), 11);
}

TEST(string_test, copy_constructor)
{
    dross::string original{ "test string" };
    dross::string copy(original);

    EXPECT_EQ(original, copy);
    EXPECT_EQ(copy.length(), 11);

    // Verify deep copy
    copy += " modified";
    EXPECT_NE(original, copy);
    EXPECT_EQ(original.length(), 11);
    EXPECT_EQ(copy.length(), 20);
}

TEST(string_test, empty_string_constructor)
{
    dross::string s1{ "" };
    dross::string s2{ std::string() };

    EXPECT_EQ(s1.length(), 0);
    EXPECT_EQ(s2.length(), 0);
    EXPECT_EQ(s1, s2);
}

// =============================================================================
// Equality and Comparison
// =============================================================================

TEST(string_test, equals_method_string)
{
    dross::string s1{ "hello" };
    dross::string s2{ "hello" };
    dross::string s3{ "world" };

    EXPECT_TRUE(s1.equals(s2));
    EXPECT_FALSE(s1.equals(s3));
    EXPECT_TRUE(s1.equals(s1));
}

TEST(string_test, equals_method_std_string)
{
    dross::string s{ "hello" };
    std::string stdstr1{ "hello" };
    std::string stdstr2{ "world" };

    EXPECT_TRUE(s.equals(stdstr1));
    EXPECT_FALSE(s.equals(stdstr2));
}

TEST(string_test, equals_method_const_char)
{
    dross::string s{ "hello" };

    EXPECT_TRUE(s.equals("hello"));
    EXPECT_FALSE(s.equals("world"));
    EXPECT_FALSE(s.equals(""));
}

TEST(string_test, equality_operators_string)
{
    dross::string s1{ "test" };
    dross::string s2{ "test" };
    dross::string s3{ "different" };

    EXPECT_TRUE(s1 == s2);
    EXPECT_FALSE(s1 == s3);
    EXPECT_FALSE(s1 != s2);
    EXPECT_TRUE(s1 != s3);
}

TEST(string_test, equality_operators_std_string)
{
    dross::string s{ "test" };
    std::string stdstr1{ "test" };
    std::string stdstr2{ "different" };

    EXPECT_TRUE(s == stdstr1);
    EXPECT_FALSE(s == stdstr2);
    EXPECT_FALSE(s != stdstr1);
    EXPECT_TRUE(s != stdstr2);
}

TEST(string_test, equality_operators_const_char)
{
    dross::string s{ "test" };

    EXPECT_TRUE(s == "test");
    EXPECT_FALSE(s == "different");
    EXPECT_FALSE(s != "test");
    EXPECT_TRUE(s != "different");
}

// =============================================================================
// Assignment Operators
// =============================================================================

TEST(string_test, assignment_operator_string)
{
    dross::string s1{ "original" };
    dross::string s2{ "new value" };

    s1 = s2;
    EXPECT_EQ(s1, s2);
    EXPECT_EQ(s1, "new value");

    // Verify deep copy
    s2 += " modified";
    EXPECT_NE(s1, s2);
}

TEST(string_test, assignment_operator_std_string)
{
    dross::string s{ "original" };
    std::string stdstr{ "new value" };

    s = stdstr;
    EXPECT_EQ(s, stdstr);
    EXPECT_EQ(s, "new value");
}

TEST(string_test, assignment_operator_const_char)
{
    dross::string s{ "original" };

    s = "new value";
    EXPECT_EQ(s, "new value");
    EXPECT_EQ(s.length(), 9);
}

TEST(string_test, self_assignment)
{
    dross::string s{ "test string" };
    dross::string& ref = s;
    s = ref;  // Self-assignment through reference

    EXPECT_EQ(s, "test string");
    EXPECT_EQ(s.length(), 11);
}

// =============================================================================
// Compound Assignment Operators
// =============================================================================

TEST(string_test, compound_assignment_string)
{
    dross::string s1{ "hello" };
    dross::string s2{ " world" };

    s1 += s2;
    EXPECT_EQ(s1, "hello world");
    EXPECT_EQ(s1.length(), 11);
}

TEST(string_test, compound_assignment_std_string)
{
    dross::string s{ "hello" };
    std::string stdstr{ " world" };

    s += stdstr;
    EXPECT_EQ(s, "hello world");
    EXPECT_EQ(s.length(), 11);
}

TEST(string_test, compound_assignment_const_char)
{
    dross::string s{ "hello" };

    s += " world";
    EXPECT_EQ(s, "hello world");
    EXPECT_EQ(s.length(), 11);
}

TEST(string_test, chained_compound_assignment)
{
    dross::string s{ "a" };

    s += "b";
    s += "c";
    s += "d";

    EXPECT_EQ(s, "abcd");
    EXPECT_EQ(s.length(), 4);
}

TEST(string_test, compound_assignment_empty_string)
{
    dross::string s{ "hello" };

    s += "";
    EXPECT_EQ(s, "hello");
    EXPECT_EQ(s.length(), 5);

    s += std::string();
    EXPECT_EQ(s, "hello");
    EXPECT_EQ(s.length(), 5);
}

// =============================================================================
// Conversion Operators
// =============================================================================

TEST(string_test, std_string_conversion)
{
    dross::string s{ "test conversion" };
    std::string stdstr = s;

    EXPECT_EQ(stdstr, "test conversion");
    EXPECT_EQ(stdstr.length(), 15);
}

TEST(string_test, explicit_std_string_conversion)
{
    dross::string s{ "explicit test" };
    std::string stdstr = static_cast<std::string>(s);

    EXPECT_EQ(stdstr, "explicit test");
}

TEST(string_test, conversion_preserves_content)
{
    const std::string original = "preserve this content!";
    dross::string s{ original };
    std::string converted = s;

    EXPECT_EQ(original, converted);
}

// =============================================================================
// String Methods
// =============================================================================

TEST(string_test, starts_with_basic)
{
    const dross::string s = "--param";
    EXPECT_TRUE(s.starts_with("-"));
    EXPECT_TRUE(s.starts_with("--"));
    EXPECT_TRUE(s.starts_with("--param"));
    EXPECT_FALSE(s.starts_with("---"));
    EXPECT_FALSE(s.starts_with("param"));
}

TEST(string_test, starts_with_empty_string)
{
    const dross::string s = "hello";
    EXPECT_TRUE(s.starts_with(""));  // Empty string should match

    const dross::string empty;
    EXPECT_TRUE(empty.starts_with(""));
    EXPECT_FALSE(empty.starts_with("a"));
}

TEST(string_test, starts_with_longer_prefix)
{
    const dross::string s = "short";
    EXPECT_FALSE(s.starts_with("longer than the string"));
}

TEST(string_test, starts_with_exact_match)
{
    const dross::string s = "exact";
    EXPECT_TRUE(s.starts_with("exact"));
}

TEST(string_test, length_method)
{
    EXPECT_EQ(dross::string().length(), 0);
    EXPECT_EQ(dross::string("").length(), 0);
    EXPECT_EQ(dross::string("a").length(), 1);
    EXPECT_EQ(dross::string("hello").length(), 5);
    EXPECT_EQ(dross::string("hello world!").length(), 12);
}

// =============================================================================
// Unicode and Special Characters
// =============================================================================

TEST(string_test, unicode_characters)
{
    dross::string s{ "Hello, 世界! 🌍" };

    EXPECT_TRUE(s.length() > 0);
    EXPECT_EQ(s, "Hello, 世界! 🌍");

    std::string converted = s;
    EXPECT_EQ(converted, "Hello, 世界! 🌍");
}

TEST(string_test, special_characters)
{
    dross::string s{ "\n\t\r\\\"" };

    EXPECT_EQ(s.length(), 5);
    EXPECT_EQ(s, "\n\t\r\\\"");
}

TEST(string_test, null_character)
{
    std::string str_with_null = "hello";
    str_with_null += '\0';
    str_with_null += "world";

    dross::string s{ str_with_null };

    EXPECT_EQ(s.length(), str_with_null.length());
    EXPECT_EQ(std::string(s), str_with_null);
}

// =============================================================================
// Edge Cases and Boundary Values
// =============================================================================

TEST(string_test, very_long_string)
{
    std::string long_str(10000, 'A');
    dross::string s{ long_str };

    EXPECT_EQ(s.length(), 10000);
    EXPECT_EQ(std::string(s), long_str);
    EXPECT_TRUE(s.starts_with("AAA"));
}

TEST(string_test, repeated_operations)
{
    dross::string s;

    for (int i = 0; i < 1000; ++i) {
        s += "a";
    }

    EXPECT_EQ(s.length(), 1000);
    EXPECT_TRUE(s.starts_with("aaa"));

    std::string converted = s;
    EXPECT_EQ(converted.length(), 1000);
    EXPECT_TRUE(std::all_of(converted.begin(), converted.end(), [](char c) {
        return c == 'a';
    }));
}

TEST(string_test, assignment_to_empty)
{
    dross::string s{ "not empty" };

    s = "";
    EXPECT_EQ(s.length(), 0);
    EXPECT_EQ(s, "");

    s = std::string();
    EXPECT_EQ(s.length(), 0);
    EXPECT_EQ(s, "");
}

TEST(string_test, compound_assignment_to_empty)
{
    dross::string s;

    s += "first";
    EXPECT_EQ(s, "first");

    s += " second";
    EXPECT_EQ(s, "first second");
}

// =============================================================================
// Copy Behavior Verification
// =============================================================================

TEST(string_test, deep_copy_verification)
{
    dross::string original{ "original content" };
    dross::string copy = original;

    // Modify copy
    copy += " modified";

    // Original should be unchanged
    EXPECT_EQ(original, "original content");
    EXPECT_EQ(copy, "original content modified");
    EXPECT_NE(original, copy);
}

TEST(string_test, assignment_chain)
{
    dross::string s1{ "first" };
    dross::string s2{ "second" };
    dross::string s3{ "third" };

    s1 = s2 = s3;

    EXPECT_EQ(s1, "third");
    EXPECT_EQ(s2, "third");
    EXPECT_EQ(s3, "third");

    // Verify independence after assignment
    s1 += " modified";
    EXPECT_EQ(s1, "third modified");
    EXPECT_EQ(s2, "third");
    EXPECT_EQ(s3, "third");
}

// =============================================================================
// Performance and Stress Tests
// =============================================================================

TEST(string_test, large_concatenation)
{
    dross::string s;
    const std::string chunk = "0123456789";

    for (int i = 0; i < 100; ++i) {
        s += chunk;
    }

    EXPECT_EQ(s.length(), 1000);
    EXPECT_TRUE(s.starts_with("012345"));

    std::string converted = s;
    EXPECT_EQ(converted.length(), 1000);
}

TEST(string_test, many_assignments)
{
    dross::string s;

    for (int i = 0; i < 100; ++i) {
        s = std::to_string(i);
        EXPECT_EQ(s, std::to_string(i));
    }

    EXPECT_EQ(s, "99");
}

TEST(string_test, mixed_operations)
{
    dross::string s{ "base" };

    s += " + ";
    dross::string& ref = s;
    s = ref;  // Self assignment through reference
    s += "more";

    dross::string copy = s;
    copy += " + copy";

    EXPECT_EQ(s, "base + more");
    EXPECT_EQ(copy, "base + more + copy");
    EXPECT_NE(s, copy);
}
