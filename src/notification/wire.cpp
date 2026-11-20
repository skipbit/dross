#include "notification/wire.h"

#include "dross/type/array.h"
#include "dross/type/data.h"
#include "dross/type/value_traits.h"
#include "format/cbor.h"

#include <cstdint>
#include <string>
#include <utility>

namespace dross::wire {

namespace {

enum class kind : std::int64_t {
    subscribe = 1,
    unsubscribe = 2,
    notice = 3,
};

array header(const kind what, const std::string& name)
{
    return array{ value_traits<std::int64_t>::to_value(static_cast<std::int64_t>(what)), value_traits<std::string>::to_value(name) };
}

error malformed(std::string detail)
{
    return error{ std::errc::invalid_argument, std::move(detail) };
}

}  // namespace

std::vector<std::byte> encode(const message& which)
{
    array items;
    if (const auto* asked = std::get_if<subscribe>(&which)) {
        items = header(kind::subscribe, asked->name);
    } else if (const auto* dropped = std::get_if<unsubscribe>(&which)) {
        items = header(kind::unsubscribe, dropped->name);
    } else {
        const auto& sent = std::get<notice>(which);
        items = header(kind::notice, sent.name);
        items.append(sent.content);
    }
    const data bytes = cbor::serialize(value{ std::move(items) });
    std::vector<std::byte> result(bytes.size());
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        result[i] = static_cast<std::byte>(bytes[i]);
    }
    return result;
}

std::expected<message, error> decode(const std::span<const std::byte> bytes)
{
    const data input{ reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size() };
    auto read = cbor::deserialize(input);
    if (! read) {
        return std::unexpected(std::move(read).error());
    }
    const auto* items = read->get_if<array>();
    if ((items == nullptr) || (items->length() < 2)) {
        return std::unexpected(malformed("a message is an array of at least two items"));
    }
    const auto what = value_traits<std::int64_t>::from_value((*items)[0]);
    auto name = value_traits<std::string>::from_value((*items)[1]);
    if (! what || ! name) {
        return std::unexpected(malformed("a message starts with its kind and a name"));
    }
    const std::size_t expected = (*what == static_cast<std::int64_t>(kind::notice)) ? 3 : 2;
    if (items->length() != expected) {
        return std::unexpected(malformed("a message has the wrong number of items for its kind"));
    }
    switch (static_cast<kind>(*what)) {
    case kind::subscribe:
        return subscribe{ std::move(*name) };
    case kind::unsubscribe:
        return unsubscribe{ std::move(*name) };
    case kind::notice:
        return notice{ std::move(*name), (*items)[2] };
    }
    return std::unexpected(malformed("unknown message kind"));
}

}  // namespace dross::wire
