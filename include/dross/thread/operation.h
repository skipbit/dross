#pragma once

#include "dross/type/error.h"

#include <any>
#include <chrono>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <ostream>
#include <system_error>
#include <type_traits>

namespace dross {

/**
 * @brief Why an operation's result holds no value.
 */
enum class operation_errc {
    cancelled = 1,      ///< The operation was taken off its queue before it ran
    not_finished = 2,   ///< The operation has not finished yet
    type_mismatch = 3,  ///< The operation returned a different type from the one asked for
};

/**
 * @brief Get the category of operation_errc, named "dross.operation".
 * @return The one category object, the same on every call
 */
const std::error_category& operation_category() noexcept;

/**
 * @brief Make an error code from an operation_errc.
 * @param e The value
 * @return An error code of operation_category()
 */
std::error_code make_error_code(operation_errc e) noexcept;

}  // namespace dross

template <>
struct std::is_error_code_enum<dross::operation_errc> : std::true_type { };

namespace dross {

/**
 * @brief Names one operation, unique within the process.
 *
 * Only a queue makes one, when it takes an operation. Ids from different
 * queues never compare equal, and an id compares less than every id made
 * after it.
 */
class operation_id final {
public:
    /**
     * @brief Copy constructor.
     * @param other The id to copy
     */
    operation_id(const operation_id& other) noexcept;

    /**
     * @brief Destructor.
     */
    ~operation_id();

    /**
     * @brief Copy assignment.
     * @param other The id to copy
     * @return Reference to this id
     */
    operation_id& operator=(const operation_id& other) noexcept;

    /**
     * @brief Test whether two ids name the same operation.
     * @param other The id to compare with
     * @return true when both name one operation
     */
    bool operator==(const operation_id& other) const noexcept;

    /**
     * @brief Order two ids by when they were made.
     * @param other The id to compare with
     * @return The order, earlier first
     */
    std::strong_ordering operator<=>(const operation_id& other) const noexcept;

private:
    explicit operation_id(std::uint64_t value) noexcept;

    std::uint64_t _value;

    friend class operation_access;
    friend struct std::hash<operation_id>;
    friend std::ostream& operator<<(std::ostream& os, const operation_id& id);
};

/**
 * @brief Write an id, as a number, for a log.
 * @param os The stream
 * @param id The id
 * @return os
 */
std::ostream& operator<<(std::ostream& os, const operation_id& id);

/**
 * @brief A type operation_result::get_as() can give: void, for an operation
 * that returns nothing, or a type it can copy out.
 */
template <typename T>
concept operation_value_type = std::is_void_v<T> || (std::is_object_v<T> && std::copy_constructible<T>);

/**
 * @brief What one operation returned, once it has finished.
 *
 * A queue hands one back when it takes an operation, before the operation
 * runs, and fills it in when the operation returns, or when
 * operation_queue::cancel() takes the operation off the queue before it
 * starts. Either way the operation has finished.
 *
 * Handle semantics:
 * - An operation_result is a handle. Copying gives another handle to the same
 *   result, and every copy sees it filled in
 * - There is no empty handle; every one names an operation
 *
 * Reading:
 * - get_as() never waits. Before the operation finishes it reports
 *   operation_errc::not_finished; wait_for() is the way to wait
 * - For a cancelled operation it reports operation_errc::cancelled
 * - Once filled in, a result never changes, so get_as() may be called any
 *   number of times, from any thread
 *
 * Thread safety:
 * - Every operation is free of data races when called from any thread
 *
 * @code
 * std::optional<dross::operation_result> result = queue.enqueue([]() {
 *     return expensive_work();
 * });
 * if (result && result->wait_for(std::chrono::seconds{ 1 })) {
 *     std::expected<int, dross::error> answer = result->get_as<int>();
 * }
 * @endcode
 */
class operation_result final {
public:
    /**
     * @brief Copy constructor, giving another handle to the same result.
     * @param other The handle to copy
     */
    operation_result(const operation_result& other);

    /**
     * @brief Destructor.
     */
    ~operation_result();

    /**
     * @brief Copy assignment, naming the same result as the source.
     * @param other The handle to copy
     * @return Reference to this handle
     */
    operation_result& operator=(const operation_result& other);

    /**
     * @brief Get the id of the operation this is the result of.
     * @return The id
     */
    operation_id id() const noexcept;

    /**
     * @brief Test whether the operation has finished.
     * @return true once the operation has returned or been cancelled
     */
    bool is_finished() const;

    /**
     * @brief Wait for the operation to finish, for at most timeout.
     * @param timeout How long to wait
     * @return true when the operation has returned or been cancelled
     *
     * A timeout of zero does not wait; it reports whether the operation has
     * already finished. Called from one of the queue's own workers, this
     * waits too, since another worker may run the operation.
     */
    bool wait_for(std::chrono::milliseconds timeout) const;

    /**
     * @brief Get what the operation returned, as a T.
     * @tparam T The type the operation returns, or void for one returning
     * nothing
     * @return A copy of the value, or the reason there is none
     *
     * Never waits. The error is one of:
     * - operation_errc::not_finished when the operation has not finished
     * - operation_errc::cancelled when it was cancelled before it started
     * - operation_errc::type_mismatch when the operation returned another
     *   type, including a value when T is void or nothing when it is not
     */
    template <operation_value_type T>
    std::expected<T, error> get_as() const;

private:
    class storage;

    explicit operation_result(std::shared_ptr<storage> store) noexcept;

    // What the operation returned, or not_finished or cancelled. Never
    // waits. The value lives as long as this result does and never changes.
    std::expected<const std::any*, error> held() const;

    std::shared_ptr<storage> _store;

    friend class operation_access;
};

template <operation_value_type T>
std::expected<T, error> operation_result::get_as() const
{
    const auto value = held();
    if (! value) {
        return std::unexpected(value.error());
    }

    // Taken out here, not in the library, so the type is looked up on the
    // same side of a shared library boundary as the one it was put in with.
    if constexpr (std::is_void_v<T>) {
        if ((*value)->has_value()) {
            return std::unexpected(error(operation_errc::type_mismatch));
        }
        return {};
    } else {
        const T* taken = std::any_cast<T>(*value);
        if (taken == nullptr) {
            return std::unexpected(error(operation_errc::type_mismatch));
        }
        return *taken;
    }
}

}  // namespace dross

template <>
struct std::hash<dross::operation_id> {
    std::size_t operator()(const dross::operation_id& id) const noexcept;
};
