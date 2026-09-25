#include "dross/type/error.h"

#include <gtest/gtest.h>

#include <compare>
#include <ios>
#include <sstream>
#include <string>
#include <system_error>
#include <type_traits>

// An error code enum declared the way a library outside std declares one: its
// own category and its own make_error_code, found only by argument-dependent
// lookup.
namespace library_errors {

enum class failure {
    first = 1,
    second = 2
};

class failure_category_type final : public std::error_category {
public:
    const char* name() const noexcept override
    {
        return "library_errors";
    }

    std::string message(int value) const override
    {
        return (value == static_cast<int>(failure::first)) ? "first failure" : "second failure";
    }
};

const std::error_category& failure_category()
{
    static const failure_category_type the_category;
    return the_category;
}

std::error_code make_error_code(failure f)
{
    return { static_cast<int>(f), failure_category() };
}

}  // namespace library_errors

template <>
struct std::is_error_code_enum<library_errors::failure> : std::true_type { };

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
    const dross::error smaller(0, std::generic_category());
    const dross::error larger(1, std::generic_category());

    EXPECT_TRUE(smaller < larger);
    EXPECT_TRUE(larger > smaller);
    EXPECT_TRUE((smaller <=> smaller) == std::strong_ordering::equal);
}

TEST(error_test, equality_operator_compares_the_underlying_error_code)
{
    const dross::error one(1, std::generic_category());
    const dross::error same(1, std::generic_category());
    const dross::error other_value(2, std::generic_category());
    const dross::error other_category(1, std::system_category());

    EXPECT_TRUE(one == same);
    EXPECT_FALSE(one != same);
    EXPECT_FALSE(one == other_value);
    EXPECT_TRUE(one != other_value);
    EXPECT_FALSE(one == other_category);
    EXPECT_TRUE(one != other_category);
}

TEST(error_test, equality_agrees_with_three_way_comparison)
{
    const dross::error errors[] = {
        dross::error(1, std::generic_category()),
        dross::error(2, std::generic_category()),
        dross::error(1, std::system_category()),
        dross::error(std::io_errc::stream),
    };

    for (const auto& a : errors) {
        for (const auto& b : errors) {
            EXPECT_EQ(a == b, (a <=> b) == std::strong_ordering::equal);
        }
    }
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

TEST(error_test, constructed_from_an_error_code_enum_declared_outside_std)
{
    const dross::error e(library_errors::failure::second);

    EXPECT_TRUE(static_cast<bool>(e));
    EXPECT_EQ(e.code(), 2);
    EXPECT_EQ(&e.category(), &library_errors::failure_category());
    EXPECT_EQ(e.message(), "second failure");
    EXPECT_EQ(e.domain(), "library_errors");
}
