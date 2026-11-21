#pragma once

#include "dross/ipc/message_channel.h"

#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace dross {

// Hides a declaration from the library's exported symbol table, as
// poller.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_SUBSCRIBER_TABLE_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_SUBSCRIBER_TABLE_HIDDEN
#endif

// The names each connection subscribed to, and how many connections want
// each name. Holds the channels of the connections that joined, no lock,
// and does no I/O.
class DROSS_SUBSCRIBER_TABLE_HIDDEN subscriber_table final {
public:
    using channel = std::shared_ptr<ipc::message_channel>;

    // Starts to keep the names of the connection.
    void join(const channel& connection);

    // Adds name to the names of the connection, which is ignored when it
    // has not joined. A name added again is not counted twice.
    void add(const channel& connection, std::string name);

    // Drops name from the names of the connection, when it is there.
    void remove(const channel& connection, std::string_view name);

    // Drops the connection with all its names.
    void leave(const channel& connection);

    // Drops every connection.
    void clear();

    // Whether any connection subscribed to name.
    bool wanted(std::string_view name) const;

    // The connections subscribed to name.
    std::vector<channel> receivers(std::string_view name) const;

private:
    // Lowers the count of name by one, and drops it at zero.
    void release(std::string_view name);

    std::map<channel, std::set<std::string, std::less<>>> _names_by_connection;
    std::map<std::string, std::size_t, std::less<>> _counts;
};

#undef DROSS_SUBSCRIBER_TABLE_HIDDEN

}  // namespace dross
