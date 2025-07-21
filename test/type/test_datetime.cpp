#include <gtest/gtest.h>
#include <dross/type/datetime.h>
#include <dross/type/value.h>
#include <chrono>
#include <thread>

// Test static factory method
TEST(datetime_test, now) {
    auto now1 = dross::datetime::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto now2 = dross::datetime::now();
    
    EXPECT_LT(now1, now2);
    EXPECT_NE(now1, now2);
}

// Test default constructor (epoch)
TEST(datetime_test, default_constructor) {
    dross::datetime epoch;
    
    EXPECT_EQ(epoch.year(), 1970);
    EXPECT_EQ(epoch.month(), 1);
    EXPECT_EQ(epoch.day(), 1);
    EXPECT_EQ(epoch.hour(), 0);
    EXPECT_EQ(epoch.minute(), 0);
    EXPECT_EQ(epoch.second(), 0);
    EXPECT_FALSE(epoch.has_timezone());
}

// Test copy constructor
TEST(datetime_test, copy_constructor) {
    dross::datetime dt1(2024, 1, 21, 15, 30, 45, 540); // +09:00
    dross::datetime dt2(dt1);
    
    EXPECT_EQ(dt1, dt2);
    EXPECT_EQ(dt2.year(), 2024);
    EXPECT_EQ(dt2.month(), 1);
    EXPECT_EQ(dt2.day(), 21);
    EXPECT_EQ(dt2.hour(), 15);
    EXPECT_EQ(dt2.minute(), 30);
    EXPECT_EQ(dt2.second(), 45);
    EXPECT_TRUE(dt2.has_timezone());
    EXPECT_EQ(dt2.timezone_offset_minutes().value(), 540);
}

// Test time_point constructor
TEST(datetime_test, time_point_constructor) {
    auto tp = std::chrono::system_clock::now();
    dross::datetime dt(tp);
    
    // Convert back and compare
    auto tp_back = static_cast<std::chrono::system_clock::time_point>(dt);
    EXPECT_EQ(tp, tp_back);
    EXPECT_FALSE(dt.has_timezone()); // No timezone info from raw time_point
}

// Test component constructor without timezone
TEST(datetime_test, component_constructor_local) {
    dross::datetime dt(2024, 12, 25, 10, 30, 45);
    
    EXPECT_EQ(dt.year(), 2024);
    EXPECT_EQ(dt.month(), 12);
    EXPECT_EQ(dt.day(), 25);
    EXPECT_EQ(dt.hour(), 10);
    EXPECT_EQ(dt.minute(), 30);
    EXPECT_EQ(dt.second(), 45);
    EXPECT_FALSE(dt.has_timezone());
}

// Test component constructor with timezone
TEST(datetime_test, component_constructor_with_timezone) {
    dross::datetime dt(2024, 12, 25, 10, 30, 45, -300); // -05:00
    
    EXPECT_EQ(dt.year(), 2024);
    EXPECT_EQ(dt.month(), 12);
    EXPECT_EQ(dt.day(), 25);
    EXPECT_EQ(dt.hour(), 10);
    EXPECT_EQ(dt.minute(), 30);
    EXPECT_EQ(dt.second(), 45);
    EXPECT_TRUE(dt.has_timezone());
    EXPECT_EQ(dt.timezone_offset_minutes().value(), -300);
}

// Test string constructor - offset datetime
TEST(datetime_test, string_constructor_offset_datetime) {
    dross::datetime dt("2024-01-21T15:30:00+09:00");
    
    EXPECT_EQ(dt.year(), 2024);
    EXPECT_EQ(dt.month(), 1);
    EXPECT_EQ(dt.day(), 21);
    EXPECT_EQ(dt.hour(), 15);
    EXPECT_EQ(dt.minute(), 30);
    EXPECT_EQ(dt.second(), 0);
    EXPECT_TRUE(dt.has_timezone());
    EXPECT_EQ(dt.timezone_offset_minutes().value(), 540);
}

// Test string constructor - UTC datetime
TEST(datetime_test, string_constructor_utc_datetime) {
    dross::datetime dt("2024-01-21T15:30:00Z");
    
    EXPECT_EQ(dt.year(), 2024);
    EXPECT_EQ(dt.month(), 1);
    EXPECT_EQ(dt.day(), 21);
    EXPECT_EQ(dt.hour(), 15);
    EXPECT_EQ(dt.minute(), 30);
    EXPECT_EQ(dt.second(), 0);
    EXPECT_TRUE(dt.has_timezone());
    EXPECT_EQ(dt.timezone_offset_minutes().value(), 0);
}

// Test string constructor - local datetime
TEST(datetime_test, string_constructor_local_datetime) {
    dross::datetime dt("2024-01-21T15:30:00");
    
    EXPECT_EQ(dt.year(), 2024);
    EXPECT_EQ(dt.month(), 1);
    EXPECT_EQ(dt.day(), 21);
    EXPECT_EQ(dt.hour(), 15);
    EXPECT_EQ(dt.minute(), 30);
    EXPECT_EQ(dt.second(), 0);
    EXPECT_FALSE(dt.has_timezone());
}

// Test string constructor - date only
TEST(datetime_test, string_constructor_date_only) {
    dross::datetime dt("2024-01-21");
    
    EXPECT_EQ(dt.year(), 2024);
    EXPECT_EQ(dt.month(), 1);
    EXPECT_EQ(dt.day(), 21);
    EXPECT_EQ(dt.hour(), 0);
    EXPECT_EQ(dt.minute(), 0);
    EXPECT_EQ(dt.second(), 0);
    EXPECT_FALSE(dt.has_timezone());
}

// Test string constructor - time only
TEST(datetime_test, string_constructor_time_only) {
    dross::datetime dt("15:30:45");
    
    EXPECT_EQ(dt.year(), 1970);  // Default to epoch date
    EXPECT_EQ(dt.month(), 1);
    EXPECT_EQ(dt.day(), 1);
    EXPECT_EQ(dt.hour(), 15);
    EXPECT_EQ(dt.minute(), 30);
    EXPECT_EQ(dt.second(), 45);
    EXPECT_FALSE(dt.has_timezone());
}

// Test const char* constructor
TEST(datetime_test, cstring_constructor) {
    const char* iso_str = "2024-06-15T12:00:00+02:00";
    dross::datetime dt(iso_str);
    
    EXPECT_EQ(dt.year(), 2024);
    EXPECT_EQ(dt.month(), 6);
    EXPECT_EQ(dt.day(), 15);
    EXPECT_EQ(dt.hour(), 12);
    EXPECT_EQ(dt.minute(), 0);
    EXPECT_EQ(dt.second(), 0);
    EXPECT_TRUE(dt.has_timezone());
    EXPECT_EQ(dt.timezone_offset_minutes().value(), 120);
}

// Test assignment operator
TEST(datetime_test, assignment_operator) {
    dross::datetime dt1(2024, 1, 21, 15, 30, 0);
    dross::datetime dt2;
    
    dt2 = dt1;
    EXPECT_EQ(dt1, dt2);
    EXPECT_EQ(dt2.year(), 2024);
    EXPECT_EQ(dt2.month(), 1);
    EXPECT_EQ(dt2.day(), 21);
}

// Test comparison operators
TEST(datetime_test, comparison_operators) {
    dross::datetime dt1(2024, 1, 21, 15, 30, 0);
    dross::datetime dt2(2024, 1, 21, 15, 31, 0); // 1 minute later
    dross::datetime dt3(2024, 1, 21, 15, 30, 0); // Same as dt1
    
    // Test ordering
    EXPECT_LT(dt1, dt2);
    EXPECT_LE(dt1, dt2);
    EXPECT_LE(dt1, dt3);
    EXPECT_GT(dt2, dt1);
    EXPECT_GE(dt2, dt1);
    EXPECT_GE(dt3, dt1);
    
    // Test equality
    EXPECT_EQ(dt1, dt3);
    EXPECT_NE(dt1, dt2);
}

// Test duration arithmetic
TEST(datetime_test, duration_arithmetic) {
    dross::datetime base(2024, 1, 21, 12, 0, 0);
    
    // Add durations
    auto plus_hour = base + std::chrono::hours(1);
    EXPECT_EQ(plus_hour.hour(), 13);
    EXPECT_EQ(plus_hour.minute(), 0);
    
    auto plus_minutes = base + std::chrono::minutes(30);
    EXPECT_EQ(plus_minutes.hour(), 12);
    EXPECT_EQ(plus_minutes.minute(), 30);
    
    auto plus_day = base + std::chrono::hours(24);
    EXPECT_EQ(plus_day.day(), 22);
    EXPECT_EQ(plus_day.hour(), 12);
    
    // Subtract durations
    auto minus_hour = base - std::chrono::hours(1);
    EXPECT_EQ(minus_hour.hour(), 11);
    
    // Difference between datetimes
    dross::datetime later(2024, 1, 21, 14, 0, 0); // 2 hours later
    auto diff = later - base;
    auto hours_diff = std::chrono::duration_cast<std::chrono::hours>(diff);
    EXPECT_EQ(hours_diff.count(), 2);
}

// Test string conversion operator
TEST(datetime_test, string_conversion) {
    dross::datetime dt(2024, 1, 21, 15, 30, 45, 540); // +09:00
    std::string iso_str = dt; // Implicit conversion
    
    // Should produce ISO 8601 format
    EXPECT_EQ(iso_str, "2024-01-21T15:30:45+09:00");
}

// Test formatting
TEST(datetime_test, formatting) {
    dross::datetime dt(2024, 1, 21, 15, 30, 45, 540); // +09:00
    
    // ISO 8601 format (default)
    EXPECT_EQ(dt.format(), "2024-01-21T15:30:45+09:00");
    EXPECT_EQ(dt.format(dross::datetime_format::iso8601), "2024-01-21T15:30:45+09:00");
    EXPECT_EQ(dt.format(dross::datetime_format::rfc3339), "2024-01-21T15:30:45+09:00");
    
    // Custom format
    EXPECT_EQ(dt.format("%Y-%m-%d"), "2024-01-21");
    EXPECT_EQ(dt.format("%H:%M:%S"), "15:30:45");
    EXPECT_EQ(dt.format("%Y-%m-%d %H:%M"), "2024-01-21 15:30");
}

// Test formatting without timezone
TEST(datetime_test, formatting_no_timezone) {
    dross::datetime dt(2024, 1, 21, 15, 30, 45); // No timezone
    
    std::string iso_str = dt.format();
    // Should not have timezone suffix
    EXPECT_EQ(iso_str, "2024-01-21T15:30:45");
}

// Test dross::value integration
TEST(datetime_test, value_integration) {
    dross::datetime dt(2024, 1, 21, 15, 30, 0);
    dross::value v(dt);
    
    // Type checking
    EXPECT_TRUE(v.is<dross::datetime>());
    EXPECT_FALSE(v.is<dross::string>());
    EXPECT_FALSE(v.is<dross::number>());
    
    // Casting back
    dross::datetime dt_back = v.as<dross::datetime>();
    EXPECT_EQ(dt, dt_back);
    EXPECT_EQ(dt_back.year(), 2024);
    EXPECT_EQ(dt_back.month(), 1);
    EXPECT_EQ(dt_back.day(), 21);
}

// Test dross::value assignment
TEST(datetime_test, value_assignment) {
    dross::datetime dt(2024, 1, 21, 15, 30, 0);
    dross::value v;
    
    v = dt;
    EXPECT_TRUE(v.is<dross::datetime>());
    EXPECT_EQ(v.as<dross::datetime>(), dt);
}

// Test edge cases
TEST(datetime_test, edge_cases) {
    // Invalid string should result in epoch
    dross::datetime invalid("invalid-date-string");
    EXPECT_EQ(invalid.year(), 1970);
    
    // Leap year
    dross::datetime leap_day(2024, 2, 29, 12, 0, 0);
    EXPECT_EQ(leap_day.year(), 2024);
    EXPECT_EQ(leap_day.month(), 2);
    EXPECT_EQ(leap_day.day(), 29);
    
    // End of year
    dross::datetime new_year(2023, 12, 31, 23, 59, 59);
    auto next_second = new_year + std::chrono::seconds(1);
    EXPECT_EQ(next_second.year(), 2024);
    EXPECT_EQ(next_second.month(), 1);
    EXPECT_EQ(next_second.day(), 1);
    EXPECT_EQ(next_second.hour(), 0);
    EXPECT_EQ(next_second.minute(), 0);
    EXPECT_EQ(next_second.second(), 0);
}

// Test timezone preservation in arithmetic
TEST(datetime_test, timezone_preservation_in_arithmetic) {
    dross::datetime dt(2024, 1, 21, 15, 30, 0, 540); // +09:00
    
    auto plus_hour = dt + std::chrono::hours(1);
    EXPECT_TRUE(plus_hour.has_timezone());
    EXPECT_EQ(plus_hour.timezone_offset_minutes().value(), 540);
    EXPECT_EQ(plus_hour.hour(), 16);
    
    auto minus_day = dt - std::chrono::hours(24);
    EXPECT_TRUE(minus_day.has_timezone());
    EXPECT_EQ(minus_day.timezone_offset_minutes().value(), 540);
    EXPECT_EQ(minus_day.day(), 20);
}