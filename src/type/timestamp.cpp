#include <dross/type/timestamp.h>
#include <sstream>
#include <iomanip>
#include <regex>
#include <chrono>
#include <optional>

namespace dross {

// Date implementation
class timestamp::date_part::impl {
public:
    std::chrono::year_month_day ymd;

    impl()
        : ymd{std::chrono::year{1970}, std::chrono::month{1}, std::chrono::day{1}}
    {
    }

    impl(int year, int month, int day)
        : ymd{std::chrono::year{year},
              std::chrono::month{static_cast<unsigned>(month)},
              std::chrono::day{static_cast<unsigned>(day)}}
    {
    }

    impl(const std::chrono::year_month_day& ymd)
        : ymd(ymd)
    {
    }

    impl(const impl& other)
        : ymd(other.ymd)
    {
    }
};

timestamp::date_part::date_part()
    : _impl(std::make_unique<impl>())
{
}

timestamp::date_part::date_part(int year, int month, int day)
    : _impl(std::make_unique<impl>(year, month, day))
{
}

timestamp::date_part::date_part(const std::chrono::year_month_day& ymd)
    : _impl(std::make_unique<impl>(ymd))
{
}

timestamp::date_part::date_part(const std::string& iso8601_date)
    : _impl(std::make_unique<impl>())
{
    // Parse ISO 8601 date format: YYYY-MM-DD
    std::regex date_regex(R"(^(\d{4})-(\d{2})-(\d{2})$)");
    std::smatch match;

    if (std::regex_match(iso8601_date, match, date_regex)) {
        int year = std::stoi(match[1].str());
        int month = std::stoi(match[2].str());
        int day = std::stoi(match[3].str());

        _impl->ymd = std::chrono::year_month_day{
            std::chrono::year{year},
            std::chrono::month{static_cast<unsigned>(month)},
            std::chrono::day{static_cast<unsigned>(day)}
        };
    }
    // If parsing fails, leave as epoch date
}

timestamp::date_part::date_part(const date_part& other)
    : _impl(std::make_unique<impl>(*other._impl))
{
}

timestamp::date_part::~date_part() = default;

timestamp::date_part& timestamp::date_part::operator=(const date_part& other)
{
    if (this != &other) {
        _impl = std::make_unique<impl>(*other._impl);
    }
    return *this;
}

int timestamp::date_part::year() const noexcept
{
    return static_cast<int>(_impl->ymd.year());
}

int timestamp::date_part::month() const noexcept
{
    return static_cast<unsigned>(_impl->ymd.month());
}

int timestamp::date_part::day() const noexcept
{
    return static_cast<unsigned>(_impl->ymd.day());
}

timestamp::date_part::operator std::string() const
{
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << year() << "-"
        << std::setw(2) << month() << "-"
        << std::setw(2) << day();
    return oss.str();
}

timestamp::date_part::operator std::chrono::year_month_day() const noexcept
{
    return _impl->ymd;
}

std::strong_ordering timestamp::date_part::operator<=>(const date_part& other) const noexcept
{
    auto this_days = std::chrono::sys_days{_impl->ymd}.time_since_epoch();
    auto other_days = std::chrono::sys_days{other._impl->ymd}.time_since_epoch();
    return this_days <=> other_days;
}

bool timestamp::date_part::operator==(const date_part& other) const noexcept
{
    return _impl->ymd == other._impl->ymd;
}

// Time implementation
class timestamp::time_part::impl {
public:
    std::chrono::hh_mm_ss<std::chrono::nanoseconds> hms;

    impl()
        : hms{std::chrono::nanoseconds{0}}
    {
    }

    impl(int hour, int minute, int second)
        : hms{std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::hours{hour} + std::chrono::minutes{minute} + std::chrono::seconds{second})}
    {
    }

    template<typename Duration>
    impl(const std::chrono::hh_mm_ss<Duration>& hms_in)
        : hms{std::chrono::duration_cast<std::chrono::nanoseconds>(hms_in.to_duration())}
    {
    }

    impl(const impl& other)
        : hms(other.hms)
    {
    }
};

// Define static member
const timestamp::time_part timestamp::time_part::midnight;

timestamp::time_part::time_part()
    : _impl(std::make_unique<impl>())
{
}

timestamp::time_part::time_part(int hour, int minute, int second)
    : _impl(std::make_unique<impl>(hour, minute, second))
{
}

timestamp::time_part::time_part(const std::string& iso8601_time)
    : _impl(std::make_unique<impl>())
{
    // Parse ISO 8601 time format: HH:MM:SS
    std::regex time_regex(R"(^(\d{2}):(\d{2}):(\d{2})$)");
    std::smatch match;

    if (std::regex_match(iso8601_time, match, time_regex)) {
        int hour = std::stoi(match[1].str());
        int minute = std::stoi(match[2].str());
        int second = std::stoi(match[3].str());

        auto duration = std::chrono::hours{hour} +
                       std::chrono::minutes{minute} +
                       std::chrono::seconds{second};
        _impl->hms = std::chrono::hh_mm_ss{
            std::chrono::duration_cast<std::chrono::nanoseconds>(duration)
        };
    }
    // If parsing fails, leave as midnight
}

template<typename Duration>
timestamp::time_part::time_part(const std::chrono::hh_mm_ss<Duration>& hms)
    : _impl(std::make_unique<impl>(hms))
{
}

// Explicit instantiations for common durations
template timestamp::time_part::time_part(const std::chrono::hh_mm_ss<std::chrono::seconds>&);
template timestamp::time_part::time_part(const std::chrono::hh_mm_ss<std::chrono::nanoseconds>&);

timestamp::time_part::time_part(const time_part& other)
    : _impl(std::make_unique<impl>(*other._impl))
{
}

timestamp::time_part::~time_part() = default;

timestamp::time_part& timestamp::time_part::operator=(const time_part& other)
{
    if (this != &other) {
        _impl = std::make_unique<impl>(*other._impl);
    }
    return *this;
}

int timestamp::time_part::hour() const noexcept
{
    return static_cast<int>(_impl->hms.hours().count());
}

int timestamp::time_part::minute() const noexcept
{
    return static_cast<int>(_impl->hms.minutes().count());
}

int timestamp::time_part::second() const noexcept
{
    return static_cast<int>(_impl->hms.seconds().count());
}

int timestamp::time_part::total_seconds() const noexcept
{
    return static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(_impl->hms.to_duration()).count());
}

timestamp::time_part::operator std::string() const
{
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << hour() << ":"
        << std::setw(2) << minute() << ":"
        << std::setw(2) << second();
    return oss.str();
}

std::chrono::hh_mm_ss<std::chrono::nanoseconds> timestamp::time_part::to_hh_mm_ss() const noexcept
{
    return _impl->hms;
}

std::strong_ordering timestamp::time_part::operator<=>(const time_part& other) const noexcept
{
    return _impl->hms.to_duration() <=> other._impl->hms.to_duration();
}

bool timestamp::time_part::operator==(const time_part& other) const noexcept
{
    return _impl->hms.to_duration() == other._impl->hms.to_duration();
}

// Timestamp implementation
class timestamp::storage {
public:
    timestamp::date_part date_value;  // Mandatory
    timestamp::time_part time_value;  // Always present (default 00:00:00)
    dross::timezone tz;  // Always present (default UTC)

    storage()
        : date_value()
        , time_value()  // Default 00:00:00
        , tz(dross::timezone::utc())  // Default UTC
    {
    }

    storage(const storage& other)
        : date_value(other.date_value)
        , time_value(other.time_value)
        , tz(other.tz)
    {
    }

    std::chrono::system_clock::time_point to_time_point() const {
        auto ymd = static_cast<std::chrono::year_month_day>(date_value);
        auto days_since_epoch = std::chrono::sys_days{ymd}.time_since_epoch();
        auto time_of_day = time_value.to_hh_mm_ss().to_duration();
        auto total_duration = days_since_epoch + time_of_day;
        return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            std::chrono::sys_time<std::chrono::nanoseconds>{total_duration});
    }
};

timestamp timestamp::now()
{
    return timestamp(std::chrono::system_clock::now());
}


timestamp::timestamp()
    : _store(std::make_unique<storage>())
{
    // Default to epoch date (1970-01-01) with no explicit time
}

timestamp::timestamp(const timestamp& other)
    : _store(std::make_unique<storage>(*other._store))
{
}

timestamp::timestamp(const std::chrono::system_clock::time_point& tp)
    : _store(std::make_unique<storage>())
{
    // Convert time_point to date and time parts
    auto days_since_epoch = std::chrono::floor<std::chrono::days>(tp);
    auto ymd = std::chrono::year_month_day{std::chrono::sys_days{days_since_epoch}};
    auto time_of_day = tp - days_since_epoch;

    _store->date_value = timestamp::date_part{ymd};

    // Always store time part (even if it's midnight)
    auto time_of_day_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(time_of_day);
    auto hms = std::chrono::hh_mm_ss{time_of_day_ns};
    _store->time_value = timestamp::time_part{hms};

    // time_point has no timezone info
}

timestamp::timestamp(const std::string& iso8601_str)
    : _store(std::make_unique<storage>())
{
    // Parse ISO 8601 format
    // Support formats:
    // - 2024-01-21T15:30:00+09:00 (offset timestamp)
    // - 2024-01-21T15:30:00Z (UTC timestamp)
    // - 2024-01-21T15:30:00 (local timestamp)
    // - 2024-01-21 (local date)

    std::regex timestamp_regex(
        R"(^(?:(\d{4})-(\d{2})-(\d{2})(?:T(\d{2}):(\d{2}):(\d{2})(?:\.(\d+))?(?:(Z)|([+-])(\d{2}):(\d{2}))?)?|\d{2}:\d{2}:\d{2}(?:\.\d+)?)$)"
    );

    std::smatch match;
    if (std::regex_match(iso8601_str, match, timestamp_regex)) {
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
                    // No timezone specified, keep default UTC
                }

                // Store both date and time
                _store->date_value = timestamp::date_part{year, month, day};
                _store->time_value = timestamp::time_part{hour, minute, second};
            } else {
                // Date without time component (time defaults to 00:00:00)
                _store->date_value = timestamp::date_part{year, month, day};
                // time remains default 00:00:00
                // No timezone specified
            }
        }
        // Reject time-only format
    }
    // If parsing fails, leave as epoch time
}

timestamp::timestamp(const char* iso8601_str)
    : timestamp(std::string(iso8601_str))
{
}

timestamp::timestamp(int year, int month, int day,
                  int hour, int minute, int second)
    : _store(std::make_unique<storage>())
{
    _store->date_value = timestamp::date_part{year, month, day};
    _store->time_value = timestamp::time_part{hour, minute, second};
    // No timezone
}

timestamp::timestamp(int year, int month, int day,
                  int hour, int minute, int second,
                  const dross::timezone& tz)
    : _store(std::make_unique<storage>())
{
    _store->tz = tz;
    _store->date_value = timestamp::date_part{year, month, day};
    _store->time_value = timestamp::time_part{hour, minute, second};
}

timestamp::~timestamp() = default;

timestamp& timestamp::operator=(const timestamp& other)
{
    if (this != &other) {
        _store = std::make_unique<storage>(*other._store);
    }
    return *this;
}

timestamp::operator std::chrono::system_clock::time_point() const
{
    auto tp = _store->to_time_point();
    // Convert to UTC time point using timezone offset
    int offset = _store->tz.offset().count();
    if (offset != 0) {
        return tp - std::chrono::minutes(offset);
    }
    return tp;
}

timestamp::operator std::string() const
{
    return format(format_type::iso8601);
}

std::strong_ordering timestamp::operator<=>(const timestamp& other) const
{
    // Compare UTC time points
    auto this_utc = static_cast<std::chrono::system_clock::time_point>(*this);
    auto other_utc = static_cast<std::chrono::system_clock::time_point>(other);
    return this_utc <=> other_utc;
}

bool timestamp::operator==(const timestamp& other) const
{
    // Compare UTC time points
    auto this_utc = static_cast<std::chrono::system_clock::time_point>(*this);
    auto other_utc = static_cast<std::chrono::system_clock::time_point>(other);
    return this_utc == other_utc;
}

std::chrono::system_clock::duration timestamp::operator-(const timestamp& other) const
{
    // Calculate difference using UTC time points
    auto this_utc = static_cast<std::chrono::system_clock::time_point>(*this);
    auto other_utc = static_cast<std::chrono::system_clock::time_point>(other);
    return this_utc - other_utc;
}

timestamp timestamp::operator+(const std::chrono::minutes& duration) const
{
    timestamp result(*this);
    auto tp = result._store->to_time_point();
    tp += duration;
    // Re-extract date and time parts
    auto days_since_epoch = std::chrono::floor<std::chrono::days>(tp);
    auto ymd = std::chrono::year_month_day{std::chrono::sys_days{days_since_epoch}};
    auto time_of_day = tp - days_since_epoch;
    auto hms = std::chrono::hh_mm_ss{std::chrono::duration_cast<std::chrono::nanoseconds>(time_of_day)};

    result._store->date_value = timestamp::date_part{ymd};
    result._store->time_value = timestamp::time_part{hms};
    return result;
}

timestamp timestamp::operator-(const std::chrono::minutes& duration) const
{
    return *this + (-duration);
}

timestamp timestamp::operator+(const std::chrono::hours& duration) const
{
    return *this + std::chrono::duration_cast<std::chrono::minutes>(duration);
}

timestamp timestamp::operator-(const std::chrono::hours& duration) const
{
    return *this + (-duration);
}

timestamp timestamp::operator+(const std::chrono::seconds& duration) const
{
    timestamp result(*this);
    auto tp = result._store->to_time_point();
    tp += duration;
    // Re-extract date and time parts
    auto days_since_epoch = std::chrono::floor<std::chrono::days>(tp);
    auto ymd = std::chrono::year_month_day{std::chrono::sys_days{days_since_epoch}};
    auto time_of_day = tp - days_since_epoch;
    auto hms = std::chrono::hh_mm_ss{std::chrono::duration_cast<std::chrono::nanoseconds>(time_of_day)};

    result._store->date_value = timestamp::date_part{ymd};
    result._store->time_value = timestamp::time_part{hms};
    return result;
}

timestamp timestamp::operator-(const std::chrono::seconds& duration) const
{
    return *this + (-duration);
}

std::string timestamp::format(format_type fmt) const
{
    switch (fmt) {
        case format_type::iso8601:
        case format_type::rfc3339:
            return format_iso8601();
    }
    return format_iso8601();
}

std::string timestamp::format(const std::string& custom_format) const
{
    // For custom formatting, we still need to use the legacy API temporarily
    // until std::format with chrono support is more widely available
    auto tp = _store->to_time_point();
    auto time_c = std::chrono::system_clock::to_time_t(tp);
    auto tm_ptr = std::gmtime(&time_c);

    if (!tm_ptr) {
        return "";
    }

    std::ostringstream oss;
    oss << std::put_time(tm_ptr, custom_format.c_str());
    return oss.str();
}

const timestamp::date_part& timestamp::date() const noexcept
{
    return _store->date_value;
}

const timestamp::time_part& timestamp::time() const noexcept
{
    return _store->time_value;
}

const dross::timezone& timestamp::timezone() const noexcept
{
    return _store->tz;
}

std::string timestamp::format_iso8601() const
{
    std::ostringstream oss;

    // Check if time is midnight (00:00:00) to decide format
    bool is_midnight = (_store->time_value.hour() == 0 &&
                       _store->time_value.minute() == 0 &&
                       _store->time_value.second() == 0);

    if (!is_midnight || _store->tz.offset().count() != 0) {
        // Full timestamp (if time is not midnight or timezone is specified)
        oss << std::setfill('0')
            << std::setw(4) << _store->date_value.year() << "-"
            << std::setw(2) << _store->date_value.month() << "-"
            << std::setw(2) << _store->date_value.day() << "T"
            << std::setw(2) << _store->time_value.hour() << ":"
            << std::setw(2) << _store->time_value.minute() << ":"
            << std::setw(2) << _store->time_value.second();

        // Add timezone information (always present now)
        oss << _store->tz.format();
    } else {
        // Date only (when time is midnight and no timezone)
        oss << static_cast<std::string>(_store->date_value);
    }

    return oss.str();
}


std::ostream& operator<<(std::ostream& os, const timestamp& ts)
{
    return os << static_cast<std::string>(ts);
}

}
