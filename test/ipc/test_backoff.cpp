#include "ipc/backoff.h"

#include <gtest/gtest.h>

#include <chrono>

namespace {

using namespace std::chrono_literals;

TEST(backoff, doubles_from_100_milliseconds_up_to_5_seconds)
{
    dross::backoff waits;
    EXPECT_EQ(waits.next(), 100ms);
    EXPECT_EQ(waits.next(), 200ms);
    EXPECT_EQ(waits.next(), 400ms);
    EXPECT_EQ(waits.next(), 800ms);
    EXPECT_EQ(waits.next(), 1600ms);
    EXPECT_EQ(waits.next(), 3200ms);
    EXPECT_EQ(waits.next(), 5000ms);
    EXPECT_EQ(waits.next(), 5000ms);
}

TEST(backoff, starts_again_from_100_milliseconds_after_a_reset)
{
    dross::backoff waits;
    waits.next();
    waits.next();
    waits.reset();
    EXPECT_EQ(waits.next(), 100ms);
}

}  // namespace
