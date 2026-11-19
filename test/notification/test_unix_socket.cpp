#include "notification/unix_socket.h"
#include "socket_directory.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

namespace {

using unix_socket_test = dross_test::socket_directory_test;

dross::path path_of(const std::string& name)
{
    return dross::path{ name };
}

// A connected pair: the listener's end and the connecting end.
struct connection final {
    dross::socket_handle accepted;
    dross::socket_handle connected;
};

connection connect_pair(dross::unix_listener& listener, const std::string& name)
{
    auto connected = dross::unix_connect(path_of(name));
    EXPECT_TRUE(connected.has_value());
    auto accepted = listener.accept();
    EXPECT_TRUE(accepted.has_value() && accepted->has_value());
    return connection{ std::move(**accepted), std::move(*connected) };
}

// Leaves a socket file at name with nothing listening on it, as a process
// that has gone does.
void leave_stale_socket(const std::string& name)
{
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_GE(fd, 0);
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    name.copy(address.sun_path, sizeof(address.sun_path) - 1);
    ASSERT_EQ(::bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)), 0);
    ::close(fd);
}

// A listener at name that never accepts, with its queue of one filled by
// connections that wait.
struct full_listener final {
    dross::socket_handle listening;
    std::vector<dross::socket_handle> waiting;
};

full_listener fill_listener(const std::string& name)
{
    full_listener made{ dross::socket_handle{ ::socket(AF_UNIX, SOCK_STREAM, 0) }, {} };
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    name.copy(address.sun_path, sizeof(address.sun_path) - 1);
    EXPECT_EQ(::bind(made.listening.get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)), 0);
    EXPECT_EQ(::listen(made.listening.get(), 1), 0);
    while (made.waiting.size() < 16) {
        auto connected = dross::unix_connect(path_of(name));
        if (! connected) {
            EXPECT_EQ(connected.error(), dross::error(std::errc::resource_unavailable_try_again));
            break;
        }
        made.waiting.push_back(std::move(*connected));
    }
    EXPECT_LT(made.waiting.size(), 16U);
    return made;
}

ino_t inode_of(const std::string& name)
{
    struct stat found { };
    EXPECT_EQ(::lstat(name.c_str(), &found), 0);
    return found.st_ino;
}

}  // namespace

TEST_F(unix_socket_test, listen_makes_a_socket_file_for_its_owner_alone)
{
    const auto name = at("s");
    auto listener = dross::unix_listener::listen(path_of(name));
    ASSERT_TRUE(listener.has_value());
    struct stat found { };
    ASSERT_EQ(::lstat(name.c_str(), &found), 0);
    EXPECT_TRUE(S_ISSOCK(found.st_mode));
    EXPECT_EQ(found.st_mode & 0777, 0600U);
}

TEST_F(unix_socket_test, bytes_sent_on_one_end_arrive_on_the_other)
{
    const auto name = at("s");
    auto listener = dross::unix_listener::listen(path_of(name));
    ASSERT_TRUE(listener.has_value());
    auto pair = connect_pair(*listener, name);

    const std::array<std::byte, 3> sent{ std::byte{ 1 }, std::byte{ 2 }, std::byte{ 3 } };
    const auto written = dross::send_some(pair.connected.get(), sent);
    ASSERT_TRUE(written.has_value());
    EXPECT_EQ(*written, sent.size());

    std::array<std::byte, 8> received{};
    EXPECT_EQ(::read(pair.accepted.get(), received.data(), received.size()), 3);
    EXPECT_EQ(received[2], std::byte{ 3 });
}

TEST_F(unix_socket_test, accept_gives_nothing_when_no_one_is_connecting)
{
    auto listener = dross::unix_listener::listen(path_of(at("s")));
    ASSERT_TRUE(listener.has_value());
    const auto accepted = listener->accept();
    ASSERT_TRUE(accepted.has_value());
    EXPECT_FALSE(accepted->has_value());
}

TEST_F(unix_socket_test, both_ends_see_the_same_user_at_the_other_end)
{
    const auto name = at("s");
    auto listener = dross::unix_listener::listen(path_of(name));
    ASSERT_TRUE(listener.has_value());
    auto pair = connect_pair(*listener, name);

    const auto at_listener = dross::peer_uid(pair.accepted.get());
    const auto at_connector = dross::peer_uid(pair.connected.get());
    ASSERT_TRUE(at_listener.has_value());
    ASSERT_TRUE(at_connector.has_value());
    EXPECT_EQ(*at_listener, ::geteuid());
    EXPECT_EQ(*at_connector, ::geteuid());
}

TEST_F(unix_socket_test, a_path_someone_listens_on_is_not_taken)
{
    const auto name = at("s");
    auto first = dross::unix_listener::listen(path_of(name));
    ASSERT_TRUE(first.has_value());
    const ino_t before = inode_of(name);

    const auto second = dross::unix_listener::listen(path_of(name));
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error(), dross::error(std::errc::address_in_use));
    EXPECT_EQ(inode_of(name), before);
    EXPECT_TRUE(dross::unix_connect(path_of(name)).has_value());
}

TEST_F(unix_socket_test, a_path_whose_listener_has_no_room_left_is_not_taken)
{
#if defined(__APPLE__)
    GTEST_SKIP() << "macOS refuses a connection to a full listener as to a stale file";
#endif
    const auto name = at("s");
    const auto full = fill_listener(name);
    const ino_t before = inode_of(name);

    const auto second = dross::unix_listener::listen(path_of(name));
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error(), dross::error(std::errc::address_in_use));
    EXPECT_EQ(inode_of(name), before);
}

TEST_F(unix_socket_test, a_socket_file_left_behind_is_replaced)
{
    const auto name = at("s");
    leave_stale_socket(name);
    const ino_t stale = inode_of(name);
    // A second name keeps the old file, so its inode number is not reused.
    ASSERT_EQ(::link(name.c_str(), at("held").c_str()), 0);

    auto listener = dross::unix_listener::listen(path_of(name));
    ASSERT_TRUE(listener.has_value());
    EXPECT_NE(inode_of(name), stale);
    EXPECT_TRUE(dross::unix_connect(path_of(name)).has_value());
}

TEST_F(unix_socket_test, a_file_that_is_not_a_socket_is_left_alone)
{
    const auto name = at("s");
    std::ofstream{ name } << "kept";

    const auto listener = dross::unix_listener::listen(path_of(name));
    ASSERT_FALSE(listener.has_value());
    EXPECT_EQ(listener.error(), dross::error(std::errc::address_in_use));
    std::string content;
    std::ifstream{ name } >> content;
    EXPECT_EQ(content, "kept");
}

TEST_F(unix_socket_test, the_socket_file_goes_with_the_listener)
{
    const auto name = at("s");
    {
        auto listener = dross::unix_listener::listen(path_of(name));
        ASSERT_TRUE(listener.has_value());
    }
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::symlink_status(name)));
}

TEST_F(unix_socket_test, a_file_put_in_its_place_outlives_the_listener)
{
    const auto name = at("s");
    {
        auto listener = dross::unix_listener::listen(path_of(name));
        ASSERT_TRUE(listener.has_value());
        ASSERT_EQ(::unlink(name.c_str()), 0);
        std::ofstream{ name } << "other";
    }
    EXPECT_TRUE(std::filesystem::exists(name));
}

TEST_F(unix_socket_test, a_moved_listener_keeps_the_socket_file)
{
    const auto name = at("s");
    auto listener = dross::unix_listener::listen(path_of(name));
    ASSERT_TRUE(listener.has_value());
    {
        dross::unix_listener moved{ std::move(*listener) };
        {
            auto emptied = std::move(listener);
        }
        EXPECT_TRUE(std::filesystem::exists(std::filesystem::symlink_status(name)));
        EXPECT_TRUE(dross::unix_connect(path_of(name)).has_value());
    }
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::symlink_status(name)));
}

TEST_F(unix_socket_test, listening_leaves_no_other_file_behind)
{
    const auto name = at("s");
    auto listener = dross::unix_listener::listen(path_of(name));
    ASSERT_TRUE(listener.has_value());
    const auto directory = std::filesystem::path{ name }.parent_path();
    std::vector<std::string> found;
    for (const auto& each : std::filesystem::directory_iterator{ directory }) {
        found.push_back(each.path().filename().string());
    }
    EXPECT_EQ(found, std::vector<std::string>{ "s" });
}

TEST_F(unix_socket_test, a_path_with_no_room_for_the_name_listened_under_first_fails)
{
    const std::size_t limit = sizeof(sockaddr_un{}.sun_path);
    const auto directory = at("");
    ASSERT_LT(directory.size() + 2, limit);
    const auto name = directory + std::string(limit - 2 - directory.size(), 'x');
    ASSERT_EQ(name.size(), limit - 2);

    const auto listener = dross::unix_listener::listen(path_of(name));
    ASSERT_FALSE(listener.has_value());
    EXPECT_EQ(listener.error(), dross::error(std::errc::filename_too_long));
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::symlink_status(name)));
}

TEST_F(unix_socket_test, a_path_too_long_for_a_socket_fails)
{
    const auto name = at(std::string(200, 'x'));
    const auto listener = dross::unix_listener::listen(path_of(name));
    ASSERT_FALSE(listener.has_value());
    EXPECT_EQ(listener.error(), dross::error(std::errc::filename_too_long));
    const auto connected = dross::unix_connect(path_of(name));
    ASSERT_FALSE(connected.has_value());
    EXPECT_EQ(connected.error(), dross::error(std::errc::filename_too_long));
}

TEST_F(unix_socket_test, connecting_where_no_one_listens_fails)
{
    const auto connected = dross::unix_connect(path_of(at("none")));
    ASSERT_FALSE(connected.has_value());
    EXPECT_EQ(connected.error(), dross::error(std::errc::no_such_file_or_directory));
}

// SIGPIPE is left at its default here, which would end the test program.
TEST_F(unix_socket_test, sending_to_a_peer_that_has_gone_fails_without_a_signal)
{
    const auto name = at("s");
    auto listener = dross::unix_listener::listen(path_of(name));
    ASSERT_TRUE(listener.has_value());
    auto pair = connect_pair(*listener, name);
    pair.accepted = dross::socket_handle{};

    const std::array<std::byte, 1> sent{ std::byte{ 1 } };
    const auto written = dross::send_some(pair.connected.get(), sent);
    ASSERT_FALSE(written.has_value());
    EXPECT_EQ(written.error(), dross::error(std::errc::broken_pipe));
}

TEST_F(unix_socket_test, sending_to_a_full_socket_sends_nothing)
{
    const auto name = at("s");
    auto listener = dross::unix_listener::listen(path_of(name));
    ASSERT_TRUE(listener.has_value());
    auto pair = connect_pair(*listener, name);

    const std::array<std::byte, 4096> block{};
    std::size_t total = 0;
    for (int i = 0; i < 4096; ++i) {
        const auto written = dross::send_some(pair.connected.get(), block);
        ASSERT_TRUE(written.has_value());
        if (*written == 0) {
            break;
        }
        total += *written;
    }
    EXPECT_GT(total, 0U);
    const auto more = dross::send_some(pair.connected.get(), block);
    ASSERT_TRUE(more.has_value());
    EXPECT_EQ(*more, 0U);
}
