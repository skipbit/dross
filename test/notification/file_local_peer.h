#pragma once

#include "dross/notification/broadcaster.h"
#include "dross/notification/listener.h"
#include "dross/notification/subscription.h"

#include <atomic>
#include <memory>

// Another source file with a type named progress in its unnamed namespace,
// carried on a topic with the same name as one the tests use, for checking
// that the two types are told apart.
namespace dross_test::file_local_peer {

// Subscribes on the sending thread to that topic, counting in received each
// call.
dross::subscription subscribe(dross::listener& ears, std::shared_ptr<std::atomic<int>> received);

// Sends one notification on that topic.
void notify(dross::broadcaster& news);

}  // namespace dross_test::file_local_peer
