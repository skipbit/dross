#include "notification/channel_calls.h"

#include <gtest/gtest.h>

namespace {

TEST(channel_calls_test, calls_one_after_another_and_then_closed_are_kept)
{
    dross::channel_calls calls;
    {
        const auto call = calls.received();
    }
    {
        const auto call = calls.received();
    }
    const auto call = calls.closed();
}

TEST(channel_calls_test, a_call_after_closed_stops_the_program)
{
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    dross::channel_calls calls;
    {
        const auto call = calls.closed();
    }
    EXPECT_DEATH((void)calls.received(), "received: called after closed");
    EXPECT_DEATH((void)calls.closed(), "closed: called after closed");
}

TEST(channel_calls_test, a_call_while_another_is_under_way_stops_the_program)
{
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    dross::channel_calls calls;
    const auto call = calls.received();
    EXPECT_DEATH((void)calls.received(), "received: called while another call was under way");
    EXPECT_DEATH((void)calls.closed(), "closed: called while another call was under way");
}

}  // namespace
