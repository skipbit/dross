#include <dross/type/timestamp.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <locale>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace dross {

namespace {

// The years 0 to 9999 that a timestamp stays within, as the first second and
// the one after the last.
constexpr std::chrono::sys_seconds first_second{ std::chrono::sys_days{ std::chrono::year{ 0 } / std::chrono::January / 1 } };
constexpr std::chrono::sys_seconds end_second{ std::chrono::sys_days{ std::chrono::year{ 10000 } / std::chrono::January / 1 } };

// Compared in its own unit, since converting a duration with more seconds
// than std::chrono::seconds holds overflows.
template <typename Duration>
std::optional<std::chrono::seconds> in_seconds(Duration duration)
{
    if ((duration > std::chrono::duration_cast<Duration>(std::chrono::seconds::max()))
        || (duration < std::chrono::duration_cast<Duration>(std::chrono::seconds::min()))) {
        return std::nullopt;
    }
    return std::chrono::seconds(duration);
}

std::chrono::year_month_day year_month_day_of(int year, int month, int day)
{
    return { std::chrono::year{ year }, std::chrono::month{ static_cast<unsigned>(month) }, std::chrono::day{ static_cast<unsigned>(day) } };
}

timestamp value_or_throw(std::optional<timestamp> result)
{
    if (! result) {
        throw std::out_of_range("timestamp arithmetic leaves the years 0 to 9999");
    }
    return *result;
}

// The year in four digits. A timestamp stays within the years 0 to 9999.
std::string year_digits(int year)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::setfill('0') << std::setw(4) << year;
    return oss.str();
}

// Writes %Y and %F as the ISO 8601 form writes them, before std::put_time
// reads the rest of the format.
std::string expand_date_conversions(const std::string& format, const timestamp::date_part& date)
{
    std::string result;
    for (std::size_t i = 0; i < format.size(); ++i) {
        if ((format[i] != '%') || (i + 1 == format.size())) {
            result += format[i];
            continue;
        }
        const char conversion = format[++i];
        if (conversion == 'Y') {
            result += year_digits(date.year());
        } else if (conversion == 'F') {
            result += static_cast<std::string>(date);
        } else {
            result += '%';
            result += conversion;
        }
    }
    return result;
}

}  // namespace

timestamp::date_part::date_part()
    : _ymd{ std::chrono::year{ 1970 }, std::chrono::month{ 1 }, std::chrono::day{ 1 } }
{
}

timestamp::date_part::date_part(const std::chrono::year_month_day& ymd)
    : _ymd(ymd)
{
}

int timestamp::date_part::year() const noexcept
{
    return static_cast<int>(_ymd.year());
}

int timestamp::date_part::month() const noexcept
{
    return static_cast<unsigned>(_ymd.month());
}

int timestamp::date_part::day() const noexcept
{
    return static_cast<unsigned>(_ymd.day());
}

timestamp::date_part::operator std::string() const
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << year_digits(year()) << "-" << std::setfill('0') << std::setw(2) << month() << "-" << std::setw(2) << day();
    return oss.str();
}

timestamp::date_part::operator std::chrono::year_month_day() const noexcept
{
    return _ymd;
}

std::strong_ordering timestamp::date_part::operator<=>(const date_part& other) const noexcept
{
    auto this_days = std::chrono::sys_days{ _ymd }.time_since_epoch();
    auto other_days = std::chrono::sys_days{ other._ymd }.time_since_epoch();
    return this_days <=> other_days;
}

bool timestamp::date_part::operator==(const date_part& other) const noexcept
{
    return _ymd == other._ymd;
}

// Define static member
const timestamp::time_part timestamp::time_part::midnight;

timestamp::time_part::time_part()
    : _since_midnight(0)
{
}

timestamp::time_part::time_part(std::chrono::nanoseconds since_midnight)
    : _since_midnight(since_midnight)
{
}

int timestamp::time_part::hour() const noexcept
{
    return static_cast<int>(to_hh_mm_ss().hours().count());
}

int timestamp::time_part::minute() const noexcept
{
    return static_cast<int>(to_hh_mm_ss().minutes().count());
}

int timestamp::time_part::second() const noexcept
{
    return static_cast<int>(to_hh_mm_ss().seconds().count());
}

int timestamp::time_part::total_seconds() const noexcept
{
    return static_cast<int>(std::chrono::floor<std::chrono::seconds>(_since_midnight).count());
}

timestamp::time_part::operator std::string() const
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::setfill('0') << std::setw(2) << hour() << ":" << std::setw(2) << minute() << ":" << std::setw(2) << second();
    return oss.str();
}

std::chrono::hh_mm_ss<std::chrono::nanoseconds> timestamp::time_part::to_hh_mm_ss() const noexcept
{
    return std::chrono::hh_mm_ss{ _since_midnight };
}

std::strong_ordering timestamp::time_part::operator<=>(const time_part& other) const noexcept
{
    return _since_midnight <=> other._since_midnight;
}

bool timestamp::time_part::operator==(const time_part& other) const noexcept
{
    return _since_midnight == other._since_midnight;
}

timestamp timestamp::now()
{
    return timestamp(std::chrono::system_clock::now());
}

timestamp::timestamp()
    : _date()
    , _timezone(dross::timezone::utc())
    , _time()
{
}

timestamp::timestamp(const std::chrono::system_clock::time_point& tp)
    : timestamp()
{
    if (auto converted = from_time_point(tp)) {
        *this = *converted;
    } else if (std::chrono::floor<std::chrono::seconds>(tp) < first_second) {
        // The nearer end keeps a time point used as a limit, such as
        // time_point::max() for a deadline that never comes, on its side of others.
        set_local(first_second, std::chrono::nanoseconds{ 0 });
    } else {
        set_local(end_second - std::chrono::seconds{ 1 }, std::chrono::nanoseconds{ 999'999'999 });
    }
}

timestamp::timestamp(const std::string& iso8601_str)
    : timestamp(from_string(iso8601_str).value_or(timestamp{}))
{
}

timestamp::timestamp(const char* iso8601_str)
    : timestamp(std::string(iso8601_str))
{
}

timestamp::timestamp(int year, int month, int day, int hour, int minute, int second)
    : timestamp(from_components(year, month, day, hour, minute, second).value_or(timestamp{}))
{
}

timestamp::timestamp(int year, int month, int day, int hour, int minute, int second, const dross::timezone& tz)
    : timestamp(from_components(year, month, day, hour, minute, second, tz).value_or(timestamp{}))
{
}

std::optional<timestamp> timestamp::from_string(const std::string& iso8601_str)
{
    // Support formats:
    // - 2024-01-21T15:30:00+09:00 (offset timestamp)
    // - 2024-01-21T15:30:00Z (UTC timestamp)
    // - 2024-01-21T15:30:00 (no offset, read as UTC)
    // - 2024-01-21 (date only, read as midnight UTC)
    static const std::regex timestamp_regex(R"(^(\d{4})-(\d{2})-(\d{2})(?:T(\d{2}):(\d{2}):(\d{2})(?:\.(\d+))?(Z|[+-]\d{2}:\d{2})?)?$)");

    std::smatch match;
    if (! std::regex_match(iso8601_str, match, timestamp_regex)) {
        return std::nullopt;
    }

    int year = std::stoi(match[1].str());
    int month = std::stoi(match[2].str());
    int day = std::stoi(match[3].str());
    int hour = 0, minute = 0, second = 0;
    if (match[4].matched) {
        hour = std::stoi(match[4].str());
        minute = std::stoi(match[5].str());
        second = std::stoi(match[6].str());
    }

    auto tz = dross::timezone::utc();
    if (match[8].matched) {
        auto parsed = dross::timezone::from_string(match[8].str());
        if (! parsed) {
            return std::nullopt;
        }
        tz = *parsed;
    }

    auto result = from_components(year, month, day, hour, minute, second, tz);
    if (result && match[7].matched) {
        // Digits below a nanosecond are dropped.
        std::string digits = match[7].str().substr(0, 9);
        digits.resize(9, '0');
        result->_time._since_midnight += std::chrono::nanoseconds(std::stoll(digits));
    }
    return result;
}

std::optional<timestamp> timestamp::from_time_point(const std::chrono::system_clock::time_point& tp)
{
    // Compared as seconds: std::chrono::year holds 16 bits, so a year far
    // outside the range can wrap around to one inside it.
    const auto seconds = std::chrono::floor<std::chrono::seconds>(tp);
    if ((seconds < first_second) || (seconds >= end_second)) {
        return std::nullopt;
    }

    timestamp result;
    result.set_local(seconds, std::chrono::duration_cast<std::chrono::nanoseconds>(tp - seconds));
    return result;
}

std::optional<timestamp> timestamp::from_components(int year, int month, int day, int hour, int minute, int second, const dross::timezone& tz)
{
    if ((year < 0) || (year > 9999) || (month < 1) || (month > 12) || (day < 1) || (day > 31)) {
        return std::nullopt;
    }
    if ((hour < 0) || (hour > 23) || (minute < 0) || (minute > 59) || (second < 0) || (second > 59)) {
        return std::nullopt;
    }

    const auto ymd = year_month_day_of(year, month, day);
    if (! ymd.ok()) {
        return std::nullopt;
    }
    timestamp result;
    result._date = date_part{ ymd };
    result._time = time_part{ std::chrono::hours{ hour } + std::chrono::minutes{ minute } + std::chrono::seconds{ second } };
    result._timezone = tz;
    return result;
}

std::chrono::sys_seconds timestamp::local_seconds() const noexcept
{
    return std::chrono::sys_days{ _date._ymd } + std::chrono::floor<std::chrono::seconds>(_time._since_midnight);
}

std::chrono::nanoseconds timestamp::subseconds() const noexcept
{
    return _time._since_midnight - std::chrono::floor<std::chrono::seconds>(_time._since_midnight);
}

std::pair<std::chrono::sys_seconds, std::chrono::nanoseconds> timestamp::instant() const noexcept
{
    return { local_seconds() - _timezone.offset(), subseconds() };
}

void timestamp::set_local(std::chrono::sys_seconds local, std::chrono::nanoseconds fraction)
{
    const auto days = std::chrono::floor<std::chrono::days>(local);
    _date = date_part{ std::chrono::year_month_day{ days } };
    _time = time_part{ local - days + fraction };
}

timestamp::operator std::chrono::system_clock::time_point() const
{
    auto [seconds, subseconds] = instant();
    return std::chrono::time_point_cast<std::chrono::system_clock::duration>(seconds)
           + std::chrono::duration_cast<std::chrono::system_clock::duration>(subseconds);
}

timestamp::operator std::string() const
{
    return format(format_type::iso8601);
}

std::strong_ordering timestamp::operator<=>(const timestamp& other) const
{
    return instant() <=> other.instant();
}

bool timestamp::operator==(const timestamp& other) const
{
    return instant() == other.instant();
}

std::chrono::system_clock::duration timestamp::operator-(const timestamp& other) const
{
    auto [seconds, subseconds] = instant();
    auto [other_seconds, other_subseconds] = other.instant();
    return std::chrono::duration_cast<std::chrono::system_clock::duration>(seconds - other_seconds)
           + std::chrono::duration_cast<std::chrono::system_clock::duration>(subseconds - other_subseconds);
}

timestamp timestamp::operator+(const std::chrono::minutes& duration) const
{
    const auto seconds = in_seconds(duration);
    return value_or_throw(seconds ? add(*seconds) : std::nullopt);
}

timestamp timestamp::operator-(const std::chrono::minutes& duration) const
{
    const auto seconds = in_seconds(duration);
    return value_or_throw(seconds ? subtract(*seconds) : std::nullopt);
}

timestamp timestamp::operator+(const std::chrono::hours& duration) const
{
    const auto seconds = in_seconds(duration);
    return value_or_throw(seconds ? add(*seconds) : std::nullopt);
}

timestamp timestamp::operator-(const std::chrono::hours& duration) const
{
    const auto seconds = in_seconds(duration);
    return value_or_throw(seconds ? subtract(*seconds) : std::nullopt);
}

timestamp timestamp::operator+(const std::chrono::seconds& duration) const
{
    return value_or_throw(add(duration));
}

timestamp timestamp::operator-(const std::chrono::seconds& duration) const
{
    return value_or_throw(subtract(duration));
}

std::optional<timestamp> timestamp::add(std::chrono::seconds duration) const
{
    // Compared as distances from here, which cannot overflow, before adding.
    const auto local = local_seconds();
    if ((duration < first_second - local) || (duration >= end_second - local)) {
        return std::nullopt;
    }

    timestamp result(*this);
    result.set_local(local + duration, subseconds());
    return result;
}

std::optional<timestamp> timestamp::subtract(std::chrono::seconds duration) const
{
    // The one duration whose negation overflows is far outside the range.
    if (duration == std::chrono::seconds::min()) {
        return std::nullopt;
    }
    return add(-duration);
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
    auto time_c = static_cast<std::time_t>(local_seconds().time_since_epoch().count());
    // gmtime_r rather than std::gmtime, whose result is shared by every thread.
    std::tm tm{};
    if (! ::gmtime_r(&time_c, &tm)) {
        return "";
    }

    const std::string expanded = expand_date_conversions(custom_format, _date);
    std::ostringstream oss;
    oss << std::put_time(&tm, expanded.c_str());
    return oss.str();
}

const timestamp::date_part& timestamp::date() const noexcept
{
    return _date;
}

const timestamp::time_part& timestamp::time() const noexcept
{
    return _time;
}

const dross::timezone& timestamp::timezone() const noexcept
{
    return _timezone;
}

std::string timestamp::format_iso8601() const
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());

    oss << static_cast<std::string>(_date) << "T" << std::setfill('0') << std::setw(2) << _time.hour() << ":" << std::setw(2) << _time.minute() << ":" << std::setw(2)
        << _time.second();

    if (auto fraction = subseconds().count(); fraction != 0) {
        std::string digits = std::to_string(fraction);
        digits.insert(0, 9 - digits.size(), '0');
        digits.erase(digits.find_last_not_of('0') + 1);
        oss << "." << digits;
    }

    oss << _timezone.format();

    return oss.str();
}

std::ostream& operator<<(std::ostream& os, const timestamp& ts)
{
    return os << static_cast<std::string>(ts);
}

}  // namespace dross
