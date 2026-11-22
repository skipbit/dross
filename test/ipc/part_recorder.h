#pragma once

#include "dross/ipc/message_acceptor.h"
#include "dross/ipc/message_channel.h"
#include "ipc/part_calls.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace dross_test {

// Hides a declaration from the symbol table, as part_calls.h does, since the
// members refer to a hidden type.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_PART_RECORDER_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_PART_RECORDER_HIDDEN
#endif

using bytes = std::vector<std::byte>;

inline constexpr auto kPatience = std::chrono::seconds{ 5 };

inline bytes bytes_of(std::initializer_list<int> values)
{
    bytes out;
    for (const int v : values) {
        out.push_back(static_cast<std::byte>(v));
    }
    return out;
}

// Records what happens on one connection. A part that breaks the order of
// the calls stops the program, as the library's own events do.
class DROSS_PART_RECORDER_HIDDEN recorder final : public dross::ipc::message_channel_events {
public:
    void received(bytes message) override
    {
        const auto call = _calls.any("received");
        if (on_receive) {
            on_receive();
        }
        const std::lock_guard guard{ _lock };
        _messages.push_back(std::move(message));
        _changed.notify_all();
    }

    void closed(dross::error reason) override
    {
        const auto call = _calls.last("closed");
        const std::lock_guard guard{ _lock };
        ++_closes;
        _reason = std::move(reason);
        _changed.notify_all();
    }

    bool wait_for_messages(std::size_t count)
    {
        std::unique_lock guard{ _lock };
        return _changed.wait_for(guard, kPatience, [&] {
            return _messages.size() >= count;
        });
    }

    bool wait_for_close()
    {
        std::unique_lock guard{ _lock };
        return _changed.wait_for(guard, kPatience, [&] {
            return _closes > 0;
        });
    }

    std::vector<bytes> messages()
    {
        const std::lock_guard guard{ _lock };
        return _messages;
    }

    int closes()
    {
        const std::lock_guard guard{ _lock };
        return _closes;
    }

    std::optional<dross::error> reason()
    {
        const std::lock_guard guard{ _lock };
        return _reason;
    }

    // Set before the connection is made.
    std::function<void()> on_receive;

private:
    dross::part_calls _calls{ dross::part_calls::for_channel_events() };
    std::mutex _lock;
    std::condition_variable _changed;
    std::vector<bytes> _messages;
    int _closes{ 0 };
    std::optional<dross::error> _reason;
};

// Records what is accepted, giving each connection a recorder.
class DROSS_PART_RECORDER_HIDDEN acceptance : public dross::ipc::message_acceptor_events {
public:
    std::shared_ptr<dross::ipc::message_channel_events> accepted(std::shared_ptr<dross::ipc::message_channel> channel) override
    {
        const auto call = _calls.any("accepted");
        auto events = std::make_shared<recorder>();
        events->on_receive = on_receive;
        if (on_accept) {
            on_accept();
        }
        const std::lock_guard guard{ _lock };
        _channels.push_back(std::move(channel));
        _events.push_back(events);
        _changed.notify_all();
        return events;
    }

    void stopped(dross::error) override
    {
        const auto call = _calls.last("stopped");
    }

    bool wait_for_accepted(std::size_t count)
    {
        std::unique_lock guard{ _lock };
        return _changed.wait_for(guard, kPatience, [&] {
            return _channels.size() >= count;
        });
    }

    std::shared_ptr<dross::ipc::message_channel> channel(std::size_t at)
    {
        const std::lock_guard guard{ _lock };
        return _channels.at(at);
    }

    std::shared_ptr<recorder> events(std::size_t at)
    {
        const std::lock_guard guard{ _lock };
        return _events.at(at);
    }

    // Set before anything connects.
    std::function<void()> on_accept;
    std::function<void()> on_receive;

private:
    dross::part_calls _calls{ dross::part_calls::for_acceptor_events() };
    std::mutex _lock;
    std::condition_variable _changed;
    std::vector<std::shared_ptr<dross::ipc::message_channel>> _channels;
    std::vector<std::shared_ptr<recorder>> _events;
};

#undef DROSS_PART_RECORDER_HIDDEN

}  // namespace dross_test
