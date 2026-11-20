#include "notification/subscriber_table.h"

#include <utility>

namespace dross {

void subscriber_table::join(const connection_id id)
{
    _names_by_connection.try_emplace(id);
}

void subscriber_table::add(const connection_id id, std::string name)
{
    const auto found = _names_by_connection.find(id);
    if (found == _names_by_connection.end()) {
        return;
    }
    const auto [kept, added] = found->second.insert(std::move(name));
    if (added) {
        ++_counts[*kept];
    }
}

void subscriber_table::remove(const connection_id id, const std::string_view name)
{
    const auto found = _names_by_connection.find(id);
    if (found == _names_by_connection.end()) {
        return;
    }
    const auto kept = found->second.find(name);
    if (kept == found->second.end()) {
        return;
    }
    release(*kept);
    found->second.erase(kept);
}

void subscriber_table::leave(const connection_id id)
{
    const auto found = _names_by_connection.find(id);
    if (found == _names_by_connection.end()) {
        return;
    }
    for (const auto& name : found->second) {
        release(name);
    }
    _names_by_connection.erase(found);
}

bool subscriber_table::wanted(const std::string_view name) const
{
    return _counts.contains(name);
}

std::vector<connection_id> subscriber_table::receivers(const std::string_view name) const
{
    std::vector<connection_id> found;
    for (const auto& [id, names] : _names_by_connection) {
        if (names.contains(name)) {
            found.push_back(id);
        }
    }
    return found;
}

void subscriber_table::release(const std::string_view name)
{
    const auto count = _counts.find(name);
    if (count == _counts.end()) {
        return;
    }
    if (--count->second == 0) {
        _counts.erase(count);
    }
}

}  // namespace dross
