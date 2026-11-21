#include "dross/ipc/message_acceptor.h"
#include "dross/ipc/message_channel.h"
#include "dross/type/value.h"
#include "notification/publisher.h"
#include "notification/wire.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {

using bytes = std::vector<std::byte>;

// A channel that keeps what is sent on it.
class kept_channel final : public dross::ipc::message_channel {
public:
    std::expected<void, dross::error> send(std::span<const std::byte> message) override
    {
        sent.emplace_back(message.begin(), message.end());
        return {};
    }

    void close() override
    {
        ++closes;
    }

    std::vector<bytes> sent;
    int closes{ 0 };
};

class idle_acceptor final : public dross::ipc::message_acceptor {
};

// Stands in for a part: keeps the events of each listen() and lets the
// test call them.
class fake_place {
public:
    dross::publisher::listen_function listen()
    {
        return [this](std::shared_ptr<dross::ipc::message_acceptor_events> events) -> std::expected<std::unique_ptr<dross::ipc::message_acceptor>, dross::error> {
            ++listens;
            this->events = events;
            if (on_listen) {
                on_listen(*events);
            }
            if (refuse) {
                return std::unexpected(dross::error{ std::errc::address_in_use });
            }
            return std::make_unique<idle_acceptor>();
        };
    }

    std::shared_ptr<dross::ipc::message_acceptor_events> events;
    std::function<void(dross::ipc::message_acceptor_events&)> on_listen;
    int listens{ 0 };
    bool refuse{ false };
};

bytes subscribe(const std::string& name)
{
    return dross::wire::encode(dross::wire::subscribe{ name });
}

bytes unsubscribe(const std::string& name)
{
    return dross::wire::encode(dross::wire::unsubscribe{ name });
}

TEST(publisher_test, a_name_is_wanted_once_a_channel_subscribes_and_until_it_closes)
{
    fake_place place;
    auto made = dross::publisher::listen(place.listen());
    ASSERT_TRUE(made.has_value());
    auto& out = **made;

    auto channel = std::make_shared<kept_channel>();
    auto events = place.events->accepted(channel);
    EXPECT_FALSE(out.wanted("a"));
    events->received(subscribe("a"));
    EXPECT_TRUE(out.wanted("a"));
    events->closed({});
    EXPECT_FALSE(out.wanted("a"));
}

TEST(publisher_test, a_notification_goes_only_to_the_channels_that_subscribed_to_its_name)
{
    fake_place place;
    auto made = dross::publisher::listen(place.listen());
    ASSERT_TRUE(made.has_value());
    auto& out = **made;

    auto wanting = std::make_shared<kept_channel>();
    auto other = std::make_shared<kept_channel>();
    auto wanting_events = place.events->accepted(wanting);
    auto other_events = place.events->accepted(other);
    wanting_events->received(subscribe("a"));
    other_events->received(subscribe("b"));

    out.send("a", dross::value{ 1 });
    ASSERT_EQ(wanting->sent.size(), 1u);
    EXPECT_EQ(wanting->sent[0], dross::wire::encode(dross::wire::notice{ "a", dross::value{ 1 } }));
    EXPECT_TRUE(other->sent.empty());

    wanting_events->received(unsubscribe("a"));
    out.send("a", dross::value{ 2 });
    EXPECT_EQ(wanting->sent.size(), 1u);
}

TEST(publisher_test, a_channel_that_sends_anything_else_is_closed)
{
    fake_place place;
    auto made = dross::publisher::listen(place.listen());
    ASSERT_TRUE(made.has_value());

    auto channel = std::make_shared<kept_channel>();
    auto events = place.events->accepted(channel);
    events->received(dross::wire::encode(dross::wire::notice{ "a", dross::value{ 1 } }));
    EXPECT_EQ(channel->closes, 1);
    events->received(bytes{ std::byte{ 0xff } });
    EXPECT_EQ(channel->closes, 2);
}

TEST(publisher_test, a_channel_accepted_before_listen_returns_is_kept)
{
    fake_place place;
    auto early = std::make_shared<kept_channel>();
    std::shared_ptr<dross::ipc::message_channel_events> early_events;
    place.on_listen = [&](dross::ipc::message_acceptor_events& events) {
        early_events = events.accepted(early);
        early_events->received(subscribe("a"));
    };
    auto made = dross::publisher::listen(place.listen());
    ASSERT_TRUE(made.has_value());
    EXPECT_TRUE((*made)->wanted("a"));
}

TEST(publisher_test, stopping_on_a_failure_listens_again_once)
{
    fake_place place;
    auto made = dross::publisher::listen(place.listen());
    ASSERT_TRUE(made.has_value());
    ASSERT_EQ(place.listens, 1);

    const auto first = place.events;
    first->stopped(dross::error{ std::errc::io_error });
    EXPECT_EQ(place.listens, 2);

    auto channel = std::make_shared<kept_channel>();
    auto events = place.events->accepted(channel);
    events->received(subscribe("a"));
    EXPECT_TRUE((*made)->wanted("a"));
}

TEST(publisher_test, when_listening_again_fails_nothing_more_is_tried)
{
    fake_place place;
    auto made = dross::publisher::listen(place.listen());
    ASSERT_TRUE(made.has_value());

    place.refuse = true;
    place.events->stopped(dross::error{ std::errc::io_error });
    EXPECT_EQ(place.listens, 2);
    (*made)->send("a", dross::value{ 1 });
}

TEST(publisher_test, a_failure_to_listen_is_returned)
{
    fake_place place;
    place.refuse = true;
    const auto made = dross::publisher::listen(place.listen());
    ASSERT_FALSE(made.has_value());
    EXPECT_EQ(made.error(), dross::error(std::errc::address_in_use));
}

TEST(publisher_test, nothing_is_listened_again_once_the_publisher_is_gone)
{
    fake_place place;
    auto made = dross::publisher::listen(place.listen());
    ASSERT_TRUE(made.has_value());
    const auto events = place.events;
    made->reset();
    events->stopped(dross::error{ std::errc::io_error });
    EXPECT_EQ(place.listens, 1);
}

TEST(publisher_test, the_channels_are_let_go_with_the_publisher)
{
    fake_place place;
    auto made = dross::publisher::listen(place.listen());
    ASSERT_TRUE(made.has_value());
    std::weak_ptr<dross::ipc::message_channel> watched;
    std::shared_ptr<dross::ipc::message_channel_events> events;
    {
        auto channel = std::make_shared<kept_channel>();
        watched = channel;
        events = place.events->accepted(channel);
    }
    EXPECT_FALSE(watched.expired());
    made->reset();
    EXPECT_TRUE(watched.expired());
}

TEST(publisher_test, channels_told_from_many_threads_at_once_are_each_kept)
{
    fake_place place;
    auto made = dross::publisher::listen(place.listen());
    ASSERT_TRUE(made.has_value());
    auto& out = **made;

    constexpr int kChannels = 8;
    std::vector<std::shared_ptr<kept_channel>> channels;
    std::vector<std::shared_ptr<dross::ipc::message_channel_events>> events;
    for (int i = 0; i < kChannels; ++i) {
        channels.push_back(std::make_shared<kept_channel>());
        events.push_back(place.events->accepted(channels.back()));
    }
    std::vector<std::thread> parts;
    for (int i = 0; i < kChannels; ++i) {
        parts.emplace_back([&events, i] {
            for (int round = 0; round < 100; ++round) {
                events[i]->received(subscribe("b"));
                events[i]->received(unsubscribe("b"));
            }
            events[i]->received(subscribe("a"));
        });
    }
    for (int round = 0; round < 100; ++round) {
        (void)out.wanted("b");
    }
    for (auto& part : parts) {
        part.join();
    }
    EXPECT_FALSE(out.wanted("b"));
    out.send("a", dross::value{ 1 });
    for (const auto& channel : channels) {
        EXPECT_EQ(channel->sent, std::vector<bytes>{ dross::wire::encode(dross::wire::notice{ "a", dross::value{ 1 } }) });
    }

    parts.clear();
    for (int i = 0; i < kChannels; ++i) {
        parts.emplace_back([&events, i] {
            events[i]->closed({});
        });
    }
    for (auto& part : parts) {
        part.join();
    }
    EXPECT_FALSE(out.wanted("a"));
}

TEST(publisher_test, a_channel_told_after_it_closed_stops_the_program)
{
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    fake_place place;
    auto made = dross::publisher::listen(place.listen());
    ASSERT_TRUE(made.has_value());
    auto events = place.events->accepted(std::make_shared<kept_channel>());
    events->closed({});
    EXPECT_DEATH(events->received(subscribe("a")), "received: called after closed");
}

}  // namespace
