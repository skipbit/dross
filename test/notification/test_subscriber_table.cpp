#include "notification/subscriber_table.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <vector>

namespace {

class quiet_channel final : public dross::ipc::message_channel {
public:
    std::expected<void, dross::error> send(std::span<const std::byte>) override
    {
        return {};
    }

    void close() override
    {
    }
};

using channels = std::vector<dross::subscriber_table::channel>;

class subscriber_table_test : public ::testing::Test {
protected:
    // The receivers of name, in the order of one, two and three.
    channels receivers(const dross::subscriber_table& table, std::string_view name) const
    {
        channels found = table.receivers(name);
        channels ordered;
        for (const auto& each : { one, two, three }) {
            if (std::ranges::find(found, each) != found.end()) {
                ordered.push_back(each);
            }
        }
        EXPECT_EQ(ordered.size(), found.size());
        return ordered;
    }

    const dross::subscriber_table::channel one = std::make_shared<quiet_channel>();
    const dross::subscriber_table::channel two = std::make_shared<quiet_channel>();
    const dross::subscriber_table::channel three = std::make_shared<quiet_channel>();
};

TEST_F(subscriber_table_test, a_name_added_before_the_connection_joined_is_ignored)
{
    dross::subscriber_table table;
    table.add(one, "a");
    EXPECT_FALSE(table.wanted("a"));
    EXPECT_TRUE(table.receivers("a").empty());
}

TEST_F(subscriber_table_test, a_name_added_twice_counts_once)
{
    dross::subscriber_table table;
    table.join(one);
    table.add(one, "a");
    table.add(one, "a");
    table.remove(one, "a");
    EXPECT_FALSE(table.wanted("a"));
}

TEST_F(subscriber_table_test, a_name_stays_wanted_until_its_last_subscriber_goes)
{
    dross::subscriber_table table;
    table.join(one);
    table.join(two);
    table.add(one, "a");
    table.add(two, "a");
    table.remove(one, "a");
    EXPECT_TRUE(table.wanted("a"));
    table.remove(two, "a");
    EXPECT_FALSE(table.wanted("a"));
}

TEST_F(subscriber_table_test, leaving_drops_all_the_names_and_counts_of_the_connection)
{
    dross::subscriber_table table;
    table.join(one);
    table.join(two);
    table.add(one, "a");
    table.add(one, "b");
    table.add(two, "b");
    table.leave(one);
    EXPECT_FALSE(table.wanted("a"));
    EXPECT_TRUE(table.wanted("b"));
    EXPECT_EQ(receivers(table, "b"), channels{ two });
    table.leave(two);
    EXPECT_FALSE(table.wanted("b"));
}

TEST_F(subscriber_table_test, receivers_are_the_connections_with_that_name)
{
    dross::subscriber_table table;
    table.join(one);
    table.join(two);
    table.join(three);
    table.add(one, "a");
    table.add(two, "b");
    table.add(three, "a");
    EXPECT_EQ(receivers(table, "a"), (channels{ one, three }));
    EXPECT_EQ(receivers(table, "b"), channels{ two });
    EXPECT_TRUE(table.receivers("c").empty());
}

TEST_F(subscriber_table_test, removing_an_unknown_name_or_connection_changes_nothing)
{
    dross::subscriber_table table;
    table.join(one);
    table.add(one, "a");
    table.remove(one, "b");
    table.remove(two, "a");
    table.leave(two);
    EXPECT_TRUE(table.wanted("a"));
    EXPECT_EQ(receivers(table, "a"), channels{ one });
}

TEST_F(subscriber_table_test, clearing_drops_every_connection_and_name)
{
    dross::subscriber_table table;
    table.join(one);
    table.join(two);
    table.add(one, "a");
    table.add(two, "b");
    table.clear();
    EXPECT_FALSE(table.wanted("a"));
    EXPECT_FALSE(table.wanted("b"));
    table.add(one, "a");
    EXPECT_FALSE(table.wanted("a"));
}

TEST_F(subscriber_table_test, the_table_holds_the_channels_that_joined_until_they_leave)
{
    dross::subscriber_table table;
    std::weak_ptr<dross::ipc::message_channel> watched;
    {
        auto held = std::make_shared<quiet_channel>();
        watched = held;
        table.join(held);
    }
    EXPECT_FALSE(watched.expired());
    table.leave(watched.lock());
    EXPECT_TRUE(watched.expired());
}

}  // namespace
