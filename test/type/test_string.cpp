#include <gtest/gtest.h>

#include "dross/type/string.h"

TEST(string_test, init_with_characters_equals_string)
{
    dross::string s{ "abc" };
    EXPECT_EQ(s, std::string{ "abc" });
}

TEST(string_test, init_with_characters_equals_chars)
{
    dross::string s{ "abc" };
    EXPECT_EQ(s, "abc");
}

TEST(string_test, change_string_contents)
{
    dross::string s = "abc";
    s = "def";
    EXPECT_EQ(s, dross::string("def"));
}

TEST(string_test, adding_content)
{
    dross::string s = "123";
    s += "456";
    EXPECT_EQ(s, "123456");
}

TEST(string_test, starts_with_string)
{
    const dross::string s = "--param";
    EXPECT_TRUE(s.starts_with("-"));
    EXPECT_TRUE(s.starts_with("--"));
    EXPECT_FALSE(s.starts_with("---"));
}
