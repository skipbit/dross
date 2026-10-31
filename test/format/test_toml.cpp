#include "dross/format/toml.h"
#include "dross/type.h"

#include <gtest/gtest.h>

#include <functional>
#include <sstream>
#include <utility>
#include <vector>

using namespace dross;

// Helper function to create data from string
data make_data(const std::string& content)
{
    return data{ content };
}

// Basic Parsing Tests
TEST(toml_test, empty_document)
{
    auto result = toml::deserialize(make_data(""));
    ASSERT_TRUE(result.has_value());
    auto& dict = result.value();
    EXPECT_TRUE(dict.empty());
}

TEST(toml_test, simple_key_value)
{
    auto result = toml::deserialize(make_data("key = \"value\""));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(dict.size(), 1u);
    EXPECT_TRUE(dict.contains("key"));
    EXPECT_EQ(std::string(dict["key"].as<string>()), "value");
}

TEST(toml_test, multiple_key_values)
{
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
TEST(toml_test, string_with_escapes)
{
    auto result = toml::deserialize(make_data(R"(text = "Line 1\nLine 2\tTabbed")"));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(std::string(dict["text"].as<string>()), "Line 1\nLine 2\tTabbed");
}

TEST(toml_test, quoted_keys)
{
    auto result = toml::deserialize(make_data(R"("special key" = "value")"));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_TRUE(dict.contains("special key"));
    EXPECT_EQ(std::string(dict["special key"].as<string>()), "value");
}

TEST(toml_test, bare_keys)
{
    auto result = toml::deserialize(make_data("Key = 1\nkey_2 = 2\nA-b = 3\n1234 = 4\n-x = 5\n_ = 6\n"));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(dict.size(), 6u);
    EXPECT_EQ(dict["Key"].as<number>(), number{ "1" });
    EXPECT_EQ(dict["key_2"].as<number>(), number{ "2" });
    EXPECT_EQ(dict["A-b"].as<number>(), number{ "3" });
    EXPECT_EQ(dict["1234"].as<number>(), number{ "4" });
    EXPECT_EQ(dict["-x"].as<number>(), number{ "5" });
    EXPECT_EQ(dict["_"].as<number>(), number{ "6" });

    dictionary input;
    input["Key-2_x"] = value(number{ "1" });
    auto output = toml::serialize(input);
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(std::string(output.value()), "Key-2_x = 1\n");
}

TEST(toml_test, bare_keys_that_start_with_a_digit_or_hyphen_read_back)
{
    dictionary table;
    table["-1"] = value(number{ "1" });
    dictionary input;
    input["1"] = value(number{ "2" });
    input["-x"] = value(table);

    auto output = toml::serialize(input);
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(std::string(output.value()), "1 = 2\n\n[-x]\n-1 = 1\n");

    auto result = toml::deserialize(output.value());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), input);
}

TEST(toml_test, empty_keys_are_written_quoted)
{
    dictionary leaf;
    leaf[""] = value(number{ "1" });
    dictionary keys;
    keys[""] = value(number{ "2" });
    keys["a"] = value(leaf);

    dictionary inner;
    inner["x"] = value(number{ "1" });
    dictionary a;
    a[""] = value(inner);
    a["list"] = value(array{ value(leaf) });
    dictionary tables;
    tables[""] = value(inner);
    tables["a"] = value(a);

    const std::vector<std::pair<dictionary, std::string>> cases = {
        { keys, "\"\" = 2\n\n[a]\n\"\" = 1\n" },
        { tables, "\n[\"\"]\nx = 1\n\n[a]\nlist = [{\"\" = 1}]\n\n[a.\"\"]\nx = 1\n" },
    };
    for (const auto& [input, expected] : cases) {
        auto output = toml::serialize(input);
        ASSERT_TRUE(output.has_value());
        EXPECT_EQ(std::string(output.value()), expected);

        auto result = toml::deserialize(output.value());
        ASSERT_TRUE(result.has_value()) << expected;
        EXPECT_EQ(result.value(), input) << expected;
    }
}

TEST(toml_test, quoted_keys_read_the_same_escapes_as_strings)
{
    auto result = toml::deserialize(make_data(R"("a\bb\fc" = "a\bb\fc")"));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    ASSERT_TRUE(dict.contains("a\bb\fc"));
    EXPECT_EQ(std::string(dict["a\bb\fc"].as<string>()), "a\bb\fc");
}

TEST(toml_test, unicode_escapes)
{
    const std::vector<std::pair<std::string, std::string>> cases = {
        { R"(\u0041)", "A" },
        { R"(\u00e9)", "\xC3\xA9" },
        { R"(\u3042)", "\xE3\x81\x82" },
        { R"(\U0001F600)", "\xF0\x9F\x98\x80" },
        { R"(\u0001)", "\x01" },
        { R"(\u007F)", "\x7F" },
    };
    for (const auto& [escape, expected] : cases) {
        auto result = toml::deserialize(make_data("\"k" + escape + "\" = \"v" + escape + "\""));
        ASSERT_TRUE(result.has_value()) << escape;

        auto& dict = result.value();
        ASSERT_TRUE(dict.contains("k" + expected)) << escape;
        EXPECT_EQ(std::string(dict["k" + expected].as<string>()), "v" + expected) << escape;
    }
}

TEST(toml_test, invalid_unicode_escapes)
{
    for (const std::string escape : { R"(\u004)", R"(\u12G4)", R"(\uD800)", R"(\uDFFF)", R"(\U00110000)", R"(\U0000D800)" }) {
        for (const auto& text : { "k = \"" + escape + "\"", "\"" + escape + "\" = 1" }) {
            auto result = toml::deserialize(make_data(text));
            EXPECT_FALSE(result.has_value()) << text;
            if (! result.has_value()) {
                EXPECT_EQ(result.error(), std::errc::invalid_argument) << text;
            }
        }
    }
}

// Number Tests
TEST(toml_test, integer_values)
{
    std::string toml_content = R"(
positive = 42
negative = -17
zero = 0
)";

    auto result = toml::deserialize(make_data(toml_content));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(dict["positive"].as<number>(), number{ "42" });
    EXPECT_EQ(dict["negative"].as<number>(), number{ "-17" });
    EXPECT_EQ(dict["zero"].as<number>(), number{ "0" });
}

TEST(toml_test, float_values)
{
    std::string toml_content = R"(
pi = 3.14159
negative = -2.5
zero_float = 0.0
)";

    auto result = toml::deserialize(make_data(toml_content));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(dict["pi"].as<number>(), number{ "3.14159" });
    EXPECT_EQ(dict["negative"].as<number>(), number{ "-2.5" });
    EXPECT_EQ(dict["zero_float"].as<number>(), number{ "0.0" });
}

TEST(toml_test, floats_with_an_exponent)
{
    std::string toml_content = R"(
large = 1e20
small = 6.626e-34
signed = 5E+22
leading_zero = 1e06
negative = -2E-2
fraction = 3.0e2
)";

    auto result = toml::deserialize(make_data(toml_content));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(dict["large"].as<number>(), number{ "100000000000000000000" });
    EXPECT_EQ(dict["small"].as<number>(), number{ "0.0000000000000000000000000000000006626" });
    EXPECT_EQ(dict["signed"].as<number>(), number{ "50000000000000000000000" });
    EXPECT_EQ(dict["leading_zero"].as<number>(), number{ "1000000" });
    EXPECT_EQ(dict["negative"].as<number>(), number{ "-0.02" });
    EXPECT_EQ(dict["fraction"].as<number>(), number{ "300" });

    for (const char* text : { "x = 1e\n", "x = 1e+\n", "x = 1E-x\n", "x = 1ee2\n", "x = 1e2.5\n" }) {
        auto invalid = toml::deserialize(make_data(text));
        EXPECT_FALSE(invalid.has_value()) << text;
        if (! invalid.has_value()) {
            EXPECT_EQ(invalid.error(), std::errc::invalid_argument) << text;
        }
    }
}

TEST(toml_test, numbers_that_number_cannot_hold)
{
    auto longest = toml::deserialize(make_data("x = 1e4095\n"));
    ASSERT_TRUE(longest.has_value());
    EXPECT_EQ(longest.value()["x"].as<number>(), number{ "1" + std::string(4095, '0') });

    for (const char* text : { "x = 1e4096\n", "x = 1e-5000\n", "x = -.\n", "x = +.\n", "x = -.e5\n" }) {
        auto result = toml::deserialize(make_data(text));
        EXPECT_FALSE(result.has_value()) << text;
        if (! result.has_value()) {
            EXPECT_EQ(result.error(), std::errc::invalid_argument) << text;
        }
    }
}

// Boolean Tests
TEST(toml_test, boolean_values)
{
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
TEST(toml_test, simple_arrays)
{
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
    EXPECT_EQ(numbers[0].as<number>(), number{ "1" });
    EXPECT_EQ(numbers[4].as<number>(), number{ "5" });

    // Test strings array
    auto strings = dict["strings"].as<array>();
    EXPECT_EQ(strings.length(), 3u);
    EXPECT_EQ(std::string(strings[0].as<string>()), "alpha");
    EXPECT_EQ(std::string(strings[2].as<string>()), "gamma");

    // Test mixed array
    auto mixed = dict["mixed"].as<array>();
    EXPECT_EQ(mixed.length(), 3u);
    EXPECT_EQ(std::string(mixed[0].as<string>()), "text");
    EXPECT_EQ(mixed[1].as<number>(), number{ "42" });
    EXPECT_TRUE(mixed[2].as<boolean>());

    // Test empty array
    auto empty = dict["empty"].as<array>();
    EXPECT_EQ(empty.length(), 0u);
}

TEST(toml_test, nested_arrays)
{
    auto result = toml::deserialize(make_data(R"(matrix = [[1, 2], [3, 4]])"));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    auto matrix = dict["matrix"].as<array>();
    EXPECT_EQ(matrix.length(), 2u);

    auto row1 = matrix[0].as<array>();
    auto row2 = matrix[1].as<array>();
    EXPECT_EQ(row1[0].as<number>(), number{ "1" });
    EXPECT_EQ(row1[1].as<number>(), number{ "2" });
    EXPECT_EQ(row2[0].as<number>(), number{ "3" });
    EXPECT_EQ(row2[1].as<number>(), number{ "4" });
}

TEST(toml_test, arrays_with_a_trailing_comma)
{
    const array expected{ value(number{ "1" }), value(number{ "2" }) };
    for (const char* text : { "x = [1, 2,]\n", "x = [\n  1,\n  2,\n]\n", "x = [\r\n  1,\r\n  2,\r\n]\r\n", "x = [1, 2, # c\n]\n" }) {
        auto result = toml::deserialize(make_data(text));
        ASSERT_TRUE(result.has_value()) << text;
        EXPECT_EQ(result.value()["x"].as<array>(), expected) << text;
    }

    auto nested = toml::deserialize(make_data("x = [[1,], [2,],]\n"));
    ASSERT_TRUE(nested.has_value());
    const array expected_nested{ value(array{ value(number{ "1" }) }), value(array{ value(number{ "2" }) }) };
    EXPECT_EQ(nested.value()["x"].as<array>(), expected_nested);

    // Only arrays allow a trailing comma, not inline tables.
    for (const char* text : { "x = [,]\n", "x = [1,,]\n", "x = [,1]\n", "x = {a = 1,}\n" }) {
        auto result = toml::deserialize(make_data(text));
        EXPECT_FALSE(result.has_value()) << text;
        if (! result.has_value()) {
            EXPECT_EQ(result.error(), std::errc::invalid_argument) << text;
        }
    }
}

// Inline Table Tests
TEST(toml_test, inline_tables)
{
    auto result = toml::deserialize(make_data(R"(point = {x = 1, y = 2})"));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    auto point = dict["point"].as<dictionary>();
    EXPECT_EQ(point["x"].as<number>(), number{ "1" });
    EXPECT_EQ(point["y"].as<number>(), number{ "2" });
}

// Table Tests
TEST(toml_test, simple_table)
{
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
    EXPECT_EQ(database["connection_max"].as<number>(), number{ "5000" });

    auto ports = database["ports"].as<array>();
    EXPECT_EQ(ports.length(), 3u);
    EXPECT_EQ(ports[0].as<number>(), number{ "8001" });
}

TEST(toml_test, nested_tables)
{
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

TEST(toml_test, a_parent_table_defined_after_its_child_keeps_the_child)
{
    auto result = toml::deserialize(make_data("[a.b]\nc = 1\n[a]\nd = 2\n"));
    ASSERT_TRUE(result.has_value());

    auto a = result.value()["a"].as<dictionary>();
    EXPECT_EQ(a["d"].as<number>(), number{ "2" });
    EXPECT_EQ(a["b"].as<dictionary>()["c"].as<number>(), number{ "1" });
}

TEST(toml_test, keys_and_tables_defined_twice)
{
    for (const char* text : { "x = 1\nx = 2\n", "\"x\" = 1\nx = 2\n", "[t]\na = 1\na = 2\n", "a = {x = 1, x = 2}\n", "[t]\n[t]\n", "[t]\n[\"t\"]\n", "[a.b]\n[a.b]\n", "[a]\n[b]\n[a]\n", "t = 1\n[t]\n", "t = {a = 1}\n[t]\n", "a = 1\n[a.b]\n", "[a]\nb = 1\n[a.b]\n", "[a]\nb = {}\n[a.b.c]\n", "[a.b]\n[a]\nb = 1\n" }) {
        auto result = toml::deserialize(make_data(text));
        EXPECT_FALSE(result.has_value()) << text;
        if (! result.has_value()) {
            EXPECT_EQ(result.error(), std::errc::invalid_argument) << text;
        }
    }

    for (const char* text : { "x = 1\n[t]\nx = 1\n", "[a]\n[a.b]\n[a.c]\n", "[a.b.c]\n[a]\n[a.b]\n" }) {
        EXPECT_TRUE(toml::deserialize(make_data(text)).has_value()) << text;
    }
}

TEST(toml_test, arrays_of_tables_are_not_read)
{
    for (const char* text : { "[[t]]\na = 1\n", "[[t]]\na = 1\n[[t]]\na = 2\n" }) {
        auto result = toml::deserialize(make_data(text));
        EXPECT_FALSE(result.has_value()) << text;
        if (! result.has_value()) {
            EXPECT_EQ(result.error(), std::errc::invalid_argument) << text;
        }
    }
}

// Comments Tests
TEST(toml_test, comments)
{
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
    EXPECT_EQ(dict["another_key"].as<number>(), number{ "42" });
}

TEST(toml_test, pairs_and_table_headers_end_their_line)
{
    for (const char* text : { "a = 1 b = 2\n", "a = 1-2 = 3\n", "a = -1 -x = 5\n", "a = \"s\" 1 = 2\n", "[a] b = 1\n", "[a]-b = 1\n", "[a]\nb = 1 c = 2\n" }) {
        auto result = toml::deserialize(make_data(text));
        EXPECT_FALSE(result.has_value()) << text;
        if (! result.has_value()) {
            EXPECT_EQ(result.error(), std::errc::invalid_argument) << text;
        }
    }

    for (const char* text : { "a = 1", "a = 1 # c\nb = 2\n", "a = 1\t\r\nb = 2\r\n", "[a] # c\nb = 1\n" }) {
        EXPECT_TRUE(toml::deserialize(make_data(text)).has_value()) << text;
    }
}

// Serialization Tests
TEST(toml_test, serialize_simple)
{
    dictionary dict;
    dict["title"] = value(string{ "Example" });
    dict["version"] = value(number{ "1.0" });
    dict["enabled"] = value(boolean{ true });

    auto result = toml::serialize(dict);
    ASSERT_TRUE(result.has_value());

    std::string output = result.value();
    EXPECT_TRUE(output.find("title = \"Example\"") != std::string::npos);
    // number keeps no record of a float, so 1.0 is written as 1.
    EXPECT_TRUE(output.find("version = 1\n") != std::string::npos);
    EXPECT_TRUE(output.find("enabled = true") != std::string::npos);
}

TEST(toml_test, serialize_array)
{
    dictionary dict;
    array numbers;
    numbers.append(value(number{ "1" }));
    numbers.append(value(number{ "2" }));
    numbers.append(value(number{ "3" }));
    dict["numbers"] = value(numbers);

    auto result = toml::serialize(dict);
    ASSERT_TRUE(result.has_value());

    std::string output = result.value();
    EXPECT_TRUE(output.find("numbers = [1, 2, 3]") != std::string::npos);
}

TEST(toml_test, serialize_nested_table)
{
    dictionary root;
    dictionary db;
    db["host"] = value(string{ "localhost" });
    db["port"] = value(number{ "5432" });
    root["database"] = value(db);

    auto result = toml::serialize(root);
    ASSERT_TRUE(result.has_value());

    std::string output = result.value();
    EXPECT_TRUE(output.find("[database]") != std::string::npos);
    EXPECT_TRUE(output.find("host = \"localhost\"") != std::string::npos);
    EXPECT_TRUE(output.find("port = 5432") != std::string::npos);
}

// Roundtrip Tests
TEST(toml_test, roundtrip_simple)
{
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

TEST(toml_test, roundtrip_complex)
{
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

TEST(toml_test, roundtrip_of_control_characters)
{
    std::string controls;
    for (int c = 0x00; c <= 0x1F; ++c) {
        controls += static_cast<char>(c);
    }
    controls += '\x7f';

    dictionary dict;
    dict["k" + controls] = value(string{ "v" + controls });
    dict["plain"] = value(string{ "a\001b" });

    auto serialized = toml::serialize(dict);
    ASSERT_TRUE(serialized.has_value());
    std::string output = serialized.value();
    EXPECT_TRUE(output.find(R"(plain = "a\u0001b")") != std::string::npos) << output;

    auto reparsed = toml::deserialize(serialized.value());
    ASSERT_TRUE(reparsed.has_value()) << output;

    auto& result = reparsed.value();
    ASSERT_TRUE(result.contains("k" + controls));
    EXPECT_EQ(std::string(result["k" + controls].as<string>()), "v" + controls);
}

// Error Handling Tests
TEST(toml_test, invalid_syntax)
{
    auto result = toml::deserialize(make_data("invalid syntax here"));
    EXPECT_FALSE(result.has_value());
}

TEST(toml_test, unterminated_string)
{
    auto result = toml::deserialize(make_data(R"(key = "unterminated)"));
    EXPECT_FALSE(result.has_value());
}

TEST(toml_test, invalid_number)
{
    auto result = toml::deserialize(make_data("key = 1.2.3"));
    EXPECT_FALSE(result.has_value());
}

TEST(toml_test, missing_equals)
{
    auto result = toml::deserialize(make_data("key value"));
    EXPECT_FALSE(result.has_value());
}

TEST(toml_test, invalid_array)
{
    auto result = toml::deserialize(make_data("arr = [1, 2,"));
    EXPECT_FALSE(result.has_value());
}

TEST(toml_test, a_parse_error_names_the_line_the_column_and_the_fault)
{
    auto result = toml::deserialize(make_data("a = 1\nb 2\n"));
    ASSERT_FALSE(result.has_value());

    const std::string code_message = std::make_error_code(std::errc::invalid_argument).message();
    EXPECT_EQ(result.error(), std::errc::invalid_argument);
    EXPECT_EQ(result.error().detail(), "line 2, column 3: Expected '='");
    EXPECT_EQ(result.error().message(), code_message + ": line 2, column 3: Expected '='");
}

TEST(toml_test, parse_errors_on_different_lines_name_their_own_line)
{
    const std::vector<std::pair<std::string, std::string>> cases = {
        { "= 1\n", "line 1, " },
        { "a=1\nb=2\nc=3\nd=4\ne=5\nf=6\n]]]\n", "line 7, " },
        { "a = \"oops\n", "line 1, " },
        { "a = 1\n!!!\n", "line 2, " },
    };
    for (const auto& [content, line] : cases) {
        auto result = toml::deserialize(make_data(content));
        ASSERT_FALSE(result.has_value()) << content;
        EXPECT_TRUE(result.error().detail().starts_with(line)) << content << result.error().detail();
    }
}

TEST(toml_test, input_that_is_not_utf8_has_a_detail)
{
    auto result = toml::deserialize(make_data("key = \"\xff\""));
    ASSERT_FALSE(result.has_value());

    EXPECT_EQ(result.error(), std::errc::invalid_argument);
    EXPECT_FALSE(result.error().detail().empty());
}

TEST(toml_test, invalid_utf8)
{
    const std::vector<std::string> invalid = {
        "key = \"\xff\xfe\"",          // bytes that never appear in UTF-8
        "key = \"\x80\"",              // continuation byte without a lead
        "key = \"\xe4\xb8\"",          // sequence cut short by the quote
        "key = 1 # \xe4\xb8",          // sequence cut short by the end of input
        "key = \"\xc0\xaf\"",          // overlong encoding of '/'
        "key = \"\xc1\xbf\"",          // overlong encoding of U+007F
        "key = \"\xe0\x9f\xbf\"",      // overlong encoding of U+07FF
        "key = \"\xf0\x8f\xbf\xbf\"",  // overlong encoding of U+FFFF
        "key = \"\xed\xa0\x80\"",      // surrogate U+D800
        "key = \"\xf4\x90\x80\x80\"",  // beyond U+10FFFF
        "key = \"\xf5\x80\x80\x80\"",  // lead byte beyond U+10FFFF
        "key = 1 # \xff\n",            // in a comment
    };
    for (const auto& content : invalid) {
        auto result = toml::deserialize(make_data(content));
        EXPECT_FALSE(result.has_value()) << content;
        if (! result.has_value()) {
            EXPECT_EQ(result.error(), std::errc::invalid_argument) << content;
        }
    }
}

TEST(toml_test, control_characters)
{
    // '@' marks where the character goes in each form.
    const std::vector<std::pair<std::string, std::string>> forms = {
        { "string", "key = \"a@b\"\n" },
        { "quoted key", "\"a@b\" = 1\n" },
        { "comment", "key = 1 # a@b\n" },
    };
    const auto with = [](std::string form, char c) {
        form[form.find('@')] = c;
        return form;
    };

    for (const auto& [name, form] : forms) {
        SCOPED_TRACE(name);

        EXPECT_TRUE(toml::deserialize(make_data(with(form, '\t'))).has_value());

        for (const char c : { '\x00', '\x01', '\x08', '\n', '\x0b', '\r', '\x1f', '\x7f' }) {
            if ((name == "comment") && (c == '\n')) {
                continue;  // a newline ends a comment
            }
            SCOPED_TRACE(static_cast<int>(c));
            auto result = toml::deserialize(make_data(with(form, c)));
            EXPECT_FALSE(result.has_value());
            if (! result.has_value()) {
                EXPECT_EQ(result.error(), std::errc::invalid_argument);
            }
        }
    }

    EXPECT_TRUE(toml::deserialize(make_data("key = 1 # a\r\n")).has_value());
}

TEST(toml_test, control_characters_outside_strings)
{
    // '@' marks where the character goes in each form. Tab is whitespace in
    // all of them.
    const std::vector<std::pair<std::string, std::string>> forms = {
        { "before equals", "key@= 1\n" },
        { "after equals", "key =@1\n" },
        { "after value", "key = 1@\n" },
        { "own line", "@\nkey = 1\n" },
        { "after table header", "[t]@\nkey = 1\n" },
        { "in array", "key = [1,@2]\n" },
        { "in inline table", "key = { a = 1,@b = 2 }\n" },
        { "end of document", "key = 1\n@" },
        { "comment line", "# a@b\nkey = 1\n" },
        { "comment in array", "key = [1, # a@b\n2]\n" },
        { "comment at end of document", "key = 1 # a@b" },
    };
    const auto with = [](std::string form, char c) {
        form[form.find('@')] = c;
        return form;
    };

    for (const auto& [name, form] : forms) {
        SCOPED_TRACE(name);

        EXPECT_TRUE(toml::deserialize(make_data(with(form, '\t'))).has_value());

        for (const char c : { '\x00', '\x01', '\x08', '\x0b', '\x0c', '\r', '\x1f', '\x7f' }) {
            if ((c == '\r') && (form[form.find('@') + 1] == '\n')) {
                continue;  // a CRLF line ending
            }
            SCOPED_TRACE(static_cast<int>(c));
            auto result = toml::deserialize(make_data(with(form, c)));
            EXPECT_FALSE(result.has_value());
            if (! result.has_value()) {
                EXPECT_EQ(result.error(), std::errc::invalid_argument);
            }
        }
    }
}

TEST(toml_test, crlf_line_endings)
{
    auto result = toml::deserialize(make_data("# head\r\nkey = 1 # tail\r\n\r\n[t]\r\nitems = [\r\n  1,\r\n  2\r\n]\r\n"));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(dict["key"].as<number>(), number{ "1" });
    EXPECT_EQ(dict["t"].as<dictionary>()["items"].as<array>().length(), 2u);
}

TEST(toml_test, utf8_boundaries)
{
    // U+0080, U+0800, U+D7FF, U+E000, U+10000 and U+10FFFF: the edges of
    // the ranges the check narrows.
    const std::vector<std::string> valid = {
        "\xc2\x80",
        "\xe0\xa0\x80",
        "\xed\x9f\xbf",
        "\xee\x80\x80",
        "\xf0\x90\x80\x80",
        "\xf4\x8f\xbf\xbf",
    };
    for (const auto& text : valid) {
        auto result = toml::deserialize(make_data("key = \"" + text + "\""));
        ASSERT_TRUE(result.has_value()) << text;
        EXPECT_EQ(std::string(result.value()["key"].as<string>()), text);
    }
}

// Edge Cases
TEST(toml_test, whitespace_handling)
{
    std::string toml_content = R"(
   key1   =   "value1"
	key2	=	42
)";

    auto result = toml::deserialize(make_data(toml_content));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(std::string(dict["key1"].as<string>()), "value1");
    EXPECT_EQ(dict["key2"].as<number>(), number{ "42" });
}

TEST(toml_test, empty_values)
{
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

TEST(toml_test, unicode_strings)
{
    auto result = toml::deserialize(make_data(R"(unicode = "Hello, 世界! 🌍")"));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_EQ(std::string(dict["unicode"].as<string>()), "Hello, 世界! 🌍");
}

// Serialization Error Tests
TEST(toml_test, serialize_empty_dictionary)
{
    dictionary empty_dict;
    auto result = toml::serialize(empty_dict);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

TEST(toml_test, serialize_rejects_invalid_utf8)
{
    const std::string bad = "\xff";

    dictionary in_value;
    in_value["key"] = value(string{ bad });

    dictionary in_key;
    in_key[bad] = value(number{ "1" });

    dictionary in_array;
    array items;
    items.append(value(string{ bad }));
    in_array["items"] = value(items);

    dictionary in_table;
    dictionary nested;
    nested["key"] = value(number{ "1" });
    in_table[bad] = value(nested);

    for (const auto* dict : { &in_value, &in_key, &in_array, &in_table }) {
        auto result = toml::serialize(*dict);
        EXPECT_FALSE(result.has_value());
        if (! result.has_value()) {
            EXPECT_EQ(result.error(), std::errc::operation_not_supported);
        }
    }
}

TEST(toml_test, serialize_rejects_nan)
{
    const std::vector<std::pair<std::string, std::function<dictionary(const number&)>>> shapes = {
        { "value",
          [](const number& n) {
        dictionary d;
        d["n"] = value(n);
        return d;
    } },
        { "array element",
          [](const number& n) {
        array items;
        items.append(value(number{ "1" }));
        items.append(value(n));
        dictionary d;
        d["items"] = value(items);
        return d;
    } },
        { "nested table",
          [](const number& n) {
        dictionary nested;
        nested["n"] = value(n);
        dictionary d;
        d["table"] = value(nested);
        return d;
    } },
    };

    for (const auto& [shape, make] : shapes) {
        SCOPED_TRACE(shape);

        // The same shape with a number in place of the NaN is written.
        EXPECT_TRUE(toml::serialize(make(number{ "2" })).has_value());

        for (const auto& nan : { number::nan(), number{ "abc" } }) {
            auto result = toml::serialize(make(nan));
            EXPECT_FALSE(result.has_value());
            if (! result.has_value()) {
                EXPECT_EQ(result.error(), std::errc::operation_not_supported);
            }
        }
    }
}

TEST(toml_test, serialize_failures_say_why)
{
    dictionary with_nan;
    with_nan["n"] = value(number::nan());
    auto nan_result = toml::serialize(with_nan);
    ASSERT_FALSE(nan_result.has_value());
    EXPECT_EQ(nan_result.error(), std::errc::operation_not_supported);
    EXPECT_EQ(nan_result.error().detail(), "NaN cannot be written as a TOML value");

    dictionary with_invalid_utf8;
    with_invalid_utf8["s"] = value(string{ std::string{ "\xff" } });
    auto utf8_result = toml::serialize(with_invalid_utf8);
    ASSERT_FALSE(utf8_result.has_value());
    EXPECT_EQ(utf8_result.error(), std::errc::operation_not_supported);
    EXPECT_FALSE(utf8_result.error().detail().empty());
}

TEST(toml_test, serialize_writes_integers_outside_64_bits_as_floats)
{
    const std::vector<std::pair<std::string, std::string>> cases = {
        { "9223372036854775807", "n = 9223372036854775807\n" },
        { "-9223372036854775808", "n = -9223372036854775808\n" },
        { "9223372036854775808", "n = 9223372036854775808.0\n" },
        { "-9223372036854775809", "n = -9223372036854775809.0\n" },
        { "1e30", "n = 1000000000000000000000000000000.0\n" },
        { "9223372036854775808.5", "n = 9223372036854775808.5\n" },
    };

    for (const auto& [text, expected] : cases) {
        SCOPED_TRACE(text);
        dictionary d;
        d["n"] = value(number{ text });

        auto written = toml::serialize(d);
        ASSERT_TRUE(written.has_value());
        EXPECT_EQ(std::string(*written), expected);

        auto read = toml::deserialize(*written);
        ASSERT_TRUE(read.has_value());
        EXPECT_EQ((*read)["n"].as<number>(), number{ text });
    }
}

TEST(toml_test, serialize_writes_back_a_large_float_it_read)
{
    const std::string document = "x = 10000000000000000000.0\n";

    auto read = toml::deserialize(data{ document });
    ASSERT_TRUE(read.has_value());

    auto written = toml::serialize(*read);
    ASSERT_TRUE(written.has_value());
    EXPECT_EQ(std::string(*written), document);
}

// Performance and Large Data Tests
TEST(toml_test, large_document)
{
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
    EXPECT_EQ(dict["key0"].as<number>(), number{ "0" });
    EXPECT_EQ(dict["key99"].as<number>(), number{ "99" });
}

// Additional tests for improved coverage
TEST(toml_test, detailed_type_validation)
{
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

TEST(toml_test, detailed_number_validation)
{
    auto result = toml::deserialize(make_data("number = 42"));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_TRUE(dict.contains("number"));
    auto value_obj = dict["number"];
    EXPECT_TRUE(value_obj.is<number>());
    auto num_value = value_obj.as<number>();
    EXPECT_EQ(num_value, number{ "42" });
}

TEST(toml_test, detailed_boolean_validation)
{
    auto result = toml::deserialize(make_data("flag = true"));
    ASSERT_TRUE(result.has_value());

    auto& dict = result.value();
    EXPECT_TRUE(dict.contains("flag"));
    auto value_obj = dict["flag"];
    EXPECT_TRUE(value_obj.is<boolean>());
    auto bool_value = value_obj.as<boolean>();
    EXPECT_TRUE(bool_value);
}
