#pragma once

#include <any>
#include <initializer_list>
#include <memory>

namespace dross {

class value;

/**
 * @brief Dynamic array container for value objects with iterator support.
 * 
 * The array class provides a dynamic container that can hold any number of
 * value objects. It supports standard container operations like iteration,
 * indexing, and modification, making it suitable for building complex data
 * structures and APIs that need flexible arrays.
 * 
 * Key features:
 * - Dynamic sizing with automatic memory management
 * - Value semantics (copyable and assignable)
 * - Range-based for loop support through iterators
 * - Type-safe element access
 * - Efficient append and removal operations
 * - Thread-safe for read operations
 * 
 * Performance characteristics:
 * - Element access: O(1) by index
 * - Append operations: Amortized O(1)
 * - Search operations: O(n) linear scan
 * - Memory usage: Proportional to number of elements
 * 
 * Thread safety:
 * - Const operations are thread-safe
 * - Non-const operations require external synchronization
 * 
 * @code
 * // Basic usage
 * array numbers = {value{1}, value{2}, value{3}};
 * 
 * // Adding elements
 * numbers.append(value{4});
 * 
 * // Iteration
 * for (const auto& item : numbers) {
 *     // Process each value
 * }
 * 
 * // Access by index
 * value& first = numbers[0];
 * 
 * // Search and removal
 * size_t pos = numbers.index_of(value{2});
 * numbers.remove(value{2});
 * @endcode
 */
class array final {
public:
    class iterator;
    class const_iterator;

    /**
     * @brief Default constructor creating an empty array.
     */
    array();
    
    /**
     * @brief Copy constructor.
     * @param other The array to copy from
     */
    array(const array& other);
    
    /**
     * @brief Construct from initializer list.
     * @param values Initializer list of values to populate the array
     * 
     * Allows convenient array creation with brace initialization.
     */
    array(const std::initializer_list<value>& values);
    
    /**
     * @brief Destructor.
     */
    ~array();

    /**
     * @brief Check if the array is empty.
     * @return true if the array contains no elements, false otherwise
     */
    bool empty() const;
    
    /**
     * @brief Get the number of elements in the array.
     * @return The number of elements currently stored
     */
    size_t length() const;
    
    /**
     * @brief Test equality with another array.
     * @param other The array to compare with
     * @return true if both arrays contain the same elements in the same order
     */
    bool equals(const array& other) const;

    /**
     * @brief Find the index of the first occurrence of a value.
     * @param val The value to search for
     * @return The index of the first occurrence, or SIZE_MAX if not found
     */
    size_t index_of(const value& val) const;
    
    /**
     * @brief Get a reference to the value at the specified index.
     * @param index The index to access
     * @return Reference to the value at the given index
     * @throws std::out_of_range if index is out of bounds
     */
    value& value_at(const size_t index) const;

    /**
     * @brief Append a value to the end of the array.
     * @param val The value to append
     */
    void append(const value& val);
    
    /**
     * @brief Append a range of values from iterators.
     * @param first Iterator pointing to the beginning of the range
     * @param last Iterator pointing to the end of the range
     */
    void append(iterator first, iterator last);
    
    /**
     * @brief Append a range of values from const iterators.
     * @param first Const iterator pointing to the beginning of the range
     * @param last Const iterator pointing to the end of the range
     */
    void append(const_iterator first, const_iterator last);
    
    /**
     * @brief Remove the first occurrence of a value from the array.
     * @param val The value to remove
     * 
     * If the value is not found, the array remains unchanged.
     */
    void remove(const value& val);

    /**
     * @brief Get an iterator to the beginning of the array.
     * @return Iterator pointing to the first element
     */
    iterator begin();
    
    /**
     * @brief Get an iterator to the end of the array.
     * @return Iterator pointing past the last element
     */
    iterator end();
    
    /**
     * @brief Get a const iterator to the beginning of the array.
     * @return Const iterator pointing to the first element
     */
    const_iterator begin() const;
    
    /**
     * @brief Get a const iterator to the end of the array.
     * @return Const iterator pointing past the last element
     */
    const_iterator end() const;
    
    /**
     * @brief Get a const iterator to the beginning of the array.
     * @return Const iterator pointing to the first element
     */
    const_iterator cbegin() const;
    
    /**
     * @brief Get a const iterator to the end of the array.
     * @return Const iterator pointing past the last element
     */
    const_iterator cend() const;

    /**
     * @brief Equality comparison operator.
     * @param other The array to compare with
     * @return true if arrays are equal
     */
    bool operator==(const array& other) const;
    
    /**
     * @brief Inequality comparison operator.
     * @param other The array to compare with
     * @return true if arrays are not equal
     */
    bool operator!=(const array& other) const;

    /**
     * @brief Copy assignment operator.
     * @param other The array to assign from
     * @return Reference to this array
     */
    array& operator=(const array& other);

    /**
     * @brief Access element by index (const version).
     * @param index The index to access
     * @return Reference to the value at the given index
     * @throws std::out_of_range if index is out of bounds
     */
    value& operator[](const size_t index) const&;
    
    /**
     * @brief Access element by index (non-const version).
     * @param index The index to access
     * @return Reference to the value at the given index
     * @throws std::out_of_range if index is out of bounds
     */
    value& operator[](const size_t index) &;

private:
    class storage;
    std::unique_ptr<storage> _store;
};

/**
 * @brief Forward iterator for array elements.
 * 
 * Provides standard iterator interface for traversing array elements.
 * Supports range-based for loops and standard library algorithms.
 */
class array::iterator final {
public:
    using value_type = value;
    using difference_type = std::ptrdiff_t;
    using pointer = value*;
    using reference = value&;
    using iterator_category = std::forward_iterator_tag;

    /**
     * @brief Copy constructor.
     * @param other The iterator to copy from
     */
    iterator(const iterator& other);
    
    /**
     * @brief Destructor.
     */
    ~iterator();

    /**
     * @brief Pre-increment operator.
     * @return Reference to this iterator after incrementing
     */
    iterator& operator++();
    
    /**
     * @brief Dereference operator.
     * @return Reference to the current element
     */
    value& operator*();
    
    /**
     * @brief Equality comparison operator.
     * @param other The iterator to compare with
     * @return true if iterators point to the same element
     */
    bool operator==(const iterator& other) const;
    
    /**
     * @brief Inequality comparison operator.
     * @param other The iterator to compare with
     * @return true if iterators point to different elements
     */
    bool operator!=(const iterator& other) const;

private:
    friend class array;
    iterator(const std::any&);

private:
    class impl;
    std::unique_ptr<impl> _impl;
};

/**
 * @brief Const forward iterator for array elements.
 * 
 * Provides read-only iterator interface for traversing array elements.
 * Supports range-based for loops and standard library algorithms.
 */
class array::const_iterator final {
public:
    using value_type = value;
    using difference_type = std::ptrdiff_t;
    using pointer = const value*;
    using reference = const value&;
    using iterator_category = std::forward_iterator_tag;

    /**
     * @brief Copy constructor.
     * @param other The const_iterator to copy from
     */
    const_iterator(const const_iterator& other);
    
    /**
     * @brief Destructor.
     */
    ~const_iterator();

    /**
     * @brief Pre-increment operator.
     * @return Reference to this iterator after incrementing
     */
    const_iterator& operator++();
    
    /**
     * @brief Dereference operator.
     * @return Const reference to the current element
     */
    const value& operator*() const;
    
    /**
     * @brief Equality comparison operator.
     * @param other The const_iterator to compare with
     * @return true if iterators point to the same element
     */
    bool operator==(const const_iterator& other) const;
    
    /**
     * @brief Inequality comparison operator.
     * @param other The const_iterator to compare with
     * @return true if iterators point to different elements
     */
    bool operator!=(const const_iterator& other) const;

private:
    friend class array;
    const_iterator(const std::any&);

private:
    class impl;
    std::unique_ptr<impl> _impl;
};

}
