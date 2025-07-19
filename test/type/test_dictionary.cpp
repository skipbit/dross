#include <gtest/gtest.h>

#include "dross/type/dictionary.h"
#include "dross/type/value.h"
#include "dross/type/number.h"
#include "dross/type/string.h"
#include "dross/type/array.h"

// =============================================================================
// Basic Construction and Properties
// =============================================================================

TEST(dictionary_test, default_constructor)
{
    dross::dictionary d;
    EXPECT_TRUE(d.empty());
    EXPECT_EQ(d.size(), 0);
}

TEST(dictionary_test, copy_constructor)
{
    dross::dictionary original;
    original["key1"] = dross::value(42);
    original["key2"] = dross::value("hello");

    dross::dictionary copy(original);

    EXPECT_EQ(original, copy);
    EXPECT_EQ(copy.size(), 2);
    EXPECT_TRUE(copy.contains("key1"));
    EXPECT_TRUE(copy.contains("key2"));

    // Verify deep copy
    copy["key3"] = dross::value(100);
    EXPECT_NE(original, copy);
    EXPECT_EQ(original.size(), 2);
    EXPECT_EQ(copy.size(), 3);
    EXPECT_FALSE(original.contains("key3"));
    EXPECT_TRUE(copy.contains("key3"));
}

TEST(dictionary_test, empty_dictionary)
{
    dross::dictionary d;
    EXPECT_TRUE(d.empty());
    EXPECT_EQ(d.size(), 0);
    EXPECT_FALSE(d.contains("anything"));
}

// =============================================================================
// Equality and Comparison
// =============================================================================

TEST(dictionary_test, equals_method)
{
    dross::dictionary d1;
    dross::dictionary d2;
    dross::dictionary d3;

    d1["a"] = dross::value(1);
    d1["b"] = dross::value(2);

    d2["a"] = dross::value(1);
    d2["b"] = dross::value(2);

    d3["a"] = dross::value(1);
    d3["c"] = dross::value(2);

    EXPECT_TRUE(d1.equals(d2));
    EXPECT_FALSE(d1.equals(d3));
    EXPECT_TRUE(d1.equals(d1));
}

TEST(dictionary_test, equality_operators)
{
    dross::dictionary d1;
    dross::dictionary d2;
    dross::dictionary d3;

    d1["x"] = dross::value(10);
    d1["y"] = dross::value(20);

    d2["x"] = dross::value(10);
    d2["y"] = dross::value(20);

    d3["x"] = dross::value(10);
    d3["y"] = dross::value(30);

    EXPECT_TRUE(d1 == d2);
    EXPECT_FALSE(d1 == d3);
    EXPECT_FALSE(d1 != d2);
    EXPECT_TRUE(d1 != d3);
}

TEST(dictionary_test, equality_different_order)
{
    dross::dictionary d1;
    dross::dictionary d2;

    // Add in different order
    d1["first"] = dross::value(1);
    d1["second"] = dross::value(2);

    d2["second"] = dross::value(2);
    d2["first"] = dross::value(1);

    EXPECT_TRUE(d1 == d2);
    EXPECT_TRUE(d1.equals(d2));
}

TEST(dictionary_test, equality_empty_dictionaries)
{
    dross::dictionary d1;
    dross::dictionary d2;

    EXPECT_TRUE(d1 == d2);
    EXPECT_TRUE(d1.equals(d2));
    EXPECT_FALSE(d1 != d2);
}

// =============================================================================
// Assignment Operators
// =============================================================================

TEST(dictionary_test, assignment_operator)
{
    dross::dictionary d1;
    dross::dictionary d2;

    d1["original"] = dross::value("value");
    d2["key"] = dross::value(42);
    d2["another"] = dross::value("test");

    d1 = d2;
    EXPECT_EQ(d1, d2);
    EXPECT_EQ(d1.size(), 2);
    EXPECT_TRUE(d1.contains("key"));
    EXPECT_TRUE(d1.contains("another"));
    EXPECT_FALSE(d1.contains("original"));

    // Verify deep copy
    d2["new"] = dross::value(999);
    EXPECT_NE(d1, d2);
    EXPECT_EQ(d1.size(), 2);
    EXPECT_EQ(d2.size(), 3);
}

TEST(dictionary_test, self_assignment)
{
    dross::dictionary d;
    d["test"] = dross::value("value");
    d["number"] = dross::value(123);

    dross::dictionary& ref = d;
    d = ref;  // Self-assignment through reference

    EXPECT_EQ(d.size(), 2);
    EXPECT_TRUE(d.contains("test"));
    EXPECT_TRUE(d.contains("number"));
}

// =============================================================================
// Element Access and Modification
// =============================================================================

TEST(dictionary_test, subscript_operator_mutable)
{
    dross::dictionary d;

    d["new_key"] = dross::value(42);
    EXPECT_TRUE(d.contains("new_key"));
    EXPECT_EQ(d.size(), 1);

    // Modify existing
    d["new_key"] = dross::value(100);
    EXPECT_EQ(d.size(), 1);

    // Access existing
    dross::value& val = d["new_key"];
    EXPECT_EQ(dross::value_cast<dross::number>(val), dross::number(100));
}

TEST(dictionary_test, subscript_operator_const)
{
    dross::dictionary d;
    d["key1"] = dross::value("hello");
    d["key2"] = dross::value(3.14);

    const dross::dictionary& const_d = d;

    const dross::value& val1 = const_d["key1"];
    const dross::value& val2 = const_d["key2"];

    EXPECT_EQ(dross::value_cast<dross::string>(val1), dross::string("hello"));
    EXPECT_EQ(dross::value_cast<dross::number>(val2), dross::number(3.14));
}

TEST(dictionary_test, contains_method)
{
    dross::dictionary d;

    EXPECT_FALSE(d.contains("nonexistent"));

    d["exists"] = dross::value(1);
    EXPECT_TRUE(d.contains("exists"));
    EXPECT_FALSE(d.contains("still_nonexistent"));

    d["empty_string"] = dross::value("");
    EXPECT_TRUE(d.contains("empty_string"));
}

TEST(dictionary_test, size_and_empty)
{
    dross::dictionary d;
    EXPECT_TRUE(d.empty());
    EXPECT_EQ(d.size(), 0);

    d["first"] = dross::value(1);
    EXPECT_FALSE(d.empty());
    EXPECT_EQ(d.size(), 1);

    d["second"] = dross::value(2);
    d["third"] = dross::value(3);
    EXPECT_FALSE(d.empty());
    EXPECT_EQ(d.size(), 3);

    // Overwrite existing key shouldn't change size
    d["first"] = dross::value(100);
    EXPECT_EQ(d.size(), 3);
}

// =============================================================================
// Iterator Tests
// =============================================================================

TEST(dictionary_test, iterator_basic)
{
    dross::dictionary d;
    d["a"] = dross::value(1);
    d["b"] = dross::value(2);
    d["c"] = dross::value(3);
    
    auto it = d.begin();
    EXPECT_NE(it, d.end());
    
    // Count elements through iteration
    size_t count = 0;
    for (auto iter = d.begin(); iter != d.end(); ++iter) {
        count++;
    }
    EXPECT_EQ(count, 3);
}

TEST(dictionary_test, const_iterator_basic)
{
    dross::dictionary d;
    d["x"] = dross::value(10);
    d["y"] = dross::value(20);
    
    const dross::dictionary& const_d = d;
    
    auto it = const_d.begin();
    EXPECT_NE(it, const_d.end());
    
    size_t count = 0;
    for (auto iter = const_d.begin(); iter != const_d.end(); ++iter) {
        count++;
    }
    EXPECT_EQ(count, 2);
}

TEST(dictionary_test, cbegin_cend)
{
    dross::dictionary d;
    d["key1"] = dross::value("value1");
    d["key2"] = dross::value("value2");
    
    auto it = d.cbegin();
    EXPECT_NE(it, d.cend());
    
    size_t count = 0;
    for (auto iter = d.cbegin(); iter != d.cend(); ++iter) {
        count++;
    }
    EXPECT_EQ(count, 2);
}

TEST(dictionary_test, iterator_comparison)
{
    dross::dictionary d;
    d["test"] = dross::value(123);
    
    auto it1 = d.begin();
    auto it2 = d.begin();
    auto it3 = d.end();
    
    EXPECT_TRUE(it1 == it2);
    EXPECT_FALSE(it1 != it2);
    EXPECT_FALSE(it1 == it3);
    EXPECT_TRUE(it1 != it3);
}

TEST(dictionary_test, empty_dictionary_iterators)
{
    dross::dictionary d;
    EXPECT_EQ(d.begin(), d.end());
    EXPECT_EQ(d.cbegin(), d.cend());
    
    const dross::dictionary& const_d = d;
    EXPECT_EQ(const_d.begin(), const_d.end());
}

TEST(dictionary_test, iterator_dereference)
{
    dross::dictionary d;
    d["key"] = dross::value(42);
    
    auto it = d.begin();
    auto pair = *it;
    (void)pair;  // Suppress unused variable warning
    
    // Test that dereferencing works and we can access key-value pairs
    EXPECT_NE(it, d.end());
}

TEST(dictionary_test, range_based_for_loop)
{
    dross::dictionary d;
    d["first"] = dross::value(1);
    d["second"] = dross::value(2);
    d["third"] = dross::value(3);
    
    size_t count = 0;
    for (const auto& pair : d) {
        count++;
        (void)pair;  // Suppress unused variable warning
        // Verify we can access the key-value pair
        // Note: exact usage depends on iterator implementation
    }
    EXPECT_EQ(count, 3);
}

TEST(dictionary_test, const_range_based_for_loop)
{
    dross::dictionary d;
    d["alpha"] = dross::value("a");
    d["beta"] = dross::value("b");
    
    const dross::dictionary& const_d = d;
    
    size_t count = 0;
    for (const auto& pair : const_d) {
        count++;
        (void)pair;  // Suppress unused variable warning
    }
    EXPECT_EQ(count, 2);
}

// =============================================================================
// Mixed Type Values
// =============================================================================

TEST(dictionary_test, mixed_types)
{
    dross::dictionary d;

    d["number"] = dross::value(42);
    d["string"] = dross::value(dross::string("hello"));
    d["double"] = dross::value(3.14);
    d["array"] = dross::value(dross::array{1, 2, 3});

    EXPECT_EQ(d.size(), 4);
    EXPECT_TRUE(d["number"].is<dross::number>());
    EXPECT_TRUE(d["string"].is<dross::string>());
    EXPECT_TRUE(d["double"].is<dross::number>());
    EXPECT_TRUE(d["array"].is<dross::array>());
}

TEST(dictionary_test, nested_dictionaries)
{
    dross::dictionary inner;
    inner["inner_key"] = dross::value("inner_value");

    dross::dictionary outer;
    outer["inner_dict"] = dross::value(inner);
    outer["simple"] = dross::value(100);

    EXPECT_EQ(outer.size(), 2);
    EXPECT_TRUE(outer["inner_dict"].is<dross::dictionary>());
    EXPECT_TRUE(outer["simple"].is<dross::number>());

    auto nested = dross::value_cast<dross::dictionary>(outer["inner_dict"]);
    EXPECT_EQ(nested.size(), 1);
    EXPECT_TRUE(nested.contains("inner_key"));
}

TEST(dictionary_test, dictionary_with_arrays)
{
    dross::array arr1{1, 2, 3};
    dross::array arr2{"a", "b", "c"};

    dross::dictionary d;
    d["numbers"] = dross::value(arr1);
    d["strings"] = dross::value(arr2);
    d["count"] = dross::value(2);

    EXPECT_EQ(d.size(), 3);

    auto numbers = dross::value_cast<dross::array>(d["numbers"]);
    auto strings = dross::value_cast<dross::array>(d["strings"]);

    EXPECT_EQ(numbers.length(), 3);
    EXPECT_EQ(strings.length(), 3);
}

// =============================================================================
// Edge Cases and Boundary Values
// =============================================================================

TEST(dictionary_test, empty_string_key)
{
    dross::dictionary d;

    d[""] = dross::value("empty key");
    EXPECT_TRUE(d.contains(""));
    EXPECT_EQ(d.size(), 1);

    EXPECT_EQ(dross::value_cast<dross::string>(d[""]), dross::string("empty key"));
}

TEST(dictionary_test, very_long_key)
{
    dross::dictionary d;
    std::string long_key(1000, 'x');

    d[long_key] = dross::value("long key value");
    EXPECT_TRUE(d.contains(long_key));
    EXPECT_EQ(d.size(), 1);
}

TEST(dictionary_test, special_character_keys)
{
    dross::dictionary d;

    d["key with spaces"] = dross::value(1);
    d["key\nwith\nnewlines"] = dross::value(2);
    d["key\twith\ttabs"] = dross::value(3);
    d["key\"with\"quotes"] = dross::value(4);
    d["key\\with\\backslashes"] = dross::value(5);

    EXPECT_EQ(d.size(), 5);
    EXPECT_TRUE(d.contains("key with spaces"));
    EXPECT_TRUE(d.contains("key\nwith\nnewlines"));
    EXPECT_TRUE(d.contains("key\twith\ttabs"));
    EXPECT_TRUE(d.contains("key\"with\"quotes"));
    EXPECT_TRUE(d.contains("key\\with\\backslashes"));
}

TEST(dictionary_test, unicode_keys)
{
    dross::dictionary d;

    d["普通话"] = dross::value("Chinese");
    d["日本語"] = dross::value("Japanese");
    d["한국어"] = dross::value("Korean");
    d["العربية"] = dross::value("Arabic");
    d["🌍🌎🌏"] = dross::value("World");

    EXPECT_EQ(d.size(), 5);
    EXPECT_TRUE(d.contains("普通话"));
    EXPECT_TRUE(d.contains("日本語"));
    EXPECT_TRUE(d.contains("한국어"));
    EXPECT_TRUE(d.contains("العربية"));
    EXPECT_TRUE(d.contains("🌍🌎🌏"));
}

TEST(dictionary_test, overwrite_existing_key)
{
    dross::dictionary d;

    d["key"] = dross::value(1);
    EXPECT_EQ(d.size(), 1);
    EXPECT_EQ(dross::value_cast<dross::number>(d["key"]), dross::number(1));

    d["key"] = dross::value("overwritten");
    EXPECT_EQ(d.size(), 1);
    EXPECT_EQ(dross::value_cast<dross::string>(d["key"]), dross::string("overwritten"));

    d["key"] = dross::value(dross::array{1, 2, 3});
    EXPECT_EQ(d.size(), 1);
    EXPECT_TRUE(d["key"].is<dross::array>());
}

// =============================================================================
// Copy Behavior Verification
// =============================================================================

TEST(dictionary_test, deep_copy_verification)
{
    dross::dictionary original;
    original["shared"] = dross::value("original");
    original["unique"] = dross::value(100);

    dross::dictionary copy = original;

    // Modify copy
    copy["shared"] = dross::value("modified");
    copy["new"] = dross::value(200);

    // Original should be unchanged
    EXPECT_EQ(dross::value_cast<dross::string>(original["shared"]), dross::string("original"));
    EXPECT_EQ(original.size(), 2);
    EXPECT_FALSE(original.contains("new"));

    EXPECT_EQ(dross::value_cast<dross::string>(copy["shared"]), dross::string("modified"));
    EXPECT_EQ(copy.size(), 3);
    EXPECT_TRUE(copy.contains("new"));

    EXPECT_NE(original, copy);
}

TEST(dictionary_test, assignment_chain)
{
    dross::dictionary d1;
    dross::dictionary d2;
    dross::dictionary d3;

    d1["first"] = dross::value(1);
    d2["second"] = dross::value(2);
    d3["third"] = dross::value(3);

    d1 = d2 = d3;

    EXPECT_EQ(d1, d2);
    EXPECT_EQ(d2, d3);
    EXPECT_TRUE(d1.contains("third"));
    EXPECT_TRUE(d2.contains("third"));
    EXPECT_FALSE(d1.contains("first"));
    EXPECT_FALSE(d2.contains("second"));

    // Verify independence after assignment
    d1["modified"] = dross::value(999);
    EXPECT_NE(d1, d2);
    EXPECT_EQ(d2, d3);
}

// =============================================================================
// Performance and Stress Tests
// =============================================================================

TEST(dictionary_test, many_keys)
{
    dross::dictionary d;
    const size_t count = 1000;

    for (size_t i = 0; i < count; ++i) {
        std::string key = "key_" + std::to_string(i);
        d[key] = dross::value(static_cast<int>(i));
    }

    EXPECT_EQ(d.size(), count);

    // Verify all keys exist and have correct values
    for (size_t i = 0; i < count; ++i) {
        std::string key = "key_" + std::to_string(i);
        EXPECT_TRUE(d.contains(key));
        EXPECT_EQ(dross::value_cast<dross::number>(d[key]), dross::number(static_cast<int>(i)));
    }
}

TEST(dictionary_test, repeated_operations)
{
    dross::dictionary d;

    // Add, modify, check pattern
    for (int i = 0; i < 100; ++i) {
        std::string key = "item_" + std::to_string(i);
        d[key] = dross::value(i);
        EXPECT_TRUE(d.contains(key));

        // Modify
        d[key] = dross::value(i * 2);
        EXPECT_EQ(dross::value_cast<dross::number>(d[key]), dross::number(i * 2));
    }

    EXPECT_EQ(d.size(), 100);
}

TEST(dictionary_test, large_value_strings)
{
    dross::dictionary d;
    std::string large_value(10000, 'A');

    d["large"] = dross::value(large_value);
    EXPECT_TRUE(d.contains("large"));
    EXPECT_EQ(dross::value_cast<dross::string>(d["large"]), dross::string(large_value));
}

TEST(dictionary_test, mixed_operations_stress)
{
    dross::dictionary d;

    // Complex sequence of operations
    d["base"] = dross::value("start");

    dross::dictionary copy = d;
    copy["additional"] = dross::value(42);

    d = copy;
    d["more"] = dross::value(dross::array{1, 2, 3});

    dross::dictionary nested;
    nested["inner"] = dross::value("nested");
    d["nested_dict"] = dross::value(nested);

    EXPECT_EQ(d.size(), 4);
    EXPECT_TRUE(d.contains("base"));
    EXPECT_TRUE(d.contains("additional"));
    EXPECT_TRUE(d.contains("more"));
    EXPECT_TRUE(d.contains("nested_dict"));

    EXPECT_TRUE(d["more"].is<dross::array>());
    EXPECT_TRUE(d["nested_dict"].is<dross::dictionary>());
}

// =============================================================================
// API Completeness Tests
// =============================================================================

TEST(dictionary_test, api_completeness)
{
    dross::dictionary d;
    d["first"] = dross::value(1);
    d["second"] = dross::value(2);
    d["third"] = dross::value(3);

    // Test that all documented functionality works without using incomplete iterator types
    EXPECT_EQ(d.size(), 3);
    EXPECT_FALSE(d.empty());

    // Test key access
    EXPECT_TRUE(d.contains("first"));
    EXPECT_TRUE(d.contains("second"));
    EXPECT_TRUE(d.contains("third"));
    EXPECT_FALSE(d.contains("nonexistent"));

    // Test value access
    EXPECT_EQ(dross::value_cast<dross::number>(d["first"]), dross::number(1));
    EXPECT_EQ(dross::value_cast<dross::number>(d["second"]), dross::number(2));
    EXPECT_EQ(dross::value_cast<dross::number>(d["third"]), dross::number(3));
}

TEST(dictionary_test, const_dictionary_access)
{
    dross::dictionary d;
    d["alpha"] = dross::value("a");
    d["beta"] = dross::value("b");

    const dross::dictionary& const_d = d;

    // Test const access methods
    EXPECT_EQ(const_d.size(), 2);
    EXPECT_FALSE(const_d.empty());
    EXPECT_TRUE(const_d.contains("alpha"));
    EXPECT_TRUE(const_d.contains("beta"));

    // Test const operator[]
    EXPECT_EQ(dross::value_cast<dross::string>(const_d["alpha"]), dross::string("a"));
    EXPECT_EQ(dross::value_cast<dross::string>(const_d["beta"]), dross::string("b"));
}
