#include "dross/type/array.h"
#include "dross/type/data.h"
#include "dross/type/value_traits.h"
#include "format/cbor.h"
#include "notification/wire.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace {

dross::wire::message round_trip(const dross::wire::message& sent)
{
    const auto back = dross::wire::decode(dross::wire::encode(sent));
    EXPECT_TRUE(back.has_value());
    return back.value_or(dross::wire::unsubscribe{ "not read" });
}

std::vector<std::byte> bytes_of(const dross::value& written)
{
    const dross::data bytes = dross::cbor::serialize(written);
    std::vector<std::byte> result;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        result.push_back(static_cast<std::byte>(bytes[i]));
    }
    return result;
}

dross::value item(const std::int64_t n)
{
    return dross::value_traits<std::int64_t>::to_value(n);
}

dross::value item(const std::string& s)
{
    return dross::value_traits<std::string>::to_value(s);
}

TEST(wire, a_subscribe_comes_back_with_its_name)
{
    const auto back = round_trip(dross::wire::subscribe{ "com.example.progress" });
    ASSERT_TRUE(std::holds_alternative<dross::wire::subscribe>(back));
    EXPECT_EQ(std::get<dross::wire::subscribe>(back).name, "com.example.progress");
}

TEST(wire, an_unsubscribe_comes_back_with_its_name)
{
    const auto back = round_trip(dross::wire::unsubscribe{ "com.example.progress" });
    ASSERT_TRUE(std::holds_alternative<dross::wire::unsubscribe>(back));
    EXPECT_EQ(std::get<dross::wire::unsubscribe>(back).name, "com.example.progress");
}

TEST(wire, a_notice_comes_back_with_its_name_and_value)
{
    const dross::value content{ dross::array{ item(3), item("ten") } };
    const auto back = round_trip(dross::wire::notice{ "com.example.progress", content });
    ASSERT_TRUE(std::holds_alternative<dross::wire::notice>(back));
    EXPECT_EQ(std::get<dross::wire::notice>(back).name, "com.example.progress");
    EXPECT_EQ(std::get<dross::wire::notice>(back).content, content);
}

TEST(wire, bytes_that_are_not_cbor_are_refused)
{
    const std::vector<std::byte> bytes{ std::byte{ 0xff } };
    EXPECT_FALSE(dross::wire::decode(bytes).has_value());
}

TEST(wire, a_value_that_is_not_an_array_is_refused)
{
    EXPECT_FALSE(dross::wire::decode(bytes_of(item("com.example.progress"))).has_value());
}

TEST(wire, an_unknown_kind_is_refused)
{
    EXPECT_FALSE(dross::wire::decode(bytes_of(dross::value{ dross::array{ item(9), item("x") } })).has_value());
}

TEST(wire, a_name_that_is_not_a_string_is_refused)
{
    EXPECT_FALSE(dross::wire::decode(bytes_of(dross::value{ dross::array{ item(1), item(2) } })).has_value());
}

TEST(wire, a_notice_without_its_value_is_refused)
{
    EXPECT_FALSE(dross::wire::decode(bytes_of(dross::value{ dross::array{ item(3), item("x") } })).has_value());
}

TEST(wire, a_subscribe_with_a_value_is_refused)
{
    EXPECT_FALSE(dross::wire::decode(bytes_of(dross::value{ dross::array{ item(1), item("x"), item(0) } })).has_value());
}

}  // namespace
