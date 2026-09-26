#include "dross/thread/operation.h"

#include "thread/deadline.h"
#include "thread/operation_access.h"
#include "thread/time_source.h"

#include <any>
#include <atomic>
#include <chrono>
#include <compare>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <system_error>
#include <utility>

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

class operation_result::storage final {
public:
    storage(operation_id id, std::shared_ptr<const time_source> source)
        : _id{ id }
        , _source{ std::move(source) }
    {
    }

    operation_id id() const noexcept
    {
        return _id;
    }

    bool is_finished()
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        return _finished;
    }

    bool wait_for(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock{ _mutex };
        if (timeout <= std::chrono::milliseconds::zero()) {
            return _finished;
        }

        const auto deadline = deadline::after(_source->now(), timeout);
        while ((! _finished) && (_source->now() < deadline)) {
            _source->wait_until(lock, _changed, deadline);
        }
        return _finished;
    }

    std::expected<const std::any*, error> held()
    {
        const std::lock_guard<std::mutex> guard{ _mutex };
        if (! _finished) {
            return std::unexpected(error(operation_errc::not_finished));
        }
        if (_cancelled) {
            return std::unexpected(error(operation_errc::cancelled));
        }
        return &_value;
    }

    void finish(std::any value)
    {
        {
            const std::lock_guard<std::mutex> guard{ _mutex };
            _value = std::move(value);
            _finished = true;
        }
        _changed.notify_all();
    }

    void cancel()
    {
        {
            const std::lock_guard<std::mutex> guard{ _mutex };
            _cancelled = true;
            _finished = true;
        }
        _changed.notify_all();
    }

private:
    // Set once, at construction, and never reassigned, so they are read
    // without _mutex.
    const operation_id _id;
    const std::shared_ptr<const time_source> _source;

    std::mutex _mutex;
    std::condition_variable _changed;
    // Written once, before _finished is set, and never again, so held()
    // hands out a pointer to it that stays good.
    std::any _value;
    // Set when the operation returns or is cancelled, whichever happens;
    // _cancelled tells which.
    bool _finished{ false };
    bool _cancelled{ false };
};

operation_result::operation_result(std::shared_ptr<storage> store) noexcept
    : _store{ std::move(store) }
{
}

operation_result::operation_result(const operation_result& other) = default;

operation_result::~operation_result() = default;

operation_result& operation_result::operator=(const operation_result& other) = default;

operation_id operation_result::id() const noexcept
{
    return _store->id();
}

bool operation_result::is_finished() const
{
    return _store->is_finished();
}

bool operation_result::wait_for(std::chrono::milliseconds timeout) const
{
    return _store->wait_for(timeout);
}

std::expected<const std::any*, error> operation_result::held() const
{
    return _store->held();
}

operation_id operation_access::next_id() noexcept
{
    static std::atomic<std::uint64_t> last{ 0 };
    return operation_id{ last.fetch_add(1, std::memory_order_relaxed) + 1 };
}

operation_result operation_access::make(std::shared_ptr<const time_source> source)
{
    return operation_result{ std::make_shared<operation_result::storage>(next_id(), std::move(source)) };
}

void operation_access::finish(const operation_result& result, std::any value)
{
    result._store->finish(std::move(value));
}

void operation_access::cancel(const operation_result& result)
{
    result._store->cancel();
}

}  // namespace dross

std::size_t std::hash<dross::operation_id>::operator()(const dross::operation_id& id) const noexcept
{
    return std::hash<std::uint64_t>{}(id._value);
}
