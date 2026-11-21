#include "dross/ipc/message_channel.h"

#include "dross/ipc/message_acceptor.h"
#include "dross/ipc/message_connector.h"

namespace dross::ipc {

message_channel_events::message_channel_events() = default;
message_channel_events::~message_channel_events() = default;

message_channel::message_channel() = default;
message_channel::~message_channel() = default;

message_acceptor_events::message_acceptor_events() = default;
message_acceptor_events::~message_acceptor_events() = default;

message_acceptor::message_acceptor() = default;
message_acceptor::~message_acceptor() = default;

message_connector::message_connector() = default;
message_connector::~message_connector() = default;

}  // namespace dross::ipc
