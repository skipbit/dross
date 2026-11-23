#include "dross/type/error.h"
#include "test_support.h"
#include "thread/start_thread.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <system_error>

#if defined(__linux__)
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace {

TEST(start_thread_test, a_started_thread_runs_the_body)
{
    std::atomic<bool> ran{ false };

    auto started = dross::start_thread([&ran]() {
        ran = true;
    });
    ASSERT_TRUE(started);
    started->join();

    EXPECT_TRUE(ran);
}

TEST(start_thread_test, a_failure_asked_for_is_reported_once)
{
    const dross_test::failing_thread_starts failing{ 1 };

    const auto failed = dross::start_thread([]() {
    });
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error(), dross::error(std::errc::resource_unavailable_try_again));

    auto started = dross::start_thread([]() {
    });
    ASSERT_TRUE(started);
    started->join();
}

#if defined(__linux__)
// Exits with 0 when start_thread() reports the failure as asked, 1 when the
// thread started, and 2 for any other error.
[[noreturn]] void start_with_no_room_for_a_thread()
{
    const rlimit none{ 1, 1 };
    if (setrlimit(RLIMIT_NPROC, &none) != 0) {
        std::_Exit(2);
    }
    auto started = dross::start_thread([]() {
    });
    if (started) {
        started->join();
        std::_Exit(1);
    }
    std::_Exit((started.error() == dross::error(std::errc::resource_unavailable_try_again)) ? 0 : 2);
}
#endif

TEST(start_thread_test, a_thread_the_system_will_not_start_is_reported)
{
#if defined(__linux__)
    if (getuid() == 0) {
        GTEST_SKIP() << "the limit on threads does not hold for root";
    }
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_EXIT(start_with_no_room_for_a_thread(), ::testing::ExitedWithCode(0), "");
#else
    GTEST_SKIP() << "the limit on threads is measured on Linux only";
#endif
}

}  // namespace
