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
