#include "dross/type/array.h"
#include "dross/type/data.h"
#include "dross/type/dictionary.h"
#include "dross/type/number.h"
#include "dross/type/timestamp.h"
#include "dross/type/value.h"
#include "format/cbor.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <system_error>
#include <vector>

namespace {

// The bytes written for input, as lowercase hexadecimal.
std::string written(const dross::value& input)
{
    const dross::data output = dross::cbor::serialize(input);
    std::string result;
    if (const auto start = output.bytes()) {
        static constexpr char kDigits[] = "0123456789abcdef";
        for (std::size_t i = 0; i < output.size(); ++i) {
            result.push_back(kDigits[(*start)[i] >> 4]);
            result.push_back(kDigits[(*start)[i] & 0x0f]);
        }
    }
    return result;
}

}  // namespace

// The expected bytes are those of RFC 8949, Appendix A, and section 3.4.4
// for the decimal fraction.

TEST(cbor_serialize_test, integers_take_the_fewest_bytes)
{
    EXPECT_EQ(written(0), "00");
    EXPECT_EQ(written(1), "01");
    EXPECT_EQ(written(10), "0a");
    EXPECT_EQ(written(23), "17");
    EXPECT_EQ(written(24), "1818");
    EXPECT_EQ(written(25), "1819");
    EXPECT_EQ(written(100), "1864");
    EXPECT_EQ(written(1000), "1903e8");
    EXPECT_EQ(written(1000000), "1a000f4240");
    EXPECT_EQ(written(1000000000000), "1b000000e8d4a51000");
    EXPECT_EQ(written(dross::number{ "18446744073709551615" }), "1bffffffffffffffff");
}

TEST(cbor_serialize_test, negative_integers_take_the_fewest_bytes)
{
    EXPECT_EQ(written(-1), "20");
    EXPECT_EQ(written(-10), "29");
    EXPECT_EQ(written(-100), "3863");
    EXPECT_EQ(written(-1000), "3903e7");
    EXPECT_EQ(written(dross::number{ "-18446744073709551616" }), "3bffffffffffffffff");
}

TEST(cbor_serialize_test, integers_beyond_64_bits_are_bignums)
{
    EXPECT_EQ(written(dross::number{ "18446744073709551616" }), "c249010000000000000000");
    EXPECT_EQ(written(dross::number{ "-18446744073709551617" }), "c349010000000000000000");
}

TEST(cbor_serialize_test, a_floating_point_number_is_a_decimal_fraction)
{
    EXPECT_EQ(written(dross::number{ "273.15" }), "c48221196ab3");
    EXPECT_EQ(written(dross::number{ "-0.5" }), "c4822024");
    EXPECT_EQ(written(dross::number{ "2.0" }), "c4820002");
    EXPECT_EQ(written(dross::number{ "0.0" }), "c4820000");
    EXPECT_EQ(written(dross::number{ "0.0015" }), "c482230f");
}

TEST(cbor_serialize_test, a_decimal_fraction_carries_a_bignum_mantissa)
{
    // 18446744073709551616 * 10^-1
    EXPECT_EQ(written(dross::number{ "1844674407370955161.6" }), "c48220c249010000000000000000");
}

TEST(cbor_serialize_test, nan_is_the_half_precision_nan)
{
    EXPECT_EQ(written(dross::number{ "NaN" }), "f97e00");
}

TEST(cbor_serialize_test, simple_values)
{
    EXPECT_EQ(written(dross::value{}), "f6");
    EXPECT_EQ(written(false), "f4");
    EXPECT_EQ(written(true), "f5");
}

TEST(cbor_serialize_test, strings_are_text_strings_of_their_bytes)
{
    EXPECT_EQ(written(""), "60");
    EXPECT_EQ(written("a"), "6161");
    EXPECT_EQ(written("IETF"), "6449455446");
    EXPECT_EQ(written("\xc3\xbc"), "62c3bc");
    EXPECT_EQ(written("\xff"), "61ff");
}

TEST(cbor_serialize_test, data_is_a_byte_string)
{
    EXPECT_EQ(written(dross::data{}), "40");
    EXPECT_EQ(written(dross::data{ 0x01, 0x02, 0x03, 0x04 }), "4401020304");
}

TEST(cbor_serialize_test, arrays_and_dictionaries_have_a_definite_length)
{
    EXPECT_EQ(written(dross::array{}), "80");
    EXPECT_EQ(written(dross::array{ 1, 2, 3 }), "83010203");
    EXPECT_EQ(written(dross::array{ 1, dross::array{ 2, 3 }, dross::array{ 4, 5 } }), "8301820203820405");

    std::vector<dross::value> many;
    for (int i = 1; i <= 25; ++i) {
        many.emplace_back(i);
    }
    dross::array long_array;
    for (const auto& item : many) {
        long_array.append(item);
    }
    EXPECT_EQ(written(long_array), "98190102030405060708090a0b0c0d0e0f101112131415161718181819");

    EXPECT_EQ(written(dross::dictionary{}), "a0");
    dross::dictionary entries;
    entries["a"] = 1;
    entries["b"] = dross::array{ 2, 3 };
    EXPECT_EQ(written(entries), "a26161016162820203");
}

TEST(cbor_serialize_test, a_timestamp_is_a_date_time_string)
{
    const auto utc = dross::timestamp::from_string("2013-03-21T20:04:00Z");
    ASSERT_TRUE(utc.has_value());
    EXPECT_EQ(written(*utc), "c074323031332d30332d32315432303a30343a30305a");

    const auto offset = dross::timestamp::from_string("2024-01-21T15:30:00.5+09:00");
    ASSERT_TRUE(offset.has_value());
    // "2024-01-21T15:30:00.5+09:00", 27 bytes
    EXPECT_EQ(written(*offset), "c0781b323032342d30312d32315431353a33303a30302e352b30393a3030");
}

namespace {

dross::data from_hex(const std::string& hex)
{
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        bytes.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return dross::data{ bytes };
}

std::expected<dross::value, dross::error> read_hex(const std::string& hex)
{
    return dross::cbor::deserialize(from_hex(hex));
}

dross::value read_ok(const std::string& hex)
{
    const auto result = read_hex(hex);
    EXPECT_TRUE(result.has_value()) << hex << ": " << (result ? std::string{} : result.error().message());
    return result.value_or(dross::value{ "unread" });
}

// The detail of the error reading hex gives, or "" when it reads.
std::string refusal(const std::string& hex)
{
    const auto result = read_hex(hex);
    if (result) {
        return {};
    }
    EXPECT_EQ(result.error(), dross::error(std::errc::invalid_argument));
    return result.error().detail();
}

void expect_round_trip(const dross::value& input)
{
    const auto back = dross::cbor::deserialize(dross::cbor::serialize(input));
    ASSERT_TRUE(back.has_value()) << back.error().message();
    EXPECT_EQ(*back, input);
}

// Arrays nested depth deep around an empty one.
std::string nested_arrays(const int depth)
{
    std::string hex;
    for (int i = 1; i < depth; ++i) {
        hex += "81";
    }
    return hex + "80";
}

}  // namespace

TEST(cbor_deserialize_test, what_is_written_comes_back_equal)
{
    const std::vector<dross::value> inputs{
        dross::value{},
        true,
        false,
        0,
        23,
        24,
        -1,
        -1000,
        dross::number{ "18446744073709551615" },
        dross::number{ "-18446744073709551616" },
        dross::number{ "18446744073709551616" },
        dross::number{ "-18446744073709551617" },
        dross::number{ "123456789012345678901234567890123456789" },
        "",
        "IETF",
        "\xff\xfe",
        dross::data{},
        dross::data{ 0x00, 0xff },
        dross::array{},
        dross::array{ 1, dross::array{ 2, 3 }, "four" },
    };
    for (const auto& input : inputs) {
        expect_round_trip(input);
    }
    dross::dictionary entries;
    entries["a"] = 1;
    entries["b"] = dross::array{ 2, dross::dictionary{} };
    expect_round_trip(entries);
}

TEST(cbor_deserialize_test, a_number_comes_back_with_its_form)
{
    for (const char* text : { "273.15", "-0.5", "2.0", "0.0", "0.0015", "1844674407370955161.6", "-12345678901234567890.123456789", "1e3" }) {
        const dross::number input{ text };
        const auto back = dross::cbor::deserialize(dross::cbor::serialize(input));
        ASSERT_TRUE(back.has_value()) << text;
        const auto* amount = back->get_if<dross::number>();
        ASSERT_NE(amount, nullptr) << text;
        EXPECT_EQ(*amount, input) << text;
        EXPECT_TRUE(amount->is_floating_point()) << text;
    }
    const auto integer = dross::cbor::deserialize(dross::cbor::serialize(dross::number{ 2 }));
    ASSERT_TRUE(integer.has_value());
    EXPECT_FALSE(integer->get_if<dross::number>()->is_floating_point());

    const auto nan = dross::cbor::deserialize(dross::cbor::serialize(dross::number{ "NaN" }));
    ASSERT_TRUE(nan.has_value());
    EXPECT_TRUE(nan->get_if<dross::number>()->is_nan());
}

TEST(cbor_deserialize_test, a_timestamp_comes_back_with_its_offset_and_nanoseconds)
{
    for (const char* text : { "2024-01-21T15:30:00.123456789+09:00", "0000-01-01T00:00:00-23:59", "9999-12-31T23:59:59.999999999Z" }) {
        const auto input = dross::timestamp::from_string(text);
        ASSERT_TRUE(input.has_value()) << text;
        const auto back = dross::cbor::deserialize(dross::cbor::serialize(*input));
        ASSERT_TRUE(back.has_value()) << text;
        const auto* moment = back->get_if<dross::timestamp>();
        ASSERT_NE(moment, nullptr) << text;
        EXPECT_EQ(moment->format(dross::timestamp::format_type::rfc3339), input->format(dross::timestamp::format_type::rfc3339));
    }
}

TEST(cbor_deserialize_test, integers_not_in_the_fewest_bytes_are_read)
{
    EXPECT_EQ(read_ok("1800"), dross::value{ 0 });
    EXPECT_EQ(read_ok("1b0000000000000001"), dross::value{ 1 });
    EXPECT_EQ(read_ok("3900ff"), dross::value{ -256 });
    EXPECT_EQ(read_ok("c2420001"), dross::value{ 1 });
}

TEST(cbor_deserialize_test, floats_are_read_as_floating_point_numbers)
{
    // RFC 8949, Appendix A.
    EXPECT_EQ(read_ok("f90000"), dross::value{ 0.0 });
    EXPECT_EQ(read_ok("f93c00"), dross::value{ 1.0 });
    EXPECT_EQ(read_ok("f93e00"), dross::value{ 1.5 });
    EXPECT_EQ(read_ok("f97bff"), dross::value{ 65504.0 });
    EXPECT_EQ(read_ok("f90001"), dross::value{ 5.960464477539063e-8 });
    EXPECT_EQ(read_ok("f90400"), dross::value{ 0.00006103515625 });
    EXPECT_EQ(read_ok("f9c400"), dross::value{ -4.0 });
    EXPECT_EQ(read_ok("fa47c35000"), dross::value{ 100000.0 });
    EXPECT_EQ(read_ok("fa7f7fffff"), dross::value{ dross::number{ 3.4028234663852886e+38F } });
    EXPECT_EQ(read_ok("fb3ff199999999999a"), dross::value{ 1.1 });
    EXPECT_EQ(read_ok("fbc010666666666666"), dross::value{ -4.1 });
    EXPECT_TRUE(read_ok("f93c00").as<dross::number>().is_floating_point());
    EXPECT_TRUE(read_ok("f97e00").as<dross::number>().is_nan());
    EXPECT_TRUE(read_ok("fa7fc00000").as<dross::number>().is_nan());
    EXPECT_TRUE(read_ok("fb7ff8000000000000").as<dross::number>().is_nan());
}

TEST(cbor_deserialize_test, an_epoch_time_is_read_as_utc)
{
    // RFC 8949, Appendix A.
    EXPECT_EQ(read_ok("c11a514b67b0").as<dross::timestamp>().format(dross::timestamp::format_type::rfc3339), "2013-03-21T20:04:00Z");
    EXPECT_EQ(read_ok("c1fb41d452d9ec200000").as<dross::timestamp>().format(dross::timestamp::format_type::rfc3339), "2013-03-21T20:04:00.5Z");
    // -1.5 seconds: 1969-12-31T23:59:58.5Z.
    EXPECT_EQ(read_ok("c1f9be00").as<dross::timestamp>().format(dross::timestamp::format_type::rfc3339), "1969-12-31T23:59:58.5Z");
}

TEST(cbor_deserialize_test, input_that_ends_inside_an_item_is_refused)
{
    EXPECT_EQ(refusal(""), "byte 0: the input ends inside a data item");
    EXPECT_EQ(refusal("18"), "byte 0: the input ends inside a data item");
    EXPECT_EQ(refusal("1901"), "byte 0: the input ends inside a data item");
    EXPECT_EQ(refusal("6261"), "byte 0: a length goes past the end of the input");
    EXPECT_EQ(refusal("820019"), "byte 2: the input ends inside a data item");
    EXPECT_EQ(refusal("a16161"), "byte 3: the input ends inside a data item");
}

TEST(cbor_deserialize_test, bytes_after_the_item_are_refused)
{
    EXPECT_EQ(refusal("0000"), "byte 1: bytes follow the data item");
}

TEST(cbor_deserialize_test, a_count_beyond_the_bytes_left_is_refused_before_it_is_made)
{
    EXPECT_EQ(refusal("830102"), "byte 0: an array has more items than bytes left");
    EXPECT_EQ(refusal("9bffffffffffffffff"), "byte 0: an array has more items than bytes left");
    EXPECT_EQ(refusal("bbffffffffffffffff"), "byte 0: a map has more entries than bytes left");
    EXPECT_EQ(refusal("5bffffffffffffffff"), "byte 0: a length goes past the end of the input");
    EXPECT_EQ(refusal("7bffffffffffffffff"), "byte 0: a length goes past the end of the input");
}

TEST(cbor_deserialize_test, nesting_is_read_to_a_depth_of_128)
{
    EXPECT_EQ(refusal(nested_arrays(128)), "");
    EXPECT_EQ(refusal(nested_arrays(129)), "byte 128: arrays and maps are nested deeper than 128");

    std::string maps;
    for (int i = 1; i < 129; ++i) {
        maps += "a16161";
    }
    EXPECT_EQ(refusal(maps + "a0"), "byte 384: arrays and maps are nested deeper than 128");
}

TEST(cbor_deserialize_test, map_keys_must_be_unique_text_strings)
{
    EXPECT_EQ(refusal("a10102"), "byte 1: a map key is not a text string");
    EXPECT_EQ(refusal("a2616101616102"), "byte 4: a map key appears twice");
}

TEST(cbor_deserialize_test, what_a_value_cannot_hold_is_refused)
{
    EXPECT_EQ(refusal("f97c00"), "byte 0: an infinity cannot be held");
    EXPECT_EQ(refusal("f9fc00"), "byte 0: an infinity cannot be held");
    EXPECT_EQ(refusal("fa7f800000"), "byte 0: an infinity cannot be held");
    EXPECT_EQ(refusal("fbfff0000000000000"), "byte 0: an infinity cannot be held");
    // An exponent of 4097.
    EXPECT_EQ(refusal("c48219100101"), "byte 5: a decimal fraction has more than 4096 digits");
    EXPECT_EQ(refusal("c063616263"), "byte 1: a date and time string is not one a timestamp holds");
    EXPECT_EQ(refusal("c11b7fffffffffffffff"), "byte 1: an epoch time is outside the years 0 to 9999");
    EXPECT_EQ(refusal("c1f97e00"), "byte 1: an epoch time is outside the years 0 to 9999");
}

TEST(cbor_deserialize_test, a_bignum_of_more_than_4096_digits_is_refused)
{
    // 10^4096 - 1 has 4096 digits and needs 1701 bytes; 1701 bytes of 0xff
    // make a number of 4097 digits.
    const std::string length = "5906a5";
    std::string all_ones;
    for (int i = 0; i < 1701; ++i) {
        all_ones += "ff";
    }
    EXPECT_EQ(refusal("c2" + length + all_ones), "byte 1: a bignum has more than 4096 digits");
    EXPECT_EQ(refusal("c25906a6" + all_ones + "ff"), "byte 1: a bignum has more than 4096 digits");

    const dross::number largest{ std::string(4096, '9') };
    expect_round_trip(largest);
}

TEST(cbor_deserialize_test, a_number_made_beyond_4096_digits_is_written_but_refused)
{
    const dross::number wide{ std::string(3000, '9') };
    const dross::number product = wide * wide;
    ASSERT_FALSE(product.is_nan());
    ASSERT_EQ(static_cast<std::string>(product).size(), 6000U);
    const auto back = dross::cbor::deserialize(dross::cbor::serialize(product));
    ASSERT_FALSE(back.has_value());
    EXPECT_EQ(back.error().detail(), "byte 1: a bignum has more than 4096 digits");
}

TEST(cbor_deserialize_test, forms_dross_does_not_write_are_refused)
{
    // RFC 8949, Appendix A.
    EXPECT_EQ(refusal("5f42010243030405ff"), "byte 0: an indefinite length is not read");
    EXPECT_EQ(refusal("9fff"), "byte 0: an indefinite length is not read");
    EXPECT_EQ(refusal("bf61610161629f0203ffff"), "byte 0: an indefinite length is not read");
    EXPECT_EQ(refusal("d74401020304"), "byte 0: tag 23 is not read");
    EXPECT_EQ(refusal("d818456449455446"), "byte 0: tag 24 is not read");
    EXPECT_EQ(refusal("f7"), "byte 0: simple value 23 is not read");
    EXPECT_EQ(refusal("f0"), "byte 0: simple value 16 is not read");
    EXPECT_EQ(refusal("f8ff"), "byte 0: simple value 255 is not read");
    EXPECT_EQ(refusal("1c"), "byte 0: the additional information is reserved");
    EXPECT_EQ(refusal("c001"), "byte 1: a date and time is not a text string");
    EXPECT_EQ(refusal("c201"), "byte 1: a bignum is not a byte string");
    EXPECT_EQ(refusal("c160"), "byte 1: an epoch time is not a number");
    EXPECT_EQ(refusal("c1f6"), "byte 1: an epoch time is not a number");
    EXPECT_EQ(refusal("c4810001"), "byte 1: a decimal fraction is not an array of two items");
    EXPECT_EQ(refusal("c482c2410101"), "byte 2: an integer is expected");
}
