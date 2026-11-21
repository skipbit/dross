#include "ipc/accept_pause.h"

#include <gtest/gtest.h>

#include <chrono>

namespace {

using namespace std::chrono_literals;

const dross::accept_pause::time_point kStart{};

TEST(accept_pause, is_not_paused_at_first)
{
    const dross::accept_pause waits;
    EXPECT_FALSE(waits.remaining(kStart).has_value());
    EXPECT_FALSE(waits.due(kStart + 1h));
}

TEST(accept_pause, waits_100_milliseconds_at_first)
{
    dross::accept_pause waits;
    waits.pause(kStart);
    EXPECT_EQ(waits.remaining(kStart), 100ms);
    EXPECT_EQ(waits.remaining(kStart + 40ms), 60ms);
    EXPECT_FALSE(waits.due(kStart + 99ms));
    EXPECT_TRUE(waits.due(kStart + 100ms));
}

TEST(accept_pause, rounds_the_time_left_up_to_a_millisecond)
{
    dross::accept_pause waits;
    waits.pause(kStart);
    EXPECT_EQ(waits.remaining(kStart + 99500us), 1ms);
}

TEST(accept_pause, is_never_left_with_a_negative_time)
{
    dross::accept_pause waits;
    waits.pause(kStart);
    EXPECT_EQ(waits.remaining(kStart + 1s), 0ms);
}

TEST(accept_pause, lengthens_the_wait_when_paused_again)
{
    dross::accept_pause waits;
    waits.pause(kStart);
    waits.pause(kStart + 100ms);
    EXPECT_EQ(waits.remaining(kStart + 100ms), 200ms);
}

TEST(accept_pause, ends_when_cleared)
{
    dross::accept_pause waits;
    waits.pause(kStart);
    waits.clear();
    EXPECT_FALSE(waits.remaining(kStart).has_value());
    EXPECT_FALSE(waits.due(kStart + 1h));
}

TEST(accept_pause, starts_again_from_100_milliseconds_once_a_connection_was_accepted)
{
    dross::accept_pause waits;
    waits.pause(kStart);
    waits.pause(kStart);
    waits.accepted();
    waits.pause(kStart);
    EXPECT_EQ(waits.remaining(kStart), 100ms);
}

}  // namespace
