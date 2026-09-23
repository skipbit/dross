#include "thread/timer_schedule.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

using std::chrono::milliseconds;

// Stands in for timer::storage, which is private to timer.
class fake_timer final {
public:
    fake_timer(std::uint64_t id, milliseconds interval, bool repeats)
        : _id{ id }
        , _interval{ interval }
        , _repeats{ repeats }
    {
    }

    std::uint64_t id() const noexcept
    {
        return _id;
    }

    milliseconds interval() const
    {
        return _interval;
    }

    bool repeats() const
    {
        return _repeats;
    }

private:
    std::uint64_t _id;
    milliseconds _interval;
    bool _repeats;
};

using handle = std::shared_ptr<fake_timer>;
using schedule = dross::timer_schedule<handle>;

// Any fixed point will do; none of these tests reads a clock.
const schedule::time_point t0 = schedule::time_point{} + std::chrono::hours{ 1 };

}  // namespace

TEST(timer_schedule_test, a_repeating_timer_is_next_due_interval_after_it_was_claimed)
{
    schedule timers;
    const auto every_50ms = std::make_shared<fake_timer>(1, milliseconds{ 50 }, true);
    timers.install(every_50ms, t0);

    // Three different times a reschedule could be measured from: the
    // deadline that was due, the boundary of the pass, and the claim.
    const auto boundary = t0 + milliseconds{ 10 };
    const auto claimed_at = t0 + milliseconds{ 200 };

    std::vector<std::uint64_t> handled;
    EXPECT_EQ(timers.take_due(boundary, claimed_at, handled), every_50ms);

    EXPECT_EQ(timers.earliest(), claimed_at + milliseconds{ 50 });
    EXPECT_EQ(timers.size(), 1U);
}

TEST(timer_schedule_test, a_one_shot_is_taken_out_when_claimed)
{
    schedule timers;
    const auto one_shot = std::make_shared<fake_timer>(1, milliseconds{ 50 }, false);
    timers.install(one_shot, t0);

    std::vector<std::uint64_t> handled;
    EXPECT_EQ(timers.take_due(t0, t0, handled), one_shot);

    EXPECT_TRUE(timers.empty());
    EXPECT_EQ(timers.earliest(), schedule::time_point::max());
}

TEST(timer_schedule_test, a_timer_due_after_the_boundary_is_not_claimed)
{
    schedule timers;
    timers.install(std::make_shared<fake_timer>(1, milliseconds{ 50 }, false), t0 + milliseconds{ 1 });

    // A late claim does not make it due; only the boundary does.
    std::vector<std::uint64_t> handled;
    EXPECT_EQ(timers.take_due(t0, t0 + milliseconds{ 500 }, handled), nullptr);

    EXPECT_EQ(timers.size(), 1U);
    EXPECT_TRUE(handled.empty());
}

TEST(timer_schedule_test, the_earliest_due_timer_is_claimed_first)
{
    schedule timers;
    const auto later = std::make_shared<fake_timer>(1, milliseconds{ 50 }, false);
    const auto earlier = std::make_shared<fake_timer>(2, milliseconds{ 50 }, false);
    timers.install(later, t0 + milliseconds{ 20 });
    timers.install(earlier, t0 + milliseconds{ 10 });

    const auto boundary = t0 + milliseconds{ 30 };
    std::vector<std::uint64_t> handled;
    EXPECT_EQ(timers.take_due(boundary, boundary, handled), earlier);
    EXPECT_EQ(timers.take_due(boundary, boundary, handled), later);
    EXPECT_EQ(timers.take_due(boundary, boundary, handled), nullptr);
}

TEST(timer_schedule_test, a_timer_claimed_in_this_pass_is_not_claimed_again_even_when_due)
{
    schedule timers;
    const auto always_due = std::make_shared<fake_timer>(1, milliseconds{ 0 }, true);
    const auto other = std::make_shared<fake_timer>(2, milliseconds{ 0 }, true);
    timers.install(always_due, t0);
    timers.install(other, t0 + milliseconds{ 1 });

    const auto boundary = t0 + milliseconds{ 1 };
    std::vector<std::uint64_t> handled;
    EXPECT_EQ(timers.take_due(boundary, t0, handled), always_due);

    // Due again at t0, before other, but already claimed in this pass.
    EXPECT_EQ(timers.earliest(), t0);
    EXPECT_EQ(timers.take_due(boundary, t0, handled), other);
    EXPECT_EQ(timers.take_due(boundary, t0, handled), nullptr);

    // A fresh pass claims it again.
    std::vector<std::uint64_t> next_pass;
    EXPECT_EQ(timers.take_due(boundary, t0, next_pass), always_due);
}

TEST(timer_schedule_test, remove_takes_out_only_the_timer_named_and_returns_it)
{
    schedule timers;
    const auto kept = std::make_shared<fake_timer>(1, milliseconds{ 50 }, true);
    const auto removed = std::make_shared<fake_timer>(2, milliseconds{ 50 }, true);
    timers.install(kept, t0 + milliseconds{ 20 });
    timers.install(removed, t0 + milliseconds{ 10 });

    EXPECT_EQ(timers.remove(removed.get()), removed);
    EXPECT_EQ(timers.remove(removed.get()), nullptr);

    EXPECT_EQ(timers.size(), 1U);
    EXPECT_EQ(timers.earliest(), t0 + milliseconds{ 20 });
}

TEST(timer_schedule_test, remove_all_returns_every_timer_and_leaves_none)
{
    schedule timers;
    const auto first = std::make_shared<fake_timer>(1, milliseconds{ 50 }, true);
    const auto second = std::make_shared<fake_timer>(2, milliseconds{ 50 }, false);
    timers.install(first, t0);
    timers.install(second, t0);

    EXPECT_EQ(timers.remove_all(), (std::vector<handle>{ first, second }));
    EXPECT_TRUE(timers.empty());
}
