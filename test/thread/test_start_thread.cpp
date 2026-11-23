#include "dross/type/error.h"
#include "test_support.h"
#include "thread/start_thread.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <string>
#include <system_error>
#include <thread>

#if defined(__linux__)
#include <fstream>
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

TEST(start_thread_test, a_failure_planned_on_one_thread_leaves_the_others_alone)
{
    const dross_test::failing_thread_starts failing{ 1 };

    std::atomic<bool> started_elsewhere{ false };
    std::thread other{ [&started_elsewhere]() {
        auto started = dross::start_thread([]() {
        });
        if (started) {
            started->join();
            started_elsewhere = true;
        }
    } };
    other.join();
    EXPECT_TRUE(started_elsewhere);

    auto here = dross::start_thread([]() {
    });
    if (here) {
        here->join();
    }
    EXPECT_FALSE(here);
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
// Whether the process holds CAP_SYS_ADMIN or CAP_SYS_RESOURCE, for which
// the system does not hold it to the limit on threads.
bool ignores_the_limit_on_threads()
{
    std::ifstream status{ "/proc/self/status" };
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("CapEff:", 0) == 0) {
            const unsigned long long effective = std::stoull(line.substr(7), nullptr, 16);
            constexpr unsigned long long sys_admin = 1ULL << 21;
            constexpr unsigned long long sys_resource = 1ULL << 24;
            return (effective & (sys_admin | sys_resource)) != 0;
        }
    }
    return false;
}

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
    if (ignores_the_limit_on_threads()) {
        GTEST_SKIP() << "the limit on threads does not hold for a process with CAP_SYS_ADMIN or CAP_SYS_RESOURCE";
    }
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_EXIT(start_with_no_room_for_a_thread(), ::testing::ExitedWithCode(0), "");
#else
    GTEST_SKIP() << "the limit on threads is measured on Linux only";
#endif
}

}  // namespace
