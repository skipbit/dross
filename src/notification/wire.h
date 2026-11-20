#pragma once

#include "dross/type/error.h"
#include "dross/type/value.h"

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <variant>
#include <vector>

// The messages a listener and a broadcaster in two processes send each
// other, each one CBOR array: the kind as a number, the topic's name and,
// for a notice, the value.
namespace dross::wire {

// Hides a declaration from the library's exported symbol table, as
// cbor.h does.
#if defined(__GNUC__) || defined(__clang__)
#define DROSS_WIRE_HIDDEN __attribute__((visibility("hidden")))
#else
#define DROSS_WIRE_HIDDEN
#endif

// From a listener: send what is notified on name from now on.
struct subscribe final {
    std::string name;
};

// From a listener: stop sending what is notified on name.
struct unsubscribe final {
    std::string name;
};

// From a broadcaster: content was notified on name.
struct notice final {
    std::string name;
    value content;
};

using message = std::variant<subscribe, unsubscribe, notice>;

DROSS_WIRE_HIDDEN std::vector<std::byte> encode(const message& which);

// Fails with std::errc::invalid_argument for bytes that are not one
// message.
DROSS_WIRE_HIDDEN std::expected<message, error> decode(std::span<const std::byte> bytes);

#undef DROSS_WIRE_HIDDEN

}  // namespace dross::wire
