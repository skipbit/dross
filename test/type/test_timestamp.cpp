#include <gtest/gtest.h>
#include <dross/type/timestamp.h>
#include <dross/type/timezone.h>
#include <dross/type/value.h>
#include <chrono>
#include <thread>

// Test static factory method
TEST(timestamp_test, now) {
    auto now1 = dross::timestamp::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto now2 = dross::timestamp::now();

    EXPECT_LT(now1, now2);
    EXPECT_NE(now1, now2);
}

// Test default constructor (epoch)
TEST(timestamp_test, default_constructor) {
    dross::timestamp epoch;

    EXPECT_EQ(epoch.date().year(), 1970);
    EXPECT_EQ(epoch.date().month(), 1);
    EXPECT_EQ(epoch.date().day(), 1);
    EXPECT_EQ(epoch.time().hour(), 0); // Default 00:00:00
    EXPECT_EQ(epoch.time().minute(), 0);
    EXPECT_EQ(epoch.time().second(), 0);
    EXPECT_TRUE(epoch.timezone().is_utc()); // Default UTC
}

// Test copy constructor
TEST(timestamp_test, copy_constructor) {
    dross::timestamp ts1(2024, 1, 21, 15, 30, 45, dross::timezone::offset(9)); // +09:00
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
TEST(timestamp_test, time_point_constructor) {
    auto tp = std::chrono::system_clock::now();
    dross::timestamp ts(tp);

    // Convert back and compare
    auto tp_back = static_cast<std::chrono::system_clock::time_point>(ts);
    EXPECT_EQ(tp, tp_back);
    EXPECT_TRUE(ts.timezone().is_utc()); // Default UTC timezone
}

// Test component constructor without timezone
TEST(timestamp_test, component_constructor_local) {
    dross::timestamp ts(2024, 12, 25, 10, 30, 45);

    EXPECT_EQ(ts.date().year(), 2024);
    EXPECT_EQ(ts.date().month(), 12);
    EXPECT_EQ(ts.date().day(), 25);
    // Time is always present (defaults to 00:00:00)
    EXPECT_EQ(ts.time().hour(), 10);
    EXPECT_EQ(ts.time().minute(), 30);
    EXPECT_EQ(ts.time().second(), 45);
    EXPECT_TRUE(ts.timezone().is_utc()); // Default UTC timezone
}

// Test component constructor with timezone
TEST(timestamp_test, component_constructor_with_timezone) {
    dross::timestamp ts(2024, 12, 25, 10, 30, 45, dross::timezone::offset(-5)); // -05:00

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
TEST(timestamp_test, string_constructor_offset_timestamp) {
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

// Test string constructor - UTC timestamp
TEST(timestamp_test, string_constructor_utc_timestamp) {
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

// Test string constructor - local timestamp
TEST(timestamp_test, string_constructor_local_timestamp) {
    dross::timestamp ts("2024-01-21T15:30:00");

    EXPECT_EQ(ts.date().year(), 2024);
    EXPECT_EQ(ts.date().month(), 1);
    EXPECT_EQ(ts.date().day(), 21);
    // Time is always present (defaults to 00:00:00)
    EXPECT_EQ(ts.time().hour(), 15);
    EXPECT_EQ(ts.time().minute(), 30);
    EXPECT_EQ(ts.time().second(), 0);
    EXPECT_TRUE(ts.timezone().is_utc()); // Default UTC timezone
}

// Test string constructor - date only
TEST(timestamp_test, string_constructor_date_only) {
    dross::timestamp ts("2024-01-21");

    EXPECT_EQ(ts.date().year(), 2024);
    EXPECT_EQ(ts.date().month(), 1);
    EXPECT_EQ(ts.date().day(), 21);
    EXPECT_EQ(ts.time().hour(), 0); // Date-only, defaults to 00:00:00
    EXPECT_EQ(ts.time().minute(), 0);
    EXPECT_EQ(ts.time().second(), 0);
    EXPECT_TRUE(ts.timezone().is_utc()); // Default UTC timezone
}

// Test const char* constructor
TEST(timestamp_test, cstring_constructor) {
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
TEST(timestamp_test, assignment_operator) {
    dross::timestamp ts1(2024, 1, 21, 15, 30, 0);
    dross::timestamp ts2;

    ts2 = ts1;
    EXPECT_EQ(ts1, ts2);
    EXPECT_EQ(ts2.date().year(), 2024);
    EXPECT_EQ(ts2.date().month(), 1);
    EXPECT_EQ(ts2.date().day(), 21);
}

// Test comparison operators
TEST(timestamp_test, comparison_operators) {
    dross::timestamp ts1(2024, 1, 21, 15, 30, 0);
    dross::timestamp ts2(2024, 1, 21, 15, 31, 0); // 1 minute later
    dross::timestamp ts3(2024, 1, 21, 15, 30, 0); // Same as ts1

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
TEST(timestamp_test, duration_arithmetic) {
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
    dross::timestamp later(2024, 1, 21, 14, 0, 0); // 2 hours later
    auto diff = later - base;
    auto hours_diff = std::chrono::duration_cast<std::chrono::hours>(diff);
    EXPECT_EQ(hours_diff.count(), 2);
}

// Test string conversion operator
TEST(timestamp_test, string_conversion) {
    dross::timestamp ts(2024, 1, 21, 15, 30, 45, dross::timezone::offset(9)); // +09:00
    std::string iso_str = ts; // Implicit conversion

    // Should produce ISO 8601 format
    EXPECT_EQ(iso_str, "2024-01-21T15:30:45+09:00");
}

// Test formatting
TEST(timestamp_test, formatting) {
    dross::timestamp ts(2024, 1, 21, 15, 30, 45, dross::timezone::offset(9)); // +09:00

    // ISO 8601 format (default)
    EXPECT_EQ(ts.format(), "2024-01-21T15:30:45+09:00");
    EXPECT_EQ(ts.format(dross::timestamp::format_type::iso8601), "2024-01-21T15:30:45+09:00");
    EXPECT_EQ(ts.format(dross::timestamp::format_type::rfc3339), "2024-01-21T15:30:45+09:00");

    // Custom format
    EXPECT_EQ(ts.format("%Y-%m-%d"), "2024-01-21");
    EXPECT_EQ(ts.format("%H:%M:%S"), "15:30:45");
    EXPECT_EQ(ts.format("%Y-%m-%d %H:%M"), "2024-01-21 15:30");
}

// Test formatting without timezone
TEST(timestamp_test, formatting_no_timezone) {
    dross::timestamp ts(2024, 1, 21, 15, 30, 45); // No timezone

    std::string iso_str = ts.format();
    // Default UTC timezone should show Z suffix
    EXPECT_EQ(iso_str, "2024-01-21T15:30:45Z");
}

// Test dross::value integration
TEST(timestamp_test, value_integration) {
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
TEST(timestamp_test, value_assignment) {
    dross::timestamp ts(2024, 1, 21, 15, 30, 0);
    dross::value v;

    v = ts;
    EXPECT_TRUE(v.is<dross::timestamp>());
    EXPECT_EQ(v.as<dross::timestamp>(), ts);
}

// Test edge cases
TEST(timestamp_test, edge_cases) {
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
TEST(timestamp_test, timezone_preservation_in_arithmetic) {
    dross::timestamp ts(2024, 1, 21, 15, 30, 0, dross::timezone::offset(9)); // +09:00

    auto plus_hour = ts + std::chrono::hours(1);
    EXPECT_EQ(plus_hour.timezone().offset().count(), 540);
    EXPECT_EQ(plus_hour.time().hour(), 16);

    auto minus_day = ts - std::chrono::hours(24);
    EXPECT_EQ(minus_day.timezone().offset().count(), 540);
    EXPECT_EQ(minus_day.date().day(), 20);
}

// Test date-only constructor
TEST(timestamp_test, date_only_constructor) {
    auto birthday = dross::timestamp(1990, 12, 25);

    EXPECT_EQ(birthday.date().year(), 1990);
    EXPECT_EQ(birthday.date().month(), 12);
    EXPECT_EQ(birthday.date().day(), 25);
    EXPECT_EQ(birthday.time().hour(), 0); // Date-only timestamps default to 00:00:00
    EXPECT_EQ(birthday.time().minute(), 0);
    EXPECT_EQ(birthday.time().second(), 0);
    EXPECT_TRUE(birthday.timezone().is_utc()); // Default UTC timezone
}

// Test date-only constructor with timezone
TEST(timestamp_test, date_only_constructor_with_timezone) {
    auto event = dross::timestamp(2024, 7, 4, 0, 0, 0, dross::timezone::offset(-5));

    EXPECT_EQ(event.date().year(), 2024);
    EXPECT_EQ(event.date().month(), 7);
    EXPECT_EQ(event.date().day(), 4);
    EXPECT_EQ(event.time().hour(), 0); // Date-only timestamps default to 00:00:00
    EXPECT_EQ(event.time().minute(), 0);
    EXPECT_EQ(event.time().second(), 0);
    EXPECT_EQ(event.timezone().offset().count(), -300);
}

// Test date component access
TEST(timestamp_test, date_component_access) {
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
TEST(timestamp_test, time_component_access) {
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
