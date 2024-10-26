#include <gtest/gtest.h>

#include "dross/type.h"

TEST(string_split_test, colon_delimited)
{
    const auto s = "foo:bar:baz";
    const auto v = dross::split(s, ':');
    ASSERT_EQ(v.size(), 3);
    EXPECT_EQ(v[0], "foo");
    EXPECT_EQ(v[1], "bar");
    EXPECT_EQ(v[2], "baz");
}

TEST(concat_test, two_number_vector_concat)
{
    const std::vector<int> a = { 1, 2, 3 };
    const std::vector<int> b = { 4, 5, 6 };
    const auto c = dross::concat(a, b);
    ASSERT_EQ(c, std::vector<int>({ 1, 2, 3, 4, 5, 6 }));
}
