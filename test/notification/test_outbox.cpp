#include "dross/notification/error.h"
#include "notification/outbox.h"
#include "notification/unix_socket.h"
#include "socket_directory.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

class outbox_test : public dross_test::socket_directory_test {
protected:
    void SetUp() override
    {
        socket_directory_test::SetUp();
        auto listener = dross::unix_listener::listen(dross::path{ at("s") });
        ASSERT_TRUE(listener.has_value());
        auto connected = dross::unix_connect(dross::path{ at("s") });
        ASSERT_TRUE(connected.has_value());
        auto accepted = listener->accept();
        ASSERT_TRUE(accepted.has_value() && accepted->has_value());
        _sending = std::move(*connected);
        _receiving = std::move(**accepted);
    }

    int sending() const noexcept
    {
        return _sending.get();
    }

    // Reads what has arrived, without waiting.
    std::vector<std::byte> drain()
    {
        std::vector<std::byte> result;
        std::vector<std::byte> buffer(65536);
        while (true) {
            const auto got = dross::receive_some(_receiving.get(), buffer);
            if (! got || ! *got || (**got == 0)) {
                return result;
            }
            result.insert(result.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(**got));
        }
    }

    void close_receiving() noexcept
    {
        _receiving = dross::socket_handle{};
    }

private:
    dross::socket_handle _sending;
    dross::socket_handle _receiving;
};

std::vector<std::byte> counting(const std::size_t size, const std::size_t from)
{
    std::vector<std::byte> result(size);
    for (std::size_t i = 0; i < size; ++i) {
        result[i] = static_cast<std::byte>((from + i) % 251);
    }
    return result;
}

}  // namespace

TEST(outbox_limit_test, bytes_up_to_the_limit_are_queued_and_one_more_is_not)
{
    dross::outbox waiting;
    ASSERT_TRUE(waiting.push(std::vector<std::byte>(dross::max_queued_bytes - 1)).has_value());
    ASSERT_TRUE(waiting.push(std::vector<std::byte>(1)).has_value());
    EXPECT_EQ(waiting.size(), dross::max_queued_bytes);

    const auto refused = waiting.push(std::vector<std::byte>(1));
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error(), dross::notification_errc::queue_overflow);
    EXPECT_EQ(waiting.size(), dross::max_queued_bytes);
}

TEST(outbox_limit_test, an_empty_push_queues_nothing)
{
    dross::outbox waiting;
    ASSERT_TRUE(waiting.push({}).has_value());
    EXPECT_TRUE(waiting.empty());
    EXPECT_EQ(waiting.size(), 0U);
}

TEST_F(outbox_test, what_the_socket_does_not_take_is_kept_and_sent_later_in_order)
{
    dross::outbox waiting;
    std::vector<std::byte> expected;
    for (std::size_t i = 0; i < 16; ++i) {
        auto chunk = counting(65536, expected.size());
        expected.insert(expected.end(), chunk.begin(), chunk.end());
        ASSERT_TRUE(waiting.push(std::move(chunk)).has_value());
    }

    ASSERT_TRUE(waiting.flush(sending()).has_value());
    EXPECT_FALSE(waiting.empty());
    EXPECT_LT(waiting.size(), expected.size());

    std::vector<std::byte> received;
    for (int round = 0; (round < 10000) && (received.size() < expected.size()); ++round) {
        ASSERT_TRUE(waiting.flush(sending()).has_value());
        const auto more = drain();
        received.insert(received.end(), more.begin(), more.end());
    }
    EXPECT_TRUE(waiting.empty());
    EXPECT_EQ(waiting.size(), 0U);
    EXPECT_EQ(received, expected);
}

TEST_F(outbox_test, flushing_to_a_peer_that_has_gone_fails)
{
    dross::outbox waiting;
    ASSERT_TRUE(waiting.push(std::vector<std::byte>(16)).has_value());
    close_receiving();
    const auto flushed = waiting.flush(sending());
    ASSERT_FALSE(flushed.has_value());
    EXPECT_EQ(flushed.error(), dross::error(std::errc::broken_pipe));
}
