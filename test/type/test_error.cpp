#include <gtest/gtest.h>

#include "dross/type/error.h"

#include <compare>
#include <ios>
#include <sstream>
#include <string>
#include <system_error>

TEST(error_test, default_constructed_error_is_falsy)
{
    const dross::error e;

    EXPECT_FALSE(static_cast<bool>(e));
}

TEST(error_test, constructed_from_an_error_code_enum_is_truthy)
{
    // io_errc is a genuine std::is_error_code_enum type (unlike std::errc,
    // which is an error_condition enum), so it exercises the templated
    // constructor the way it is actually meant to be used.
    const dross::error e(std::io_errc::stream);

    EXPECT_TRUE(static_cast<bool>(e));
}

TEST(error_test, code_and_message_reflect_the_underlying_error_code)
{
    const auto ec = std::make_error_code(std::io_errc::stream);
    const dross::error e(std::io_errc::stream);

    EXPECT_EQ(e.code(), ec.value());
    EXPECT_EQ(e.message(), ec.message());
    EXPECT_EQ(e.domain(), ec.category().name());
}

TEST(error_test, category_and_condition_match_the_error_code_enum)
{
    const auto ec = std::make_error_code(std::io_errc::stream);
    const dross::error e(std::io_errc::stream);

    EXPECT_EQ(e.category(), std::iostream_category());
    EXPECT_EQ(e.condition(), ec.default_error_condition());
}

TEST(error_test, equality_operator_compares_against_the_error_code_enum)
{
    const dross::error e(std::io_errc::stream);

    EXPECT_TRUE(e == std::io_errc::stream);
    EXPECT_FALSE(e != std::io_errc::stream);
}

TEST(error_test, equality_operator_compares_against_the_error_category)
{
    const dross::error e(std::io_errc::stream);

    EXPECT_TRUE(e == std::iostream_category());
    EXPECT_FALSE(e == std::generic_category());
    EXPECT_TRUE(e != std::generic_category());
}

TEST(error_test, copy_constructor_preserves_the_underlying_code)
{
    const dross::error original(1, std::generic_category());
    const dross::error copy(original);

    EXPECT_EQ(copy.code(), original.code());
    EXPECT_EQ(copy.domain(), original.domain());
}

TEST(error_test, three_way_comparison_orders_by_the_underlying_error_code)
{
    // dross::error has no operator==(const error&) of its own (only the
    // category/enum overloads), so equality here is checked through <=>.
    const dross::error smaller(0, std::generic_category());
    const dross::error larger(1, std::generic_category());

    EXPECT_TRUE(smaller < larger);
    EXPECT_TRUE(larger > smaller);
    EXPECT_TRUE((smaller <=> smaller) == std::strong_ordering::equal);
}

TEST(error_test, stream_insertion_writes_domain_and_code)
{
    const dross::error e(1, std::generic_category());

    std::ostringstream out;
    out << e;

    const std::string expected = e.domain() + ":" + std::to_string(e.code());
    EXPECT_EQ(out.str(), expected);
}

TEST(error_test, value_and_category_constructor)
{
    const dross::error e(5, std::generic_category());

    EXPECT_EQ(e.code(), 5);
    EXPECT_EQ(e.category(), std::generic_category());
}
