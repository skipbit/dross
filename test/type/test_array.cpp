#include <gtest/gtest.h>

#include "dross/type/array.h"
#include "dross/type/number.h"
#include "dross/type/value.h"

TEST(array_test, default_is_empty)
{
    dross::array a;
    EXPECT_TRUE(a.empty());
}

TEST(array_test, init_with_number_list_and_ranged_for)
{
    dross::array a = { 1, 2, 3 };
    dross::array b;
    for (const auto& n : a) {
        b.append(n);
    }
    EXPECT_EQ(a, b);
}

TEST(array_test, const_number_list_and_ranged_for)
{
    const dross::array a = { 1, 2, 3 };
    dross::array b;
    for (const auto& n : a) {
        b.append(n);
    }
    EXPECT_EQ(a, b);
}
