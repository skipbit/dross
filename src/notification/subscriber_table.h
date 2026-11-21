#pragma once

#include "ipc/transport.h"

#include <cstddef>
#include <map>
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
// each name. Holds no lock and does no I/O.
class DROSS_SUBSCRIBER_TABLE_HIDDEN subscriber_table final {
public:
    // Starts to keep the names of the connection id.
    void join(connection_id id);

    // Adds name to the names of id, which is ignored when id has not joined.
    // A name added again is not counted twice.
    void add(connection_id id, std::string name);

    // Drops name from the names of id, when it is there.
    void remove(connection_id id, std::string_view name);

    // Drops id with all its names.
    void leave(connection_id id);

    // Whether any connection subscribed to name.
    bool wanted(std::string_view name) const;

    // The connections subscribed to name.
    std::vector<connection_id> receivers(std::string_view name) const;

private:
    // Lowers the count of name by one, and drops it at zero.
    void release(std::string_view name);

    std::map<connection_id, std::set<std::string, std::less<>>> _names_by_connection;
    std::map<std::string, std::size_t, std::less<>> _counts;
};

#undef DROSS_SUBSCRIBER_TABLE_HIDDEN

}  // namespace dross
