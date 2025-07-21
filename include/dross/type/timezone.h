#pragma once

#include <string>
#include <optional>
#include <compare>
#include <iosfwd>
#include <memory>

namespace dross {

/**
 * @brief Timezone representation with offset and formatting support.
 *
 * The timezone class provides a type-safe and intuitive way to handle
 * timezone information in datetime objects. It supports UTC, local time,
 * and fixed offset timezones with clear, readable API.
 *
 * Key features:
 * - Type-safe timezone representation
 * - Clear factory methods for common timezone types
 * - ISO 8601 compliant formatting
 * - Integration with datetime class
 * - Value semantics (copyable and assignable)
 *
 * Supported timezone types:
 * - UTC: Coordinated Universal Time (offset +00:00)
 * - Local: System local timezone (no offset stored)
 * - Fixed offset: Custom offset from UTC in hours/minutes
 *
 * @code
 * // Factory methods
 * auto utc = timezone::utc();
 * auto jst = timezone::offset(9);           // +09:00
 * auto pdt = timezone::offset(-7, 0);       // -07:00
 * auto custom = timezone::from_string("+05:30");  // India Standard Time
 *
 * // Usage with datetime
 * datetime dt(2024, 1, 21, 15, 30, 0, timezone::offset(9));
 * @endcode
 */
class timezone final {
public:
    /**
     * @brief Create UTC timezone (+00:00).
     * @return timezone representing UTC
     */
    static timezone utc();

    /**
     * @brief Create local system timezone.
     * @return timezone representing local system time
     */
    static timezone local();

    /**
     * @brief Create timezone with fixed offset.
     * @param hours Offset hours from UTC (-12 to +14)
     * @param minutes Offset minutes from UTC (0 to 59)
     * @return timezone with specified offset
     *
     * Positive values are east of UTC, negative values are west of UTC.
     * The minutes parameter is always added to the absolute value of hours.
     */
    static timezone offset(int hours, int minutes = 0);

    /**
     * @brief Parse timezone from ISO 8601 offset string.
     * @param tz_str Timezone string (e.g., "+09:00", "-05:30", "Z")
     * @return timezone parsed from string, or local timezone if parsing fails
     */
    static timezone from_string(const std::string& tz_str);

    /**
     * @brief Default constructor creating local timezone.
     */
    timezone();

    /**
     * @brief Copy constructor.
     * @param other The timezone to copy from
     */
    timezone(const timezone& other);

    /**
     * @brief Destructor.
     */
    ~timezone();

    /**
     * @brief Copy assignment operator.
     */
    timezone& operator=(const timezone& other);

    /**
     * @brief Check if this is a local timezone.
     * @return True if timezone represents local system time
     */
    bool is_local() const noexcept;

    /**
     * @brief Check if this is UTC timezone.
     * @return True if timezone represents UTC (+00:00)
     */
    bool is_utc() const noexcept;

    /**
     * @brief Check if this timezone has a fixed offset.
     * @return True if timezone has a specific offset from UTC
     */
    bool has_offset() const noexcept;

    /**
     * @brief Get the timezone offset in minutes from UTC.
     * @return Offset in minutes, or nullopt for local timezone
     *
     * Positive values indicate east of UTC, negative values indicate west of UTC.
     */
    std::optional<int> offset_minutes() const noexcept;

    /**
     * @brief Format timezone as ISO 8601 offset string.
     * @return Formatted string (e.g., "+09:00", "-05:30", "Z", or empty for local)
     */
    std::string format() const;

    /**
     * @brief Implicit conversion to std::string.
     * @return ISO 8601 formatted timezone string
     */
    operator std::string() const;

    /**
     * @brief Equality comparison.
     * @param other The timezone to compare with
     * @return True if timezones are equivalent
     */
    bool operator==(const timezone& other) const noexcept;

    /**
     * @brief Three-way comparison operator.
     * @param other The timezone to compare with
     * @return Comparison result based on offset
     */
    std::strong_ordering operator<=>(const timezone& other) const noexcept;

private:
    class storage;
    std::unique_ptr<storage> _store;

    /**
     * @brief Private constructor for internal use.
     * @param offset_minutes Optional offset in minutes from UTC
     */
    explicit timezone(std::optional<int> offset_minutes);
};

/**
 * @brief Stream output operator for timezone.
 * @param os Output stream
 * @param tz timezone to output
 * @return Reference to the output stream
 *
 * Outputs the timezone in ISO 8601 format to the stream.
 */
std::ostream& operator<<(std::ostream& os, const timezone& tz);

}