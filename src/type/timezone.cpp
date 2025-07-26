#include <dross/type/timezone.h>
#include <memory>
#include <regex>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <chrono>

namespace dross {

class timezone::storage {
public:
    int offset_minutes;

    storage() : offset_minutes(0) {}  // Default UTC

    storage(int offset) : offset_minutes(offset) {}

    storage(const storage& other) : offset_minutes(other.offset_minutes) {}
};

timezone timezone::utc()
{
    return timezone(std::chrono::minutes(0));
}


timezone timezone::offset(int hours, int minutes)
{
    // Validate input ranges
    if (hours < -12 || hours > 14) {
        return timezone::utc(); // Invalid hours, fallback to UTC
    }
    if (minutes < 0 || minutes > 59) {
        return timezone::utc(); // Invalid minutes, fallback to UTC
    }

    // Calculate total offset in minutes
    int total_minutes = std::abs(hours) * 60 + minutes;
    if (hours < 0) {
        total_minutes = -total_minutes;
    }

    return timezone(std::chrono::minutes(total_minutes));
}

timezone timezone::offset(std::chrono::minutes offset_duration)
{
    // Validate range: -12 hours to +14 hours
    const auto min_offset = std::chrono::minutes(-12 * 60);
    const auto max_offset = std::chrono::minutes(14 * 60);

    if (offset_duration < min_offset || offset_duration > max_offset) {
        return timezone::utc(); // Invalid offset, fallback to UTC
    }

    return timezone(offset_duration);
}

std::optional<timezone> timezone::from_string(const std::string& tz_str)
{
    // Handle UTC indicators
    if (tz_str == "Z" || tz_str == "z") {
        return utc();
    }

    // Parse offset format: [+-]HH:MM or [+-]HHMM
    std::regex offset_regex(R"(^([+-])(\d{1,2}):?(\d{2})$)");
    std::smatch match;

    if (std::regex_match(tz_str, match, offset_regex)) {
        char sign = match[1].str()[0];
        int hours = std::stoi(match[2].str());
        int minutes = std::stoi(match[3].str());

        // Validate ranges
        if (hours > 14 || minutes > 59) {
            return std::nullopt; // Invalid format
        }

        // Calculate offset
        int total_minutes = hours * 60 + minutes;
        if (sign == '-') {
            total_minutes = -total_minutes;
        }

        return timezone(std::chrono::minutes(total_minutes));
    }

    // Parsing failed
    return std::nullopt;
}

timezone::timezone() : _store(std::make_unique<storage>())
{
}

timezone::timezone(const timezone& other) : _store(std::make_unique<storage>(*other._store))
{
}

timezone::timezone(std::chrono::minutes offset) : _store(std::make_unique<storage>(offset.count()))
{
}

timezone::~timezone() = default;

timezone& timezone::operator=(const timezone& other)
{
    if (this != &other) {
        _store = std::make_unique<storage>(*other._store);
    }
    return *this;
}

bool timezone::is_utc() const noexcept
{
    return _store->offset_minutes == 0;
}

std::chrono::minutes timezone::offset() const noexcept
{
    return std::chrono::minutes(_store->offset_minutes);
}

std::string timezone::format() const
{
    int offset = _store->offset_minutes;
    if (offset == 0) {
        return "Z"; // UTC
    }

    // Format as [+-]HH:MM
    char sign = offset >= 0 ? '+' : '-';
    int abs_offset = std::abs(offset);
    int hours = abs_offset / 60;
    int minutes = abs_offset % 60;

    std::ostringstream oss;
    oss << sign << std::setfill('0') << std::setw(2) << hours
        << ":" << std::setw(2) << minutes;

    return oss.str();
}

timezone::operator std::string() const
{
    return format();
}

bool timezone::operator==(const timezone& other) const noexcept
{
    return _store->offset_minutes == other._store->offset_minutes;
}

std::strong_ordering timezone::operator<=>(const timezone& other) const noexcept
{
    // Simply compare offset minutes
    return _store->offset_minutes <=> other._store->offset_minutes;
}

std::ostream& operator<<(std::ostream& os, const timezone& tz)
{
    return os << tz.format();
}

}
