#include <dross/type/timezone.h>
#include <memory>
#include <regex>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace dross {

class timezone::storage {
public:
    std::optional<int> offset_minutes;

    storage() : offset_minutes(std::nullopt) {}

    storage(std::optional<int> offset) : offset_minutes(offset) {}

    storage(const storage& other) : offset_minutes(other.offset_minutes) {}
};

timezone timezone::utc()
{
    return timezone(0);
}

timezone timezone::local()
{
    return timezone(std::nullopt);
}

timezone timezone::offset(int hours, int minutes)
{
    // Validate input ranges
    if (hours < -12 || hours > 14) {
        return timezone::local(); // Invalid hours, fallback to local
    }
    if (minutes < 0 || minutes > 59) {
        return timezone::local(); // Invalid minutes, fallback to local
    }

    // Calculate total offset in minutes
    int total_minutes = std::abs(hours) * 60 + minutes;
    if (hours < 0) {
        total_minutes = -total_minutes;
    }

    return timezone(total_minutes);
}

timezone timezone::from_string(const std::string& tz_str)
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
            return local(); // Invalid format, fallback to local
        }

        // Calculate offset
        int total_minutes = hours * 60 + minutes;
        if (sign == '-') {
            total_minutes = -total_minutes;
        }

        return timezone(total_minutes);
    }

    // Parsing failed, return local timezone
    return local();
}

timezone::timezone() : _store(std::make_unique<storage>())
{
}

timezone::timezone(const timezone& other) : _store(std::make_unique<storage>(*other._store))
{
}

timezone::timezone(std::optional<int> offset_minutes) : _store(std::make_unique<storage>(offset_minutes))
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

bool timezone::is_local() const noexcept
{
    return !_store->offset_minutes.has_value();
}

bool timezone::is_utc() const noexcept
{
    return _store->offset_minutes.has_value() && _store->offset_minutes.value() == 0;
}

bool timezone::has_offset() const noexcept
{
    return _store->offset_minutes.has_value();
}

std::optional<int> timezone::offset_minutes() const noexcept
{
    return _store->offset_minutes;
}

std::string timezone::format() const
{
    if (!_store->offset_minutes.has_value()) {
        return ""; // Local timezone has no string representation
    }

    int offset = _store->offset_minutes.value();
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
    // Local timezones are considered equal to each other
    if (!_store->offset_minutes.has_value() && !other._store->offset_minutes.has_value()) {
        return std::strong_ordering::equal;
    }

    // Local timezone is always "less than" any fixed offset
    if (!_store->offset_minutes.has_value()) {
        return std::strong_ordering::less;
    }
    if (!other._store->offset_minutes.has_value()) {
        return std::strong_ordering::greater;
    }

    // Compare offset values
    return _store->offset_minutes.value() <=> other._store->offset_minutes.value();
}

std::ostream& operator<<(std::ostream& os, const timezone& tz)
{
    return os << tz.format();
}

}