// Replaces operator new to count allocations, which would reach every test
// in the program, so this file is a test executable of its own.
#include "dross/format/toml.h"
#include "dross/type/array.h"
#include "dross/type/dictionary.h"
#include "dross/type/number.h"
#include "dross/type/string.h"
#include "dross/type/timestamp.h"
#include "dross/type/value.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <new>
#include <string>
#include <utility>

namespace {

std::atomic<long> allocations{ 0 };

template <typename Function>
long count_allocations(Function function)
{
    const long before = allocations.load();
    function();
    return allocations.load() - before;
}

template <typename Function>
void expect_no_allocation(const char* what, Function function)
{
    EXPECT_EQ(count_allocations(function), 0) << what;
}

}  // namespace

void* operator new(std::size_t size)
{
    allocations.fetch_add(1);
    if (void* p = std::malloc((size != 0) ? size : 1)) {
        return p;
    }
    throw std::bad_alloc();
}

// The nothrow forms are replaced too, so that memory is never freed by an
// operator delete from another allocator.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    allocations.fetch_add(1);
    return std::malloc((size != 0) ? size : 1);
}

void operator delete(void* p) noexcept
{
    std::free(p);
}

void operator delete(void* p, std::size_t) noexcept
{
    std::free(p);
}

void operator delete(void* p, const std::nothrow_t&) noexcept
{
    std::free(p);
}

// A timestamp holds its date, time and timezone directly.
TEST(timestamp_allocation_test, building_and_copying_allocate_nothing)
{
    const auto tz = dross::timezone::offset(9);
    const dross::timestamp original(2024, 1, 21, 15, 30, 0, tz);
    const std::chrono::system_clock::time_point tp = original;

    expect_no_allocation("default", [&] {
        const dross::timestamp built;
    });
    expect_no_allocation("components", [&] {
        const dross::timestamp built(2024, 1, 21, 15, 30, 0, tz);
    });
    expect_no_allocation("from_components", [&] {
        (void)dross::timestamp::from_components(2024, 1, 21, 15, 30, 0, tz);
    });
    expect_no_allocation("time point", [&] {
        const dross::timestamp built(tp);
    });
    expect_no_allocation("from_time_point", [&] {
        (void)dross::timestamp::from_time_point(tp);
    });
    expect_no_allocation("now", [&] {
        (void)dross::timestamp::now();
    });
    expect_no_allocation("add", [&] {
        (void)original.add(std::chrono::seconds(1));
    });
    expect_no_allocation("subtract", [&] {
        (void)original.subtract(std::chrono::seconds(1));
    });
    expect_no_allocation("arithmetic operators", [&] {
        (void)(original + std::chrono::seconds(1));
        (void)(original - std::chrono::seconds(1));
        (void)(original + std::chrono::minutes(1));
        (void)(original - std::chrono::minutes(1));
        (void)(original + std::chrono::hours(1));
        (void)(original - std::chrono::hours(1));
    });
    expect_no_allocation("rejected components", [&] {
        const dross::timestamp built(2024, 13, 1);
    });
    expect_no_allocation("time point limits", [&] {
        const dross::timestamp latest(std::chrono::system_clock::time_point::max());
        const dross::timestamp earliest(std::chrono::system_clock::time_point::min());
    });
    expect_no_allocation("copy and move", [&] {
        dross::timestamp copied(original);
        dross::timestamp moved(std::move(copied));
        copied = moved;
        moved = std::move(copied);
    });
}

TEST(timestamp_allocation_test, copying_the_parts_allocates_nothing)
{
    const dross::timestamp original(2024, 1, 21, 15, 30, 0, dross::timezone::offset(9));

    expect_no_allocation("date", [&] {
        auto date = original.date();
        date = original.date();
    });
    expect_no_allocation("time", [&] {
        auto time = original.time();
        time = original.time();
    });
    expect_no_allocation("timezone", [&] {
        auto tz = original.timezone();
        tz = dross::timezone::utc();
        tz = dross::timezone::offset(9);
    });
}

// Moving hands the source one new, empty storage, whatever it held.
TEST(dictionary_allocation_test, moving_allocates_at_most_once)
{
    dross::dictionary original;
    for (int i = 0; i < 10; ++i) {
        original[std::to_string(i)] = dross::value(dross::number(i));
    }

    dross::dictionary source(original);
    const long construction = count_allocations([&] {
        const dross::dictionary moved(std::move(source));
    });
    EXPECT_LE(construction, 1);

    dross::dictionary assigned_from(original);
    dross::dictionary target;
    const long assignment = count_allocations([&] {
        target = std::move(assigned_from);
    });
    EXPECT_EQ(assignment, 0);
    EXPECT_EQ(target, original);
}

TEST(array_allocation_test, moving_allocates_at_most_once)
{
    dross::array original;
    for (int i = 0; i < 10; ++i) {
        original.append(dross::value(dross::number(i)));
    }

    dross::array source(original);
    const long construction = count_allocations([&] {
        const dross::array moved(std::move(source));
    });
    EXPECT_LE(construction, 1);

    dross::array assigned_from(original);
    dross::array target;
    const long assignment = count_allocations([&] {
        target = std::move(assigned_from);
    });
    EXPECT_EQ(assignment, 0);
    EXPECT_EQ(target, original);
}

TEST(string_allocation_test, moving_allocates_at_most_once)
{
    const dross::string original{ "a string long enough that it does not fit in the small string buffer" };

    dross::string source(original);
    const long construction = count_allocations([&] {
        const dross::string moved(std::move(source));
    });
    EXPECT_LE(construction, 1);

    dross::string assigned_from(original);
    dross::string target;
    const long assignment = count_allocations([&] {
        target = std::move(assigned_from);
    });
    EXPECT_EQ(assignment, 0);
    EXPECT_EQ(target, original);
}

namespace {

std::string tables(const std::string& parent, int count)
{
    std::string text;
    for (int i = 0; i < count; ++i) {
        const std::string n = std::to_string(i);
        text += "[" + parent + ".s" + n + "]\nhost = \"h" + n + "\"\nport = " + n + "\n";
    }
    return text;
}

}  // namespace

// Doubling the tables under one parent roughly doubles the allocations of
// reading them, where copying the parent or one above it for each header
// would quadruple them.
TEST(toml_allocation_test, reading_tables_under_one_parent_allocates_in_proportion)
{
    for (const std::string parent : { "servers", "org.servers" }) {
        SCOPED_TRACE(parent);
        const dross::data small{ tables(parent, 200) };
        const dross::data large{ tables(parent, 400) };

        const long small_count = count_allocations([&] {
            ASSERT_TRUE(dross::toml::deserialize(small));
        });
        const long large_count = count_allocations([&] {
            ASSERT_TRUE(dross::toml::deserialize(large));
        });

        ASSERT_GT(small_count, 0);
        EXPECT_LE(large_count, 3 * small_count);
    }
}
