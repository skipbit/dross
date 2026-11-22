#include "dross/ipc/grpc/grpc_address.h"
#include "dross/ipc/grpc/grpc_target.h"
#include "dross/ipc/message_place.h"
#include "ipc/grpc/free_port.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace {

using namespace std::chrono_literals;

static_assert(dross::ipc::message_listenable_place_type<dross::ipc::grpc_address>);
static_assert(! dross::ipc::message_connectable_place_type<dross::ipc::grpc_address>);
static_assert(dross::ipc::message_connectable_place_type<dross::ipc::grpc_target>);
static_assert(! dross::ipc::message_listenable_place_type<dross::ipc::grpc_target>);

class nothing_told final : public dross::ipc::message_channel_events {
public:
    void received(std::vector<std::byte>) override
    {
    }

    void closed(dross::error) override
    {
    }
};

class nothing_accepted final : public dross::ipc::message_acceptor_events {
public:
    std::shared_ptr<dross::ipc::message_channel_events> accepted(std::shared_ptr<dross::ipc::message_channel>) override
    {
        return std::make_shared<nothing_told>();
    }

    void stopped(dross::error) override
    {
    }
};

TEST(grpc_address_test, the_address_is_kept)
{
    const dross::ipc::grpc_address place{ "127.0.0.1:50051", grpc::InsecureServerCredentials() };
    EXPECT_EQ(place.address(), "127.0.0.1:50051");
}

TEST(grpc_address_test, null_credentials_stop_the_program)
{
    EXPECT_DEATH((void)dross::ipc::grpc_address("127.0.0.1:50051", nullptr), "grpc_address: the credentials are null");
}

TEST(grpc_address_test, a_port_another_listens_at_is_not_shared)
{
    const auto address = dross_test::free_loopback_address();
    ASSERT_FALSE(address.empty());
    const dross::ipc::grpc_address place{ address, grpc::InsecureServerCredentials() };
    const auto first = place.listen(std::make_shared<nothing_accepted>());
    ASSERT_TRUE(first.has_value());

    const auto second = place.listen(std::make_shared<nothing_accepted>());
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error(), std::errc::address_not_available);
}

TEST(grpc_target_test, the_target_is_kept)
{
    const dross::ipc::grpc_target place{ "localhost:50051", grpc::InsecureChannelCredentials() };
    EXPECT_EQ(place.target(), "localhost:50051");
}

TEST(grpc_target_test, null_credentials_stop_the_program)
{
    EXPECT_DEATH((void)dross::ipc::grpc_target("localhost:50051", nullptr), "grpc_target: the credentials are null");
}

TEST(grpc_target_test, a_try_waits_5_seconds_for_a_peer_that_does_not_answer)
{
    const auto address = dross_test::free_loopback_address();
    ASSERT_FALSE(address.empty());
    const dross::ipc::grpc_target place{ address, grpc::InsecureChannelCredentials() };

    const auto start = std::chrono::steady_clock::now();
    const auto channel = place.connector()->connect(std::make_shared<nothing_told>());
    const auto took = std::chrono::steady_clock::now() - start;
    ASSERT_FALSE(channel.has_value());
    EXPECT_EQ(channel.error(), std::errc::timed_out);
    EXPECT_GE(took, 5s);
    EXPECT_LT(took, 10s);
}

TEST(grpc_target_test, a_place_reaches_what_listens_at_the_same_address)
{
    const auto address = dross_test::free_loopback_address();
    ASSERT_FALSE(address.empty());
    const auto acceptor = dross::ipc::grpc_address{ address, grpc::InsecureServerCredentials() }.listen(std::make_shared<nothing_accepted>());
    ASSERT_TRUE(acceptor.has_value());

    const auto channel = dross::ipc::grpc_target{ address, grpc::InsecureChannelCredentials() }.connector()->connect(std::make_shared<nothing_told>());
    EXPECT_TRUE(channel.has_value());
}

}  // namespace
