#include "dross/thread/operation.h"
#include "dross/type/error.h"
#include "observed_time_source.h"
#include "test_support.h"
#include "thread/operation_access.h"
#include "thread/time_source.h"

#include <gtest/gtest.h>

#include <any>
#include <atomic>
#include <chrono>
#include <compare>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_set>

namespace {

using dross_test::event;
using dross_test::kTimeout;

dross::operation_result unfinished()
{
    return dross::operation_access::make(dross::time_source::steady());
}

}  // namespace

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

TEST(operation_result_test, reports_not_finished_until_filled_in)
{
    const dross::operation_result result = unfinished();

    EXPECT_FALSE(result.is_finished());
    EXPECT_FALSE(result.wait_for(std::chrono::milliseconds::zero()));
    const auto value = result.get_as<int>();
    ASSERT_FALSE(value.has_value());
    EXPECT_TRUE(value.error() == dross::operation_errc::not_finished);
    const auto nothing = result.get_as<void>();
    ASSERT_FALSE(nothing.has_value());
    EXPECT_TRUE(nothing.error() == dross::operation_errc::not_finished);
}

TEST(operation_result_test, gives_the_value_as_its_own_type_any_number_of_times)
{
    const dross::operation_result result = unfinished();
    dross::operation_access::finish(result, std::make_any<std::string>("answer"));

    EXPECT_TRUE(result.is_finished());
    EXPECT_TRUE(result.wait_for(std::chrono::milliseconds::zero()));
    EXPECT_EQ(result.get_as<std::string>(), "answer");
    EXPECT_EQ(result.get_as<std::string>(), "answer");
}

TEST(operation_result_test, reports_type_mismatch_for_any_other_type)
{
    const dross::operation_result result = unfinished();
    dross::operation_access::finish(result, std::make_any<int>(7));

    const auto as_long = result.get_as<long>();
    ASSERT_FALSE(as_long.has_value());
    EXPECT_TRUE(as_long.error() == dross::operation_errc::type_mismatch);
    const auto as_void = result.get_as<void>();
    ASSERT_FALSE(as_void.has_value());
    EXPECT_TRUE(as_void.error() == dross::operation_errc::type_mismatch);
    EXPECT_EQ(result.get_as<int>(), 7);
}

TEST(operation_result_test, one_that_returned_nothing_is_read_as_void_only)
{
    const dross::operation_result result = unfinished();
    dross::operation_access::finish(result, std::any{});

    EXPECT_TRUE(result.get_as<void>().has_value());
    const auto as_int = result.get_as<int>();
    ASSERT_FALSE(as_int.has_value());
    EXPECT_TRUE(as_int.error() == dross::operation_errc::type_mismatch);
}

TEST(operation_result_test, copies_share_the_result_and_the_id)
{
    const dross::operation_result result = unfinished();
    const dross::operation_result copy = result;
    dross::operation_result assigned = unfinished();
    assigned = result;

    dross::operation_access::finish(result, std::make_any<int>(1));

    EXPECT_TRUE(copy.id() == result.id());
    EXPECT_TRUE(assigned.id() == result.id());
    EXPECT_EQ(copy.get_as<int>(), 1);
    EXPECT_EQ(assigned.get_as<int>(), 1);
}

TEST(operation_result_test, each_result_has_an_id_of_its_own)
{
    EXPECT_FALSE(unfinished().id() == unfinished().id());
}

TEST(operation_result_test, wait_for_returns_once_another_thread_fills_it_in)
{
    const auto source = std::make_shared<dross_test::observed_time_source>();
    const dross::operation_result result = dross::operation_access::make(source);

    // The waiter's own bound is longer than the test waits for it, so only
    // the result being filled in, not its deadline, ends the wait in time.
    std::atomic<bool> finished{ false };
    event returned;
    std::thread waiter{ [result, &finished, &returned]() {
        finished.store(result.wait_for(kTimeout * 2));
        returned.set();
    } };
    // Filled in only once the waiter is waiting, so filling it in is what
    // wakes it.
    const bool saw_wait = source->await_timed_waits(1);
    dross::operation_access::finish(result, std::make_any<int>(3));
    const bool woke = returned.wait();
    waiter.join();

    EXPECT_TRUE(saw_wait);
    EXPECT_TRUE(woke);
    EXPECT_TRUE(finished.load());
    EXPECT_EQ(result.get_as<int>(), 3);
}

TEST(operation_result_test, wait_for_times_out_while_unfinished)
{
    const dross::operation_result result = unfinished();

    EXPECT_FALSE(result.wait_for(std::chrono::milliseconds{ 1 }));
    EXPECT_FALSE(result.is_finished());
}

TEST(operation_result_test, a_cancelled_one_counts_as_finished_and_reports_cancelled)
{
    const dross::operation_result result = unfinished();
    dross::operation_access::cancel(result);

    EXPECT_TRUE(result.is_finished());
    EXPECT_TRUE(result.wait_for(std::chrono::milliseconds::zero()));
    const auto value = result.get_as<int>();
    ASSERT_FALSE(value.has_value());
    EXPECT_TRUE(value.error() == dross::operation_errc::cancelled);
    const auto nothing = result.get_as<void>();
    ASSERT_FALSE(nothing.has_value());
    EXPECT_TRUE(nothing.error() == dross::operation_errc::cancelled);
}

TEST(operation_result_test, wait_for_returns_once_another_thread_cancels_it)
{
    const auto source = std::make_shared<dross_test::observed_time_source>();
    const dross::operation_result result = dross::operation_access::make(source);

    std::atomic<bool> finished{ false };
    event returned;
    std::thread waiter{ [result, &finished, &returned]() {
        finished.store(result.wait_for(kTimeout * 2));
        returned.set();
    } };
    const bool saw_wait = source->await_timed_waits(1);
    dross::operation_access::cancel(result);
    const bool woke = returned.wait();
    waiter.join();

    EXPECT_TRUE(saw_wait);
    EXPECT_TRUE(woke);
    EXPECT_TRUE(finished.load());
}
