#include "notification/poller.h"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

using namespace std::chrono_literals;

// A connected pair of stream sockets, closed at the end of the test.
class socket_pair final {
public:
    socket_pair()
    {
        EXPECT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, _fds), 0);
    }

    ~socket_pair()
    {
        close_one();
        close_other();
    }

    socket_pair(const socket_pair& other) = delete;
    socket_pair& operator=(const socket_pair& other) = delete;

    int one() const noexcept
    {
        return _fds[0];
    }

    int other() const noexcept
    {
        return _fds[1];
    }

    void close_other() noexcept
    {
        if (_fds[1] >= 0) {
            ::close(_fds[1]);
            _fds[1] = -1;
        }
    }

private:
    void close_one() noexcept
    {
        if (_fds[0] >= 0) {
            ::close(_fds[0]);
            _fds[0] = -1;
        }
    }

    int _fds[2]{ -1, -1 };
};

dross::poller make_poller()
{
    auto made = dross::poller::create();
    EXPECT_TRUE(made.has_value());
    return std::move(*made);
}

std::optional<dross::poller::event> find(const std::vector<dross::poller::event>& events, const int fd)
{
    for (const auto& each : events) {
        if (each.fd == fd) {
            return each;
        }
    }
    return std::nullopt;
}

}  // namespace

TEST(poller_test, a_socket_with_room_is_writable)
{
    socket_pair pair;
    auto watcher = make_poller();
    ASSERT_TRUE(watcher.add(pair.one(), dross::poller::interest::write).has_value());
    const auto ready = watcher.wait(1000ms);
    ASSERT_TRUE(ready.has_value());
    const auto found = find(*ready, pair.one());
    ASSERT_TRUE(found.has_value());
    EXPECT_TRUE(found->writable);
    EXPECT_FALSE(found->readable);
}

TEST(poller_test, a_socket_is_readable_once_the_peer_writes)
{
    socket_pair pair;
    auto watcher = make_poller();
    ASSERT_TRUE(watcher.add(pair.one(), dross::poller::interest::read).has_value());

    const auto before = watcher.wait(0ms);
    ASSERT_TRUE(before.has_value());
    EXPECT_FALSE(find(*before, pair.one()).has_value());

    const char byte = 'x';
    ASSERT_EQ(::write(pair.other(), &byte, 1), 1);
    const auto after = watcher.wait(1000ms);
    ASSERT_TRUE(after.has_value());
    const auto found = find(*after, pair.one());
    ASSERT_TRUE(found.has_value());
    EXPECT_TRUE(found->readable);
    EXPECT_FALSE(found->writable);
}

TEST(poller_test, change_replaces_what_is_waited_for)
{
    socket_pair pair;
    auto watcher = make_poller();
    ASSERT_TRUE(watcher.add(pair.one(), dross::poller::interest::write).has_value());
    ASSERT_TRUE(watcher.change(pair.one(), dross::poller::interest::read).has_value());

    const auto quiet = watcher.wait(0ms);
    ASSERT_TRUE(quiet.has_value());
    EXPECT_FALSE(find(*quiet, pair.one()).has_value());

    ASSERT_TRUE(watcher.change(pair.one(), dross::poller::interest::read_write).has_value());
    const auto both = watcher.wait(1000ms);
    ASSERT_TRUE(both.has_value());
    const auto found = find(*both, pair.one());
    ASSERT_TRUE(found.has_value());
    EXPECT_TRUE(found->writable);
}

TEST(poller_test, a_removed_socket_is_not_reported)
{
    socket_pair pair;
    auto watcher = make_poller();
    ASSERT_TRUE(watcher.add(pair.one(), dross::poller::interest::read_write).has_value());
    watcher.remove(pair.one());
    const auto ready = watcher.wait(0ms);
    ASSERT_TRUE(ready.has_value());
    EXPECT_TRUE(ready->empty());
}

TEST(poller_test, a_peer_that_goes_is_seen_as_readable)
{
    socket_pair pair;
    auto watcher = make_poller();
    ASSERT_TRUE(watcher.add(pair.one(), dross::poller::interest::read).has_value());
    pair.close_other();
    const auto ready = watcher.wait(1000ms);
    ASSERT_TRUE(ready.has_value());
    const auto found = find(*ready, pair.one());
    ASSERT_TRUE(found.has_value());
    EXPECT_TRUE(found->readable);
    char byte = 0;
    EXPECT_EQ(::read(pair.one(), &byte, 1), 0);
}

TEST(poller_test, wait_returns_empty_when_the_timeout_passes)
{
    auto watcher = make_poller();
    const auto started = std::chrono::steady_clock::now();
    const auto ready = watcher.wait(50ms);
    ASSERT_TRUE(ready.has_value());
    EXPECT_TRUE(ready->empty());
    EXPECT_GE(std::chrono::steady_clock::now() - started, 40ms);
}

TEST(poller_test, wake_from_another_thread_ends_a_wait_without_a_timeout)
{
    auto watcher = make_poller();
    std::thread waker([&watcher] {
        std::this_thread::sleep_for(20ms);
        watcher.wake();
    });
    const auto ready = watcher.wait();
    waker.join();
    ASSERT_TRUE(ready.has_value());
    EXPECT_TRUE(ready->empty());
}

TEST(poller_test, a_wake_before_the_wait_ends_the_next_wait_once)
{
    auto watcher = make_poller();
    watcher.wake();
    const auto first = watcher.wait(1000ms);
    ASSERT_TRUE(first.has_value());
    EXPECT_TRUE(first->empty());

    const auto started = std::chrono::steady_clock::now();
    const auto second = watcher.wait(50ms);
    ASSERT_TRUE(second.has_value());
    EXPECT_GE(std::chrono::steady_clock::now() - started, 40ms);
}

TEST(poller_test, adding_a_descriptor_that_is_not_open_fails)
{
    auto watcher = make_poller();
    const auto added = watcher.add(-1, dross::poller::interest::read);
    ASSERT_FALSE(added.has_value());
    EXPECT_EQ(added.error(), dross::error(std::errc::bad_file_descriptor));
}
