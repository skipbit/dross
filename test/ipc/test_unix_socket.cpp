#include "dross/ipc/unix_socket.h"

#include <gtest/gtest.h>

#include <string>

namespace {

TEST(ipc_unix_socket, keeps_the_path_it_is_made_with)
{
    const dross::ipc::unix_socket place{ dross::path{ std::string{ "/run/user/1000/player.sock" } } };
    EXPECT_EQ(place.location().string(), "/run/user/1000/player.sock");
}

TEST(ipc_unix_socket, copies_keep_the_same_path)
{
    const dross::ipc::unix_socket place{ dross::path{ std::string{ "/tmp/a.sock" } } };
    const dross::ipc::unix_socket copy = place;
    EXPECT_EQ(copy.location().string(), place.location().string());
}

}  // namespace
