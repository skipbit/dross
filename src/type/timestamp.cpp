#include <dross/type/timestamp.h>

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <locale>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace dross {

namespace {

// The years 0 to 9999 that parsing accepts, as the first second and the one
// after the last.
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

timestamp value_or_throw(std::optional<timestamp> result)
{
    if (! result) {
        throw std::out_of_range("timestamp arithmetic leaves the years 0 to 9999");
    }
    return std::move(*result);
}

// The year in at least four digits, with a minus sign before a negative one.
// Arithmetic keeps a timestamp within the years 0 to 9999; a
// system_clock::time_point outside them can still give a year with a sign or
// five digits.
std::string year_digits(int year)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    if (year < 0) {
        oss << "-";
    }
    oss << std::setfill('0') << std::setw(4) << std::abs(year);
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

// Date implementation
class timestamp::date_part::impl {
public:
    std::chrono::year_month_day ymd;

    impl()
        : ymd{ std::chrono::year{ 1970 }, std::chrono::month{ 1 }, std::chrono::day{ 1 } }
    {
    }

    impl(int year, int month, int day)
        : ymd{ std::chrono::year{ year },
               std::chrono::month{ static_cast<unsigned>(month) },
               std::chrono::day{ static_cast<unsigned>(day) } }
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
    oss.imbue(std::locale::classic());
    oss << year_digits(year()) << "-" << std::setfill('0') << std::setw(2) << month() << "-" << std::setw(2) << day();
    return oss.str();
}

timestamp::date_part::operator std::chrono::year_month_day() const noexcept
{
    return _impl->ymd;
}

std::strong_ordering timestamp::date_part::operator<=>(const date_part& other) const noexcept
{
    auto this_days = std::chrono::sys_days{ _impl->ymd }.time_since_epoch();
    auto other_days = std::chrono::sys_days{ other._impl->ymd }.time_since_epoch();
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
        : hms{ std::chrono::nanoseconds{ 0 } }
    {
    }

    impl(int hour, int minute, int second)
        : hms{ std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours{ hour } + std::chrono::minutes{ minute }
                                                                    + std::chrono::seconds{ second }) }
    {
    }

    template <typename Duration>
    impl(const std::chrono::hh_mm_ss<Duration>& hms_in)
        : hms{ std::chrono::duration_cast<std::chrono::nanoseconds>(hms_in.to_duration()) }
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

template <typename Duration>
timestamp::time_part::time_part(const std::chrono::hh_mm_ss<Duration>& hms)
    : _impl(std::make_unique<impl>(hms))
{
}

// Explicit instantiation for nanoseconds, the only duration in use
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
    oss.imbue(std::locale::classic());
    oss << std::setfill('0') << std::setw(2) << hour() << ":" << std::setw(2) << minute() << ":" << std::setw(2) << second();
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
    dross::timezone tz;               // Always present (default UTC)

    storage()
        : date_value()
        , time_value()                // Default 00:00:00
        , tz(dross::timezone::utc())  // Default UTC
    {
    }

    storage(int year, int month, int day, int hour, int minute, int second, const dross::timezone& zone)
        : date_value(year, month, day)
        , time_value(hour, minute, second)
        , tz(zone)
    {
    }

    storage(const storage& other)
        : date_value(other.date_value)
        , time_value(other.time_value)
        , tz(other.tz)
    {
    }

    // Wall-clock seconds since the epoch, before the offset is applied.
    std::chrono::sys_seconds local_seconds() const
    {
        auto ymd = static_cast<std::chrono::year_month_day>(date_value);
        auto time_of_day = std::chrono::floor<std::chrono::seconds>(time_value.to_hh_mm_ss().to_duration());
        return std::chrono::sys_days{ ymd } + time_of_day;
    }

    std::chrono::nanoseconds subseconds() const
    {
        auto time_of_day = time_value.to_hh_mm_ss().to_duration();
        return time_of_day - std::chrono::floor<std::chrono::seconds>(time_of_day);
    }

    // Splits wall-clock seconds and a fraction into the date and time parts.
    void set_local(std::chrono::sys_seconds local, std::chrono::nanoseconds subseconds)
    {
        auto days = std::chrono::floor<std::chrono::days>(local);
        date_value = timestamp::date_part{ std::chrono::year_month_day{ days } };
        time_value = timestamp::time_part{ std::chrono::hh_mm_ss{ std::chrono::duration_cast<std::chrono::nanoseconds>(local - days) + subseconds } };
    }

    // The instant in UTC, kept in seconds so that no year from 0 to 9999 overflows.
    std::pair<std::chrono::sys_seconds, std::chrono::nanoseconds> instant() const
    {
        return { local_seconds() - tz.offset(), subseconds() };
    }
};

timestamp timestamp::now()
{
    return timestamp(std::chrono::system_clock::now());
}

timestamp::timestamp()
    : _store(std::make_unique<storage>())
{
}

timestamp::timestamp(std::unique_ptr<storage> store)
    : _store(std::move(store))
{
}

timestamp::timestamp(const timestamp& other)
    : _store(std::make_unique<storage>(*other._store))
{
}

timestamp::timestamp(const std::chrono::system_clock::time_point& tp)
    : _store(std::make_unique<storage>())
{
    auto seconds = std::chrono::floor<std::chrono::seconds>(tp);
    _store->set_local(seconds, std::chrono::duration_cast<std::chrono::nanoseconds>(tp - seconds));
}

timestamp::timestamp(const std::string& iso8601_str)
{
    if (auto parsed = from_string(iso8601_str)) {
        _store = std::move(parsed->_store);
    } else {
        _store = std::make_unique<storage>();
    }
}

timestamp::timestamp(const char* iso8601_str)
    : timestamp(std::string(iso8601_str))
{
}

timestamp::timestamp(int year, int month, int day, int hour, int minute, int second)
{
    if (auto parsed = from_components(year, month, day, hour, minute, second)) {
        _store = std::move(parsed->_store);
    } else {
        _store = std::make_unique<storage>();
    }
}

timestamp::timestamp(int year, int month, int day, int hour, int minute, int second, const dross::timezone& tz)
{
    if (auto parsed = from_components(year, month, day, hour, minute, second, tz)) {
        _store = std::move(parsed->_store);
    } else {
        _store = std::make_unique<storage>();
    }
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
        result->_store->set_local(result->_store->local_seconds(), std::chrono::nanoseconds(std::stoll(digits)));
    }
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

    const std::chrono::year_month_day ymd{ std::chrono::year{ year },
                                           std::chrono::month{ static_cast<unsigned>(month) },
                                           std::chrono::day{ static_cast<unsigned>(day) } };
    if (! ymd.ok()) {
        return std::nullopt;
    }
    return timestamp(std::make_unique<storage>(year, month, day, hour, minute, second, tz));
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
    auto [seconds, subseconds] = _store->instant();
    return std::chrono::time_point_cast<std::chrono::system_clock::duration>(seconds)
           + std::chrono::duration_cast<std::chrono::system_clock::duration>(subseconds);
}

timestamp::operator std::string() const
{
    return format(format_type::iso8601);
}

std::strong_ordering timestamp::operator<=>(const timestamp& other) const
{
    return _store->instant() <=> other._store->instant();
}

bool timestamp::operator==(const timestamp& other) const
{
    return _store->instant() == other._store->instant();
}

std::chrono::system_clock::duration timestamp::operator-(const timestamp& other) const
{
    auto [seconds, subseconds] = _store->instant();
    auto [other_seconds, other_subseconds] = other._store->instant();
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
    const auto local = _store->local_seconds();
    if ((duration < first_second - local) || (duration >= end_second - local)) {
        return std::nullopt;
    }

    timestamp result(*this);
    result._store->set_local(local + duration, _store->subseconds());
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
    auto time_c = static_cast<std::time_t>(_store->local_seconds().time_since_epoch().count());
    // gmtime_r rather than std::gmtime, whose result is shared by every thread.
    std::tm tm{};
    if (! ::gmtime_r(&time_c, &tm)) {
        return "";
    }

    const std::string expanded = expand_date_conversions(custom_format, _store->date_value);
    std::ostringstream oss;
    oss << std::put_time(&tm, expanded.c_str());
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
    oss.imbue(std::locale::classic());

    oss << static_cast<std::string>(_store->date_value) << "T" << std::setfill('0') << std::setw(2) << _store->time_value.hour() << ":" << std::setw(2)
        << _store->time_value.minute() << ":" << std::setw(2) << _store->time_value.second();

    if (auto fraction = _store->subseconds().count(); fraction != 0) {
        std::string digits = std::to_string(fraction);
        digits.insert(0, 9 - digits.size(), '0');
        digits.erase(digits.find_last_not_of('0') + 1);
        oss << "." << digits;
    }

    oss << _store->tz.format();

    return oss.str();
}

std::ostream& operator<<(std::ostream& os, const timestamp& ts)
{
    return os << static_cast<std::string>(ts);
}

}  // namespace dross
