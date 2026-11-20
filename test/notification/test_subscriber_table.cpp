#include "notification/subscriber_table.h"

#include <gtest/gtest.h>

#include <vector>

namespace {

using ids = std::vector<dross::connection_id>;

TEST(subscriber_table, a_name_added_before_the_connection_joined_is_ignored)
{
    dross::subscriber_table table;
    table.add(1, "a");
    EXPECT_FALSE(table.wanted("a"));
    EXPECT_TRUE(table.receivers("a").empty());
}

TEST(subscriber_table, a_name_added_twice_counts_once)
{
    dross::subscriber_table table;
    table.join(1);
    table.add(1, "a");
    table.add(1, "a");
    table.remove(1, "a");
    EXPECT_FALSE(table.wanted("a"));
}

TEST(subscriber_table, a_name_stays_wanted_until_its_last_subscriber_goes)
{
    dross::subscriber_table table;
    table.join(1);
    table.join(2);
    table.add(1, "a");
    table.add(2, "a");
    table.remove(1, "a");
    EXPECT_TRUE(table.wanted("a"));
    table.remove(2, "a");
    EXPECT_FALSE(table.wanted("a"));
}

TEST(subscriber_table, leaving_drops_all_the_names_and_counts_of_the_connection)
{
    dross::subscriber_table table;
    table.join(1);
    table.join(2);
    table.add(1, "a");
    table.add(1, "b");
    table.add(2, "b");
    table.leave(1);
    EXPECT_FALSE(table.wanted("a"));
    EXPECT_TRUE(table.wanted("b"));
    EXPECT_EQ(table.receivers("b"), ids{ 2 });
    table.leave(2);
    EXPECT_FALSE(table.wanted("b"));
}

TEST(subscriber_table, receivers_are_the_connections_with_that_name)
{
    dross::subscriber_table table;
    table.join(1);
    table.join(2);
    table.join(3);
    table.add(1, "a");
    table.add(2, "b");
    table.add(3, "a");
    EXPECT_EQ(table.receivers("a"), (ids{ 1, 3 }));
    EXPECT_EQ(table.receivers("b"), ids{ 2 });
    EXPECT_TRUE(table.receivers("c").empty());
}

TEST(subscriber_table, removing_an_unknown_name_or_connection_changes_nothing)
{
    dross::subscriber_table table;
    table.join(1);
    table.add(1, "a");
    table.remove(1, "b");
    table.remove(2, "a");
    table.leave(2);
    EXPECT_TRUE(table.wanted("a"));
    EXPECT_EQ(table.receivers("a"), ids{ 1 });
}

}  // namespace
