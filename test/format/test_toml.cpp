#include <gtest/gtest.h>
#include "dross/format/toml.h"
#include "dross/type.h"
#include <sstream>

using namespace dross;

// Helper function to create data from string
data make_data(const std::string& content) {
    return data{content};
}

// Basic Parsing Tests
TEST(toml_test, empty_document) {
    auto result = toml::deserialize(make_data(""));
    ASSERT_TRUE(result.has_value());
    auto& dict = result.value();
    EXPECT_TRUE(dict.empty());
}

TEST(toml_test, simple_key_value) {
    auto result = toml::deserialize(make_data("key = \"value\""));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(dict.size(), 1u);
    EXPECT_TRUE(dict.contains("key"));
    EXPECT_EQ(std::string(dict["key"].as<string>()), "value");
}

TEST(toml_test, multiple_key_values) {
    std::string toml_content = R"(
title = "Example"
version = "1.0.0"
description = "A test configuration"
)";

    auto result = toml::deserialize(make_data(toml_content));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(dict.size(), 3u);
    EXPECT_EQ(std::string(dict["title"].as<string>()), "Example");
    EXPECT_EQ(std::string(dict["version"].as<string>()), "1.0.0");
    EXPECT_EQ(std::string(dict["description"].as<string>()), "A test configuration");
}

// String Tests
TEST(toml_test, string_with_escapes) {
    auto result = toml::deserialize(make_data(R"(text = "Line 1\nLine 2\tTabbed")"));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(std::string(dict["text"].as<string>()), "Line 1\nLine 2\tTabbed");
}

TEST(toml_test, quoted_keys) {
    auto result = toml::deserialize(make_data(R"("special key" = "value")"));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_TRUE(dict.contains("special key"));
    EXPECT_EQ(std::string(dict["special key"].as<string>()), "value");
}

// Number Tests
TEST(toml_test, integer_values) {
    std::string toml_content = R"(
positive = 42
negative = -17
zero = 0
)";

    auto result = toml::deserialize(make_data(toml_content));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(dict["positive"].as<number>(), number{"42"});
    EXPECT_EQ(dict["negative"].as<number>(), number{"-17"});
    EXPECT_EQ(dict["zero"].as<number>(), number{"0"});
}

TEST(toml_test, float_values) {
    std::string toml_content = R"(
pi = 3.14159
negative = -2.5
zero_float = 0.0
)";

    auto result = toml::deserialize(make_data(toml_content));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(dict["pi"].as<number>(), number{"3.14159"});
    EXPECT_EQ(dict["negative"].as<number>(), number{"-2.5"});
    EXPECT_EQ(dict["zero_float"].as<number>(), number{"0.0"});
}

// Boolean Tests
TEST(toml_test, boolean_values) {
    std::string toml_content = R"(
enabled = true
disabled = false
)";

    auto result = toml::deserialize(make_data(toml_content));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_TRUE(dict["enabled"].as<boolean>());
    EXPECT_FALSE(dict["disabled"].as<boolean>());
}

// Array Tests
TEST(toml_test, simple_arrays) {
    std::string toml_content = R"(
numbers = [1, 2, 3, 4, 5]
strings = ["alpha", "beta", "gamma"]
mixed = ["text", 42, true]
empty = []
)";

    auto result = toml::deserialize(make_data(toml_content));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();

    // Test numbers array
    auto numbers = dict["numbers"].as<array>();
    EXPECT_EQ(numbers.length(), 5u);
    EXPECT_EQ(numbers[0].as<number>(), number{"1"});
    EXPECT_EQ(numbers[4].as<number>(), number{"5"});

    // Test strings array
    auto strings = dict["strings"].as<array>();
    EXPECT_EQ(strings.length(), 3u);
    EXPECT_EQ(std::string(strings[0].as<string>()), "alpha");
    EXPECT_EQ(std::string(strings[2].as<string>()), "gamma");

    // Test mixed array
    auto mixed = dict["mixed"].as<array>();
    EXPECT_EQ(mixed.length(), 3u);
    EXPECT_EQ(std::string(mixed[0].as<string>()), "text");
    EXPECT_EQ(mixed[1].as<number>(), number{"42"});
    EXPECT_TRUE(mixed[2].as<boolean>());

    // Test empty array
    auto empty = dict["empty"].as<array>();
    EXPECT_EQ(empty.length(), 0u);
}

TEST(toml_test, nested_arrays) {
    auto result = toml::deserialize(make_data(R"(matrix = [[1, 2], [3, 4]])"));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    auto matrix = dict["matrix"].as<array>();
    EXPECT_EQ(matrix.length(), 2u);

    auto row1 = matrix[0].as<array>();
    auto row2 = matrix[1].as<array>();
    EXPECT_EQ(row1[0].as<number>(), number{"1"});
    EXPECT_EQ(row1[1].as<number>(), number{"2"});
    EXPECT_EQ(row2[0].as<number>(), number{"3"});
    EXPECT_EQ(row2[1].as<number>(), number{"4"});
}

// Inline Table Tests
TEST(toml_test, inline_tables) {
    auto result = toml::deserialize(make_data(R"(point = {x = 1, y = 2})"));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    auto point = dict["point"].as<dictionary>();
    EXPECT_EQ(point["x"].as<number>(), number{"1"});
    EXPECT_EQ(point["y"].as<number>(), number{"2"});
}

// Table Tests
TEST(toml_test, simple_table) {
    std::string toml_content = R"(
title = "Main"

[database]
server = "192.168.1.1"
ports = [8001, 8001, 8002]
connection_max = 5000
)";

    auto result = toml::deserialize(make_data(toml_content));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(std::string(dict["title"].as<string>()), "Main");

    auto database = dict["database"].as<dictionary>();
    EXPECT_EQ(std::string(database["server"].as<string>()), "192.168.1.1");
    EXPECT_EQ(database["connection_max"].as<number>(), number{"5000"});

    auto ports = database["ports"].as<array>();
    EXPECT_EQ(ports.length(), 3u);
    EXPECT_EQ(ports[0].as<number>(), number{"8001"});
}

TEST(toml_test, nested_tables) {
    std::string toml_content = R"(
[servers.alpha]
ip = "10.0.0.1"
dc = "eqdc10"

[servers.beta]
ip = "10.0.0.2"
dc = "eqdc10"
)";

    auto result = toml::deserialize(make_data(toml_content));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    auto servers = dict["servers"].as<dictionary>();

    auto alpha = servers["alpha"].as<dictionary>();
    EXPECT_EQ(std::string(alpha["ip"].as<string>()), "10.0.0.1");
    EXPECT_EQ(std::string(alpha["dc"].as<string>()), "eqdc10");

    auto beta = servers["beta"].as<dictionary>();
    EXPECT_EQ(std::string(beta["ip"].as<string>()), "10.0.0.2");
    EXPECT_EQ(std::string(beta["dc"].as<string>()), "eqdc10");
}

// Comments Tests
TEST(toml_test, comments) {
    std::string toml_content = R"(
# This is a comment
key = "value" # This is also a comment
# Another comment
another_key = 42
)";

    auto result = toml::deserialize(make_data(toml_content));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(dict.size(), 2u);
    EXPECT_EQ(std::string(dict["key"].as<string>()), "value");
    EXPECT_EQ(dict["another_key"].as<number>(), number{"42"});
}

// Serialization Tests
TEST(toml_test, serialize_simple) {
    dictionary dict;
    dict["title"] = value(string{"Example"});
    dict["version"] = value(number{"1.0"});
    dict["enabled"] = value(boolean{true});

    auto result = toml::serialize(dict);
    ASSERT_TRUE(result.has_value());

    std::string output = result.value();
    EXPECT_TRUE(output.find("title = \"Example\"") != std::string::npos);
    EXPECT_TRUE(output.find("version = 1.0") != std::string::npos);
    EXPECT_TRUE(output.find("enabled = true") != std::string::npos);
}

TEST(toml_test, serialize_array) {
    dictionary dict;
    array numbers;
    numbers.append(value(number{"1"}));
    numbers.append(value(number{"2"}));
    numbers.append(value(number{"3"}));
    dict["numbers"] = value(numbers);

    auto result = toml::serialize(dict);
    ASSERT_TRUE(result.has_value());

    std::string output = result.value();
    EXPECT_TRUE(output.find("numbers = [1, 2, 3]") != std::string::npos);
}

TEST(toml_test, serialize_nested_table) {
    dictionary root;
    dictionary db;
    db["host"] = value(string{"localhost"});
    db["port"] = value(number{"5432"});
    root["database"] = value(db);

    auto result = toml::serialize(root);
    ASSERT_TRUE(result.has_value());

    std::string output = result.value();
    EXPECT_TRUE(output.find("[database]") != std::string::npos);
    EXPECT_TRUE(output.find("host = \"localhost\"") != std::string::npos);
    EXPECT_TRUE(output.find("port = 5432") != std::string::npos);
}

// Roundtrip Tests
TEST(toml_test, roundtrip_simple) {
    std::string original = R"(title = "Test"
version = 1.5
enabled = true
items = [1, 2, 3])";

    auto parsed = toml::deserialize(make_data(original));
    ASSERT_TRUE(parsed.has_value());

    auto serialized = toml::serialize(parsed.value());
    ASSERT_TRUE(serialized.has_value());

    auto reparsed = toml::deserialize(serialized.value());
    ASSERT_TRUE(reparsed.has_value());

    // Verify content is preserved
    auto& dict1 = parsed.value();
    auto& dict2 = reparsed.value();

    EXPECT_EQ(std::string(dict1["title"].as<string>()), std::string(dict2["title"].as<string>()));
    EXPECT_EQ(dict1["version"].as<number>(), dict2["version"].as<number>());
    EXPECT_EQ(dict1["enabled"].as<boolean>(), dict2["enabled"].as<boolean>());
}

TEST(toml_test, roundtrip_complex) {
    std::string original = R"(
title = "Complex Example"

[database]
host = "localhost"
port = 5432
enabled = true

[servers.alpha]
ip = "10.0.0.1"
ports = [80, 443]

[servers.beta]
ip = "10.0.0.2"
ports = [8080, 8443]
)";

    auto parsed = toml::deserialize(make_data(original));
    ASSERT_TRUE(parsed.has_value()) << "Failed to parse original TOML";

    auto serialized = toml::serialize(parsed.value());
    ASSERT_TRUE(serialized.has_value()) << "Failed to serialize parsed data";

    auto reparsed = toml::deserialize(serialized.value());
    ASSERT_TRUE(reparsed.has_value()) << "Failed to reparse serialized data";

    // Verify structure is preserved
    auto& original_dict = parsed.value();
    auto& roundtrip_dict = reparsed.value();

    EXPECT_EQ(std::string(original_dict["title"].as<string>()), std::string(roundtrip_dict["title"].as<string>()));

    auto orig_db = original_dict["database"].as<dictionary>();
    auto rt_db = roundtrip_dict["database"].as<dictionary>();
    EXPECT_EQ(std::string(orig_db["host"].as<string>()), std::string(rt_db["host"].as<string>()));
    EXPECT_EQ(orig_db["port"].as<number>(), rt_db["port"].as<number>());
}

// Error Handling Tests
TEST(toml_test, invalid_syntax) {
    auto result = toml::deserialize(make_data("invalid syntax here"));
    EXPECT_FALSE(result.has_value());
}

TEST(toml_test, unterminated_string) {
    auto result = toml::deserialize(make_data(R"(key = "unterminated)"));
    EXPECT_FALSE(result.has_value());
}

TEST(toml_test, invalid_number) {
    auto result = toml::deserialize(make_data("key = 1.2.3"));
    EXPECT_FALSE(result.has_value());
}

TEST(toml_test, missing_equals) {
    auto result = toml::deserialize(make_data("key value"));
    EXPECT_FALSE(result.has_value());
}

TEST(toml_test, invalid_array) {
    auto result = toml::deserialize(make_data("arr = [1, 2,"));
    EXPECT_FALSE(result.has_value());
}

// Edge Cases
TEST(toml_test, whitespace_handling) {
    std::string toml_content = R"(
   key1   =   "value1"
	key2	=	42
)";

    auto result = toml::deserialize(make_data(toml_content));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(std::string(dict["key1"].as<string>()), "value1");
    EXPECT_EQ(dict["key2"].as<number>(), number{"42"});
}

TEST(toml_test, empty_values) {
    std::string toml_content = R"(
empty_string = ""
empty_array = []
)";

    auto result = toml::deserialize(make_data(toml_content));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(std::string(dict["empty_string"].as<string>()), "");
    EXPECT_TRUE(dict["empty_array"].as<array>().empty());
}

TEST(toml_test, unicode_strings) {
    auto result = toml::deserialize(make_data(R"(unicode = "Hello, 世界! 🌍")"));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(std::string(dict["unicode"].as<string>()), "Hello, 世界! 🌍");
}

// Serialization Error Tests
TEST(toml_test, serialize_empty_dictionary) {
    dictionary empty_dict;
    auto result = toml::serialize(empty_dict);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

// Performance and Large Data Tests
TEST(toml_test, large_document) {
    std::ostringstream toml_stream;

    // Generate a large TOML document
    for (int i = 0; i < 100; ++i) {
        toml_stream << "key" << i << " = " << i << "\n";
    }

    std::string large_toml = toml_stream.str();
    auto result = toml::deserialize(make_data(large_toml));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(dict.size(), 100u);
    EXPECT_EQ(dict["key0"].as<number>(), number{"0"});
    EXPECT_EQ(dict["key99"].as<number>(), number{"99"});
}

// Additional tests for improved coverage
TEST(toml_test, detailed_type_validation) {
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

TEST(toml_test, detailed_number_validation) {
    auto result = toml::deserialize(make_data("number = 42"));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_TRUE(dict.contains("number"));
    auto value_obj = dict["number"];
    EXPECT_TRUE(value_obj.is<number>());
    auto num_value = value_obj.as<number>();
    EXPECT_EQ(num_value, number{"42"});
}

TEST(toml_test, detailed_boolean_validation) {
    auto result = toml::deserialize(make_data("flag = true"));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_TRUE(dict.contains("flag"));
    auto value_obj = dict["flag"];
    EXPECT_TRUE(value_obj.is<boolean>());
    auto bool_value = value_obj.as<boolean>();
    EXPECT_TRUE(bool_value);
}
