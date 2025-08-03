#include <gtest/gtest.h>
#include "dross/type/data.h"
#include "dross/type/error.h"
#include "dross/type/value.h"
#include "dross/type.h"
#include <vector>
#include <string>
#include <algorithm>
#include <system_error>

using namespace dross;

// Helper data for tests
const std::vector<uint8_t> hello_bytes = {0x48, 0x65, 0x6C, 0x6C, 0x6F};

// Construction Tests
TEST(data_test, default_construction) {
    data d;
    EXPECT_TRUE(d.empty());
    EXPECT_EQ(d.size(), 0u);
    EXPECT_FALSE(d.bytes().has_value());
}

TEST(data_test, initializer_list_construction) {
    data d{0x48, 0x65, 0x6C, 0x6C, 0x6F};
    EXPECT_FALSE(d.empty());
    EXPECT_EQ(d.size(), 5u);
    EXPECT_EQ(d[0], 0x48);
    EXPECT_EQ(d[4], 0x6F);
}

TEST(data_test, vector_construction) {
    data d{hello_bytes};
    EXPECT_FALSE(d.empty());
    EXPECT_EQ(d.size(), 5u);
    EXPECT_EQ(d[0], 0x48);
    EXPECT_EQ(d[4], 0x6F);
}

TEST(data_test, buffer_construction) {
    const uint8_t buffer[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
    data d{buffer, 5};
    EXPECT_FALSE(d.empty());
    EXPECT_EQ(d.size(), 5u);
    EXPECT_EQ(d[0], 0x48);
    EXPECT_EQ(d[4], 0x6F);
}

TEST(data_test, string_construction) {
    data d{"Hello"};
    EXPECT_FALSE(d.empty());
    EXPECT_EQ(d.size(), 5u);
    EXPECT_EQ(d[0], 'H');
    EXPECT_EQ(d[4], 'o');
}

TEST(data_test, cstring_construction) {
    const char* str = "Hello";
    data d{str};
    EXPECT_FALSE(d.empty());
    EXPECT_EQ(d.size(), 5u);
    EXPECT_EQ(d[0], 'H');
    EXPECT_EQ(d[4], 'o');
}

TEST(data_test, copy_construction) {
    data original{"Hello"};
    data copy{original};
    EXPECT_EQ(copy.size(), 5u);
    EXPECT_EQ(copy, original);
    EXPECT_NE(copy.bytes().value(), original.bytes().value()); // Different memory locations
}

TEST(data_test, move_construction) {
    data original{"Hello"};
    data moved{std::move(original)};
    EXPECT_EQ(moved.size(), 5u);
    EXPECT_EQ(to_string(moved), "Hello");
    // original should be in valid but unspecified state
}

// Access Tests
TEST(data_test, index_access) {
    data d{"Hello"};
    EXPECT_EQ(d[0], 'H');
    EXPECT_EQ(d[1], 'e');
    EXPECT_EQ(d[2], 'l');
    EXPECT_EQ(d[3], 'l');
    EXPECT_EQ(d[4], 'o');
}

TEST(data_test, at_access) {
    data d{"Hello"};
    
    auto at0 = d.at(0);
    EXPECT_TRUE(at0.has_value());
    EXPECT_EQ(at0.value().get(), 'H');
    
    auto at4 = d.at(4);
    EXPECT_TRUE(at4.has_value());
    EXPECT_EQ(at4.value().get(), 'o');
    
    auto at5 = d.at(5);
    EXPECT_FALSE(at5.has_value());
    EXPECT_EQ(at5.error().code(), static_cast<int>(std::errc::result_out_of_range));
}


TEST(data_test, bytes_access) {
    data d{"Hello"};
    auto bytes_opt = d.bytes();
    EXPECT_TRUE(bytes_opt.has_value());
    const uint8_t* bytes = bytes_opt.value();
    EXPECT_EQ(bytes[0], 'H');
    EXPECT_EQ(bytes[4], 'o');
    
    data empty;
    EXPECT_FALSE(empty.bytes().has_value());
}

TEST(data_test, bytes_operator) {
    data d{"Hello"};
    const uint8_t* bytes = d;
    EXPECT_NE(bytes, nullptr);
    EXPECT_EQ(bytes[0], 'H');
    EXPECT_EQ(bytes[4], 'o');
}

// Modification Tests


TEST(data_test, append_data) {
    data d1{"Hello"};
    data d2{", World!"};
    d1.append(d2);
    EXPECT_EQ(d1.size(), 13u);
    EXPECT_EQ(to_string(d1), "Hello, World!");
}

TEST(data_test, append_buffer) {
    data d{"Hello"};
    const uint8_t buffer[] = {',', ' ', 'W', 'o', 'r', 'l', 'd', '!'};
    d.append(buffer, 8);
    EXPECT_EQ(d.size(), 13u);
    EXPECT_EQ(to_string(d), "Hello, World!");
}

TEST(data_test, append_initializer_list) {
    data d{"Hello"};
    d.append({',', ' ', 'W', 'o', 'r', 'l', 'd', '!'});
    EXPECT_EQ(d.size(), 13u);
    EXPECT_EQ(to_string(d), "Hello, World!");
}

TEST(data_test, clear) {
    data d{"Hello"};
    EXPECT_FALSE(d.empty());
    d.clear();
    EXPECT_TRUE(d.empty());
    EXPECT_EQ(d.size(), 0u);
}

TEST(data_test, resize) {
    data d{"Hello"};
    
    // Resize to larger size with default fill
    d.resize(10);
    EXPECT_EQ(d.size(), 10u);
    EXPECT_EQ(d[4], 'o');
    EXPECT_EQ(d[5], 0);
    
    // Resize to larger size with custom fill
    d.resize(12, 'X');
    EXPECT_EQ(d.size(), 12u);
    EXPECT_EQ(d[10], 'X');
    EXPECT_EQ(d[11], 'X');
    
    // Resize to smaller size
    d.resize(3);
    EXPECT_EQ(d.size(), 3u);
    EXPECT_EQ(to_string(d), "Hel");
}

TEST(data_test, reserve) {
    data d;
    d.reserve(100);
    // Hard to test capacity directly, but should not crash
    EXPECT_TRUE(d.empty());
}

// Comparison Tests
TEST(data_test, equality) {
    data d1{"Hello"};
    data d2{"Hello"};
    data d3{"World"};
    data empty1;
    data empty2;
    
    EXPECT_TRUE(d1.equals(d2));
    EXPECT_FALSE(d1.equals(d3));
    EXPECT_TRUE(empty1.equals(empty2));
    
    EXPECT_EQ(d1, d2);
    EXPECT_NE(d1, d3);
    EXPECT_EQ(empty1, empty2);
}

TEST(data_test, three_way_comparison) {
    data d1{"Hello"};
    data d2{"Hello"};
    data d3{"World"};
    
    EXPECT_EQ(d1 <=> d2, std::strong_ordering::equal);
    EXPECT_EQ(d1 <=> d3, std::strong_ordering::less);
    EXPECT_EQ(d3 <=> d1, std::strong_ordering::greater);
}

// Operations Tests
TEST(data_test, concatenation) {
    data d1{"Hello"};
    data d2{", World!"};
    data result = d1 + d2;
    
    EXPECT_EQ(result.size(), 13u);
    EXPECT_EQ(to_string(result), "Hello, World!");
    
    // Original data unchanged
    EXPECT_EQ(to_string(d1), "Hello");
    EXPECT_EQ(to_string(d2), ", World!");
}

TEST(data_test, concatenation_assignment) {
    data d1{"Hello"};
    data d2{", World!"};
    d1 += d2;
    
    EXPECT_EQ(d1.size(), 13u);
    EXPECT_EQ(to_string(d1), "Hello, World!");
    EXPECT_EQ(to_string(d2), ", World!"); // d2 unchanged
}

// Assignment Tests
TEST(data_test, copy_assignment) {
    data d1{"Hello"};
    data d2{"World"};
    d2 = d1;
    
    EXPECT_EQ(to_string(d2), "Hello");
    EXPECT_EQ(d1, d2);
    EXPECT_NE(d1.bytes().value(), d2.bytes().value()); // Different memory
}

TEST(data_test, move_assignment) {
    data d1{"Hello"};
    data d2{"World"};
    d2 = std::move(d1);
    
    EXPECT_EQ(to_string(d2), "Hello");
    // d1 is in valid but unspecified state
}

TEST(data_test, self_assignment) {
    data d{"Hello"};
    #ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wself-assign-overloaded"
    #endif
    d = d; // Should not crash
    #ifdef __clang__
    #pragma clang diagnostic pop
    #endif
    EXPECT_EQ(to_string(d), "Hello");
}

// Conversion Tests
TEST(data_test, string_conversion) {
    data d{"Hello, 世界!"};
    std::string str = to_string(d);
    EXPECT_EQ(str, "Hello, 世界!");
    
    // Implicit conversion
    std::string implicit = d;
    EXPECT_EQ(implicit, "Hello, 世界!");
}

TEST(data_test, empty_string_conversion) {
    data d;
    EXPECT_EQ(to_string(d), "");
    
    std::string implicit = d;
    EXPECT_EQ(implicit, "");
}

// Iterator Tests
TEST(data_test, iterator_basic) {
    data d{"Hello"};
    auto it = d.begin();
    EXPECT_EQ(*it, 'H');
    ++it;
    EXPECT_EQ(*it, 'e');
    it++;
    EXPECT_EQ(*it, 'l');
}

TEST(data_test, iterator_range_based_for) {
    data d{"Hello"};
    std::string result;
    for (const auto& byte : d) {
        result += static_cast<char>(byte);
    }
    EXPECT_EQ(result, "Hello");
}

TEST(data_test, iterator_empty) {
    data d;
    EXPECT_EQ(d.begin(), d.end());
    EXPECT_EQ(d.cbegin(), d.cend());
}

TEST(data_test, iterator_stl_algorithms) {
    data d{0x05, 0x01, 0x04, 0x02, 0x03};
    
    // Sort using STL algorithm
    std::sort(d.begin(), d.end());
    
    EXPECT_EQ(d[0], 0x01);
    EXPECT_EQ(d[1], 0x02);
    EXPECT_EQ(d[2], 0x03);
    EXPECT_EQ(d[3], 0x04);
    EXPECT_EQ(d[4], 0x05);
    
    // Find using STL algorithm
    auto it = std::find(d.begin(), d.end(), 0x03);
    EXPECT_NE(it, d.end());
    EXPECT_EQ(*it, 0x03);
}

TEST(data_test, const_iterator) {
    const data d{"Hello"};
    
    auto it = d.begin();
    EXPECT_EQ(*it, 'H');
    
    auto cit = d.cbegin();
    EXPECT_EQ(*cit, 'H');
    
    // Range-based for with const data
    std::string result;
    for (const auto& byte : d) {
        result += static_cast<char>(byte);
    }
    EXPECT_EQ(result, "Hello");
}

// Value Integration Tests
TEST(data_test, value_integration) {
    data d{"Hello"};
    value v = d;
    
    EXPECT_TRUE(v.is<data>());
    EXPECT_FALSE(v.is<string>());
    
    auto retrieved = value_cast<data>(v);
    EXPECT_EQ(to_string(retrieved), "Hello");
    EXPECT_EQ(retrieved, d);
}

TEST(data_test, value_assignment) {
    data d{"Hello"};
    value v;
    v = d;
    
    EXPECT_TRUE(v.is<data>());
    
    auto retrieved = v.as<data>();
    EXPECT_EQ(to_string(retrieved), "Hello");
}

// Size and Capacity Tests
TEST(data_test, size_properties) {
    data d;
    EXPECT_TRUE(d.empty());
    EXPECT_GT(d.max_size(), 0u);
    
    d.append({'A'});
    EXPECT_EQ(d.size(), 1u);
    EXPECT_FALSE(d.empty());
}

// Stream Output Tests
TEST(data_test, stream_output) {
    data d{"Hello"};
    std::ostringstream oss;
    oss << d;
    EXPECT_EQ(oss.str(), "Hello");
}

TEST(data_test, stream_output_empty) {
    data d;
    std::ostringstream oss;
    oss << d;
    EXPECT_EQ(oss.str(), "");
}

// Binary Data Tests
TEST(data_test, binary_data) {
    data d{0x00, 0xFF, 0x80, 0x7F, 0x01};
    
    EXPECT_EQ(d.size(), 5u);
    EXPECT_EQ(d[0], 0x00);
    EXPECT_EQ(d[1], 0xFF);
    EXPECT_EQ(d[2], 0x80);
    EXPECT_EQ(d[3], 0x7F);
    EXPECT_EQ(d[4], 0x01);
}

// Edge Cases Tests
TEST(data_test, large_data) {
    std::vector<uint8_t> large_data(10000, 0xAB);
    data d{large_data};
    
    EXPECT_EQ(d.size(), 10000u);
    EXPECT_EQ(d[0], 0xAB);
    EXPECT_EQ(d[9999], 0xAB);
}

TEST(data_test, unicode_string) {
    data d{"Hello, 世界! 🌍"};
    std::string str = to_string(d);
    EXPECT_EQ(str, "Hello, 世界! 🌍");
    EXPECT_GT(d.size(), 12u); // More bytes than characters due to UTF-8
}

// Memory Safety Tests
TEST(data_test, memory_safety) {
    data d{"Hello"};
    
    // Add more data to potentially trigger reallocation
    d.append({',', ' ', 'W', 'o', 'r', 'l', 'd', '!'});
    
    // Data should still be correct after potential reallocation
    EXPECT_EQ(to_string(d), "Hello, World!");
    EXPECT_EQ(d.size(), 13u);
}