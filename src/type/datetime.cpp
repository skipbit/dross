#include <dross/type/datetime.h>
#include <dross/type/timezone.h>
#include <sstream>
#include <iomanip>
#include <regex>
#include <chrono>

namespace dross {

class datetime::storage {
public:
    std::chrono::system_clock::time_point time_point;
    dross::timezone tz;
    enum datetime::precision precision;

    storage()
        : time_point(std::chrono::system_clock::time_point{})
        , tz(dross::timezone::local())
        , precision(datetime::precision::datetime)
    {
    }

    storage(const storage& other)
        : time_point(other.time_point)
        , tz(other.tz)
        , precision(other.precision)
    {
    }
};

datetime datetime::now()
{
    return datetime(std::chrono::system_clock::now());
}

datetime datetime::date(int year, int month, int day, const dross::timezone& tz)
{
    datetime dt(year, month, day, 0, 0, 0, tz);
    dt._store->precision = datetime::precision::date_only;
    return dt;
}

datetime datetime::time(int hour, int minute, int second, const dross::timezone& tz)
{
    datetime dt(1970, 1, 1, hour, minute, second, tz); // Use epoch date
    dt._store->precision = datetime::precision::time_only;
    return dt;
}

datetime::datetime()
    : _store(std::make_unique<storage>())
{
}

datetime::datetime(const datetime& other)
    : _store(std::make_unique<storage>(*other._store))
{
}

datetime::datetime(const std::chrono::system_clock::time_point& tp)
    : _store(std::make_unique<storage>())
{
    _store->time_point = tp;
    _store->tz = dross::timezone::local(); // time_point has no timezone info
}

datetime::datetime(const std::string& iso8601_str)
    : _store(std::make_unique<storage>())
{
    // Parse ISO 8601 format
    // Support formats:
    // - 2024-01-21T15:30:00+09:00 (offset datetime)
    // - 2024-01-21T15:30:00Z (UTC datetime)
    // - 2024-01-21T15:30:00 (local datetime)
    // - 2024-01-21 (local date)
    // - 15:30:00 (local time)

    std::regex datetime_regex(
        R"(^(?:(\d{4})-(\d{2})-(\d{2})(?:T(\d{2}):(\d{2}):(\d{2})(?:\.(\d+))?(?:(Z)|([+-])(\d{2}):(\d{2}))?)?|(\d{2}):(\d{2}):(\d{2})(?:\.(\d+))?)$)"
    );

    std::smatch match;
    if (std::regex_match(iso8601_str, match, datetime_regex)) {
        int year = 1970, month = 1, day = 1;
        int hour = 0, minute = 0, second = 0;

        if (match[1].matched) { // Full date
            year = std::stoi(match[1].str());
            month = std::stoi(match[2].str());
            day = std::stoi(match[3].str());

            if (match[4].matched) { // Time component
                hour = std::stoi(match[4].str());
                minute = std::stoi(match[5].str());
                second = std::stoi(match[6].str());

                // Handle timezone
                if (match[8].matched) { // Z (UTC)
                    _store->tz = dross::timezone::utc();
                } else if (match[9].matched) { // +/- offset
                    int tz_hour = std::stoi(match[10].str());
                    int tz_minute = std::stoi(match[11].str());
                    int offset = tz_hour * 60 + tz_minute;
                    if (match[9].str() == "-") {
                        offset = -offset;
                    }
                    _store->tz = dross::timezone::offset(offset / 60, std::abs(offset) % 60);
                } else {
                    _store->tz = dross::timezone::local();
                }
            } else {
                // Date without time component
                _store->precision = datetime::precision::date_only;
                _store->tz = dross::timezone::local();
            }
        } else if (match[12].matched) { // Time only
            hour = std::stoi(match[12].str());
            minute = std::stoi(match[13].str());
            second = std::stoi(match[14].str());
            // Time without date component
            _store->precision = datetime::precision::time_only;
            _store->tz = dross::timezone::local();
        }

        // Convert to time_point using modern chrono
        auto ymd = std::chrono::year_month_day{std::chrono::year{year},
                                               std::chrono::month{static_cast<unsigned>(month)},
                                               std::chrono::day{static_cast<unsigned>(day)}};

        auto hms = std::chrono::hh_mm_ss{std::chrono::hours{hour} + std::chrono::minutes{minute} + std::chrono::seconds{second}};

        // Create time_point from date and time
        auto days_since_epoch = std::chrono::sys_days{ymd}.time_since_epoch();
        auto time_of_day = hms.to_duration();

        _store->time_point = std::chrono::system_clock::time_point{days_since_epoch + time_of_day};
    }
    // If parsing fails, leave as epoch time
}

datetime::datetime(const char* iso8601_str)
    : datetime(std::string(iso8601_str))
{
}

datetime::datetime(int year, int month, int day,
                  int hour, int minute, int second,
                  const dross::timezone& tz)
    : _store(std::make_unique<storage>())
{
    _store->tz = tz;

    // Use modern chrono calendar types
    auto ymd = std::chrono::year_month_day{std::chrono::year{year},
                                           std::chrono::month{static_cast<unsigned>(month)},
                                           std::chrono::day{static_cast<unsigned>(day)}};

    auto hms = std::chrono::hh_mm_ss{std::chrono::hours{hour} + std::chrono::minutes{minute} + std::chrono::seconds{second}};

    // Create time_point from date and time
    auto days_since_epoch = std::chrono::sys_days{ymd}.time_since_epoch();
    auto time_of_day = hms.to_duration();

    _store->time_point = std::chrono::system_clock::time_point{days_since_epoch + time_of_day};
}

datetime::~datetime() = default;

datetime& datetime::operator=(const datetime& other)
{
    if (this != &other) {
        _store = std::make_unique<storage>(*other._store);
    }
    return *this;
}

datetime::operator std::chrono::system_clock::time_point() const
{
    // Return UTC time point if timezone is specified
    if (_store->tz.has_offset()) {
        return _store->time_point - std::chrono::minutes(_store->tz.offset_minutes().value());
    }
    return _store->time_point;
}

datetime::operator std::string() const
{
    return format(format_type::iso8601);
}

std::strong_ordering datetime::operator<=>(const datetime& other) const
{
    // Compare UTC time points
    auto this_utc = static_cast<std::chrono::system_clock::time_point>(*this);
    auto other_utc = static_cast<std::chrono::system_clock::time_point>(other);
    return this_utc <=> other_utc;
}

bool datetime::operator==(const datetime& other) const
{
    // Compare UTC time points
    auto this_utc = static_cast<std::chrono::system_clock::time_point>(*this);
    auto other_utc = static_cast<std::chrono::system_clock::time_point>(other);
    return this_utc == other_utc;
}

std::chrono::system_clock::duration datetime::operator-(const datetime& other) const
{
    // Calculate difference using UTC time points
    auto this_utc = static_cast<std::chrono::system_clock::time_point>(*this);
    auto other_utc = static_cast<std::chrono::system_clock::time_point>(other);
    return this_utc - other_utc;
}

datetime datetime::operator+(const std::chrono::minutes& duration) const
{
    datetime result(*this);
    result._store->time_point += duration;
    return result;
}

datetime datetime::operator-(const std::chrono::minutes& duration) const
{
    datetime result(*this);
    result._store->time_point -= duration;
    return result;
}

datetime datetime::operator+(const std::chrono::hours& duration) const
{
    datetime result(*this);
    result._store->time_point += duration;
    return result;
}

datetime datetime::operator-(const std::chrono::hours& duration) const
{
    datetime result(*this);
    result._store->time_point -= duration;
    return result;
}

datetime datetime::operator+(const std::chrono::seconds& duration) const
{
    datetime result(*this);
    result._store->time_point += duration;
    return result;
}

datetime datetime::operator-(const std::chrono::seconds& duration) const
{
    datetime result(*this);
    result._store->time_point -= duration;
    return result;
}

std::string datetime::format(format_type fmt) const
{
    switch (fmt) {
        case format_type::iso8601:
        case format_type::rfc3339:
            return format_iso8601();
    }
    return format_iso8601();
}

std::string datetime::format(const std::string& custom_format) const
{
    // For custom formatting, we still need to use the legacy API temporarily
    // until std::format with chrono support is more widely available
    auto time_c = std::chrono::system_clock::to_time_t(_store->time_point);
    auto tm_ptr = std::gmtime(&time_c);

    if (!tm_ptr) {
        return "";
    }

    std::ostringstream oss;
    oss << std::put_time(tm_ptr, custom_format.c_str());
    return oss.str();
}

int datetime::year() const
{
    auto days_since_epoch = std::chrono::floor<std::chrono::days>(_store->time_point);
    auto ymd = std::chrono::year_month_day{std::chrono::sys_days{days_since_epoch}};

    return static_cast<int>(ymd.year());
}

int datetime::month() const
{
    auto days_since_epoch = std::chrono::floor<std::chrono::days>(_store->time_point);
    auto ymd = std::chrono::year_month_day{std::chrono::sys_days{days_since_epoch}};

    return static_cast<unsigned>(ymd.month());
}

int datetime::day() const
{
    auto days_since_epoch = std::chrono::floor<std::chrono::days>(_store->time_point);
    auto ymd = std::chrono::year_month_day{std::chrono::sys_days{days_since_epoch}};

    return static_cast<unsigned>(ymd.day());
}

int datetime::hour() const
{
    auto days_since_epoch = std::chrono::floor<std::chrono::days>(_store->time_point);
    auto time_of_day = _store->time_point - days_since_epoch;
    auto hms = std::chrono::hh_mm_ss{time_of_day};

    return static_cast<int>(hms.hours().count());
}

int datetime::minute() const
{
    auto days_since_epoch = std::chrono::floor<std::chrono::days>(_store->time_point);
    auto time_of_day = _store->time_point - days_since_epoch;
    auto hms = std::chrono::hh_mm_ss{time_of_day};

    return static_cast<int>(hms.minutes().count());
}

int datetime::second() const
{
    auto days_since_epoch = std::chrono::floor<std::chrono::days>(_store->time_point);
    auto time_of_day = _store->time_point - days_since_epoch;
    auto hms = std::chrono::hh_mm_ss{time_of_day};

    return static_cast<int>(hms.seconds().count());
}

bool datetime::has_timezone() const noexcept
{
    return _store->tz.has_offset();
}

dross::timezone datetime::timezone() const noexcept
{
    return _store->tz;
}

enum datetime::precision datetime::precision() const noexcept
{
    return _store->precision;
}


std::string datetime::format_iso8601() const
{
    std::ostringstream oss;

    if (_store->precision == datetime::precision::date_only) {
        // Format date only using modern chrono
        auto days_since_epoch = std::chrono::floor<std::chrono::days>(_store->time_point);
        auto ymd = std::chrono::year_month_day{std::chrono::sys_days{days_since_epoch}};

        oss << std::setfill('0')
            << std::setw(4) << static_cast<int>(ymd.year()) << "-"
            << std::setw(2) << static_cast<unsigned>(ymd.month()) << "-"
            << std::setw(2) << static_cast<unsigned>(ymd.day());
    } else if (_store->precision == datetime::precision::time_only) {
        // Format time only using modern chrono
        auto days_since_epoch = std::chrono::floor<std::chrono::days>(_store->time_point);
        auto time_of_day = _store->time_point - days_since_epoch;
        auto hms = std::chrono::hh_mm_ss{time_of_day};

        oss << std::setfill('0')
            << std::setw(2) << hms.hours().count() << ":"
            << std::setw(2) << hms.minutes().count() << ":"
            << std::setw(2) << hms.seconds().count();
    } else {
        // Full datetime using modern chrono
        auto days_since_epoch = std::chrono::floor<std::chrono::days>(_store->time_point);
        auto ymd = std::chrono::year_month_day{std::chrono::sys_days{days_since_epoch}};
        auto time_of_day = _store->time_point - days_since_epoch;
        auto hms = std::chrono::hh_mm_ss{time_of_day};

        oss << std::setfill('0')
            << std::setw(4) << static_cast<int>(ymd.year()) << "-"
            << std::setw(2) << static_cast<unsigned>(ymd.month()) << "-"
            << std::setw(2) << static_cast<unsigned>(ymd.day()) << "T"
            << std::setw(2) << hms.hours().count() << ":"
            << std::setw(2) << hms.minutes().count() << ":"
            << std::setw(2) << hms.seconds().count();

        // Add timezone information
        if (_store->tz.has_offset()) {
            oss << _store->tz.format();
        }
    }

    return oss.str();
}

std::ostream& operator<<(std::ostream& os, const datetime& dt)
{
    return os << static_cast<std::string>(dt);
}

}
