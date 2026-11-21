#include "dross/ipc/error.h"

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <system_error>

namespace {

TEST(ipc_error, the_category_is_named)
{
    const std::error_code code = dross::ipc_errc::message_too_large;
    EXPECT_STREQ(code.category().name(), "dross.ipc");
}

TEST(ipc_error, each_value_has_its_own_description)
{
    std::set<std::string> descriptions;
    for (const auto e : { dross::ipc_errc::message_too_large, dross::ipc_errc::queue_overflow, dross::ipc_errc::peer_not_same_user, dross::ipc_errc::message_cut_short }) {
        const std::error_code code = e;
        EXPECT_FALSE(code.message().empty());
        descriptions.insert(code.message());
    }
    EXPECT_EQ(descriptions.size(), 4u);
}

}  // namespace
