#pragma once

#include <memory>
#include <string>
#include <chrono>
#include <compare>
#include <optional>
#include "timezone.h"

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
 * - Integration with std::chrono for duration arithmetic
 * - Value semantics (copyable and assignable)
 * - Thread-safe for read operations
 * - Support for date-only and full timestamp values
 * - Factory methods for common timestamp patterns
 * - Compositional design with separate date and time components
 *
 * Supported timestamp formats (ISO 8601 standard):
 * - Offset timestamp: 2024-01-21T15:30:00+09:00
 * - UTC timestamp: 2024-01-21T15:30:00Z
 * - Local timestamp: 2024-01-21T15:30:00
 * - Local date: 2024-01-21
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
 * timestamp now = timestamp::now();
 * timestamp meeting(2024, 1, 21, 15, 30, 0, timezone::offset(9)); // +09:00
 * timestamp utc_meeting(2024, 1, 21, 6, 30, 0, timezone::utc());   // UTC
 *
 * // Date-only timestamps using constructor
 * timestamp birthday(1990, 12, 25);  // Date only (time defaults to 00:00:00)
 *
 * // String parsing
 * timestamp iso_meeting("2024-01-21T15:30:00+09:00");
 *
 * // Duration arithmetic
 * timestamp tomorrow = now + std::chrono::hours(24);
 * auto age = now - birthday;
 *
 * // Accessing components
 * std::cout << "Year: " << meeting.date().year() << std::endl;
 * std::cout << "Hour: " << meeting.time().hour() << std::endl;
 * std::cout << "Timezone: " << meeting.timezone().format() << std::endl;
 *
 * // Formatting
 * std::string iso_str = meeting.format(timestamp::format_type::iso8601);
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
        iso8601,    // ISO 8601 format: 2024-01-21T15:30:00+09:00
        rfc3339     // RFC 3339 format (essentially same as ISO 8601)
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
        class impl;
        std::unique_ptr<impl> _impl;

        // Private constructors - only timestamp can create date objects
        date_part();
        date_part(int year, int month, int day);
        date_part(const std::chrono::year_month_day& ymd);
        date_part(const std::string& iso8601_date);

    public:
        /**
         * @brief Copy constructor.
         */
        date_part(const date_part& other);

        /**
         * @brief Destructor.
         */
        ~date_part();

        /**
         * @brief Copy assignment operator.
         */
        date_part& operator=(const date_part& other);

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
     * to hour, minute, and second components with microsecond precision.
     */
    class time_part {
        friend class timestamp;
    private:
        class impl;
        std::unique_ptr<impl> _impl;

        // Private constructors - only timestamp can create time objects
        time_part();
        time_part(int hour, int minute, int second);
        time_part(const std::string& iso8601_time);
        template<typename Duration>
        time_part(const std::chrono::hh_mm_ss<Duration>& hms);

    public:
        /**
         * @brief Static midnight constant.
         */
        static const time_part midnight;

        /**
         * @brief Copy constructor.
         */
        time_part(const time_part& other);

        /**
         * @brief Destructor.
         */
        ~time_part();

        /**
         * @brief Copy assignment operator.
         */
        time_part& operator=(const time_part& other);

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
        std::chrono::hh_mm_ss<std::chrono::microseconds> to_hh_mm_ss() const noexcept;

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
     * @brief Default constructor creating epoch time (1970-01-01T00:00:00Z).
     */
    timestamp();

    /**
     * @brief Copy constructor.
     * @param other The timestamp to copy from
     */
    timestamp(const timestamp& other);

    /**
     * @brief Construct from std::chrono::system_clock::time_point.
     * @param tp The time point to construct from
     *
     * The resulting timestamp will be in UTC (no timezone offset).
     */
    timestamp(const std::chrono::system_clock::time_point& tp);

    /**
     * @brief Construct from ISO 8601 string.
     * @param iso8601_str The ISO 8601 formatted string
     *
     * Supports various ISO 8601 formats:
     * - 2024-01-21T15:30:00+09:00 (offset timestamp)
     * - 2024-01-21T15:30:00 (timestamp without timezone)
     * - 2024-01-21 (date only)
     *
     * Invalid formats will result in epoch time.
     */
    timestamp(const std::string& iso8601_str);

    /**
     * @brief Construct from const char*.
     * @param iso8601_str The ISO 8601 formatted string
     */
    timestamp(const char* iso8601_str);

    /**
     * @brief Construct from individual date and time components.
     * @param year Year (e.g., 2024)
     * @param month Month (1-12)
     * @param day Day of month (1-31)
     * @param hour Hour (0-23, default 0)
     * @param minute Minute (0-59, default 0)
     * @param second Second (0-59, default 0)
     */
    timestamp(int year, int month, int day,
             int hour = 0, int minute = 0, int second = 0);

    /**
     * @brief Construct from individual date and time components with timezone.
     * @param year Year (e.g., 2024)
     * @param month Month (1-12)
     * @param day Day of month (1-31)
     * @param hour Hour (0-23)
     * @param minute Minute (0-59)
     * @param second Second (0-59)
     * @param tz Timezone information
     */
    timestamp(int year, int month, int day,
             int hour, int minute, int second,
             const timezone& tz);

    /**
     * @brief Destructor.
     */
    ~timestamp();

    /**
     * @brief Copy assignment operator.
     */
    timestamp& operator=(const timestamp& other);

    /**
     * @brief Implicit conversion to std::chrono::system_clock::time_point.
     * @return The time point representation (always in UTC)
     */
    operator std::chrono::system_clock::time_point() const;

    /**
     * @brief Implicit conversion to std::string in ISO 8601 format.
     * @return ISO 8601 formatted string representation
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
     */
    timestamp operator+(const std::chrono::minutes& duration) const;

    /**
     * @brief Subtract a duration from this timestamp.
     * @param duration The duration to subtract (in minutes)
     * @return New timestamp with the duration subtracted
     */
    timestamp operator-(const std::chrono::minutes& duration) const;

    /**
     * @brief Add hours to this timestamp.
     * @param duration The duration to add (in hours)
     * @return New timestamp with the duration added
     */
    timestamp operator+(const std::chrono::hours& duration) const;

    /**
     * @brief Subtract hours from this timestamp.
     * @param duration The duration to subtract (in hours)
     * @return New timestamp with the duration subtracted
     */
    timestamp operator-(const std::chrono::hours& duration) const;

    /**
     * @brief Add seconds to this timestamp.
     * @param duration The duration to add (in seconds)
     * @return New timestamp with the duration added
     */
    timestamp operator+(const std::chrono::seconds& duration) const;

    /**
     * @brief Subtract seconds from this timestamp.
     * @param duration The duration to subtract (in seconds)
     * @return New timestamp with the duration subtracted
     */
    timestamp operator-(const std::chrono::seconds& duration) const;

    /**
     * @brief Calculate the duration between two timestamps.
     * @param other The other timestamp
     * @return Duration from other to this timestamp
     */
    std::chrono::system_clock::duration operator-(const timestamp& other) const;

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
    class storage;
    std::unique_ptr<storage> _store;

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

}
