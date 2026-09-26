#include "dross/thread/operation.h"
#include "dross/type/error.h"

#include <gtest/gtest.h>

#include <string>
#include <system_error>

TEST(operation_errc_test, error_carries_the_value_and_the_category)
{
    const dross::error e(dross::operation_errc::type_mismatch);

    EXPECT_TRUE(static_cast<bool>(e));
    EXPECT_EQ(e.code(), 3);
    EXPECT_EQ(&e.category(), &dross::operation_category());
    EXPECT_EQ(e.domain(), "dross.operation");
    EXPECT_TRUE(e == dross::operation_errc::type_mismatch);
    EXPECT_FALSE(e == dross::operation_errc::cancelled);
}

TEST(operation_errc_test, every_value_has_its_own_message)
{
    EXPECT_EQ(dross::error(dross::operation_errc::cancelled).message(), "operation was cancelled");
    EXPECT_EQ(dross::error(dross::operation_errc::not_finished).message(), "operation has not finished");
    EXPECT_EQ(dross::error(dross::operation_errc::type_mismatch).message(), "operation result is not of the requested type");
    EXPECT_EQ(dross::operation_category().message(0), "unknown operation error");
}

TEST(operation_errc_test, category_is_one_object)
{
    EXPECT_EQ(&dross::operation_category(), &dross::operation_category());
    EXPECT_EQ(&dross::make_error_code(dross::operation_errc::cancelled).category(), &dross::operation_category());
}
