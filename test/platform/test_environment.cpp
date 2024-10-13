#include <gtest/gtest.h>

#include "dross/platform/environment.h"

TEST(environment_test, existing_value)
{
    const auto v = dross::environment::value("HOME");
    EXPECT_TRUE(v.has_value());
    EXPECT_EQ(v.value(), getenv("HOME"));
}

TEST(environment_test, value_not_found)
{
    const auto v = dross::environment::value("FOO");
    EXPECT_FALSE(v.has_value());
}
