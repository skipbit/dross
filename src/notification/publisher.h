#pragma once

#include "dross/platform/path.h"
#include "dross/type/error.h"
#include "dross/type/value.h"
#include "notification/subscriber_table.h"
#include "notification/transport.h"

#include <cstddef>
#include <expected>
#include <memory>
#include <mutex>
#include <string_view>

namespace dross {

// Hides a declaration from the library's exported symbol table, as
// poller.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_PUBLISHER_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_PUBLISHER_HIDDEN
#endif

class unix_transport;

// The part of a published broadcaster that faces other processes: it
// listens at a path, keeps the names each listener there subscribed to,
// and sends a notification to those that want it.
//
// A failure of the transport's waiting is met by listening at the path
// again, once; when that fails too, nothing more is sent.
class DROSS_PUBLISHER_HIDDEN publisher final : private transport_events {
public:
    static std::expected<std::unique_ptr<publisher>, error> listen(const path& where);

    ~publisher() override;

    publisher(const publisher& other) = delete;
    publisher& operator=(const publisher& other) = delete;
    publisher(publisher&& other) = delete;
    publisher& operator=(publisher&& other) = delete;

    // Whether a listener subscribed to name.
    bool wanted(std::string_view name) const;

    // Sends content on name to each listener subscribed to it.
    void send(std::string_view name, const value& content);

private:
    explicit publisher(path where);

    void connected(connection_id id) override;
    void received(connection_id id, std::vector<std::byte> message) override;
    void closed(connection_id id, error reason) override;
    void stopped(error reason) override;

    // The transport, which stopped() may replace while send() runs.
    std::shared_ptr<unix_transport> transport() const;

    const path _where;
    mutable std::mutex _lock;
    std::shared_ptr<unix_transport> _transport;
    subscriber_table _subscribers;
    // Set by the destructor, so that stopped() does not start a transport
    // that nothing would stop.
    bool _stopping{ false };
};

#undef DROSS_PUBLISHER_HIDDEN

}  // namespace dross
