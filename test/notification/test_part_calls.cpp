#include "notification/part_calls.h"

#include <gtest/gtest.h>

namespace {

TEST(part_calls_test, calls_one_after_another_and_then_closed_are_kept)
{
    dross::part_calls calls{ "message_channel_events" };
    {
        const auto call = calls.any("received");
    }
    {
        const auto call = calls.any("received");
    }
    const auto call = calls.last("closed");
}

TEST(part_calls_test, a_call_after_closed_stops_the_program)
{
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    dross::part_calls calls{ "message_channel_events" };
    {
        const auto call = calls.last("closed");
    }
    EXPECT_DEATH((void)calls.any("received"), "message_channel_events::received: called after closed\\(\\)");
    EXPECT_DEATH((void)calls.last("closed"), "closed: called after closed");
}

TEST(part_calls_test, a_call_while_another_is_under_way_stops_the_program)
{
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    dross::part_calls calls{ "message_channel_events" };
    const auto call = calls.any("received");
    EXPECT_DEATH((void)calls.any("received"), "received: called while another call was under way");
    EXPECT_DEATH((void)calls.last("closed"), "closed: called while another call was under way");
}

}  // namespace
