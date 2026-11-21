#include "dross/ipc/error.h"
#include "ipc/frame.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <initializer_list>
#include <span>
#include <vector>

namespace {

std::vector<std::byte> bytes_of(const std::initializer_list<int> values)
{
    std::vector<std::byte> result;
    for (const int each : values) {
        result.push_back(static_cast<std::byte>(each));
    }
    return result;
}

std::vector<std::byte> framed(const std::vector<std::byte>& message)
{
    auto made = dross::frame_message(message);
    EXPECT_TRUE(made.has_value());
    return made.value_or(std::vector<std::byte>{});
}

std::vector<std::byte> joined(const std::vector<std::byte>& one, const std::vector<std::byte>& other)
{
    std::vector<std::byte> result = one;
    result.insert(result.end(), other.begin(), other.end());
    return result;
}

}  // namespace

TEST(frame_test, the_length_goes_in_front_in_big_endian_order)
{
    EXPECT_EQ(framed(bytes_of({ 7, 8, 9 })), bytes_of({ 0, 0, 0, 3, 7, 8, 9 }));
    const std::vector<std::byte> large(0x1234, std::byte{ 1 });
    const auto head = framed(large);
    ASSERT_EQ(head.size(), 4U + 0x1234U);
    EXPECT_EQ(std::vector<std::byte>(head.begin(), head.begin() + 4), bytes_of({ 0, 0, 0x12, 0x34 }));
}

TEST(frame_test, a_message_of_the_largest_size_is_framed_and_one_byte_more_is_not)
{
    const std::vector<std::byte> largest(dross::max_message_size, std::byte{ 1 });
    EXPECT_TRUE(dross::frame_message(largest).has_value());

    const std::vector<std::byte> over(dross::max_message_size + 1, std::byte{ 1 });
    const auto refused = dross::frame_message(over);
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error(), dross::ipc_errc::message_too_large);
}

TEST(frame_test, a_whole_frame_gives_its_message)
{
    dross::frame_reader reader;
    const auto taken = reader.take(framed(bytes_of({ 7, 8, 9 })));
    ASSERT_TRUE(taken.has_value());
    ASSERT_EQ(taken->size(), 1U);
    EXPECT_EQ((*taken)[0], bytes_of({ 7, 8, 9 }));
    EXPECT_FALSE(reader.holds_partial());
}

TEST(frame_test, an_empty_message_comes_through)
{
    dross::frame_reader reader;
    const auto taken = reader.take(framed({}));
    ASSERT_TRUE(taken.has_value());
    ASSERT_EQ(taken->size(), 1U);
    EXPECT_TRUE((*taken)[0].empty());
}

TEST(frame_test, frames_that_arrive_together_come_out_in_order)
{
    dross::frame_reader reader;
    const auto taken = reader.take(joined(framed(bytes_of({ 1 })), framed(bytes_of({ 2, 3 }))));
    ASSERT_TRUE(taken.has_value());
    ASSERT_EQ(taken->size(), 2U);
    EXPECT_EQ((*taken)[0], bytes_of({ 1 }));
    EXPECT_EQ((*taken)[1], bytes_of({ 2, 3 }));
}

TEST(frame_test, a_frame_that_arrives_a_byte_at_a_time_is_held_until_its_last_byte)
{
    dross::frame_reader reader;
    const auto whole = framed(bytes_of({ 7, 8, 9 }));
    for (std::size_t i = 0; i + 1 < whole.size(); ++i) {
        const auto taken = reader.take(std::span{ whole }.subspan(i, 1));
        ASSERT_TRUE(taken.has_value());
        EXPECT_TRUE(taken->empty());
        EXPECT_TRUE(reader.holds_partial());
    }
    const auto last = reader.take(std::span{ whole }.last(1));
    ASSERT_TRUE(last.has_value());
    ASSERT_EQ(last->size(), 1U);
    EXPECT_EQ((*last)[0], bytes_of({ 7, 8, 9 }));
    EXPECT_FALSE(reader.holds_partial());
}

TEST(frame_test, the_part_of_a_second_frame_waits_for_its_rest)
{
    dross::frame_reader reader;
    const auto stream = joined(framed(bytes_of({ 1 })), framed(bytes_of({ 2, 3 })));
    const auto first = reader.take(std::span{ stream }.first(7));
    ASSERT_TRUE(first.has_value());
    ASSERT_EQ(first->size(), 1U);
    EXPECT_TRUE(reader.holds_partial());

    const auto second = reader.take(std::span{ stream }.subspan(7));
    ASSERT_TRUE(second.has_value());
    ASSERT_EQ(second->size(), 1U);
    EXPECT_EQ((*second)[0], bytes_of({ 2, 3 }));
}

TEST(frame_test, a_message_of_the_largest_size_is_read)
{
    dross::frame_reader reader;
    const std::vector<std::byte> largest(dross::max_message_size, std::byte{ 5 });
    const auto taken = reader.take(framed(largest));
    ASSERT_TRUE(taken.has_value());
    ASSERT_EQ(taken->size(), 1U);
    EXPECT_EQ((*taken)[0], largest);
}

TEST(frame_test, a_length_over_the_largest_fails_before_its_bytes_arrive)
{
    dross::frame_reader reader;
    const auto taken = reader.take(bytes_of({ 0, 1, 0, 1 }));
    ASSERT_FALSE(taken.has_value());
    EXPECT_EQ(taken.error(), dross::ipc_errc::message_too_large);
}

TEST(frame_test, a_length_with_the_top_bit_set_fails)
{
    dross::frame_reader reader;
    const auto taken = reader.take(bytes_of({ 0x80, 0, 0, 0 }));
    ASSERT_FALSE(taken.has_value());
    EXPECT_EQ(taken.error(), dross::ipc_errc::message_too_large);
}

TEST(frame_test, a_reader_that_failed_keeps_failing)
{
    dross::frame_reader reader;
    ASSERT_FALSE(reader.take(bytes_of({ 0xff, 0xff, 0xff, 0xff })).has_value());
    const auto after = reader.take(framed(bytes_of({ 1 })));
    ASSERT_FALSE(after.has_value());
    EXPECT_EQ(after.error(), dross::ipc_errc::message_too_large);
}
