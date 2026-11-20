#pragma once

#include "dross/notification/link_event.h"
#include "dross/platform/path.h"
#include "dross/type/error.h"
#include "dross/type/value.h"
#include "notification/broadcaster_access.h"
#include "notification/connector.h"
#include "notification/topic_address.h"

#include <cstddef>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <typeinfo>
#include <vector>

namespace dross {

// Hides a declaration from the library's exported symbol table, as
// poller.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_REMOTE_BROADCASTER_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_REMOTE_BROADCASTER_HIDDEN
#endif

// A broadcaster in another process, as a listener here hears it: it keeps
// the subscribers here, tells that broadcaster the names they want, and
// hands each value it sends to them, made back into their type. What
// happens to the connection goes to the subscribers of listener::link.
class DROSS_REMOTE_BROADCASTER_HIDDEN remote_broadcaster final : public connector_events {
public:
    using registry = broadcaster_access::storage_type;

    // Makes the value a dross::value holds, or says why it could not.
    using value_reader = std::expected<std::shared_ptr<const void>, error> (*)(const value& held);

    // Starts connecting to the broadcaster at where.
    static std::shared_ptr<remote_broadcaster> connect(const path& where);

    ~remote_broadcaster() override;

    remote_broadcaster(const remote_broadcaster& other) = delete;
    remote_broadcaster& operator=(const remote_broadcaster& other) = delete;
    remote_broadcaster(remote_broadcaster&& other) = delete;
    remote_broadcaster& operator=(remote_broadcaster&& other) = delete;

    // The subscribers here, which a listener adds to and prunes.
    std::weak_ptr<registry> subscribers() const noexcept;

    // A subscriber of name came, of the type read makes; or one went.
    void want(std::string_view name, const std::type_info& type, value_reader read);
    void unwant(std::string_view name, const std::type_info& type);

    void connected() override;
    void received(std::vector<std::byte> message) override;
    void disconnected(error reason) override;

private:
    remote_broadcaster();

    // How the subscribers of one name and type read a value, and how many
    // of them there are.
    struct reading final {
        std::string type;
        const std::type_info* identity;
        value_reader read;
        std::size_t count;
    };

    // Hands value to the subscribers at where, on this thread.
    void hand(const topic_address& where, const std::shared_ptr<const void>& value) const;
    void tell(link_event what) const;

    const std::shared_ptr<registry> _subscribers;

    std::mutex _lock;
    std::map<std::string, std::vector<reading>, std::less<>> _wanted;
    // Between connected() and disconnected(), so each name is asked for once
    // per connection.
    bool _linked{ false };

    // Last, so it goes first, and no event comes while the rest goes.
    std::unique_ptr<connector> _connector;
};

#undef DROSS_REMOTE_BROADCASTER_HIDDEN

}  // namespace dross
