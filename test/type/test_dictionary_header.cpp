// Only dictionary.h comes before these checks, so they fail to compile when
// it leaves a type its iterators need incomplete.
#include "dross/type/dictionary.h"

#include <algorithm>
#include <iterator>
#include <string>

static_assert(std::forward_iterator<dross::dictionary::iterator>);
static_assert(std::forward_iterator<dross::dictionary::const_iterator>);

#include <gtest/gtest.h>

TEST(dictionary_header_test, ranges_algorithms_work_with_only_this_header)
{
    // Empty, so nothing here needs value itself.
    dross::dictionary d;
    const dross::dictionary& const_d = d;

    EXPECT_FALSE(std::ranges::any_of(d, [](auto kv) {
        return kv.first == "a";
    }));
    EXPECT_FALSE(std::ranges::any_of(const_d, [](auto kv) {
        return kv.first == "a";
    }));
}
