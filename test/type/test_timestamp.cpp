#include <dross/type/timestamp.h>
#include <dross/type/timezone.h>
#include <dross/type/value.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>

// Test static factory method
TEST(timestamp_test, now)
{
    auto now1 = dross::timestamp::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto now2 = dross::timestamp::now();

    EXPECT_LT(now1, now2);
    EXPECT_NE(now1, now2);
}

// Test default constructor (epoch)
TEST(timestamp_test, default_constructor)
{
    dross::timestamp epoch;

    EXPECT_EQ(epoch.date().year(), 1970);
    EXPECT_EQ(epoch.date().month(), 1);
    EXPECT_EQ(epoch.date().day(), 1);
    EXPECT_EQ(epoch.time().hour(), 0);  // Default 00:00:00
    EXPECT_EQ(epoch.time().minute(), 0);
    EXPECT_EQ(epoch.time().second(), 0);
    EXPECT_TRUE(epoch.timezone().is_utc());  // Default UTC
}

// Test copy constructor
TEST(timestamp_test, copy_constructor)
{
    dross::timestamp ts1(2024, 1, 21, 15, 30, 45, dross::timezone::offset(9));  // +09:00
    dross::timestamp ts2(ts1);

    EXPECT_EQ(ts1, ts2);
    EXPECT_EQ(ts2.date().year(), 2024);
    EXPECT_EQ(ts2.date().month(), 1);
    EXPECT_EQ(ts2.date().day(), 21);
    EXPECT_EQ(ts2.time().hour(), 15);
    EXPECT_EQ(ts2.time().minute(), 30);
    EXPECT_EQ(ts2.time().second(), 45);
    EXPECT_EQ(ts2.timezone().offset().count(), 540);
}

// Test time_point constructor
TEST(timestamp_test, time_point_constructor)
{
    auto tp = std::chrono::system_clock::now();
    dross::timestamp ts(tp);

    // Convert back and compare
    auto tp_back = static_cast<std::chrono::system_clock::time_point>(ts);
    EXPECT_EQ(tp, tp_back);
    EXPECT_TRUE(ts.timezone().is_utc());  // Default UTC timezone
}

// Test component constructor without timezone
TEST(timestamp_test, component_constructor_without_timezone_is_utc)
{
    dross::timestamp ts(2024, 12, 25, 10, 30, 45);

    EXPECT_EQ(ts.date().year(), 2024);
    EXPECT_EQ(ts.date().month(), 12);
    EXPECT_EQ(ts.date().day(), 25);
    // Time is always present (defaults to 00:00:00)
    EXPECT_EQ(ts.time().hour(), 10);
    EXPECT_EQ(ts.time().minute(), 30);
    EXPECT_EQ(ts.time().second(), 45);
    EXPECT_TRUE(ts.timezone().is_utc());  // Default UTC timezone
}

// Test component constructor with timezone
TEST(timestamp_test, component_constructor_with_timezone)
{
    dross::timestamp ts(2024, 12, 25, 10, 30, 45, dross::timezone::offset(-5));  // -05:00

    EXPECT_EQ(ts.date().year(), 2024);
    EXPECT_EQ(ts.date().month(), 12);
    EXPECT_EQ(ts.date().day(), 25);
    // Time is always present (defaults to 00:00:00)
    EXPECT_EQ(ts.time().hour(), 10);
    EXPECT_EQ(ts.time().minute(), 30);
    EXPECT_EQ(ts.time().second(), 45);
    EXPECT_EQ(ts.timezone().offset().count(), -300);
}

// Test string constructor - offset timestamp
TEST(timestamp_test, string_constructor_offset_timestamp)
{
    dross::timestamp ts("2024-01-21T15:30:00+09:00");

    EXPECT_EQ(ts.date().year(), 2024);
    EXPECT_EQ(ts.date().month(), 1);
    EXPECT_EQ(ts.date().day(), 21);
    // Time is always present (defaults to 00:00:00)
    EXPECT_EQ(ts.time().hour(), 15);
    EXPECT_EQ(ts.time().minute(), 30);
    EXPECT_EQ(ts.time().second(), 0);
    EXPECT_EQ(ts.timezone().offset().count(), 540);
}

// A negative offset under an hour keeps its sign
TEST(timestamp_test, string_constructor_negative_offset_under_an_hour)
{
    EXPECT_EQ(dross::timestamp("2024-01-21T15:30:00-00:30").timezone().offset().count(), -30);
    EXPECT_EQ(dross::timestamp("2024-01-21T15:30:00-00:01").timezone().offset().count(), -1);
    EXPECT_EQ(std::string(dross::timestamp("2024-01-21T15:30:00-00:30")), "2024-01-21T15:30:00-00:30");

    // Controls: an hour or more, and a positive offset under an hour
    EXPECT_EQ(dross::timestamp("2024-01-21T15:30:00-01:30").timezone().offset().count(), -90);
    EXPECT_EQ(dross::timestamp("2024-01-21T15:30:00+00:30").timezone().offset().count(), 30);

    // The ends of the accepted range are kept
    EXPECT_EQ(dross::timestamp("2024-01-21T15:30:00-12:00").timezone().offset().count(), -720);
    EXPECT_EQ(dross::timestamp("2024-01-21T15:30:00+14:00").timezone().offset().count(), 840);
}

// Test string constructor - UTC timestamp
TEST(timestamp_test, string_constructor_utc_timestamp)
{
    dross::timestamp ts("2024-01-21T15:30:00Z");

    EXPECT_EQ(ts.date().year(), 2024);
    EXPECT_EQ(ts.date().month(), 1);
    EXPECT_EQ(ts.date().day(), 21);
    // Time is always present (defaults to 00:00:00)
    EXPECT_EQ(ts.time().hour(), 15);
    EXPECT_EQ(ts.time().minute(), 30);
    EXPECT_EQ(ts.time().second(), 0);
    EXPECT_EQ(ts.timezone().offset().count(), 0);
}

// Test string constructor - no offset
TEST(timestamp_test, string_constructor_without_offset_is_utc)
{
    dross::timestamp ts("2024-01-21T15:30:00");

    EXPECT_EQ(ts.date().year(), 2024);
    EXPECT_EQ(ts.date().month(), 1);
    EXPECT_EQ(ts.date().day(), 21);
    // Time is always present (defaults to 00:00:00)
    EXPECT_EQ(ts.time().hour(), 15);
    EXPECT_EQ(ts.time().minute(), 30);
    EXPECT_EQ(ts.time().second(), 0);
    EXPECT_TRUE(ts.timezone().is_utc());  // Default UTC timezone
}

// Test string constructor - date only
TEST(timestamp_test, string_constructor_date_only)
{
    dross::timestamp ts("2024-01-21");

    EXPECT_EQ(ts.date().year(), 2024);
    EXPECT_EQ(ts.date().month(), 1);
    EXPECT_EQ(ts.date().day(), 21);
    EXPECT_EQ(ts.time().hour(), 0);  // Date-only, defaults to 00:00:00
    EXPECT_EQ(ts.time().minute(), 0);
    EXPECT_EQ(ts.time().second(), 0);
    EXPECT_TRUE(ts.timezone().is_utc());  // Default UTC timezone
}

// Test const char* constructor
TEST(timestamp_test, cstring_constructor)
{
    const char* iso_str = "2024-06-15T12:00:00+02:00";
    dross::timestamp ts(iso_str);

    EXPECT_EQ(ts.date().year(), 2024);
    EXPECT_EQ(ts.date().month(), 6);
    EXPECT_EQ(ts.date().day(), 15);
    // Time is always present (defaults to 00:00:00)
    EXPECT_EQ(ts.time().hour(), 12);
    EXPECT_EQ(ts.time().minute(), 0);
    EXPECT_EQ(ts.time().second(), 0);
    EXPECT_EQ(ts.timezone().offset().count(), 120);
}

// Test assignment operator
TEST(timestamp_test, assignment_operator)
{
    dross::timestamp ts1(2024, 1, 21, 15, 30, 0);
    dross::timestamp ts2;

    ts2 = ts1;
    EXPECT_EQ(ts1, ts2);
    EXPECT_EQ(ts2.date().year(), 2024);
    EXPECT_EQ(ts2.date().month(), 1);
    EXPECT_EQ(ts2.date().day(), 21);
}

// Test comparison operators
TEST(timestamp_test, comparison_operators)
{
    dross::timestamp ts1(2024, 1, 21, 15, 30, 0);
    dross::timestamp ts2(2024, 1, 21, 15, 31, 0);  // 1 minute later
    dross::timestamp ts3(2024, 1, 21, 15, 30, 0);  // Same as ts1

    // Test ordering
    EXPECT_LT(ts1, ts2);
    EXPECT_LE(ts1, ts2);
    EXPECT_LE(ts1, ts3);
    EXPECT_GT(ts2, ts1);
    EXPECT_GE(ts2, ts1);
    EXPECT_GE(ts3, ts1);

    // Test equality
    EXPECT_EQ(ts1, ts3);
    EXPECT_NE(ts1, ts2);
}

// Test duration arithmetic
TEST(timestamp_test, duration_arithmetic)
{
    dross::timestamp base(2024, 1, 21, 12, 0, 0);

    // Add durations
    auto plus_hour = base + std::chrono::hours(1);
    EXPECT_EQ(plus_hour.time().hour(), 13);
    EXPECT_EQ(plus_hour.time().minute(), 0);

    auto plus_minutes = base + std::chrono::minutes(30);
    EXPECT_EQ(plus_minutes.time().hour(), 12);
    EXPECT_EQ(plus_minutes.time().minute(), 30);

    auto plus_day = base + std::chrono::hours(24);
    EXPECT_EQ(plus_day.date().day(), 22);
    EXPECT_EQ(plus_day.time().hour(), 12);

    // Subtract durations
    auto minus_hour = base - std::chrono::hours(1);
    EXPECT_EQ(minus_hour.time().hour(), 11);

    // Difference between timestamps
    dross::timestamp later(2024, 1, 21, 14, 0, 0);  // 2 hours later
    auto diff = later - base;
    auto hours_diff = std::chrono::duration_cast<std::chrono::hours>(diff);
    EXPECT_EQ(hours_diff.count(), 2);
}

// Test string conversion operator
TEST(timestamp_test, string_conversion)
{
    dross::timestamp ts(2024, 1, 21, 15, 30, 45, dross::timezone::offset(9));  // +09:00
    std::string iso_str = ts;                                                  // Implicit conversion

    // Should produce ISO 8601 format
    EXPECT_EQ(iso_str, "2024-01-21T15:30:45+09:00");
}

// Test formatting
TEST(timestamp_test, formatting)
{
    dross::timestamp ts(2024, 1, 21, 15, 30, 45, dross::timezone::offset(9));  // +09:00

    // ISO 8601 format (default)
    EXPECT_EQ(ts.format(), "2024-01-21T15:30:45+09:00");
    EXPECT_EQ(ts.format(dross::timestamp::format_type::iso8601), "2024-01-21T15:30:45+09:00");
    EXPECT_EQ(ts.format(dross::timestamp::format_type::rfc3339), "2024-01-21T15:30:45+09:00");

    // Custom format
    EXPECT_EQ(ts.format("%Y-%m-%d"), "2024-01-21");
    EXPECT_EQ(ts.format("%H:%M:%S"), "15:30:45");
    EXPECT_EQ(ts.format("%Y-%m-%d %H:%M"), "2024-01-21 15:30");
}

// A custom format reads the time into a std::tm of its own, so two threads
// formatting different timestamps each get their own text.
TEST(timestamp_test, custom_formatting_from_two_threads)
{
    const dross::timestamp first(2024, 1, 21, 15, 30, 0);
    const dross::timestamp second(1990, 12, 25, 1, 2, 3);
    constexpr int rounds = 20000;
    std::atomic<int> wrong{ 0 };

    const auto format_repeatedly = [&wrong](const dross::timestamp& ts, const std::string& expected) {
        for (int i = 0; i < rounds; ++i) {
            if (ts.format("%Y-%m-%d %H:%M:%S") != expected) {
                ++wrong;
            }
        }
    };

    std::thread one(format_repeatedly, std::cref(first), "2024-01-21 15:30:00");
    std::thread two(format_repeatedly, std::cref(second), "1990-12-25 01:02:03");
    one.join();
    two.join();

    EXPECT_EQ(wrong.load(), 0);
}

// Test formatting without timezone
TEST(timestamp_test, formatting_no_timezone)
{
    dross::timestamp ts(2024, 1, 21, 15, 30, 45);  // No timezone

    std::string iso_str = ts.format();
    // Default UTC timezone should show Z suffix
    EXPECT_EQ(iso_str, "2024-01-21T15:30:45Z");
}

// Test dross::value integration
TEST(timestamp_test, value_integration)
{
    dross::timestamp ts(2024, 1, 21, 15, 30, 0);
    dross::value v(ts);

    // Type checking
    EXPECT_TRUE(v.is<dross::timestamp>());
    EXPECT_FALSE(v.is<dross::string>());
    EXPECT_FALSE(v.is<dross::number>());

    // Casting back
    dross::timestamp ts_back = v.as<dross::timestamp>();
    EXPECT_EQ(ts, ts_back);
    EXPECT_EQ(ts_back.date().year(), 2024);
    EXPECT_EQ(ts_back.date().month(), 1);
    EXPECT_EQ(ts_back.date().day(), 21);
}

// Test dross::value assignment
TEST(timestamp_test, value_assignment)
{
    dross::timestamp ts(2024, 1, 21, 15, 30, 0);
    dross::value v;

    v = ts;
    EXPECT_TRUE(v.is<dross::timestamp>());
    EXPECT_EQ(v.as<dross::timestamp>(), ts);
}

// Test edge cases
TEST(timestamp_test, edge_cases)
{
    // Invalid string should result in epoch
    dross::timestamp invalid("invalid-date-string");
    EXPECT_EQ(invalid.date().year(), 1970);

    // Leap year
    dross::timestamp leap_day(2024, 2, 29, 12, 0, 0);
    EXPECT_EQ(leap_day.date().year(), 2024);
    EXPECT_EQ(leap_day.date().month(), 2);
    EXPECT_EQ(leap_day.date().day(), 29);

    // End of year
    dross::timestamp new_year(2023, 12, 31, 23, 59, 59);
    auto next_second = new_year + std::chrono::seconds(1);
    EXPECT_EQ(next_second.date().year(), 2024);
    EXPECT_EQ(next_second.date().month(), 1);
    EXPECT_EQ(next_second.date().day(), 1);
    EXPECT_EQ(next_second.time().hour(), 0);
    EXPECT_EQ(next_second.time().minute(), 0);
    EXPECT_EQ(next_second.time().second(), 0);
}

// Test timezone preservation in arithmetic
TEST(timestamp_test, timezone_preservation_in_arithmetic)
{
    dross::timestamp ts(2024, 1, 21, 15, 30, 0, dross::timezone::offset(9));  // +09:00

    auto plus_hour = ts + std::chrono::hours(1);
    EXPECT_EQ(plus_hour.timezone().offset().count(), 540);
    EXPECT_EQ(plus_hour.time().hour(), 16);

    auto minus_day = ts - std::chrono::hours(24);
    EXPECT_EQ(minus_day.timezone().offset().count(), 540);
    EXPECT_EQ(minus_day.date().day(), 20);
}

// Test date-only constructor
TEST(timestamp_test, date_only_constructor)
{
    auto birthday = dross::timestamp(1990, 12, 25);

    EXPECT_EQ(birthday.date().year(), 1990);
    EXPECT_EQ(birthday.date().month(), 12);
    EXPECT_EQ(birthday.date().day(), 25);
    EXPECT_EQ(birthday.time().hour(), 0);  // Date-only timestamps default to 00:00:00
    EXPECT_EQ(birthday.time().minute(), 0);
    EXPECT_EQ(birthday.time().second(), 0);
    EXPECT_TRUE(birthday.timezone().is_utc());  // Default UTC timezone
}

// Test date-only constructor with timezone
TEST(timestamp_test, date_only_constructor_with_timezone)
{
    auto event = dross::timestamp(2024, 7, 4, 0, 0, 0, dross::timezone::offset(-5));

    EXPECT_EQ(event.date().year(), 2024);
    EXPECT_EQ(event.date().month(), 7);
    EXPECT_EQ(event.date().day(), 4);
    EXPECT_EQ(event.time().hour(), 0);  // Date-only timestamps default to 00:00:00
    EXPECT_EQ(event.time().minute(), 0);
    EXPECT_EQ(event.time().second(), 0);
    EXPECT_EQ(event.timezone().offset().count(), -300);
}

// Test date component access
TEST(timestamp_test, date_component_access)
{
    dross::timestamp ts(2024, 1, 21, 15, 30, 0);
    const auto& date_ref = ts.date();

    EXPECT_EQ(date_ref.year(), 2024);
    EXPECT_EQ(date_ref.month(), 1);
    EXPECT_EQ(date_ref.day(), 21);

    // Test date string conversion
    std::string date_str = date_ref;
    EXPECT_EQ(date_str, "2024-01-21");
}

// Test time component access
TEST(timestamp_test, time_component_access)
{
    dross::timestamp ts(2024, 1, 21, 15, 30, 45);
    const auto& time_ref = ts.time();

    EXPECT_EQ(time_ref.hour(), 15);
    EXPECT_EQ(time_ref.minute(), 30);
    EXPECT_EQ(time_ref.second(), 45);
    EXPECT_EQ(time_ref.total_seconds(), 15 * 3600 + 30 * 60 + 45);

    // Test time string conversion
    std::string time_str = time_ref;
    EXPECT_EQ(time_str, "15:30:45");
}

TEST(timestamp_test, from_string_rejects_what_it_cannot_represent)
{
    const char* const inputs[] = {
        "garbage",
        "",
        "15:30:00",
        "2024-13-01",
        "2024-00-01",
        "2024-01-32",
        "2023-02-29",
        "2024-04-31",
        "2024-01-21T24:00:00Z",
        "2024-01-21T15:60:00Z",
        "2024-01-21T15:30:60Z",
        "2024-13-45T99:99:99Z",
        "2024-01-21T15:30:00+24:00",
        "2024-01-21T15:30:00+09:60",
    };
    for (const char* s : inputs) {
        EXPECT_FALSE(dross::timestamp::from_string(s).has_value()) << s;
        EXPECT_EQ(dross::timestamp(s), dross::timestamp()) << s;
    }
}

TEST(timestamp_test, from_string_keeps_every_offset_rfc3339_allows)
{
    for (const char* s : { "2024-01-21T15:30:00+14:30", "2024-01-21T15:30:00-13:00", "2024-01-21T15:30:00+23:59" }) {
        auto ts = dross::timestamp::from_string(s);
        ASSERT_TRUE(ts.has_value()) << s;
        EXPECT_EQ(std::string(*ts), s);
    }
}

TEST(timestamp_test, from_string_accepts_valid_input)
{
    auto ts = dross::timestamp::from_string("2024-02-29T23:59:59-05:30");
    ASSERT_TRUE(ts.has_value());
    EXPECT_EQ(std::string(*ts), "2024-02-29T23:59:59-05:30");

    auto date = dross::timestamp::from_string("0000-01-01");
    ASSERT_TRUE(date.has_value());
    EXPECT_EQ(date->date().year(), 0);
}

TEST(timestamp_test, from_components_rejects_values_out_of_range)
{
    EXPECT_FALSE(dross::timestamp::from_components(-1, 1, 1).has_value());
    EXPECT_FALSE(dross::timestamp::from_components(10000, 1, 1).has_value());
    EXPECT_FALSE(dross::timestamp::from_components(2024, 0, 1).has_value());
    EXPECT_FALSE(dross::timestamp::from_components(2024, 13, 1).has_value());
    EXPECT_FALSE(dross::timestamp::from_components(2024, 1, 0).has_value());
    EXPECT_FALSE(dross::timestamp::from_components(2023, 2, 29).has_value());
    EXPECT_FALSE(dross::timestamp::from_components(2024, 1, 21, 24).has_value());
    EXPECT_FALSE(dross::timestamp::from_components(2024, 1, 21, -1).has_value());
    EXPECT_FALSE(dross::timestamp::from_components(2024, 1, 21, 15, 60).has_value());
    EXPECT_FALSE(dross::timestamp::from_components(2024, 1, 21, 15, 30, 60).has_value());

    EXPECT_EQ(dross::timestamp(2024, 13, 45, 99, 99, 99), dross::timestamp());
    EXPECT_EQ(dross::timestamp(2023, 2, 29, 0, 0, 0, dross::timezone::offset(9)), dross::timestamp());
}

TEST(timestamp_test, from_components_accepts_valid_values)
{
    auto ts = dross::timestamp::from_components(2024, 2, 29, 23, 59, 59, dross::timezone::offset(9));
    ASSERT_TRUE(ts.has_value());
    EXPECT_EQ(std::string(*ts), "2024-02-29T23:59:59+09:00");
    EXPECT_EQ(dross::timestamp::from_components(2024, 1, 21)->timezone(), dross::timezone::utc());
}

TEST(timestamp_test, arithmetic_holds_far_from_1970)
{
    const dross::timestamp ref("2024-01-01T00:00:00Z");
    for (const char* s : { "2262-04-11T23:47:17Z", "2300-01-01T00:00:00Z", "9999-12-31T23:59:59+09:00" }) {
        const dross::timestamp ts(s);
        EXPECT_GT(ts, ref) << s;
        EXPECT_EQ(ts + std::chrono::hours(1) - std::chrono::hours(1), ts) << s;
    }
    for (const char* s : { "1677-09-21T00:12:43Z", "1600-01-01T00:00:00Z", "0000-01-01T00:00:00-09:00" }) {
        const dross::timestamp ts(s);
        EXPECT_LT(ts, ref) << s;
        EXPECT_EQ(ts + std::chrono::hours(1) - std::chrono::hours(1), ts) << s;
    }

    EXPECT_EQ(dross::timestamp("2300-01-01T09:00:00+09:00"), dross::timestamp("2300-01-01T00:00:00Z"));
    EXPECT_EQ(std::string(dross::timestamp("2300-01-01T00:00:00Z") + std::chrono::hours(1)), "2300-01-01T01:00:00Z");
    EXPECT_EQ(std::string(dross::timestamp("9999-12-31T23:00:00Z") + std::chrono::minutes(59)), "9999-12-31T23:59:00Z");
    EXPECT_EQ(std::string(dross::timestamp("1600-03-01T00:30:00Z") - std::chrono::hours(1)), "1600-02-29T23:30:00Z");
    EXPECT_EQ(dross::timestamp("9999-12-31T23:59:59Z").format("%Y-%m-%d %H:%M:%S"), "9999-12-31 23:59:59");
}

TEST(timestamp_test, fractional_seconds_are_kept)
{
    const dross::timestamp with("2024-01-21T15:30:00.75Z");
    EXPECT_EQ(std::string(with), "2024-01-21T15:30:00.75Z");
    EXPECT_NE(with, dross::timestamp("2024-01-21T15:30:00Z"));
    EXPECT_EQ(with - dross::timestamp("2024-01-21T15:30:00Z"), std::chrono::milliseconds(750));

    EXPECT_EQ(std::string(dross::timestamp("2024-01-21T15:30:00.000000001+09:00")), "2024-01-21T15:30:00.000000001+09:00");
    EXPECT_EQ(std::string(dross::timestamp("2024-01-21T15:30:00.1234567899Z")), "2024-01-21T15:30:00.123456789Z");
    EXPECT_EQ(std::string(dross::timestamp("2024-01-21T15:30:00.000Z")), "2024-01-21T15:30:00Z");
    EXPECT_EQ(std::string(dross::timestamp("2024-01-21T23:59:59.5Z") + std::chrono::seconds(2)), "2024-01-22T00:00:01.5Z");

    const dross::timestamp now{ std::chrono::system_clock::now() };
    EXPECT_EQ(dross::timestamp(std::string(now)), now);
}

TEST(timestamp_test, midnight_utc_is_written_in_full)
{
    EXPECT_EQ(std::string(dross::timestamp("2024-01-21T00:00:00Z")), "2024-01-21T00:00:00Z");
    EXPECT_EQ(std::string(dross::timestamp("2024-01-21")), "2024-01-21T00:00:00Z");
    EXPECT_EQ(std::string(dross::timestamp("2024-01-21T23:59:59.5Z") + std::chrono::seconds(1)), "2024-01-22T00:00:00.5Z");
}
