#pragma once

#include <string>
#include <compare>
#include <iosfwd>
#include <memory>
#include <chrono>
#include <optional>

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
 * - Integration with timestamp class
 * - Value semantics (copyable and assignable)
 *
 * Supported timezone types:
 * - UTC: Coordinated Universal Time (offset +00:00)
 * - Fixed offset: Custom offset from UTC in hours/minutes
 *
 * @code
 * // Factory methods
 * auto utc = timezone::utc();
 * auto jst = timezone::offset(9);           // +09:00
 * auto pdt = timezone::offset(-7, 0);       // -07:00
 * auto custom = timezone::from_string("+05:30");  // India Standard Time
 *
 * // Usage with timestamp
 * timestamp ts(2024, 1, 21, 15, 30, 0, timezone::offset(9));
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
     * @brief Create timezone with chrono-based offset.
     * @param offset_duration Offset from UTC as chrono::minutes
     * @return timezone with specified offset
     *
     * Positive values are east of UTC, negative values are west of UTC.
     * @code
     * auto jst = timezone::offset(std::chrono::minutes(540));  // +09:00
     * auto ist = timezone::offset(std::chrono::hours(5) + std::chrono::minutes(30));  // +05:30
     * @endcode
     */
    static timezone offset(std::chrono::minutes offset_duration);

    /**
     * @brief Parse timezone from ISO 8601 offset string.
     * @param tz_str Timezone string (e.g., "+09:00", "-05:30", "Z")
     * @return optional timezone parsed from string, nullopt if parsing fails
     *
     * @code
     * auto tz = timezone::from_string("+09:00");
     * if (tz) {
     *     std::cout << "Parsed: " << tz->format() << std::endl;
     * } else {
     *     std::cout << "Invalid timezone string" << std::endl;
     * }
     * @endcode
     */
    static std::optional<timezone> from_string(const std::string& tz_str);

    /**
     * @brief Default constructor creating UTC timezone.
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
     * @brief Check if this is UTC timezone.
     * @return True if timezone represents UTC (+00:00)
     */
    bool is_utc() const noexcept;

    /**
     * @brief Get the timezone offset from UTC.
     * @return Offset as chrono::minutes (UTC=0min, JST=540min, PST=-480min)
     *
     * Positive values indicate east of UTC, negative values indicate west of UTC.
     * Use std::chrono::duration_cast to convert to hours if needed.
     */
    std::chrono::minutes offset() const noexcept;

    /**
     * @brief Format timezone as ISO 8601 offset string.
     * @return Formatted string (e.g., "+09:00", "-05:30", "Z" for UTC)
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
     * @param offset Offset from UTC as chrono::minutes
     */
    explicit timezone(std::chrono::minutes offset);
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
