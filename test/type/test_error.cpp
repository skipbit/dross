#include "dross/type/error.h"

#include <gtest/gtest.h>

#include <cerrno>
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

// A category that maps a code to a condition through equivalent() and leaves
// default_error_condition() alone.
namespace http_errors {

class http_category_type final : public std::error_category {
public:
    const char* name() const noexcept override
    {
        return "http";
    }

    std::string message(int value) const override
    {
        return "http " + std::to_string(value);
    }

    bool equivalent(int value, const std::error_condition& condition) const noexcept override
    {
        return (value == 400) && (condition == std::errc::invalid_argument);
    }
};

const std::error_category& http_category()
{
    static const http_category_type the_category;
    return the_category;
}

}  // namespace http_errors

// An error code enum whose codes are in generic_category().
namespace posix_errors {

enum class posix_code {
    no_such_file = ENOENT
};

std::error_code make_error_code(posix_code c)
{
    return { static_cast<int>(c), std::generic_category() };
}

}  // namespace posix_errors

template <>
struct std::is_error_code_enum<posix_errors::posix_code> : std::true_type { };

// An error condition enum whose category reports which codes it covers
// through equivalent(), the way a library outside std groups codes.
namespace library_conditions {

enum class condition {
    io_failure = 1
};

class condition_category_type final : public std::error_category {
public:
    const char* name() const noexcept override
    {
        return "library_conditions";
    }

    std::string message(int) const override
    {
        return "io failure";
    }

    bool equivalent(const std::error_code& code, int value) const noexcept override
    {
        return (value == static_cast<int>(condition::io_failure)) && (code == std::errc::io_error);
    }
};

const std::error_category& condition_category()
{
    static const condition_category_type the_category;
    return the_category;
}

std::error_condition make_error_condition(condition c)
{
    return { static_cast<int>(c), condition_category() };
}

}  // namespace library_conditions

template <>
struct std::is_error_condition_enum<library_conditions::condition> : std::true_type { };

// An enum that is both an error code enum and an error condition enum, with
// a category that reports every pair equivalent, so a match through
// equivalent() differs from a match on the category and the value.
namespace dual_errors {

enum class dual {
    one = 1
};

class dual_category_type final : public std::error_category {
public:
    const char* name() const noexcept override
    {
        return "dual_errors";
    }

    std::string message(int) const override
    {
        return "dual";
    }

    bool equivalent(int, const std::error_condition&) const noexcept override
    {
        return true;
    }

    bool equivalent(const std::error_code&, int) const noexcept override
    {
        return true;
    }
};

const std::error_category& dual_category()
{
    static const dual_category_type the_category;
    return the_category;
}

std::error_code make_error_code(dual d)
{
    return { static_cast<int>(d), dual_category() };
}

std::error_condition make_error_condition(dual d)
{
    return { static_cast<int>(d), dual_category() };
}

}  // namespace dual_errors

template <>
struct std::is_error_code_enum<dual_errors::dual> : std::true_type { };

template <>
struct std::is_error_condition_enum<dual_errors::dual> : std::true_type { };

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

TEST(error_test, an_error_without_a_detail_has_an_empty_one)
{
    const dross::error e = std::errc::invalid_argument;

    EXPECT_EQ(e.detail(), "");
    EXPECT_EQ(e.message(), std::make_error_code(std::errc::invalid_argument).message());
}

TEST(error_test, message_joins_the_code_message_and_the_detail)
{
    const dross::error e(std::errc::invalid_argument, "line 7, column 1: Expected '='");
    const std::string code_message = std::make_error_code(std::errc::invalid_argument).message();

    EXPECT_EQ(e.detail(), "line 7, column 1: Expected '='");
    EXPECT_EQ(e.message(), code_message + ": line 7, column 1: Expected '='");
    EXPECT_EQ(e, std::errc::invalid_argument);
}

TEST(error_test, copy_and_assignment_keep_the_detail)
{
    const dross::error original(std::errc::invalid_argument, "detail");
    const dross::error copy(original);
    dross::error assigned;
    assigned = original;

    EXPECT_EQ(copy.detail(), "detail");
    EXPECT_EQ(assigned.detail(), "detail");
}

TEST(error_test, comparisons_ignore_the_detail)
{
    const dross::error plain = std::errc::invalid_argument;
    const dross::error one(std::errc::invalid_argument, "one");
    const dross::error other(std::errc::invalid_argument, "other");

    EXPECT_TRUE(one == other);
    EXPECT_TRUE(one == plain);
    EXPECT_TRUE((one <=> other) == std::strong_ordering::equal);
    EXPECT_TRUE((one <=> plain) == std::strong_ordering::equal);
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

TEST(error_test, constructed_from_std_errc)
{
    const dross::error e = std::errc::invalid_argument;

    EXPECT_TRUE(static_cast<bool>(e));
    EXPECT_EQ(e.code(), EINVAL);
    EXPECT_EQ(e.category(), std::generic_category());
    EXPECT_EQ(e, dross::error(EINVAL, std::generic_category()));
}

TEST(error_test, equality_operator_compares_against_std_errc)
{
    const dross::error e(std::errc::invalid_argument);

    EXPECT_TRUE(e == std::errc::invalid_argument);
    EXPECT_FALSE(e != std::errc::invalid_argument);
    EXPECT_TRUE(e != std::errc::io_error);
    EXPECT_FALSE(e == std::errc::io_error);
}

TEST(error_test, a_system_code_equals_the_std_errc_it_maps_to)
{
    const dross::error e(ENOENT, std::system_category());

    EXPECT_TRUE(e == std::errc::no_such_file_or_directory);
    EXPECT_FALSE(e == std::errc::permission_denied);
}

TEST(error_test, a_code_equals_the_std_errc_its_category_maps_it_to)
{
    const dross::error e(400, http_errors::http_category());

    EXPECT_TRUE(e == std::errc::invalid_argument);
    EXPECT_FALSE(e != std::errc::invalid_argument);
    EXPECT_FALSE(e == std::errc::io_error);
    EXPECT_TRUE(e != std::errc::io_error);
    EXPECT_TRUE(std::errc::invalid_argument == e);
    EXPECT_FALSE(std::errc::invalid_argument != e);
}

TEST(error_test, an_error_code_enum_equals_only_the_same_category_and_value)
{
    const dross::error e(ENOENT, std::system_category());

    EXPECT_FALSE(e == posix_errors::posix_code::no_such_file);
    EXPECT_TRUE(e != posix_errors::posix_code::no_such_file);
    EXPECT_FALSE(posix_errors::posix_code::no_such_file == e);
    EXPECT_TRUE(posix_errors::posix_code::no_such_file != e);
    EXPECT_TRUE(dross::error(posix_errors::posix_code::no_such_file) == posix_errors::posix_code::no_such_file);
}

TEST(error_test, a_code_equals_an_error_condition_enum_whose_category_covers_it)
{
    const dross::error io(EIO, std::system_category());
    const dross::error missing(ENOENT, std::system_category());

    EXPECT_TRUE(io == library_conditions::condition::io_failure);
    EXPECT_FALSE(io != library_conditions::condition::io_failure);
    EXPECT_TRUE(library_conditions::condition::io_failure == io);
    EXPECT_FALSE(missing == library_conditions::condition::io_failure);
    EXPECT_TRUE(missing != library_conditions::condition::io_failure);
}

TEST(error_test, a_code_equals_an_error_condition_its_category_maps_it_to)
{
    const dross::error e(400, http_errors::http_category());
    const std::error_condition invalid_argument = std::make_error_condition(std::errc::invalid_argument);
    const std::error_condition io_error = std::make_error_condition(std::errc::io_error);

    EXPECT_TRUE(e == invalid_argument);
    EXPECT_FALSE(e != invalid_argument);
    EXPECT_TRUE(invalid_argument == e);
    EXPECT_FALSE(e == io_error);
    EXPECT_TRUE(e != io_error);
}

TEST(error_test, an_enum_that_is_also_a_condition_enum_compares_as_an_error_code_enum)
{
    const dross::error other(1, std::generic_category());

    EXPECT_FALSE(other == dual_errors::dual::one);
    // std::error_code == dual_errors::dual is ambiguous, so the comparison
    // that matches is the one with the enum's own std::error_code.
    EXPECT_EQ(other == dual_errors::dual::one,
              std::error_code(other.code(), other.category()) == std::error_code(dual_errors::dual::one));
    EXPECT_TRUE(dross::error(dual_errors::dual::one) == dual_errors::dual::one);
}

TEST(error_test, compares_with_a_type_derived_from_std_error_condition)
{
    struct derived_condition : std::error_condition {
        derived_condition()
            : std::error_condition(std::make_error_condition(std::errc::io_error))
        {
        }
    };
    const dross::error io(EIO, std::system_category());
    const derived_condition condition;

    EXPECT_TRUE(io == condition);
    EXPECT_TRUE(condition == io);
    EXPECT_FALSE(dross::error(ENOENT, std::system_category()) == condition);
}

TEST(error_test, comparison_with_an_error_condition_agrees_with_std_error_code)
{
    const std::error_code codes[] = {
        { EIO, std::system_category() },
        { ENOENT, std::system_category() },
        { EIO, std::generic_category() },
        { 400, http_errors::http_category() },
    };
    const std::error_condition conditions[] = {
        library_conditions::make_error_condition(library_conditions::condition::io_failure),
        std::make_error_condition(std::errc::io_error),
        std::make_error_condition(std::errc::invalid_argument),
        std::make_error_condition(std::errc::no_such_file_or_directory),
    };

    for (const auto& code : codes) {
        const dross::error e(code.value(), code.category());
        for (const auto& condition : conditions) {
            EXPECT_EQ(e == condition, code == condition) << code.category().name() << ":" << code.value();
        }
    }
}
