#include "dross/thread/operation.h"
#include "dross/type/error.h"
#include "thread/operation_access.h"

#include <gtest/gtest.h>

#include <compare>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_set>

TEST(operation_errc_test, error_carries_the_value_and_the_category)
{
    const dross::error e(dross::operation_errc::type_mismatch);

    EXPECT_TRUE(static_cast<bool>(e));
    EXPECT_EQ(e.code(), 3);
    EXPECT_EQ(&e.category(), &dross::operation_category());
    EXPECT_EQ(e.domain(), "dross.operation");
    EXPECT_TRUE(e == dross::operation_errc::type_mismatch);
    EXPECT_FALSE(e == dross::operation_errc::cancelled);
}

TEST(operation_errc_test, every_value_has_its_own_message)
{
    EXPECT_EQ(dross::error(dross::operation_errc::cancelled).message(), "operation was cancelled");
    EXPECT_EQ(dross::error(dross::operation_errc::not_finished).message(), "operation has not finished");
    EXPECT_EQ(dross::error(dross::operation_errc::type_mismatch).message(), "operation result is not of the requested type");
    EXPECT_EQ(dross::operation_category().message(0), "unknown operation error");
}

TEST(operation_errc_test, category_is_one_object)
{
    EXPECT_EQ(&dross::operation_category(), &dross::operation_category());
    EXPECT_EQ(&dross::make_error_code(dross::operation_errc::cancelled).category(), &dross::operation_category());
}

TEST(operation_id_test, copies_compare_equal_and_later_ids_compare_greater)
{
    const dross::operation_id first = dross::operation_access::next_id();
    const dross::operation_id second = dross::operation_access::next_id();
    const dross::operation_id copy = first;

    EXPECT_TRUE(copy == first);
    EXPECT_EQ(copy <=> first, std::strong_ordering::equal);
    EXPECT_FALSE(first == second);
    EXPECT_TRUE(first != second);
    EXPECT_EQ(first <=> second, std::strong_ordering::less);
    EXPECT_TRUE(second > first);
}

TEST(operation_id_test, serves_as_a_key_of_both_kinds_of_map)
{
    const dross::operation_id first = dross::operation_access::next_id();
    const dross::operation_id second = dross::operation_access::next_id();

    EXPECT_EQ(std::hash<dross::operation_id>{}(first), std::hash<dross::operation_id>{}(dross::operation_id{ first }));

    std::unordered_set<dross::operation_id> unordered{ first, second, first };
    EXPECT_EQ(unordered.size(), 2U);

    std::map<dross::operation_id, int> ordered{ { second, 2 }, { first, 1 } };
    EXPECT_EQ(ordered.begin()->first, first);
}

TEST(operation_id_test, writes_as_a_number)
{
    const dross::operation_id first = dross::operation_access::next_id();
    const dross::operation_id second = dross::operation_access::next_id();

    std::ostringstream one;
    std::ostringstream two;
    one << first;
    two << second;

    EXPECT_FALSE(one.str().empty());
    EXPECT_EQ(one.str().find_first_not_of("0123456789"), std::string::npos);
    EXPECT_NE(one.str(), two.str());
}
