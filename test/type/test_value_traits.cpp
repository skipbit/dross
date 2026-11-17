#include "dross/type/value_traits.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct point {
    int x;
    int y;

    bool operator==(const point& other) const = default;
};

struct opaque { };

template <typename T>
void expect_round_trip(const T& object)
{
    const auto back = dross::value_traits<T>::from_value(dross::value_traits<T>::to_value(object));
    ASSERT_TRUE(back.has_value()) << back.error().message();
    EXPECT_EQ(*back, object);
}

template <typename T>
void expect_failure(const dross::value& held, const dross::value_errc expected)
{
    const auto back = dross::value_traits<T>::from_value(held);
    ASSERT_FALSE(back.has_value());
    EXPECT_EQ(back.error(), dross::error(expected));
}

}  // namespace

template <>
struct dross::value_traits<point> {
    static dross::value to_value(const point& p)
    {
        dross::dictionary d;
        d["x"] = p.x;
        d["y"] = p.y;
        return d;
    }

    static std::expected<point, dross::error> from_value(const dross::value& v)
    {
        const auto* d = v.get_if<dross::dictionary>();
        if (d == nullptr || ! d->contains("x") || ! d->contains("y")) {
            return std::unexpected(dross::value_errc::kind_mismatch);
        }
        auto x = dross::value_traits<int>::from_value((*d)["x"]);
        auto y = dross::value_traits<int>::from_value((*d)["y"]);
        if (! x || ! y) {
            return std::unexpected(! x ? x.error() : y.error());
        }
        return point{ *x, *y };
    }
};

static_assert(dross::value_convertible_type<dross::value>);
static_assert(dross::value_convertible_type<dross::boolean>);
static_assert(dross::value_convertible_type<dross::number>);
static_assert(dross::value_convertible_type<dross::string>);
static_assert(dross::value_convertible_type<dross::array>);
static_assert(dross::value_convertible_type<dross::dictionary>);
static_assert(dross::value_convertible_type<dross::timestamp>);
static_assert(dross::value_convertible_type<dross::data>);
static_assert(dross::value_convertible_type<bool>);
static_assert(dross::value_convertible_type<std::int8_t>);
static_assert(dross::value_convertible_type<std::uint64_t>);
static_assert(dross::value_convertible_type<float>);
static_assert(dross::value_convertible_type<long double>);
static_assert(dross::value_convertible_type<std::string>);
static_assert(dross::value_convertible_type<point>);
static_assert(dross::value_convertible_type<std::vector<point>>);
static_assert(dross::value_convertible_type<std::optional<std::vector<int>>>);
static_assert(dross::value_convertible_type<std::map<std::string, point>>);
static_assert(dross::value_convertible_type<std::unordered_map<std::string, std::vector<double>>>);

static_assert(! dross::value_convertible_type<char>);
static_assert(! dross::value_convertible_type<char8_t>);
static_assert(! dross::value_convertible_type<wchar_t>);
static_assert(! dross::value_convertible_type<const char*>);
static_assert(! dross::value_convertible_type<dross::timezone>);
static_assert(! dross::value_convertible_type<opaque>);
static_assert(! dross::value_convertible_type<std::vector<opaque>>);
static_assert(! dross::value_convertible_type<std::optional<char>>);
static_assert(! dross::value_convertible_type<std::map<int, int>>);
static_assert(! dross::value_convertible_type<std::unordered_map<std::string, opaque>>);

TEST(value_traits_test, value_is_kept_as_it_is)
{
    expect_round_trip(dross::value{});
    expect_round_trip(dross::value{ 42 });
    expect_round_trip(dross::value{ "text" });
}

TEST(value_traits_test, each_kind_comes_back)
{
    expect_round_trip(dross::boolean{ true });
    expect_round_trip(dross::number{ "123456789012345678901234567890.5" });
    expect_round_trip(dross::string{ "text" });
    expect_round_trip(dross::array{ dross::value{ 1 }, dross::value{ "two" } });
    dross::dictionary d;
    d["key"] = 1;
    expect_round_trip(d);
    expect_round_trip(dross::timestamp{ 2024, 1, 21, 15, 30, 0, dross::timezone::offset(9) });
    expect_round_trip(dross::data{ 0x00, 0xff });
}

TEST(value_traits_test, a_kind_rejects_another_kind_and_null)
{
    expect_failure<dross::number>(dross::value{ "1" }, dross::value_errc::kind_mismatch);
    expect_failure<dross::string>(dross::value{ 1 }, dross::value_errc::kind_mismatch);
    expect_failure<dross::array>(dross::value{}, dross::value_errc::kind_mismatch);
}

TEST(value_traits_test, bool_comes_back_and_rejects_a_number)
{
    expect_round_trip(true);
    expect_round_trip(false);
    expect_failure<bool>(dross::value{ 1 }, dross::value_errc::kind_mismatch);
}

TEST(value_traits_test, integers_come_back_at_their_limits)
{
    expect_round_trip(std::numeric_limits<std::int8_t>::lowest());
    expect_round_trip(std::numeric_limits<std::int8_t>::max());
    expect_round_trip(std::numeric_limits<std::uint8_t>::max());
    expect_round_trip(std::numeric_limits<std::int64_t>::lowest());
    expect_round_trip(std::numeric_limits<std::int64_t>::max());
    expect_round_trip(std::numeric_limits<std::uint64_t>::max());
    expect_round_trip(0);
}

TEST(value_traits_test, an_integer_rejects_what_does_not_fit)
{
    expect_failure<std::int8_t>(dross::value{ 128 }, dross::value_errc::out_of_range);
    expect_failure<std::int8_t>(dross::value{ -129 }, dross::value_errc::out_of_range);
    expect_failure<unsigned>(dross::value{ -1 }, dross::value_errc::out_of_range);
    expect_failure<std::uint64_t>(dross::value{ dross::number{ "18446744073709551616" } }, dross::value_errc::out_of_range);
    expect_failure<int>(dross::value{ 2.5 }, dross::value_errc::out_of_range);
    expect_failure<int>(dross::value{ dross::number{ "NaN" } }, dross::value_errc::out_of_range);
    expect_failure<int>(dross::value{ "1" }, dross::value_errc::kind_mismatch);
}

TEST(value_traits_test, an_integer_takes_a_floating_point_number_without_a_fraction)
{
    const auto back = dross::value_traits<int>::from_value(dross::value{ dross::number{ "2.0" } });
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(*back, 2);
}

TEST(value_traits_test, floating_point_comes_back)
{
    expect_round_trip(0.1);
    expect_round_trip(-1.5e-300);
    expect_round_trip(std::numeric_limits<double>::max());
    expect_round_trip(0.1F);
    expect_round_trip(std::numeric_limits<float>::max());
    expect_round_trip(0.5L);
}

TEST(value_traits_test, floating_point_takes_an_integer_number)
{
    const auto back = dross::value_traits<double>::from_value(dross::value{ 3 });
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(*back, 3.0);
}

TEST(value_traits_test, nan_comes_back_and_infinity_becomes_nan)
{
    const auto nan = dross::value_traits<double>::from_value(dross::value_traits<double>::to_value(std::numeric_limits<double>::quiet_NaN()));
    ASSERT_TRUE(nan.has_value());
    EXPECT_TRUE(std::isnan(*nan));

    const auto inf = dross::value_traits<float>::from_value(dross::value_traits<float>::to_value(std::numeric_limits<float>::infinity()));
    ASSERT_TRUE(inf.has_value());
    EXPECT_TRUE(std::isnan(*inf));
}

TEST(value_traits_test, floating_point_rejects_what_is_beyond_its_largest_value)
{
    expect_failure<float>(dross::value{ 1e39 }, dross::value_errc::out_of_range);
    expect_failure<float>(dross::value{ -1e39 }, dross::value_errc::out_of_range);
    expect_failure<double>(dross::value{ dross::number{ "1e400" } }, dross::value_errc::out_of_range);
    expect_failure<long double>(dross::value{ dross::number{ "1e400" } }, dross::value_errc::out_of_range);
    expect_failure<double>(dross::value{ true }, dross::value_errc::kind_mismatch);
}

namespace {

// (2^digits - 1) * 2^(max_exponent - digits) of T, every digit: its largest
// finite value.
template <typename T>
dross::number largest_finite()
{
    using limits = std::numeric_limits<T>;
    dross::number result{ (std::uint64_t{ 1 } << limits::digits) - 1 };
    for (int i = 0; i < limits::max_exponent - limits::digits; ++i) {
        result = result * dross::number{ 2 };
    }
    return result;
}

// Halfway between the largest finite value of T and the next power of two:
// the least magnitude that rounds to infinity.
template <typename T>
dross::number halfway_to_infinity()
{
    using limits = std::numeric_limits<T>;
    dross::number half_ulp{ 1 };
    for (int i = 0; i < limits::max_exponent - limits::digits - 1; ++i) {
        half_ulp = half_ulp * dross::number{ 2 };
    }
    return largest_finite<T>() + half_ulp;
}

}  // namespace

TEST(value_traits_test, floating_point_takes_its_largest_value_written_in_full)
{
    const dross::number largest = largest_finite<double>();
    ASSERT_EQ(static_cast<std::string>(largest).size(), 309U);
    const auto positive = dross::value_traits<double>::from_value(largest);
    ASSERT_TRUE(positive.has_value());
    EXPECT_EQ(*positive, std::numeric_limits<double>::max());
    const auto negative = dross::value_traits<double>::from_value(dross::number{} - largest);
    ASSERT_TRUE(negative.has_value());
    EXPECT_EQ(*negative, std::numeric_limits<double>::lowest());

    const auto single = dross::value_traits<float>::from_value(largest_finite<float>());
    ASSERT_TRUE(single.has_value());
    EXPECT_EQ(*single, std::numeric_limits<float>::max());
}

TEST(value_traits_test, floating_point_rejects_what_rounds_to_infinity)
{
    expect_failure<double>(halfway_to_infinity<double>(), dross::value_errc::out_of_range);
    expect_failure<double>(dross::number{} - halfway_to_infinity<double>(), dross::value_errc::out_of_range);
    const auto below = dross::value_traits<double>::from_value(halfway_to_infinity<double>() - dross::number{ 1 });
    ASSERT_TRUE(below.has_value());
    EXPECT_EQ(*below, std::numeric_limits<double>::max());

    expect_failure<float>(halfway_to_infinity<float>(), dross::value_errc::out_of_range);
    // Just below the bound, rounding to double first lands on the bound.
    expect_failure<float>(halfway_to_infinity<float>() - dross::number{ 1 }, dross::value_errc::out_of_range);
}

TEST(value_traits_test, std_string_comes_back_byte_for_byte)
{
    expect_round_trip(std::string{ "text" });
    expect_round_trip(std::string{ "\xff\xfe not UTF-8" });
    expect_round_trip(std::string{});
    expect_failure<std::string>(dross::value{ 1 }, dross::value_errc::kind_mismatch);
}

TEST(value_traits_test, a_type_given_a_conversion_comes_back)
{
    expect_round_trip(point{ 1, -2 });

    dross::dictionary missing;
    missing["x"] = 1;
    expect_failure<point>(missing, dross::value_errc::kind_mismatch);

    dross::dictionary too_big;
    too_big["x"] = 1;
    too_big["y"] = dross::number{ "1e20" };
    expect_failure<point>(too_big, dross::value_errc::out_of_range);
}

TEST(value_traits_test, errors_name_their_category)
{
    const dross::error mismatch{ dross::value_errc::kind_mismatch };
    EXPECT_EQ(mismatch.domain(), "dross.value");
    EXPECT_EQ(mismatch.message(), "value holds another kind");
    EXPECT_EQ(dross::error(dross::value_errc::out_of_range).message(), "number is out of range for the type");
    EXPECT_EQ(&dross::value_category(), &dross::make_error_code(dross::value_errc::kind_mismatch).category());
}

TEST(value_traits_test, a_vector_comes_back_as_an_array)
{
    expect_round_trip(std::vector<int>{ 1, -2, 3 });
    expect_round_trip(std::vector<point>{ { 1, 2 }, { 3, 4 } });
    expect_round_trip(std::vector<std::string>{});
    expect_round_trip(std::vector<bool>{ true, false });
    EXPECT_EQ(dross::value_traits<std::vector<int>>::to_value({ 1, 2 }), dross::value(dross::array{ dross::value{ 1 }, dross::value{ 2 } }));
}

TEST(value_traits_test, a_vector_rejects_another_kind_and_a_bad_element)
{
    expect_failure<std::vector<int>>(dross::value{ 1 }, dross::value_errc::kind_mismatch);
    expect_failure<std::vector<int>>(dross::array{ dross::value{ 1 }, dross::value{ "two" } }, dross::value_errc::kind_mismatch);
    expect_failure<std::vector<std::int8_t>>(dross::array{ dross::value{ 1 }, dross::value{ 300 } }, dross::value_errc::out_of_range);
}

TEST(value_traits_test, an_optional_is_null_when_empty)
{
    expect_round_trip(std::optional<int>{});
    expect_round_trip(std::optional<int>{ 7 });
    expect_round_trip(std::optional<std::vector<int>>{ std::vector<int>{ 1 } });
    EXPECT_EQ(dross::value_traits<std::optional<int>>::to_value(std::nullopt), dross::value{});
    expect_failure<std::optional<int>>(dross::value{ "7" }, dross::value_errc::kind_mismatch);
}

TEST(value_traits_test, an_empty_optional_inside_an_optional_comes_back_as_an_empty_one)
{
    const std::optional<std::optional<int>> inner_empty{ std::optional<int>{} };
    const auto back = dross::value_traits<std::optional<std::optional<int>>>::from_value(dross::value_traits<std::optional<std::optional<int>>>::to_value(inner_empty));
    ASSERT_TRUE(back.has_value());
    EXPECT_FALSE(back->has_value());
}

TEST(value_traits_test, a_map_keyed_by_string_comes_back_as_a_dictionary)
{
    expect_round_trip(std::map<std::string, int>{ { "a", 1 }, { "b", 2 } });
    expect_round_trip(std::unordered_map<std::string, point>{ { "here", { 1, 2 } } });
    expect_round_trip(std::map<std::string, std::vector<int>>{});

    dross::dictionary expected;
    expected["a"] = 1;
    using string_map = std::map<std::string, int>;
    EXPECT_EQ(dross::value_traits<string_map>::to_value(string_map{ { "a", 1 } }), dross::value(expected));
}

TEST(value_traits_test, a_map_rejects_another_kind_and_a_bad_entry)
{
    expect_failure<std::map<std::string, int>>(dross::array{}, dross::value_errc::kind_mismatch);
    dross::dictionary bad;
    bad["a"] = 1;
    bad["b"] = 2.5;
    expect_failure<std::unordered_map<std::string, int>>(bad, dross::value_errc::out_of_range);
}
