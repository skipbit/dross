#include <gtest/gtest.h>

#include "dross/type/array.h"
#include "dross/type/value.h"

TEST(value_test, init_with_raw_int)
{
    dross::value v = 1;
    EXPECT_EQ(v, 1);
}

TEST(value_test, init_with_raw_char)
{
    dross::value v = "abc";
    EXPECT_EQ(v, "abc");
}

TEST(value_test, init_with_array)
{
    dross::value v = { 1, 2, 3 };
    EXPECT_EQ(v, dross::array({ 1, 2, 3 }));
}
