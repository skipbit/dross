#include <gtest/gtest.h>

#include "dross/type/array.h"
#include "dross/type/number.h"
#include "dross/type/string.h"
#include "dross/type/value.h"
#include <stdexcept>

// =============================================================================
// Basic Construction and Properties
// =============================================================================

TEST(array_test, default_constructor)
{
    dross::array a;
    EXPECT_TRUE(a.empty());
    EXPECT_EQ(a.length(), 0);
}

TEST(array_test, initializer_list_constructor)
{
    dross::array a = { 1, 2, 3, 4, 5 };
    EXPECT_FALSE(a.empty());
    EXPECT_EQ(a.length(), 5);
}

TEST(array_test, copy_constructor)
{
    dross::array a = { 1, 2, 3 };
    dross::array b(a);
    EXPECT_EQ(a, b);
    EXPECT_EQ(b.length(), 3);

    // Verify deep copy
    b.append(4);
    EXPECT_NE(a, b);
    EXPECT_EQ(a.length(), 3);
    EXPECT_EQ(b.length(), 4);
}

TEST(array_test, empty_initializer_list)
{
    dross::array a = {};
    EXPECT_TRUE(a.empty());
    EXPECT_EQ(a.length(), 0);
}

// =============================================================================
// Equality and Comparison
// =============================================================================

TEST(array_test, equals_method)
{
    dross::array a = { 1, 2, 3 };
    dross::array b = { 1, 2, 3 };
    dross::array c = { 1, 2, 4 };
    dross::array d = { 1, 2 };

    EXPECT_TRUE(a.equals(b));
    EXPECT_FALSE(a.equals(c));
    EXPECT_FALSE(a.equals(d));
}

TEST(array_test, equality_operators)
{
    dross::array a = { 1, 2, 3 };
    dross::array b = { 1, 2, 3 };
    dross::array c = { 1, 2, 4 };

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_FALSE(a != b);
    EXPECT_TRUE(a != c);
}

TEST(array_test, assignment_operator)
{
    dross::array a = { 1, 2, 3 };
    dross::array b = { 4, 5 };

    b = a;
    EXPECT_EQ(a, b);
    EXPECT_EQ(b.length(), 3);

    // Verify deep copy
    b.append(6);
    EXPECT_NE(a, b);
}

TEST(array_test, self_assignment)
{
    dross::array a = { 1, 2, 3 };
    dross::array& ref = a;
    a = ref;  // Self-assignment through reference
    EXPECT_EQ(a.length(), 3);
    EXPECT_EQ(a[0], dross::value(1));
}

// =============================================================================
// Element Access
// =============================================================================

TEST(array_test, subscript_operator_const)
{
    const dross::array a = { 10, 20, 30 };
    EXPECT_EQ(a[0], dross::value(10));
    EXPECT_EQ(a[1], dross::value(20));
    EXPECT_EQ(a[2], dross::value(30));
}

TEST(array_test, subscript_operator_mutable)
{
    dross::array a = { 10, 20, 30 };
    a[1] = dross::value(25);
    EXPECT_EQ(a[1], dross::value(25));
}

TEST(array_test, value_at_method)
{
    dross::array a = { 100, 200, 300 };
    EXPECT_EQ(a.value_at(0), dross::value(100));
    EXPECT_EQ(a.value_at(1), dross::value(200));
    EXPECT_EQ(a.value_at(2), dross::value(300));
}

TEST(array_test, index_of_found)
{
    dross::array a = { 10, 20, 30, 20, 40 };
    EXPECT_EQ(a.index_of(dross::value(10)), 0);
    EXPECT_EQ(a.index_of(dross::value(20)), 1);  // First occurrence
    EXPECT_EQ(a.index_of(dross::value(30)), 2);
    EXPECT_EQ(a.index_of(dross::value(40)), 4);
}

TEST(array_test, index_of_not_found)
{
    dross::array a = { 10, 20, 30 };
    EXPECT_EQ(a.index_of(dross::value(50)), static_cast<size_t>(-1));
    EXPECT_EQ(a.index_of(dross::value(0)), static_cast<size_t>(-1));
}

TEST(array_test, index_of_empty_array)
{
    dross::array a;
    EXPECT_EQ(a.index_of(dross::value(10)), static_cast<size_t>(-1));
}

// =============================================================================
// Element Modification
// =============================================================================

TEST(array_test, append_single_value)
{
    dross::array a;
    a.append(dross::value(1));
    EXPECT_EQ(a.length(), 1);
    EXPECT_EQ(a[0], dross::value(1));

    a.append(dross::value(2));
    a.append(dross::value(3));
    EXPECT_EQ(a.length(), 3);
    EXPECT_EQ(a[2], dross::value(3));
}

TEST(array_test, append_iterator_range)
{
    dross::array a = { 1, 2, 3 };
    dross::array b = { 4, 5, 6 };

    a.append(b.begin(), b.end());
    EXPECT_EQ(a.length(), 6);
    EXPECT_EQ(a[3], dross::value(4));
    EXPECT_EQ(a[5], dross::value(6));
}

TEST(array_test, append_const_iterator_range)
{
    dross::array a = { 1, 2, 3 };
    const dross::array b = { 4, 5, 6 };

    a.append(b.begin(), b.end());
    EXPECT_EQ(a.length(), 6);
    EXPECT_EQ(a[3], dross::value(4));
    EXPECT_EQ(a[5], dross::value(6));
}

TEST(array_test, append_empty_range)
{
    dross::array a = { 1, 2, 3 };
    dross::array b;

    a.append(b.begin(), b.end());
    EXPECT_EQ(a.length(), 3);
}

TEST(array_test, remove_existing_value)
{
    dross::array a = { 1, 2, 3, 2, 4 };
    a.remove(dross::value(2));

    EXPECT_EQ(a.length(), 3);
    EXPECT_EQ(a[0], dross::value(1));
    EXPECT_EQ(a[1], dross::value(3));
    EXPECT_EQ(a[2], dross::value(4));
}

TEST(array_test, remove_non_existing_value)
{
    dross::array a = { 1, 2, 3 };
    a.remove(dross::value(10));

    EXPECT_EQ(a.length(), 3);
}

TEST(array_test, remove_from_empty_array)
{
    dross::array a;
    a.remove(dross::value(1));

    EXPECT_TRUE(a.empty());
}

TEST(array_test, remove_all_elements)
{
    dross::array a = { 1, 1, 1 };
    a.remove(dross::value(1));

    EXPECT_TRUE(a.empty());
}

// =============================================================================
// Iterator Tests
// =============================================================================

TEST(array_test, iterator_basic)
{
    dross::array a = { 10, 20, 30 };
    auto it = a.begin();

    EXPECT_EQ(*it, dross::value(10));
    ++it;
    EXPECT_EQ(*it, dross::value(20));
    ++it;
    EXPECT_EQ(*it, dross::value(30));
    ++it;
    EXPECT_EQ(it, a.end());
}

TEST(array_test, const_iterator_basic)
{
    const dross::array a = { 10, 20, 30 };
    auto it = a.begin();

    EXPECT_EQ(*it, dross::value(10));
    ++it;
    EXPECT_EQ(*it, dross::value(20));
}

TEST(array_test, cbegin_cend)
{
    dross::array a = { 5, 10, 15 };
    auto it = a.cbegin();

    EXPECT_EQ(*it, dross::value(5));
    ++it;
    EXPECT_EQ(*it, dross::value(10));
    ++it;
    EXPECT_EQ(*it, dross::value(15));
    ++it;
    EXPECT_EQ(it, a.cend());
}

TEST(array_test, iterator_comparison)
{
    dross::array a = { 1, 2, 3 };
    auto it1 = a.begin();
    auto it2 = a.begin();
    auto it3 = a.end();

    EXPECT_TRUE(it1 == it2);
    EXPECT_FALSE(it1 != it2);
    EXPECT_FALSE(it1 == it3);
    EXPECT_TRUE(it1 != it3);
}

TEST(array_test, empty_array_iterators)
{
    dross::array a;
    EXPECT_EQ(a.begin(), a.end());
    EXPECT_EQ(a.cbegin(), a.cend());
}

TEST(array_test, iterator_copy)
{
    dross::array a = { 1, 2, 3 };
    auto it1 = a.begin();
    ++it1;
    auto it2 = it1;  // Copy constructor

    EXPECT_EQ(it1, it2);
    EXPECT_EQ(*it1, *it2);
}

// =============================================================================
// Range-based for Loop Tests
// =============================================================================

TEST(array_test, range_for_mutable)
{
    dross::array a = { 1, 2, 3 };
    int sum = 0;

    for (auto& val : a) {
        sum += static_cast<int>(dross::value_cast<dross::number>(val));
    }

    EXPECT_EQ(sum, 6);
}

TEST(array_test, range_for_const)
{
    const dross::array a = { 10, 20, 30 };
    int sum = 0;

    for (const auto& val : a) {
        sum += static_cast<int>(dross::value_cast<dross::number>(val));
    }

    EXPECT_EQ(sum, 60);
}

// =============================================================================
// Mixed Type Arrays
// =============================================================================

TEST(array_test, mixed_types)
{
    dross::array a;
    a.append(dross::value(42));
    a.append(dross::value(dross::string("hello")));
    a.append(dross::value(3.14));

    EXPECT_EQ(a.length(), 3);
    EXPECT_TRUE(a[0].is<dross::number>());
    EXPECT_TRUE(a[1].is<dross::string>());
    EXPECT_TRUE(a[2].is<dross::number>());
}

TEST(array_test, nested_arrays)
{
    dross::array inner = { 1, 2, 3 };
    dross::array outer;
    outer.append(dross::value(inner));
    outer.append(dross::value(100));

    EXPECT_EQ(outer.length(), 2);
    EXPECT_TRUE(outer[0].is<dross::array>());
    EXPECT_TRUE(outer[1].is<dross::number>());

    auto nested = dross::value_cast<dross::array>(outer[0]);
    EXPECT_EQ(nested.length(), 3);
}

// =============================================================================
// Boundary and Error Cases
// =============================================================================

TEST(array_test, out_of_bounds_access_subscript)
{
    dross::array a = { 1, 2, 3 };

    EXPECT_THROW(a[3], std::out_of_range);
    EXPECT_THROW(a[100], std::out_of_range);

    const dross::array b = { 1, 2, 3 };
    EXPECT_THROW(b[3], std::out_of_range);
}

TEST(array_test, out_of_bounds_value_at)
{
    dross::array a = { 1, 2, 3 };

    EXPECT_THROW(a.value_at(3), std::out_of_range);
    EXPECT_THROW(a.value_at(100), std::out_of_range);
}

TEST(array_test, empty_array_access)
{
    dross::array a;

    EXPECT_THROW(a[0], std::out_of_range);
    EXPECT_THROW(a.value_at(0), std::out_of_range);
}

// =============================================================================
// Performance and Stress Tests
// =============================================================================

TEST(array_test, large_array)
{
    dross::array a;
    const size_t size = 10000;

    for (size_t i = 0; i < size; ++i) {
        a.append(dross::value(static_cast<int>(i)));
    }

    EXPECT_EQ(a.length(), size);
    EXPECT_EQ(a[0], dross::value(0));
    EXPECT_EQ(a[size - 1], dross::value(static_cast<int>(size - 1)));
}

TEST(array_test, many_operations)
{
    dross::array a;

    // Append many elements
    for (int i = 0; i < 100; ++i) {
        a.append(dross::value(i));
    }

    // Remove half of them
    for (int i = 0; i < 100; i += 2) {
        a.remove(dross::value(i));
    }

    EXPECT_EQ(a.length(), 50);

    // Verify remaining elements
    for (size_t i = 0; i < a.length(); ++i) {
        int val = static_cast<int>(dross::value_cast<dross::number>(a[i]));
        EXPECT_EQ(val % 2, 1);  // Only odd numbers remain
    }
}
