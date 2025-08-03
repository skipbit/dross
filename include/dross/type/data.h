#pragma once

#include <cstddef>
#include <cstdint>
#include <compare>
#include <expected>
#include <functional>
#include <initializer_list>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "error.h"

namespace dross {

/**
 * @brief Binary data container for byte storage and manipulation.
 *
 * The data class provides a robust container for binary data storage with
 * STL-compatible interface and seamless integration with the dross type system.
 * It serves as a pure binary data container without built-in encoding or I/O
 * operations, following the single responsibility principle.
 *
 * Key features:
 * - Binary data storage with efficient memory management
 * - Value semantics (copyable and assignable)
 * - STL-compatible interface and iterators
 * - Range-based for loop support
 * - Thread-safe for read operations
 * - Seamless integration with value type
 * - Direct memory access for performance-critical operations
 *
 * Performance characteristics:
 * - Data access: O(1) by index
 * - Append operations: Amortized O(1)
 * - Memory usage: Proportional to data size with minimal overhead
 * - Iterator operations: O(1) for random access
 *
 * Thread safety:
 * - Const operations are thread-safe
 * - Non-const operations require external synchronization
 *
 * Design philosophy:
 * - Pure binary data container (no encoding/decoding)
 * - No file I/O operations (use separate file classes)
 * - Focused on data storage and manipulation only
 * - Encoding/decoding handled by separate encoder/decoder classes
 *
 * @code
 * // Basic usage
 * data binary_data;
 * binary_data.append({0x48, 0x65, 0x6C, 0x6C, 0x6F}); // "Hello"
 *
 * // Construction from various sources
 * data from_string{"Hello, World!"};
 * data from_vector{std::vector<uint8_t>{0x01, 0x02, 0x03}};
 *
 * // Direct memory access
 * auto bytes_opt = binary_data.bytes();
 * if (bytes_opt) {
 *     const uint8_t* raw_bytes = *bytes_opt;
 *     size_t size = binary_data.size();
 * }
 *
 * // STL-compatible access
 * for (uint8_t byte : binary_data) {
 *     // Process each byte
 * }
 *
 * // String conversion
 * std::string text = binary_data; // Implicit conversion (UTF-8 interpretation)
 * // or explicit conversion:
 * std::string explicit_text = to_string(binary_data);
 * @endcode
 */
class data final {
public:
    class iterator;
    class const_iterator;

    /**
     * @brief Default constructor creating empty data.
     */
    data();

    /**
     * @brief Copy constructor.
     * @param other The data to copy from
     */
    data(const data& other);

    /**
     * @brief Move constructor.
     * @param other The data to move from
     */
    data(data&& other) noexcept;

    /**
     * @brief Construct from initializer list of bytes.
     * @param bytes Initializer list of uint8_t values
     */
    data(const std::initializer_list<uint8_t>& bytes);

    /**
     * @brief Construct from vector of bytes.
     * @param bytes Vector of uint8_t values to copy
     */
    data(const std::vector<uint8_t>& bytes);

    /**
     * @brief Construct from raw byte buffer.
     * @param buffer Pointer to byte data
     * @param length Number of bytes to copy
     */
    data(const uint8_t* buffer, size_t length);

    /**
     * @brief Construct from string (UTF-8 encoded).
     * @param str String to convert to byte data
     *
     * Stores the UTF-8 byte representation of the string.
     */
    data(const std::string& str);

    /**
     * @brief Construct from C-style string.
     * @param str Null-terminated string to convert
     */
    data(const char* str);

    /**
     * @brief Destructor.
     */
    ~data();

    /**
     * @brief Check if the data is empty.
     * @return true if no bytes are stored, false otherwise
     */
    bool empty() const noexcept;

    /**
     * @brief Get the number of bytes stored.
     * @return Number of bytes in the data
     */
    size_t size() const noexcept;

    /**
     * @brief Get maximum possible size.
     * @return Maximum number of bytes that can be stored
     */
    size_t max_size() const noexcept;

    /**
     * @brief Get raw pointer to bytes.
     * @return Optional pointer to first byte (nullopt if empty)
     */
    std::optional<const uint8_t*> bytes() const noexcept;

    /**
     * @brief Get mutable raw pointer to bytes.
     * @return Optional pointer to first byte (nullopt if empty)
     */
    std::optional<uint8_t*> bytes() noexcept;

    /**
     * @brief Access byte at index with bounds checking.
     * @param index Index of byte to access
     * @return Expected reference to byte at index (error if out of bounds)
     */
    std::expected<std::reference_wrapper<const uint8_t>, error> at(size_t index) const noexcept;

    /**
     * @brief Access byte at index with bounds checking.
     * @param index Index of byte to access
     * @return Expected reference to byte at index (error if out of bounds)
     */
    std::expected<std::reference_wrapper<uint8_t>, error> at(size_t index) noexcept;

    /**
     * @brief Access byte at index without bounds checking.
     * @param index Index of byte to access
     * @return Reference to byte at index
     */
    const uint8_t& operator[](size_t index) const noexcept;

    /**
     * @brief Access byte at index without bounds checking.
     * @param index Index of byte to access
     * @return Reference to byte at index
     */
    uint8_t& operator[](size_t index) noexcept;

    /**
     * @brief Append another data object.
     * @param other Data to append
     */
    void append(const data& other);

    /**
     * @brief Append raw byte buffer.
     * @param buffer Pointer to bytes to append
     * @param length Number of bytes to append
     */
    void append(const uint8_t* buffer, size_t length);

    /**
     * @brief Append initializer list of bytes.
     * @param bytes Bytes to append
     */
    void append(const std::initializer_list<uint8_t>& bytes);

    /**
     * @brief Clear all data.
     */
    void clear() noexcept;

    /**
     * @brief Resize the data container.
     * @param new_size New size in bytes
     * @param fill_value Value to use for new bytes if expanding
     */
    void resize(size_t new_size, uint8_t fill_value = 0);

    /**
     * @brief Reserve memory for at least the specified number of bytes.
     * @param capacity Number of bytes to reserve
     */
    void reserve(size_t capacity);

    /**
     * @brief Test equality with another data object.
     * @param other The data to compare with
     * @return true if both contain the same byte sequence
     */
    bool equals(const data& other) const noexcept;

    /**
     * @brief Copy assignment operator.
     * @param other The data to assign from
     * @return Reference to this data
     */
    data& operator=(const data& other);

    /**
     * @brief Move assignment operator.
     * @param other The data to move from
     * @return Reference to this data
     */
    data& operator=(data&& other) noexcept;

    /**
     * @brief Equality comparison operator.
     * @param other The data to compare with
     * @return true if data are equal
     */
    bool operator==(const data& other) const noexcept;

    /**
     * @brief Inequality comparison operator.
     * @param other The data to compare with
     * @return true if data are not equal
     */
    bool operator!=(const data& other) const noexcept;

    /**
     * @brief Three-way comparison operator.
     * @param other The data to compare with
     * @return Comparison result (lexicographical ordering)
     */
    std::strong_ordering operator<=>(const data& other) const noexcept;

    /**
     * @brief Concatenation operator.
     * @param other The data to concatenate
     * @return New data object containing concatenated bytes
     */
    data operator+(const data& other) const;

    /**
     * @brief Concatenation assignment operator.
     * @param other The data to append
     * @return Reference to this data after concatenation
     */
    data& operator+=(const data& other);

    /**
     * @brief Implicit conversion to string (UTF-8 interpretation).
     * @return String representation of the byte data
     */
    operator std::string() const;

    /**
     * @brief Implicit conversion to const uint8_t pointer.
     * @return Pointer to the underlying byte data
     *
     * Provides direct access to the raw byte data for performance-critical operations.
     * Returns nullptr if the data is empty.
     */
    operator const uint8_t*() const noexcept;

    /**
     * @brief Get iterator to beginning.
     * @return Iterator pointing to first byte
     */
    iterator begin() noexcept;

    /**
     * @brief Get iterator to end.
     * @return Iterator pointing past last byte
     */
    iterator end() noexcept;

    /**
     * @brief Get const iterator to beginning.
     * @return Const iterator pointing to first byte
     */
    const_iterator begin() const noexcept;

    /**
     * @brief Get const iterator to end.
     * @return Const iterator pointing past last byte
     */
    const_iterator end() const noexcept;

    /**
     * @brief Get const iterator to beginning.
     * @return Const iterator pointing to first byte
     */
    const_iterator cbegin() const noexcept;

    /**
     * @brief Get const iterator to end.
     * @return Const iterator pointing past last byte
     */
    const_iterator cend() const noexcept;

private:
    class storage;
    std::unique_ptr<storage> _store;
};

/**
 * @brief Random access iterator for data elements.
 *
 * Provides standard iterator interface for traversing data bytes.
 * Supports range-based for loops and standard library algorithms.
 */
class data::iterator final {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = uint8_t;
    using difference_type = std::ptrdiff_t;
    using pointer = uint8_t*;
    using reference = uint8_t&;

    iterator() : _ptr(nullptr) {}
    explicit iterator(uint8_t* ptr) : _ptr(ptr) {}

    uint8_t& operator*() const { return *_ptr; }
    uint8_t* operator->() const { return _ptr; }
    uint8_t& operator[](std::ptrdiff_t n) const { return _ptr[n]; }

    iterator& operator++() { ++_ptr; return *this; }
    iterator operator++(int) { auto tmp = *this; ++_ptr; return tmp; }
    iterator& operator--() { --_ptr; return *this; }
    iterator operator--(int) { auto tmp = *this; --_ptr; return tmp; }

    iterator& operator+=(std::ptrdiff_t n) { _ptr += n; return *this; }
    iterator& operator-=(std::ptrdiff_t n) { _ptr -= n; return *this; }

    iterator operator+(std::ptrdiff_t n) const { return iterator(_ptr + n); }
    iterator operator-(std::ptrdiff_t n) const { return iterator(_ptr - n); }
    std::ptrdiff_t operator-(const iterator& other) const { return _ptr - other._ptr; }

    bool operator==(const iterator& other) const noexcept { return _ptr == other._ptr; }
    bool operator!=(const iterator& other) const noexcept { return _ptr != other._ptr; }
    bool operator<(const iterator& other) const noexcept { return _ptr < other._ptr; }
    bool operator<=(const iterator& other) const noexcept { return _ptr <= other._ptr; }
    bool operator>(const iterator& other) const noexcept { return _ptr > other._ptr; }
    bool operator>=(const iterator& other) const noexcept { return _ptr >= other._ptr; }

private:
    uint8_t* _ptr;
};

/**
 * @brief Const random access iterator for data elements.
 *
 * Provides read-only iterator interface for traversing data bytes.
 * Supports range-based for loops and standard library algorithms.
 */
class data::const_iterator final {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = uint8_t;
    using difference_type = std::ptrdiff_t;
    using pointer = const uint8_t*;
    using reference = const uint8_t&;

    const_iterator() : _ptr(nullptr) {}
    explicit const_iterator(const uint8_t* ptr) : _ptr(ptr) {}
    const_iterator(const iterator& iter) : _ptr(&(*iter)) {}

    const uint8_t& operator*() const { return *_ptr; }
    const uint8_t* operator->() const { return _ptr; }
    const uint8_t& operator[](std::ptrdiff_t n) const { return _ptr[n]; }

    const_iterator& operator++() { ++_ptr; return *this; }
    const_iterator operator++(int) { auto tmp = *this; ++_ptr; return tmp; }
    const_iterator& operator--() { --_ptr; return *this; }
    const_iterator operator--(int) { auto tmp = *this; --_ptr; return tmp; }

    const_iterator& operator+=(std::ptrdiff_t n) { _ptr += n; return *this; }
    const_iterator& operator-=(std::ptrdiff_t n) { _ptr -= n; return *this; }

    const_iterator operator+(std::ptrdiff_t n) const { return const_iterator(_ptr + n); }
    const_iterator operator-(std::ptrdiff_t n) const { return const_iterator(_ptr - n); }
    std::ptrdiff_t operator-(const const_iterator& other) const { return _ptr - other._ptr; }

    bool operator==(const const_iterator& other) const noexcept { return _ptr == other._ptr; }
    bool operator!=(const const_iterator& other) const noexcept { return _ptr != other._ptr; }
    bool operator<(const const_iterator& other) const noexcept { return _ptr < other._ptr; }
    bool operator<=(const const_iterator& other) const noexcept { return _ptr <= other._ptr; }
    bool operator>(const const_iterator& other) const noexcept { return _ptr > other._ptr; }
    bool operator>=(const const_iterator& other) const noexcept { return _ptr >= other._ptr; }

private:
    const uint8_t* _ptr;
};

/**
 * @brief Stream output operator for data.
 * @param os The output stream
 * @param d The data to output
 * @return Reference to the output stream
 *
 * Outputs the data as UTF-8 string to the stream.
 */
std::ostream& operator<<(std::ostream& os, const data& d);

}
