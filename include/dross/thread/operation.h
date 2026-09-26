#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
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

}  // namespace dross

template <>
struct std::hash<dross::operation_id> {
    std::size_t operator()(const dross::operation_id& id) const noexcept;
};

template <>
struct std::is_error_code_enum<dross::operation_errc> : std::true_type { };
