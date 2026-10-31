// Replaces operator new to count allocations, which would reach every test
// in the program, so this file is a test executable of its own.
#include "dross/format/toml.h"
#include "dross/type/timestamp.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <new>
#include <string>

namespace {

std::atomic<long> allocations{ 0 };

template <typename Function>
long count_allocations(Function function)
{
    const long before = allocations.load();
    function();
    return allocations.load() - before;
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

// Building a timestamp from its parts allocates what a copy does, and the
// optional that from_components() returns holds a copy of it, so neither
// allocates more than two copies.
TEST(timestamp_allocation_test, building_from_components_allocates_no_more_than_two_copies)
{
    const auto tz = dross::timezone::utc();
    const dross::timestamp original(2024, 1, 21, 15, 30, 0, tz);

    const long copy = count_allocations([&] {
        const dross::timestamp copied(original);
    });
    const long from_components = count_allocations([&] {
        (void)dross::timestamp::from_components(2024, 1, 21, 15, 30, 0, tz);
    });
    const long constructor = count_allocations([&] {
        const dross::timestamp built(2024, 1, 21, 15, 30, 0, tz);
    });

    ASSERT_GT(copy, 0);
    EXPECT_LE(from_components, 2 * copy);
    EXPECT_LE(constructor, 2 * copy);
}

namespace {

std::string servers(int count)
{
    std::string text;
    for (int i = 0; i < count; ++i) {
        const std::string n = std::to_string(i);
        text += "[servers.s" + n + "]\nhost = \"h" + n + "\"\nport = " + n + "\n";
    }
    return text;
}

}  // namespace

// Doubling the tables under one parent roughly doubles the allocations of
// reading them, where copying the parent for each header would quadruple them.
TEST(toml_allocation_test, reading_tables_under_one_parent_allocates_in_proportion)
{
    const dross::data small{ servers(200) };
    const dross::data large{ servers(400) };

    const long small_count = count_allocations([&] {
        ASSERT_TRUE(dross::toml::deserialize(small));
    });
    const long large_count = count_allocations([&] {
        ASSERT_TRUE(dross::toml::deserialize(large));
    });

    ASSERT_GT(small_count, 0);
    EXPECT_LE(large_count, 3 * small_count);
}
