#include <gtest/gtest.h>
#include "dross/format/toml.h"
#include "dross/type.h"

using namespace dross;

// Helper function to create data from string
data make_data(const std::string& content) {
    return data{content};
}

// Basic Parsing Tests
TEST(toml_simple_test, empty_document) {
    auto result = toml::deserialize(make_data(""));
    ASSERT_TRUE(result.has_value());
    auto& dict = result.value();
    EXPECT_TRUE(dict.empty());
}

TEST(toml_simple_test, simple_key_value) {
    auto result = toml::deserialize(make_data("key = \"value\""));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(dict.size(), 1u);
    EXPECT_TRUE(dict.contains("key"));
    auto value_obj = dict["key"];
    EXPECT_TRUE(value_obj.is<string>());
    auto str_value = value_obj.as<string>();
    EXPECT_EQ(std::string(str_value), "value");
}

TEST(toml_simple_test, integer_value) {
    auto result = toml::deserialize(make_data("number = 42"));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_TRUE(dict.contains("number"));
    auto value_obj = dict["number"];
    EXPECT_TRUE(value_obj.is<number>());
    auto num_value = value_obj.as<number>();
    EXPECT_EQ(num_value, number{"42"});
}

TEST(toml_simple_test, boolean_value) {
    auto result = toml::deserialize(make_data("flag = true"));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_TRUE(dict.contains("flag"));
    auto value_obj = dict["flag"];
    EXPECT_TRUE(value_obj.is<boolean>());
    auto bool_value = value_obj.as<boolean>();
    EXPECT_TRUE(bool_value);
}

// Serialization Tests
TEST(toml_simple_test, serialize_simple) {
    dictionary dict;
    dict["title"] = value(string{"Example"});
    dict["version"] = value(number{"1.0"});

    auto result = toml::serialize(dict);
    ASSERT_TRUE(result.has_value());

    std::string output = result.value();
    EXPECT_FALSE(output.empty());
    // Just check that it contains the expected keys
    EXPECT_TRUE(output.find("title") != std::string::npos);
    EXPECT_TRUE(output.find("version") != std::string::npos);
}

// Error Handling Tests
TEST(toml_simple_test, invalid_syntax) {
    auto result = toml::deserialize(make_data("invalid syntax here"));
    EXPECT_FALSE(result.has_value());
}

TEST(toml_simple_test, serialize_non_dictionary_root) {
    // This test is no longer relevant since serialize now takes dictionary directly
    // We can test serialize with an empty dictionary instead
    dictionary empty_dict;
    auto result = toml::serialize(empty_dict);
    EXPECT_TRUE(result.has_value());
}
