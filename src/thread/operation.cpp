#include "dross/thread/operation.h"

#include "thread/operation_access.h"

#include <atomic>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ostream>
#include <string>
#include <system_error>

namespace dross {

namespace {

class operation_category_type final : public std::error_category {
public:
    const char* name() const noexcept override
    {
        return "dross.operation";
    }

    std::string message(int value) const override
    {
        switch (static_cast<operation_errc>(value)) {
        case operation_errc::cancelled:
            return "operation was cancelled";
        case operation_errc::not_finished:
            return "operation has not finished";
        case operation_errc::type_mismatch:
            return "operation result is not of the requested type";
        }
        return "unknown operation error";
    }
};

}  // namespace

const std::error_category& operation_category() noexcept
{
    static const operation_category_type the_category;
    return the_category;
}

std::error_code make_error_code(operation_errc e) noexcept
{
    return { static_cast<int>(e), operation_category() };
}

operation_id::operation_id(std::uint64_t value) noexcept
    : _value{ value }
{
}

operation_id::operation_id(const operation_id& other) noexcept = default;

operation_id::~operation_id() = default;

operation_id& operation_id::operator=(const operation_id& other) noexcept = default;

bool operation_id::operator==(const operation_id& other) const noexcept
{
    return (_value == other._value);
}

std::strong_ordering operation_id::operator<=>(const operation_id& other) const noexcept
{
    return (_value <=> other._value);
}

std::ostream& operator<<(std::ostream& os, const operation_id& id)
{
    return (os << id._value);
}

operation_id operation_access::next_id() noexcept
{
    static std::atomic<std::uint64_t> last{ 0 };
    return operation_id{ last.fetch_add(1, std::memory_order_relaxed) + 1 };
}

}  // namespace dross

std::size_t std::hash<dross::operation_id>::operator()(const dross::operation_id& id) const noexcept
{
    return std::hash<std::uint64_t>{}(id._value);
}
