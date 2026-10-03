#include <dross/type/timezone.h>

#include <chrono>
#include <cmath>
#include <iomanip>
#include <locale>
#include <memory>
#include <regex>
#include <sstream>

namespace dross {

namespace {

// RFC 3339 allows offsets up to 23:59 either side of UTC.
constexpr std::chrono::minutes max_offset = std::chrono::hours(23) + std::chrono::minutes(59);

bool in_range(std::chrono::minutes offset)
{
    return (-max_offset <= offset) && (offset <= max_offset);
}

}  // namespace

class timezone::storage {
public:
    int offset_minutes;

    storage()
        : offset_minutes(0)
    {
    }  // Default UTC

    storage(int offset)
        : offset_minutes(offset)
    {
    }

    storage(const storage& other)
        : offset_minutes(other.offset_minutes)
    {
    }
};

timezone timezone::utc()
{
    return timezone(std::chrono::minutes(0));
}

timezone timezone::offset(int hours, int minutes)
{
    if ((minutes < 0) || (minutes > 59)) {
        return timezone::utc();  // Invalid minutes, fallback to UTC
    }

    // The minutes extend the hours away from UTC.
    std::chrono::minutes total = std::chrono::abs(std::chrono::hours(hours)) + std::chrono::minutes(minutes);
    return offset((hours < 0) ? -total : total);
}

timezone timezone::offset(std::chrono::minutes offset_duration)
{
    if (! in_range(offset_duration)) {
        return timezone::utc();  // Invalid offset, fallback to UTC
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

        if (minutes > 59) {
            return std::nullopt;  // Invalid format
        }

        // Calculate offset
        int total_minutes = (hours * 60 + minutes);
        if (sign == '-') {
            total_minutes = -total_minutes;
        }

        if (! in_range(std::chrono::minutes(total_minutes))) {
            return std::nullopt;
        }
        return timezone(std::chrono::minutes(total_minutes));
    }

    // Parsing failed
    return std::nullopt;
}

timezone::timezone()
    : _store(std::make_unique<storage>())
{
}

timezone::timezone(const timezone& other)
    : _store(std::make_unique<storage>(*other._store))
{
}

timezone::timezone(std::chrono::minutes offset)
    : _store(std::make_unique<storage>(offset.count()))
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
    return (_store->offset_minutes == 0);
}

std::chrono::minutes timezone::offset() const noexcept
{
    return std::chrono::minutes(_store->offset_minutes);
}

std::string timezone::format() const
{
    int offset = _store->offset_minutes;
    if (offset == 0) {
        return "Z";  // UTC
    }

    // Format as [+-]HH:MM
    char sign = offset >= 0 ? '+' : '-';
    int abs_offset = std::abs(offset);
    int hours = (abs_offset / 60);
    int minutes = (abs_offset % 60);

    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << sign << std::setfill('0') << std::setw(2) << hours << ":" << std::setw(2) << minutes;

    return oss.str();
}

timezone::operator std::string() const
{
    return format();
}

bool timezone::operator==(const timezone& other) const noexcept
{
    return (_store->offset_minutes == other._store->offset_minutes);
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

}  // namespace dross
