#include "dross/ipc/error.h"
#include "ipc/grpc/stream_state.h"
#include "ipc/limits.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace {

using dross::ipc::stream_state;

// Records what the state starts. It does not call back into the state.
class recording_operations final : public dross::ipc::stream_operations {
public:
    void write(const dross::ipc::grpc_stream::Message& message) override
    {
        writes.push_back(message.data());
    }

    void end() override
    {
        ++ends;
    }

    std::vector<std::string> writes;
    int ends{ 0 };
};

class stream_state_test : public ::testing::Test {
protected:
    std::vector<std::byte> bytes_of(const std::string& text) const
    {
        std::vector<std::byte> out;
        for (const char c : text) {
            out.push_back(static_cast<std::byte>(c));
        }
        return out;
    }

    recording_operations operations;
    const std::shared_ptr<std::atomic<bool>> gone{ std::make_shared<std::atomic<bool>>(false) };
    stream_state state{ operations, gone };
};

TEST_F(stream_state_test, a_message_sent_is_written_and_the_next_waits_for_it)
{
    EXPECT_TRUE(state.send(bytes_of("a")));
    ASSERT_EQ(operations.writes.size(), 1u);
    EXPECT_EQ(operations.writes[0], "a");

    EXPECT_TRUE(state.send(bytes_of("b")));
    EXPECT_EQ(operations.writes.size(), 1u);

    state.written(true);
    ASSERT_EQ(operations.writes.size(), 2u);
    EXPECT_EQ(operations.writes[1], "b");
}

TEST_F(stream_state_test, closing_with_no_write_under_way_ends_the_stream_once)
{
    state.close();
    EXPECT_EQ(operations.ends, 1);

    state.close();
    EXPECT_EQ(operations.ends, 1);

    const auto sent = state.send(bytes_of("a"));
    ASSERT_FALSE(sent);
    EXPECT_EQ(sent.error(), dross::error(std::errc::not_connected));
    EXPECT_TRUE(operations.writes.empty());
}

TEST_F(stream_state_test, closing_during_a_write_ends_the_stream_after_it_and_writes_nothing_more)
{
    EXPECT_TRUE(state.send(bytes_of("a")));
    EXPECT_TRUE(state.send(bytes_of("b")));

    state.close();
    EXPECT_EQ(operations.ends, 0);

    state.written(true);
    EXPECT_EQ(operations.ends, 1);
    EXPECT_EQ(operations.writes.size(), 1u);
}

TEST_F(stream_state_test, a_failed_write_ends_the_stream)
{
    EXPECT_TRUE(state.send(bytes_of("a")));

    state.written(false);
    EXPECT_EQ(operations.ends, 1);

    const auto sent = state.send(bytes_of("b"));
    ASSERT_FALSE(sent);
    EXPECT_EQ(sent.error(), dross::error(std::errc::not_connected));
}

TEST_F(stream_state_test, a_message_larger_than_the_limit_is_refused_and_the_stream_stays_open)
{
    const std::vector<std::byte> large(dross::max_message_size + 1);

    const auto refused = state.send(large);
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error(), dross::ipc_errc::message_too_large);

    EXPECT_TRUE(state.send(bytes_of("a")));
    EXPECT_EQ(operations.ends, 0);
}

TEST_F(stream_state_test, more_waiting_than_the_limit_ends_the_stream_after_the_write_under_way)
{
    EXPECT_TRUE(state.send(bytes_of("a")));

    const std::vector<std::byte> large(dross::max_message_size);
    std::optional<dross::error> failure;
    for (std::size_t sent = 0; ! failure && sent <= dross::max_queued_bytes / dross::max_message_size; ++sent) {
        if (const auto result = state.send(large); ! result) {
            failure = result.error();
        }
    }
    ASSERT_TRUE(failure);
    EXPECT_EQ(*failure, dross::ipc_errc::queue_overflow);
    EXPECT_EQ(operations.ends, 0);

    state.written(true);
    EXPECT_EQ(operations.ends, 1);
    EXPECT_EQ(operations.writes.size(), 1u);

    const auto reason = state.finished();
    ASSERT_TRUE(reason);
    EXPECT_EQ(*reason, dross::ipc_errc::queue_overflow);
}

TEST_F(stream_state_test, a_stream_is_open_until_either_side_ends_it)
{
    EXPECT_TRUE(state.is_open());
    state.close();
    EXPECT_FALSE(state.is_open());

    recording_operations others;
    stream_state ended_by_the_peer{ others, nullptr };
    ended_by_the_peer.peer_ended();
    EXPECT_FALSE(ended_by_the_peer.is_open());
}

TEST_F(stream_state_test, finished_tells_that_this_side_closed)
{
    state.close();

    const auto reason = state.finished();
    ASSERT_TRUE(reason);
    EXPECT_EQ(*reason, dross::error{});
}

TEST_F(stream_state_test, finished_tells_no_reason_when_the_peer_ended_the_stream)
{
    state.peer_ended();
    EXPECT_EQ(operations.ends, 1);

    EXPECT_FALSE(state.finished());
}

TEST_F(stream_state_test, closing_after_finished_starts_nothing)
{
    EXPECT_TRUE(state.send(bytes_of("a")));
    EXPECT_FALSE(state.finished());

    state.close();
    EXPECT_EQ(operations.ends, 0);
}

TEST_F(stream_state_test, sending_fails_once_what_the_stream_belongs_to_is_gone)
{
    gone->store(true);

    const auto sent = state.send(bytes_of("a"));
    ASSERT_FALSE(sent);
    EXPECT_EQ(sent.error(), dross::error(std::errc::not_connected));
    EXPECT_TRUE(operations.writes.empty());
}

}  // namespace
