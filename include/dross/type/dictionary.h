#pragma once

#include "dross/type/value.h"

#include <any>
#include <memory>
#include <string>

namespace dross {

/**
 * @brief Key-value container for string keys and value objects.
 *
 * The dictionary class provides an associative container that maps string keys
 * to value objects. It supports standard dictionary operations like key lookup,
 * insertion, and iteration, making it suitable for configuration data, JSON-like
 * structures, and dynamic object representations.
 *
 * Key features:
 * - String-to-value mapping with efficient lookup
 * - Value semantics (copyable and assignable)
 * - Iterator support for key-value traversal
 * - Dynamic sizing with automatic memory management
 * - Type-safe element access
 * - Thread-safe for read operations
 *
 * Performance characteristics:
 * - Key lookup: O(log n) or better depending on implementation
 * - Insertion: O(log n) or better
 * - Iteration: O(n) to visit all elements
 * - Memory usage: Proportional to number of key-value pairs
 *
 * Thread safety:
 * - Const operations are thread-safe
 * - Non-const operations require external synchronization
 *
 * @code
 * // Basic usage
 * dross::dictionary config;
 * config["host"] = dross::value{dross::string{"localhost"}};
 * config["port"] = dross::value{dross::number{8080}};
 * config["ssl"] = dross::value{dross::string{"true"}};
 *
 * // Key checking and access
 * if (config.contains("timeout")) {
 *     auto timeout = config["timeout"];
 * }
 *
 * // Iteration
 * for (const auto& [key, val] : config) {
 *     // Process each key-value pair
 * }
 *
 * // Size checking
 * if (!config.empty()) {
 *     size_t count = config.size();
 * }
 * @endcode
 */
class dictionary final {
public:
    /**
     * @brief Default constructor creating an empty dictionary.
     */
    dictionary();

    /**
     * @brief Copy constructor.
     * @param other The dictionary to copy from
     */
    dictionary(const dictionary& other);

    /**
     * @brief Destructor.
     */
    ~dictionary();

    /**
     * @brief Test equality with another dictionary.
     * @param other The dictionary to compare with
     * @return true if both dictionaries contain the same key-value pairs
     */
    bool equals(const dictionary& other) const;

    /**
     * @brief Check if a key exists in the dictionary.
     * @param key The key to search for
     * @return true if the key exists, false otherwise
     */
    bool contains(const std::string& key) const;

    /**
     * @brief Check if the dictionary is empty.
     * @return true if the dictionary contains no key-value pairs
     */
    bool empty() const;

    /**
     * @brief Get the number of key-value pairs in the dictionary.
     * @return The number of entries currently stored
     */
    size_t size() const;

    /**
     * @brief Equality comparison operator.
     * @param other The dictionary to compare with
     * @return true if dictionaries are equal
     */
    bool operator==(const dictionary& other) const;

    /**
     * @brief Inequality comparison operator.
     * @param other The dictionary to compare with
     * @return true if dictionaries are not equal
     */
    bool operator!=(const dictionary& other) const;

    /**
     * @brief Copy assignment operator.
     * @param other The dictionary to assign from
     * @return Reference to this dictionary
     */
    dictionary& operator=(const dictionary& other);

    /**
     * @brief Access or create a value by key (non-const version).
     * @param key The key to access or create
     * @return Reference to the value associated with the key
     *
     * If the key doesn't exist, a new entry is created with a default value.
     */
    value& operator[](const std::string& key);

    /**
     * @brief Access a value by key (const version).
     * @param key The key to access
     * @return Const reference to the value associated with the key
     * @throws std::out_of_range if the key doesn't exist
     */
    const value& operator[](const std::string& key) const;

    class iterator;
    class const_iterator;

    /**
     * @brief Get an iterator to the beginning of the dictionary.
     * @return Iterator pointing to the first key-value pair
     */
    iterator begin();

    /**
     * @brief Get an iterator to the end of the dictionary.
     * @return Iterator pointing past the last key-value pair
     */
    iterator end();

    /**
     * @brief Get a const iterator to the beginning of the dictionary.
     * @return Const iterator pointing to the first key-value pair
     */
    const_iterator begin() const;

    /**
     * @brief Get a const iterator to the end of the dictionary.
     * @return Const iterator pointing past the last key-value pair
     */
    const_iterator end() const;

    /**
     * @brief Get a const iterator to the beginning of the dictionary.
     * @return Const iterator pointing to the first key-value pair
     */
    const_iterator cbegin() const;

    /**
     * @brief Get a const iterator to the end of the dictionary.
     * @return Const iterator pointing past the last key-value pair
     */
    const_iterator cend() const;

private:
    class storage;
    std::unique_ptr<storage> _store;
};

/**
 * @brief Forward iterator for dictionary key-value pairs.
 *
 * Provides iterator interface for traversing dictionary entries.
 * Each dereferenced iterator returns a pair of key and value references.
 * value_type is a pair of copies, so an element kept as a value_type, such
 * as through std::iter_value_t, does not change with the dictionary.
 *
 * The pair is returned by value, so the iterator models std::forward_iterator
 * but declares std::input_iterator_tag as its iterator_category, and has no
 * pointer type, as the standard range adaptors do.
 */
class dictionary::iterator final {
public:
    using value_type = std::pair<std::string, value>;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = std::pair<const std::string&, value&>;
    using iterator_concept = std::forward_iterator_tag;
    using iterator_category = std::input_iterator_tag;

    /**
     * @brief Default constructor. Default-constructed iterators compare equal.
     */
    iterator();

    /**
     * @brief Copy constructor.
     * @param other The iterator to copy from
     */
    iterator(const iterator& other);

    /**
     * @brief Copy assignment operator.
     * @param other The iterator to copy from
     * @return Reference to this iterator
     */
    iterator& operator=(const iterator& other);

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
     * @brief Post-increment operator.
     * @return Copy of this iterator before incrementing
     */
    iterator operator++(int);

    /**
     * @brief Dereference operator.
     * @return Pair containing key and value references
     */
    reference operator*() const;

    /**
     * @brief Equality comparison operator. != is derived from it.
     * @param lhs The iterator to compare
     * @param rhs The iterator to compare with
     * @return true if iterators point to the same element
     */
    friend bool operator==(const iterator& lhs, const iterator& rhs);

private:
    friend class dictionary;
    friend class const_iterator;
    iterator(const std::any&);

private:
    class impl;
    std::unique_ptr<impl> _impl;
};

/**
 * @brief Const forward iterator for dictionary key-value pairs.
 *
 * Provides read-only iterator interface for traversing dictionary entries.
 * Each dereferenced iterator returns a pair of const key and value references.
 * value_type is a pair of copies, the same as for iterator.
 *
 * The pair is returned by value, so the iterator models std::forward_iterator
 * but declares std::input_iterator_tag as its iterator_category, and has no
 * pointer type, as the standard range adaptors do.
 */
class dictionary::const_iterator final {
public:
    using value_type = std::pair<std::string, value>;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = std::pair<const std::string&, const value&>;
    using iterator_concept = std::forward_iterator_tag;
    using iterator_category = std::input_iterator_tag;

    /**
     * @brief Default constructor. Default-constructed iterators compare equal.
     */
    const_iterator();

    /**
     * @brief Copy constructor.
     * @param other The const_iterator to copy from
     */
    const_iterator(const const_iterator& other);

    /**
     * @brief Converting constructor from a mutable iterator.
     * @param other The iterator to convert, pointing to the same element
     */
    const_iterator(const iterator& other);

    /**
     * @brief Copy assignment operator.
     * @param other The const_iterator to copy from
     * @return Reference to this iterator
     */
    const_iterator& operator=(const const_iterator& other);

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
     * @brief Post-increment operator.
     * @return Copy of this iterator before incrementing
     */
    const_iterator operator++(int);

    /**
     * @brief Dereference operator.
     * @return Pair containing const key and value references
     */
    reference operator*() const;

    /**
     * @brief Equality comparison operator.
     *
     * Either side can be an iterator, which converts to a const_iterator.
     * != is derived from it.
     *
     * @param lhs The const_iterator to compare
     * @param rhs The const_iterator to compare with
     * @return true if iterators point to the same element
     */
    friend bool operator==(const const_iterator& lhs, const const_iterator& rhs);

private:
    friend class dictionary;
    const_iterator(const std::any&);

private:
    class impl;
    std::unique_ptr<impl> _impl;
};

}  // namespace dross
