#pragma once

#include <memory>
#include <string>
#include <chrono>
#include <optional>
#include <compare>

namespace dross {

/**
 * @brief Format options for datetime string representation.
 */
enum class datetime_format {
    iso8601,    // ISO 8601 format: 2024-01-21T15:30:00+09:00
    rfc3339,    // RFC 3339 format (essentially same as ISO 8601)
    custom      // Custom format using strftime-style format string
};

/**
 * @brief Concept that defines types suitable for datetime construction.
 *
 * This concept accepts types that can be used to construct a datetime object:
 * - std::chrono::system_clock::time_point for precise time point construction
 * - std::string for ISO 8601 formatted datetime strings
 * - const char* for ISO 8601 formatted datetime string literals
 *
 * This enables flexible datetime construction from various time representations
 * commonly used in applications, configuration files, and data interchange formats.
 */
template <typename T>
concept datetime_type = std::same_as<T, std::chrono::system_clock::time_point> ||
                       std::same_as<T, std::string> ||
                       std::same_as<T, const char*>;

/**
 * @brief Date and time handling with timezone support.
 *
 * The datetime class provides comprehensive date and time operations with
 * full timezone support. It uses std::chrono internally for high precision
 * time calculations while providing a user-friendly API for common operations.
 * This class is designed as a general-purpose datetime type suitable for
 * configuration files, data serialization, logging, scheduling, and any
 * application requiring robust datetime handling.
 *
 * Key features:
 * - ISO 8601 and RFC 3339 format support
 * - Timezone-aware operations
 * - Integration with std::chrono for duration arithmetic
 * - Value semantics (copyable and assignable)
 * - Thread-safe for read operations
 * - Support for date-only, time-only, and full datetime values
 *
 * Supported datetime formats (ISO 8601 standard):
 * - Offset datetime: 2024-01-21T15:30:00+09:00
 * - Local datetime: 2024-01-21T15:30:00
 * - Local date: 2024-01-21
 * - Local time: 15:30:00
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
 * // Basic usage
 * datetime now = datetime::now();
 * datetime birthday(1990, 12, 25, 10, 30, 0);
 *
 * // String parsing
 * datetime meeting("2024-01-21T15:30:00+09:00");
 *
 * // Duration arithmetic
 * datetime tomorrow = now + std::chrono::hours(24);
 * auto age = now - birthday;
 *
 * // Formatting
 * std::string iso_str = meeting.format(datetime_format::iso8601);
 * std::string custom = meeting.format("%Y-%m-%d %H:%M");
 *
 * // Conversion to std::chrono
 * auto time_point = static_cast<std::chrono::system_clock::time_point>(now);
 * @endcode
 */
class datetime final {
public:
    /**
     * @brief Create a datetime representing the current moment.
     * @return datetime object set to current system time with local timezone
     */
    static datetime now();

    /**
     * @brief Default constructor creating epoch time (1970-01-01T00:00:00Z).
     */
    datetime();

    /**
     * @brief Copy constructor.
     * @param other The datetime to copy from
     */
    datetime(const datetime& other);

    /**
     * @brief Construct from std::chrono::system_clock::time_point.
     * @param tp The time point to construct from
     *
     * The resulting datetime will be in UTC (no timezone offset).
     */
    datetime(const std::chrono::system_clock::time_point& tp);

    /**
     * @brief Construct from ISO 8601 string.
     * @param iso8601_str The ISO 8601 formatted string
     *
     * Supports various ISO 8601 formats:
     * - 2024-01-21T15:30:00+09:00 (offset datetime)
     * - 2024-01-21T15:30:00 (local datetime)
     * - 2024-01-21 (local date)
     * - 15:30:00 (local time)
     *
     * Invalid formats will result in epoch time.
     */
    datetime(const std::string& iso8601_str);

    /**
     * @brief Construct from const char*.
     * @param iso8601_str The ISO 8601 formatted string
     */
    datetime(const char* iso8601_str);

    /**
     * @brief Construct from individual date and time components.
     * @param year Year (e.g., 2024)
     * @param month Month (1-12)
     * @param day Day of month (1-31)
     * @param hour Hour (0-23, default 0)
     * @param minute Minute (0-59, default 0)
     * @param second Second (0-59, default 0)
     * @param timezone_offset_minutes Optional timezone offset in minutes from UTC
     *
     * If timezone_offset_minutes is not specified, the datetime is treated as local.
     * Positive offsets are east of UTC, negative offsets are west of UTC.
     */
    datetime(int year, int month, int day,
             int hour = 0, int minute = 0, int second = 0,
             std::optional<int> timezone_offset_minutes = std::nullopt);

    /**
     * @brief Destructor.
     */
    ~datetime();

    /**
     * @brief Copy assignment operator.
     */
    datetime& operator=(const datetime& other);

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
     * @param other The datetime to compare with
     * @return Comparison result (strong ordering)
     */
    std::strong_ordering operator<=>(const datetime& other) const;

    /**
     * @brief Equality comparison operator.
     * @param other The datetime to compare with
     * @return True if the datetimes represent the same moment in time
     */
    bool operator==(const datetime& other) const;

    /**
     * @brief Add a duration to this datetime.
     * @param duration The duration to add (in minutes)
     * @return New datetime with the duration added
     */
    datetime operator+(const std::chrono::minutes& duration) const;

    /**
     * @brief Subtract a duration from this datetime.
     * @param duration The duration to subtract (in minutes)
     * @return New datetime with the duration subtracted
     */
    datetime operator-(const std::chrono::minutes& duration) const;

    /**
     * @brief Add hours to this datetime.
     * @param duration The duration to add (in hours)
     * @return New datetime with the duration added
     */
    datetime operator+(const std::chrono::hours& duration) const;

    /**
     * @brief Subtract hours from this datetime.
     * @param duration The duration to subtract (in hours)
     * @return New datetime with the duration subtracted
     */
    datetime operator-(const std::chrono::hours& duration) const;

    /**
     * @brief Add seconds to this datetime.
     * @param duration The duration to add (in seconds)
     * @return New datetime with the duration added
     */
    datetime operator+(const std::chrono::seconds& duration) const;

    /**
     * @brief Subtract seconds from this datetime.
     * @param duration The duration to subtract (in seconds)
     * @return New datetime with the duration subtracted
     */
    datetime operator-(const std::chrono::seconds& duration) const;

    /**
     * @brief Calculate the duration between two datetimes.
     * @param other The other datetime
     * @return Duration from other to this datetime
     */
    std::chrono::system_clock::duration operator-(const datetime& other) const;

    /**
     * @brief Format the datetime using predefined format.
     * @param fmt The format to use
     * @return Formatted string representation
     */
    std::string format(datetime_format fmt = datetime_format::iso8601) const;

    /**
     * @brief Format the datetime using custom format string.
     * @param custom_format strftime-style format string
     * @return Formatted string representation
     */
    std::string format(const std::string& custom_format) const;

    /**
     * @brief Get the year component.
     * @return Year (e.g., 2024)
     */
    int year() const;

    /**
     * @brief Get the month component.
     * @return Month (1-12)
     */
    int month() const;

    /**
     * @brief Get the day component.
     * @return Day of month (1-31)
     */
    int day() const;

    /**
     * @brief Get the hour component.
     * @return Hour (0-23)
     */
    int hour() const;

    /**
     * @brief Get the minute component.
     * @return Minute (0-59)
     */
    int minute() const;

    /**
     * @brief Get the second component.
     * @return Second (0-59)
     */
    int second() const;

    /**
     * @brief Check if this datetime has timezone information.
     * @return True if timezone offset is available
     */
    bool has_timezone() const noexcept;

    /**
     * @brief Get the timezone offset in minutes.
     * @return Timezone offset from UTC in minutes, or nullopt if no timezone info
     *
     * Positive values indicate east of UTC, negative values indicate west of UTC.
     */
    std::optional<int> timezone_offset_minutes() const noexcept;

private:
    class storage;
    std::unique_ptr<storage> _store;
    
    /**
     * @brief Private helper method for ISO 8601 formatting.
     */
    std::string format_iso8601() const;
};

/**
 * @brief Stream output operator for datetime.
 * @param os Output stream
 * @param dt datetime to output
 * @return Reference to the output stream
 *
 * Outputs the datetime in ISO 8601 format to the stream.
 */
std::ostream& operator<<(std::ostream& os, const datetime& dt);

}