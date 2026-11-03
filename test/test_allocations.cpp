// Replaces operator new to count allocations, which would reach every test
// in the program, so this file is a test executable of its own.
#include "dross/format/toml.h"
#include "dross/type/array.h"
#include "dross/type/data.h"
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
std::atomic<long> deallocations{ 0 };

template <typename Function>
long count_allocations(Function function)
{
    const long before = allocations.load();
    function();
    return allocations.load() - before;
}

template <typename Function>
long count_deallocations(Function function)
{
    const long before = deallocations.load();
    function();
    return deallocations.load() - before;
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
    deallocations.fetch_add(1);
    std::free(p);
}

void operator delete(void* p, std::size_t) noexcept
{
    deallocations.fetch_add(1);
    std::free(p);
}

void operator delete(void* p, const std::nothrow_t&) noexcept
{
    deallocations.fetch_add(1);
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

TEST(value_allocation_test, moving_allocates_at_most_once)
{
    dross::array elements;
    for (int i = 0; i < 10; ++i) {
        elements.append(dross::value(dross::number(i)));
    }
    const dross::value original{ elements };

    dross::value source(original);
    const long construction = count_allocations([&] {
        const dross::value moved(std::move(source));
    });
    EXPECT_LE(construction, 1);

    dross::value assigned_from(original);
    dross::value target;
    const long assignment = count_allocations([&] {
        target = std::move(assigned_from);
    });
    EXPECT_EQ(assignment, 0);
    EXPECT_EQ(target, original);

    dross::dictionary entries;
    for (int i = 0; i < 10; ++i) {
        entries[std::to_string(i)] = dross::value(dross::number(i));
    }
    dross::value holding_a_dictionary{ entries };
    const long from_a_dictionary = count_allocations([&] {
        const dross::value moved(std::move(holding_a_dictionary));
    });
    EXPECT_LE(from_a_dictionary, 1);
}

// One storage for the value and one that the source of the move is left with.
TEST(value_allocation_test, building_from_a_temporary_does_not_copy_its_contents)
{
    dross::array elements;
    dross::dictionary entries;
    for (int i = 0; i < 10; ++i) {
        elements.append(dross::value(dross::number(i)));
        entries[std::to_string(i)] = dross::value(dross::number(i));
    }

    const long from_array = count_allocations([&] {
        const dross::value built(std::move(elements));
    });
    EXPECT_LE(from_array, 2);

    const long from_dictionary = count_allocations([&] {
        const dross::value built(std::move(entries));
    });
    EXPECT_LE(from_dictionary, 2);

    dross::string text{ std::string(1000, 'x') };
    const long from_string = count_allocations([&] {
        const dross::value built(std::move(text));
    });
    EXPECT_LE(from_string, 2);

    dross::data bytes{ std::string(1000, 'x') };
    const long from_data = count_allocations([&] {
        const dross::value built(std::move(bytes));
    });
    EXPECT_LE(from_data, 2);
}

TEST(value_allocation_test, assigning_a_temporary_does_not_copy_its_contents)
{
    dross::array elements;
    dross::dictionary entries;
    for (int i = 0; i < 10; ++i) {
        elements.append(dross::value(dross::number(i)));
        entries[std::to_string(i)] = dross::value(dross::number(i));
    }
    dross::value target;

    const long from_array = count_allocations([&] {
        target = std::move(elements);
    });
    EXPECT_LE(from_array, 1);

    const long from_dictionary = count_allocations([&] {
        target = std::move(entries);
    });
    EXPECT_LE(from_dictionary, 1);
}

// One allocation for the array to hold the element, and one for the empty
// storage the moved-from value keeps.
TEST(array_allocation_test, appending_a_temporary_does_not_copy_its_contents)
{
    dross::array elements;
    for (int i = 0; i < 10; ++i) {
        elements.append(dross::value(dross::number(i)));
    }
    dross::value element{ std::move(elements) };
    dross::array target;

    const long count = count_allocations([&] {
        target.append(std::move(element));
    });
    EXPECT_LE(count, 2);
}

// The target of a move assignment gives up what it held there and then,
// rather than when the source goes away.
TEST(string_allocation_test, move_assignment_releases_the_old_text)
{
    const std::string text(1000, 'x');
    dross::string source{ text };
    dross::string target{ text };
    const long released = count_deallocations([&] {
        target = std::move(source);
    });
    EXPECT_GE(released, 1);
}

TEST(array_allocation_test, move_assignment_releases_the_old_elements)
{
    dross::array source;
    dross::array target;
    for (int i = 0; i < 10; ++i) {
        source.append(dross::value(dross::number(i)));
        target.append(dross::value(dross::number(i)));
    }
    const long released = count_deallocations([&] {
        target = std::move(source);
    });
    EXPECT_GE(released, 10);
}

TEST(dictionary_allocation_test, move_assignment_releases_the_old_entries)
{
    dross::dictionary source;
    dross::dictionary target;
    for (int i = 0; i < 10; ++i) {
        source[std::to_string(i)] = dross::value(dross::number(i));
        target[std::to_string(i)] = dross::value(dross::number(i));
    }
    const long released = count_deallocations([&] {
        target = std::move(source);
    });
    EXPECT_GE(released, 10);
}

TEST(value_allocation_test, move_assignment_releases_what_was_held)
{
    dross::array elements;
    for (int i = 0; i < 10; ++i) {
        elements.append(dross::value(dross::number(i)));
    }
    dross::value source{ elements };
    dross::value target{ elements };
    const long released = count_deallocations([&] {
        target = std::move(source);
    });
    EXPECT_GE(released, 10);
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
