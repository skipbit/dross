#include <gtest/gtest.h>

#include "dross/type.h"

#include <list>
#include <deque>

TEST(string_split_test, colon_delimited)
{
    const auto s = "foo:bar:baz";
    const auto v = dross::split(s, ":");
    ASSERT_EQ(v.size(), 3);
    EXPECT_EQ(v[0], "foo");
    EXPECT_EQ(v[1], "bar");
    EXPECT_EQ(v[2], "baz");
}

TEST(string_split_test, empty_string)
{
    const auto v = dross::split("", ":");
    ASSERT_EQ(v.size(), 0);
}

TEST(string_split_test, empty_delimiter)
{
    const auto v = dross::split("abc", "");
    ASSERT_EQ(v.size(), 3);
    EXPECT_EQ(v[0], "a");
    EXPECT_EQ(v[1], "b");
    EXPECT_EQ(v[2], "c");
}

TEST(string_split_test, delimiter_not_found)
{
    const auto v = dross::split("foobar", ":");
    ASSERT_EQ(v.size(), 1);
    EXPECT_EQ(v[0], "foobar");
}

TEST(string_split_test, consecutive_delimiters)
{
    const auto v = dross::split("foo::bar", ":");
    ASSERT_EQ(v.size(), 3);
    EXPECT_EQ(v[0], "foo");
    EXPECT_EQ(v[1], "");
    EXPECT_EQ(v[2], "bar");
}

TEST(string_split_test, delimiter_at_beginning)
{
    const auto v = dross::split(":foo:bar", ":");
    ASSERT_EQ(v.size(), 3);
    EXPECT_EQ(v[0], "");
    EXPECT_EQ(v[1], "foo");
    EXPECT_EQ(v[2], "bar");
}

TEST(string_split_test, delimiter_at_end)
{
    const auto v = dross::split("foo:bar:", ":");
    ASSERT_EQ(v.size(), 3);
    EXPECT_EQ(v[0], "foo");
    EXPECT_EQ(v[1], "bar");
    EXPECT_EQ(v[2], "");
}

TEST(string_split_test, multi_character_delimiter)
{
    const auto v = dross::split("foo::bar::baz", "::");
    ASSERT_EQ(v.size(), 3);
    EXPECT_EQ(v[0], "foo");
    EXPECT_EQ(v[1], "bar");
    EXPECT_EQ(v[2], "baz");
}

TEST(string_split_test, unicode_string_and_delimiter)
{
    const auto v = dross::split("こんにちは→世界→！", "→");
    ASSERT_EQ(v.size(), 3);
    EXPECT_EQ(v[0], "こんにちは");
    EXPECT_EQ(v[1], "世界");
    EXPECT_EQ(v[2], "！");
}

TEST(vector_join_test, comma_delimited)
{
    const std::vector<std::string> v = { "foo", "bar", "baz" };
    const auto s = dross::join(v, ",");
    EXPECT_EQ(s, "foo,bar,baz");
}

TEST(vector_join_test, empty_vector)
{
    const std::vector<std::string> v = {};
    const auto s = dross::join(v, ",");
    EXPECT_EQ(s, "");
}

TEST(vector_join_test, single_element)
{
    const std::vector<std::string> v = { "alone" };
    const auto s = dross::join(v, ",");
    EXPECT_EQ(s, "alone");
}

TEST(vector_join_test, empty_strings_in_vector)
{
    const std::vector<std::string> v = { "foo", "", "bar", "", "baz" };
    const auto s = dross::join(v, "||");
    EXPECT_EQ(s, "foo||||bar||||baz");
}

TEST(vector_join_test, multi_character_delimiter)
{
    const std::vector<std::string> v = { "apple", "banana", "cherry" };
    const auto s = dross::join(v, " and ");
    EXPECT_EQ(s, "apple and banana and cherry");
}

TEST(vector_join_test, unicode_strings_and_delimiter)
{
    const std::vector<std::string> v = { "こんにちは", "世界", "！" };
    const auto s = dross::join(v, "→");
    EXPECT_EQ(s, "こんにちは→世界→！");
}

TEST(concat_test, two_number_vector_concat)
{
    const std::vector<int> a = { 1, 2, 3 };
    const std::vector<int> b = { 4, 5, 6 };
    const auto c = dross::concat(a, b);
    ASSERT_EQ(c, std::vector<int>({ 1, 2, 3, 4, 5, 6 }));
}

TEST(concat_test, three_string_vector_concat)
{
    const std::vector<std::string> a = { "foo", "bar" };
    const std::vector<std::string> b = { "baz", "qux" };
    const std::vector<std::string> c = { "quux", "corge" };
    const auto d = dross::concat(a, b, c);
    ASSERT_EQ(d, std::vector<std::string>({ "foo", "bar", "baz", "qux", "quux", "corge" }));
}

TEST(concat_test, two_number_deque_concat)
{
    const std::deque<int> a = { 1, 2, 3 };
    const std::deque<int> b = { 4, 5, 6 };
    const auto c = dross::concat(a, b);
    ASSERT_EQ(c, std::deque<int>({ 1, 2, 3, 4, 5, 6 }));
}

TEST(concat_test, two_number_list_concat)
{
    const std::list<int> a = { 1, 2, 3 };
    const std::list<int> b = { 4, 5, 6 };
    const auto c = dross::concat(a, b);
    ASSERT_EQ(c, std::list<int>({ 1, 2, 3, 4, 5, 6 }));
}

