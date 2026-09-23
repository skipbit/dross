#include "dross/type/number.h"
#include "dross/type/timestamp.h"
#include "dross/type/timezone.h"

#include <gtest/gtest.h>

#include <locale>
#include <sstream>
#include <string>

namespace {

// Groups digits in threes and writes a comma for the decimal point, the way
// many locales do. Built here rather than looked up by name, so the tests do
// not depend on which locales the machine has installed.
class grouping_numpunct final : public std::numpunct<char> {
protected:
    char do_decimal_point() const override
    {
        return ',';
    }

    char do_thousands_sep() const override
    {
        return '.';
    }

    std::string do_grouping() const override
    {
        return "\3";
    }
};

// Makes the grouping locale global for the test, and puts the old one back.
class scoped_grouping_locale final {
public:
    scoped_grouping_locale()
        : _previous{ std::locale::global(std::locale{ std::locale::classic(), new grouping_numpunct }) }
    {
    }

    ~scoped_grouping_locale()
    {
        std::locale::global(_previous);
    }

    scoped_grouping_locale(const scoped_grouping_locale&) = delete;
    scoped_grouping_locale& operator=(const scoped_grouping_locale&) = delete;

private:
    std::locale _previous;
};

}  // namespace

TEST(locale_independence_test, the_grouping_locale_groups_a_stream_it_reaches)
{
    // Control: without this, the tests below would pass for a locale that
    // never took effect.
    const scoped_grouping_locale grouping;
    std::ostringstream oss;
    oss << 1234.5;

    EXPECT_EQ(oss.str(), "1.234,5");
}

TEST(locale_independence_test, number_is_written_without_the_global_locale)
{
    const scoped_grouping_locale grouping;

    EXPECT_EQ(std::string(dross::number{ "1.5e3" }), "1500");
    EXPECT_EQ(std::string(dross::number{ "1.25e1" }), "12.5");
}

TEST(locale_independence_test, timestamp_is_written_without_the_global_locale)
{
    const scoped_grouping_locale grouping;
    const dross::timestamp afternoon{ 2024, 1, 21, 15, 30, 0 };
    const dross::timestamp midnight{ 2024, 1, 21, 0, 0, 0 };
    const dross::timestamp tokyo{ 2024, 1, 21, 15, 30, 0, dross::timezone::offset(9) };

    EXPECT_EQ(std::string(afternoon), "2024-01-21T15:30:00Z");
    EXPECT_EQ(std::string(midnight), "2024-01-21");
    EXPECT_EQ(std::string(afternoon.date()), "2024-01-21");
    EXPECT_EQ(std::string(afternoon.time()), "15:30:00");
    EXPECT_EQ(std::string(tokyo), "2024-01-21T15:30:00+09:00");
}
