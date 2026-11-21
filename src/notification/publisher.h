#pragma once

#include "dross/ipc/message_acceptor.h"
#include "dross/type/error.h"
#include "dross/type/value.h"

#include <expected>
#include <functional>
#include <memory>
#include <string_view>

namespace dross {

// Hides a declaration from the library's exported symbol table, as
// poller.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_PUBLISHER_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_PUBLISHER_HIDDEN
#endif

// The part of a published broadcaster that faces other processes: it
// listens at a place, keeps the names each listener there subscribed to,
// and sends a notification to those that want it.
//
// When what listens stops on a failure of its own, the place is listened
// at again, once; when that fails too, nothing more is sent.
class DROSS_PUBLISHER_HIDDEN publisher final {
public:
    // Starts listening at a place with the events given.
    using listen_function = std::function<std::expected<std::unique_ptr<ipc::message_acceptor>, error>(std::shared_ptr<ipc::message_acceptor_events>)>;

    static std::expected<std::unique_ptr<publisher>, error> listen(listen_function listen);

    // Stops listening, and returns once nothing more is called from it.
    ~publisher();

    publisher(const publisher& other) = delete;
    publisher& operator=(const publisher& other) = delete;
    publisher(publisher&& other) = delete;
    publisher& operator=(publisher&& other) = delete;

    // Whether a listener subscribed to name.
    bool wanted(std::string_view name) const;

    // Sends content on name to each listener subscribed to it.
    void send(std::string_view name, const value& content);

private:
    class core;

    explicit publisher(std::shared_ptr<core> shared);

    const std::shared_ptr<core> _core;
};

#undef DROSS_PUBLISHER_HIDDEN

}  // namespace dross
