#include "file_local_peer.h"

#include "dross/notification/delivery.h"
#include "dross/notification/topic.h"

#include <atomic>
#include <memory>
#include <string>
#include <utility>

namespace {

struct progress {
    std::string text;
};

inline constexpr dross::topic<progress> kProgress{ "com.example.test.progress" };

}  // namespace

namespace dross_test::file_local_peer {

dross::subscription subscribe(dross::listener& ears, std::shared_ptr<std::atomic<int>> received)
{
    auto made = ears.subscribe(kProgress, [received = std::move(received)](const progress&) {
        ++*received;
    }, dross::delivery::direct());
    return made ? std::move(*made) : dross::subscription{};
}

void notify(dross::broadcaster& news)
{
    news.notify(kProgress, progress{ "from the other file" });
}

}  // namespace dross_test::file_local_peer
