#pragma once

#include "timezone.h"

#include <chrono>
#include <compare>
#include <optional>
#include <string>
#include <utility>

namespace dross {

/**
 * @brief Timestamp handling with modern timezone support.
 *
 * The timestamp class provides comprehensive date and time operations with
 * full timezone support using type-safe timezone objects. It uses std::chrono
 * internally for high precision time calculations while providing a user-friendly
 * API for common operations. This class is designed as a general-purpose timestamp
 * type suitable for configuration files, data serialization, logging, scheduling,
 * and any application requiring robust timestamp handling.
 *
 * Key features:
 * - ISO 8601 and RFC 3339 format support
 * - Type-safe timezone operations with timezone class
 * - Integration with std::chrono for duration arithmetic, kept within the
 *   years 0 to 9999 that parsing accepts
 * - Value semantics (copyable and assignable, without allocating)
 * - Thread-safe for read operations
 * - A date alone is read as midnight and written in full
 * - Factory methods for common timestamp patterns
 * - Compositional design with separate date and time components
 *
 * Supported timestamp formats (ISO 8601 standard):
 * - Offset timestamp: 2024-01-21T15:30:00+09:00
 * - Fractional seconds: 2024-01-21T15:30:00.5+09:00 (kept to nanoseconds)
 * - UTC timestamp: 2024-01-21T15:30:00Z
 * - Timestamp without an offset, read as UTC: 2024-01-21T15:30:00
 * - Date, read as midnight UTC: 2024-01-21
 *
 * Performance characteristics:
 * - Construction: O(1) for time_point, O(n) for string parsing
 * - Arithmetic operations: O(1) using std::chrono
 * - Comparisons: O(1)
 * - Formatting: O(n) where n is the output string length
 *
 * Thread safety:
 * - Const operations are thread-safe
 * - Non-const operations require external synchronization
 *
 * @code
 * // Basic usage with timezone objects
 * dross::timestamp now = dross::timestamp::now();
 * dross::timestamp meeting(2024, 1, 21, 15, 30, 0, dross::timezone::offset(9)); // +09:00
 * dross::timestamp utc_meeting(2024, 1, 21, 6, 30, 0, dross::timezone::utc());  // UTC
 *
 * // Date-only timestamps using constructor
 * dross::timestamp birthday(1990, 12, 25);  // Date only (time defaults to 00:00:00)
 *
 * // String parsing
 * dross::timestamp iso_meeting("2024-01-21T15:30:00+09:00");
 *
 * // Duration arithmetic
 * dross::timestamp tomorrow = now + std::chrono::hours(24);
 * auto age = now - birthday;
 *
 * // Accessing components
 * std::cout << "Year: " << meeting.date().year() << std::endl;
 * std::cout << "Hour: " << meeting.time().hour() << std::endl;
 * std::cout << "Timezone: " << meeting.timezone().format() << std::endl;
 *
 * // Formatting
 * std::string iso_str = meeting.format(dross::timestamp::format_type::iso8601);
 * std::string custom = meeting.format("%Y-%m-%d %H:%M");
 *
 * // Conversion to std::chrono
 * auto time_point = static_cast<std::chrono::system_clock::time_point>(now);
 * @endcode
 */
class timestamp final {
public:
    /**
     * @brief Format options for timestamp string representation.
     */
    enum class format_type {
        iso8601,  // ISO 8601 format: 2024-01-21T15:30:00+09:00
        rfc3339   // RFC 3339 format (essentially same as ISO 8601)
    };

    /**
     * @brief Date component of a timestamp.
     *
     * This class represents the date part of a timestamp and can only be
     * constructed through the timestamp class. It provides read-only access
     * to year, month, and day components.
     */
    class date_part {
        friend class timestamp;

    private:
        std::chrono::year_month_day _ymd;

        // Private constructors - only timestamp can create date objects
        date_part();
        date_part(const std::chrono::year_month_day& ymd);

    public:
        /**
         * @brief Get the year component.
         * @return Year (e.g., 2024)
         */
        int year() const noexcept;

        /**
         * @brief Get the month component.
         * @return Month (1-12)
         */
        int month() const noexcept;

        /**
         * @brief Get the day component.
         * @return Day of month (1-31)
         */
        int day() const noexcept;

        /**
         * @brief Implicit conversion to std::string in ISO 8601 format.
         * @return Date string in format "YYYY-MM-DD"
         */
        operator std::string() const;

        /**
         * @brief Conversion to std::chrono::year_month_day.
         * @return The year_month_day representation
         */
        operator std::chrono::year_month_day() const noexcept;

        /**
         * @brief Three-way comparison operator.
         * @param other The date to compare with
         * @return Comparison result (strong ordering)
         */
        std::strong_ordering operator<=>(const date_part& other) const noexcept;

        /**
         * @brief Equality comparison operator.
         * @param other The date to compare with
         * @return True if the dates are equal
         */
        bool operator==(const date_part& other) const noexcept;
    };

    /**
     * @brief Time component of a timestamp.
     *
     * This class represents the time part of a timestamp and can only be
     * constructed through the timestamp class. It provides read-only access
     * to hour, minute, and second components with nanosecond precision.
     */
    class time_part {
        friend class timestamp;

    private:
        std::chrono::nanoseconds _since_midnight;

        // Private constructors - only timestamp can create time objects
        time_part();
        explicit time_part(std::chrono::nanoseconds since_midnight);

    public:
        /**
         * @brief Static midnight constant.
         */
        static const time_part midnight;

        /**
         * @brief Get the hour component.
         * @return Hour (0-23)
         */
        int hour() const noexcept;

        /**
         * @brief Get the minute component.
         * @return Minute (0-59)
         */
        int minute() const noexcept;

        /**
         * @brief Get the second component.
         * @return Second (0-59)
         */
        int second() const noexcept;

        /**
         * @brief Get the total seconds since midnight.
         * @return Seconds since 00:00:00
         */
        int total_seconds() const noexcept;

        /**
         * @brief Implicit conversion to std::string in ISO 8601 format.
         * @return Time string in format "HH:MM:SS"
         */
        operator std::string() const;

        /**
         * @brief Conversion to std::chrono::hh_mm_ss.
         * @return The hh_mm_ss representation
         */
        std::chrono::hh_mm_ss<std::chrono::nanoseconds> to_hh_mm_ss() const noexcept;

        /**
         * @brief Three-way comparison operator.
         * @param other The time to compare with
         * @return Comparison result (strong ordering)
         */
        std::strong_ordering operator<=>(const time_part& other) const noexcept;

        /**
         * @brief Equality comparison operator.
         * @param other The time to compare with
         * @return True if the times are equal
         */
        bool operator==(const time_part& other) const noexcept;
    };

    /**
     * @brief Create a timestamp representing the current moment.
     * @return timestamp object set to current system time in UTC
     */
    static timestamp now();

    /**
     * @brief Parse an ISO 8601 string, reporting input it cannot represent.
     * @param iso8601_str The ISO 8601 formatted string
     * @return The timestamp, or nullopt if the string does not parse, names a
     *         date or time that does not exist, or has an offset outside
     *         -23:59..+23:59
     *
     * Accepts the forms listed for the string constructor. Fractional
     * seconds are kept to the nanosecond; further digits are dropped.
     */
    static std::optional<timestamp> from_string(const std::string& iso8601_str);

    /**
     * @brief Build a timestamp from components, reporting values out of range.
     * @param year Year (0-9999)
     * @param month Month (1-12)
     * @param day Day of month (1 to the last day of the month)
     * @param hour Hour (0-23)
     * @param minute Minute (0-59)
     * @param second Second (0-59)
     * @param tz Timezone information
     * @return The timestamp, or nullopt if any component is out of range
     */
    static std::optional<timestamp> from_components(int year, int month, int day, int hour = 0, int minute = 0, int second = 0, const dross::timezone& tz = dross::timezone::utc());

    /**
     * @brief Build a timestamp from a time point, reporting one out of range.
     * @param tp The time point, read as UTC
     * @return The timestamp, or nullopt if the time point is outside the
     *         years 0 to 9999
     *
     * system_clock reaches past those years where its duration counts
     * microseconds. A timestamp near their ends can be outside them as a time
     * point, when its offset moves it there. A time_point with a coarser
     * duration, such as std::chrono::sys_days, converts before the call, and
     * can overflow there where the duration of system_clock counts
     * nanoseconds.
     */
    static std::optional<timestamp> from_time_point(const std::chrono::system_clock::time_point& tp);

    /**
     * @brief Default constructor creating epoch time (1970-01-01T00:00:00Z).
     */
    timestamp();

    /**
     * @brief Construct from std::chrono::system_clock::time_point.
     * @param tp The time point to construct from
     *
     * The resulting timestamp will be in UTC (no timezone offset). A time
     * point that from_time_point() rejects results in the nearer of the first
     * and the last nanosecond of the years 0 to 9999, so it does not order on
     * the wrong side of another timestamp.
     */
    timestamp(const std::chrono::system_clock::time_point& tp);

    /**
     * @brief Construct from ISO 8601 string.
     * @param iso8601_str The ISO 8601 formatted string
     *
     * Supports various ISO 8601 formats:
     * - 2024-01-21T15:30:00+09:00 (offset timestamp)
     * - 2024-01-21T15:30:00 (no offset, read as UTC)
     * - 2024-01-21 (date only, read as midnight UTC)
     *
     * Input that from_string() rejects results in epoch time.
     */
    timestamp(const std::string& iso8601_str);

    /**
     * @brief Construct from const char*.
     * @param iso8601_str The ISO 8601 formatted string
     *
     * Input that from_string() rejects results in epoch time.
     */
    timestamp(const char* iso8601_str);

    /**
     * @brief Construct from individual date and time components.
     * @param year Year (0-9999)
     * @param month Month (1-12)
     * @param day Day of month (1 to the last day of the month)
     * @param hour Hour (0-23, default 0)
     * @param minute Minute (0-59, default 0)
     * @param second Second (0-59, default 0)
     *
     * The timezone is UTC. Components that from_components() rejects result
     * in epoch time.
     */
    timestamp(int year, int month, int day, int hour = 0, int minute = 0, int second = 0);

    /**
     * @brief Construct from individual date and time components with timezone.
     * @param year Year (0-9999)
     * @param month Month (1-12)
     * @param day Day of month (1 to the last day of the month)
     * @param hour Hour (0-23)
     * @param minute Minute (0-59)
     * @param second Second (0-59)
     * @param tz Timezone information
     *
     * Components that from_components() rejects result in epoch time.
     */
    timestamp(int year, int month, int day, int hour, int minute, int second, const timezone& tz);

    /**
     * @brief Convert to std::chrono::system_clock::time_point, reporting a
     * timestamp outside the range of the clock.
     * @return The time point (always in UTC), rounded down to a tick of the
     *         clock, or nullopt if it is outside the range of system_clock
     *
     * Comparison and adding or subtracting a duration work for every year
     * from 0 to 9999, but system_clock reaches only about 292 years either
     * side of 1970 where its duration counts nanoseconds, and so does a
     * difference between two timestamps: see duration_since().
     */
    std::optional<std::chrono::system_clock::time_point> to_time_point() const noexcept;

    /**
     * @brief Implicit conversion to std::chrono::system_clock::time_point.
     * @return The time point (always in UTC), rounded down to a tick of the
     *         clock
     *
     * A timestamp that to_time_point() rejects results in the nearer of
     * time_point::min() and time_point::max(), so it does not order on the
     * wrong side of another time point.
     */
    operator std::chrono::system_clock::time_point() const noexcept;

    /**
     * @brief Implicit conversion to std::string in ISO 8601 format.
     * @return ISO 8601 formatted string representation
     *
     * Always writes the date, the time and the offset, so the string reads
     * back as the same timestamp for any year from 0 to 9999, which
     * arithmetic keeps.
     */
    operator std::string() const;

    /**
     * @brief Three-way comparison operator.
     * @param other The timestamp to compare with
     * @return Comparison result (strong ordering)
     */
    std::strong_ordering operator<=>(const timestamp& other) const;

    /**
     * @brief Equality comparison operator.
     * @param other The timestamp to compare with
     * @return True if the timestamps represent the same moment in time
     */
    bool operator==(const timestamp& other) const;

    /**
     * @brief Add a duration to this timestamp.
     * @param duration The duration to add (in minutes)
     * @return New timestamp with the duration added
     * @throws std::out_of_range if the result is outside the years 0 to 9999;
     *         add() and subtract() report that as nullopt instead
     */
    timestamp operator+(const std::chrono::minutes& duration) const;

    /**
     * @brief Subtract a duration from this timestamp.
     * @param duration The duration to subtract (in minutes)
     * @return New timestamp with the duration subtracted
     * @throws std::out_of_range if the result is outside the years 0 to 9999;
     *         add() and subtract() report that as nullopt instead
     */
    timestamp operator-(const std::chrono::minutes& duration) const;

    /**
     * @brief Add hours to this timestamp.
     * @param duration The duration to add (in hours)
     * @return New timestamp with the duration added
     * @throws std::out_of_range if the result is outside the years 0 to 9999;
     *         add() and subtract() report that as nullopt instead
     */
    timestamp operator+(const std::chrono::hours& duration) const;

    /**
     * @brief Subtract hours from this timestamp.
     * @param duration The duration to subtract (in hours)
     * @return New timestamp with the duration subtracted
     * @throws std::out_of_range if the result is outside the years 0 to 9999;
     *         add() and subtract() report that as nullopt instead
     */
    timestamp operator-(const std::chrono::hours& duration) const;

    /**
     * @brief Add seconds to this timestamp.
     * @param duration The duration to add (in seconds)
     * @return New timestamp with the duration added
     * @throws std::out_of_range if the result is outside the years 0 to 9999;
     *         add() and subtract() report that as nullopt instead
     */
    timestamp operator+(const std::chrono::seconds& duration) const;

    /**
     * @brief Subtract seconds from this timestamp.
     * @param duration The duration to subtract (in seconds)
     * @return New timestamp with the duration subtracted
     * @throws std::out_of_range if the result is outside the years 0 to 9999;
     *         add() and subtract() report that as nullopt instead
     */
    timestamp operator-(const std::chrono::seconds& duration) const;

    /**
     * @brief Add a duration, reporting a result outside the years 0 to 9999.
     * @param duration The duration to add, which minutes and hours convert to
     * @return The timestamp with the duration added, or nullopt if its year,
     *         in this timestamp's offset, is outside 0 to 9999
     *
     * The years are those parsing accepts and the string writes, so the
     * result always reads back.
     */
    std::optional<timestamp> add(std::chrono::seconds duration) const;

    /**
     * @brief Subtract a duration, reporting a result outside the years 0 to
     * 9999.
     * @param duration The duration to subtract, which minutes and hours
     *        convert to
     * @return The timestamp with the duration subtracted, or nullopt if its
     *         year, in this timestamp's offset, is outside 0 to 9999
     */
    std::optional<timestamp> subtract(std::chrono::seconds duration) const;

    /**
     * @brief Calculate the duration from another timestamp, reporting one
     * that system_clock::duration cannot hold.
     * @param other The other timestamp
     * @return Duration from other to this timestamp, rounded toward zero to a
     *         tick of the clock, or nullopt if system_clock::duration cannot
     *         hold it
     *
     * system_clock::duration holds about 292 years where it counts
     * nanoseconds. A conversion to time_point rounds down instead, so
     * subtracting the converted time points can differ by a tick.
     */
    std::optional<std::chrono::system_clock::duration> duration_since(const timestamp& other) const noexcept;

    /**
     * @brief Calculate the duration between two timestamps.
     * @param other The other timestamp
     * @return Duration from other to this timestamp, rounded toward zero to a
     *         tick of the clock
     *
     * A duration that duration_since() rejects results in the nearer of
     * duration::min() and duration::max(). Negating duration::min()
     * overflows, as it has no positive counterpart, so take a difference
     * that may be outside the duration with duration_since().
     */
    std::chrono::system_clock::duration operator-(const timestamp& other) const noexcept;

    /**
     * @brief Format the timestamp using predefined format.
     * @param fmt The format to use
     * @return Formatted string representation
     */
    std::string format(format_type fmt = format_type::iso8601) const;

    /**
     * @brief Format the timestamp using custom format string.
     * @param custom_format strftime-style format string
     * @return Formatted string representation
     *
     * %Y and %F write the year as the ISO 8601 form does: in four digits, so
     * the year 999 is "0999". The other conversions are those of std::put_time.
     */
    std::string format(const std::string& custom_format) const;

    /**
     * @brief Get the date component.
     * @return Reference to the date part
     */
    const date_part& date() const noexcept;

    /**
     * @brief Get the time component.
     * @return Reference to the time part (always present, defaults to 00:00:00)
     */
    const time_part& time() const noexcept;

    /**
     * @brief Get the timezone information.
     * @return Reference to the timezone (always present, defaults to UTC)
     */
    const dross::timezone& timezone() const noexcept;

private:
    // The two 4-byte members side by side keep this to 16 bytes.
    date_part _date;
    dross::timezone _timezone;
    time_part _time;

    // Wall-clock seconds since the epoch, before the offset is applied.
    std::chrono::sys_seconds local_seconds() const noexcept;

    std::chrono::nanoseconds subseconds() const noexcept;

    // The instant in UTC, kept in seconds so that no year from 0 to 9999 overflows.
    std::pair<std::chrono::sys_seconds, std::chrono::nanoseconds> instant() const noexcept;

    // Splits wall-clock seconds and a fraction into the date and time parts.
    void set_local(std::chrono::sys_seconds local, std::chrono::nanoseconds subseconds);

    /**
     * @brief Private helper method for ISO 8601 formatting.
     */
    std::string format_iso8601() const;
};

/**
 * @brief Stream output operator for timestamp.
 * @param os Output stream
 * @param ts timestamp to output
 * @return Reference to the output stream
 *
 * Outputs the timestamp in ISO 8601 format to the stream.
 */
std::ostream& operator<<(std::ostream& os, const timestamp& ts);

}  // namespace dross
