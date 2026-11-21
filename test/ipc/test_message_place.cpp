#include "dross/ipc/message_place.h"

#include <gtest/gtest.h>

#include <expected>
#include <memory>

namespace {

using dross::ipc::message_acceptor;
using dross::ipc::message_acceptor_events;
using dross::ipc::message_connector;

struct listen_only {
    std::expected<std::unique_ptr<message_acceptor>, dross::error> listen(std::shared_ptr<message_acceptor_events> events) const;
};

struct connect_only {
    std::shared_ptr<message_connector> connector() const;
};

struct both {
    std::expected<std::unique_ptr<message_acceptor>, dross::error> listen(std::shared_ptr<message_acceptor_events> events) const;
    std::shared_ptr<message_connector> connector() const;
};

struct neither {
};

struct not_copyable {
    not_copyable() = default;
    not_copyable(const not_copyable& other) = delete;
    std::expected<std::unique_ptr<message_acceptor>, dross::error> listen(std::shared_ptr<message_acceptor_events> events) const;
};

struct listen_returns_another_type {
    std::unique_ptr<message_acceptor> listen(std::shared_ptr<message_acceptor_events> events) const;
};

TEST(ipc_message_place, a_place_with_listen_can_be_listened_at)
{
    EXPECT_TRUE(dross::ipc::message_listenable_place_type<listen_only>);
    EXPECT_FALSE(dross::ipc::message_connectable_place_type<listen_only>);
}

TEST(ipc_message_place, a_place_with_connector_can_be_connected_to)
{
    EXPECT_FALSE(dross::ipc::message_listenable_place_type<connect_only>);
    EXPECT_TRUE(dross::ipc::message_connectable_place_type<connect_only>);
}

TEST(ipc_message_place, a_place_can_be_both)
{
    EXPECT_TRUE(dross::ipc::message_listenable_place_type<both>);
    EXPECT_TRUE(dross::ipc::message_connectable_place_type<both>);
}

TEST(ipc_message_place, a_type_without_either_is_no_place)
{
    EXPECT_FALSE(dross::ipc::message_listenable_place_type<neither>);
    EXPECT_FALSE(dross::ipc::message_connectable_place_type<neither>);
}

TEST(ipc_message_place, a_place_to_listen_at_is_copied)
{
    EXPECT_FALSE(dross::ipc::message_listenable_place_type<not_copyable>);
}

TEST(ipc_message_place, listen_returns_an_acceptor_or_an_error)
{
    EXPECT_FALSE(dross::ipc::message_listenable_place_type<listen_returns_another_type>);
}

}  // namespace
