#pragma once

#include <any>
#include <memory>
#include <string>

namespace dross {

class value;

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
 * dictionary config;
 * config["host"] = value{string{"localhost"}};
 * config["port"] = value{number{8080}};
 * config["ssl"] = value{string{"true"}};
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
 */
class dictionary::iterator final {
public:
    using value_type = std::pair<const std::string&, value&>;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type;
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
     * @return Pair containing key and value references
     */
    value_type operator*() const;
    
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
    friend class dictionary;
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
 */
class dictionary::const_iterator final {
public:
    using value_type = std::pair<const std::string&, const value&>;
    using difference_type = std::ptrdiff_t;
    using pointer = const value_type*;
    using reference = value_type;
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
     * @return Pair containing const key and value references
     */
    value_type operator*() const;
    
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
    friend class dictionary;
    const_iterator(const std::any&);

private:
    class impl;
    std::unique_ptr<impl> _impl;
};

}
