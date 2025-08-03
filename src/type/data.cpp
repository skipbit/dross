#include "dross/type/data.h"
#include "dross/type/error.h"

#include <system_error>
#include <vector>

namespace dross {

// Internal storage class using Pimpl idiom
class data::storage {
public:
    std::vector<uint8_t> bytes;

    storage() = default;

    explicit storage(const std::vector<uint8_t>& b) : bytes(b) {}

    explicit storage(std::vector<uint8_t>&& b) : bytes(std::move(b)) {}

    storage(const uint8_t* buffer, size_t length)
        : bytes(buffer, buffer + length) {}

    storage(const std::string& str)
        : bytes(str.begin(), str.end()) {}
};


// Constructors and destructor
data::data() : _store(std::make_unique<storage>()) {}

data::data(const data& other) : _store(std::make_unique<storage>(*other._store)) {}

data::data(data&& other) noexcept : _store(std::move(other._store)) {
    if (!_store) {
        _store = std::make_unique<storage>();
    }
}

data::data(const std::initializer_list<uint8_t>& bytes)
    : _store(std::make_unique<storage>(std::vector<uint8_t>(bytes))) {}

data::data(const std::vector<uint8_t>& bytes)
    : _store(std::make_unique<storage>(bytes)) {}

data::data(const uint8_t* buffer, size_t length)
    : _store(std::make_unique<storage>(buffer, length)) {}

data::data(const std::string& str)
    : _store(std::make_unique<storage>(str)) {}

data::data(const char* str)
    : _store(std::make_unique<storage>(std::string(str))) {}

data::~data() = default;

// Basic properties
bool data::empty() const noexcept {
    return _store->bytes.empty();
}

size_t data::size() const noexcept {
    return _store->bytes.size();
}

size_t data::max_size() const noexcept {
    return _store->bytes.max_size();
}

// Data access
std::optional<const uint8_t*> data::bytes() const noexcept {
    return _store->bytes.empty() ? std::nullopt : std::make_optional(_store->bytes.data());
}

std::optional<uint8_t*> data::bytes() noexcept {
    return _store->bytes.empty() ? std::nullopt : std::make_optional(_store->bytes.data());
}

std::expected<std::reference_wrapper<const uint8_t>, error> data::at(size_t index) const noexcept {
    if (index >= _store->bytes.size()) {
        auto ec = std::make_error_code(std::errc::result_out_of_range);
        return std::unexpected(error(ec.value(), ec.category()));
    }
    return std::cref(_store->bytes[index]);
}

std::expected<std::reference_wrapper<uint8_t>, error> data::at(size_t index) noexcept {
    if (index >= _store->bytes.size()) {
        auto ec = std::make_error_code(std::errc::result_out_of_range);
        return std::unexpected(error(ec.value(), ec.category()));
    }
    return std::ref(_store->bytes[index]);
}

const uint8_t& data::operator[](size_t index) const noexcept {
    return _store->bytes[index];
}

uint8_t& data::operator[](size_t index) noexcept {
    return _store->bytes[index];
}

// Modifiers

void data::append(const data& other) {
    _store->bytes.insert(_store->bytes.end(),
                        other._store->bytes.begin(),
                        other._store->bytes.end());
}

void data::append(const uint8_t* buffer, size_t length) {
    _store->bytes.insert(_store->bytes.end(), buffer, buffer + length);
}

void data::append(const std::initializer_list<uint8_t>& bytes) {
    _store->bytes.insert(_store->bytes.end(), bytes.begin(), bytes.end());
}

void data::clear() noexcept {
    _store->bytes.clear();
}

void data::resize(size_t new_size, uint8_t fill_value) {
    _store->bytes.resize(new_size, fill_value);
}

void data::reserve(size_t capacity) {
    _store->bytes.reserve(capacity);
}


// Comparison
bool data::equals(const data& other) const noexcept {
    return _store->bytes == other._store->bytes;
}

// Assignment operators
data& data::operator=(const data& other) {
    if (this != &other) {
        _store = std::make_unique<storage>(*other._store);
    }
    return *this;
}

data& data::operator=(data&& other) noexcept {
    if (this != &other) {
        _store = std::move(other._store);
        if (!_store) {
            _store = std::make_unique<storage>();
        }
    }
    return *this;
}

// Comparison operators
bool data::operator==(const data& other) const noexcept {
    return equals(other);
}

bool data::operator!=(const data& other) const noexcept {
    return !equals(other);
}

std::strong_ordering data::operator<=>(const data& other) const noexcept {
    return _store->bytes <=> other._store->bytes;
}

// Arithmetic operators
data data::operator+(const data& other) const {
    data result(*this);
    result.append(other);
    return result;
}

data& data::operator+=(const data& other) {
    append(other);
    return *this;
}

// String conversion
data::operator std::string() const {
    return std::string(_store->bytes.begin(), _store->bytes.end());
}

// Bytes operator
data::operator const uint8_t*() const noexcept {
    auto opt_bytes = bytes();
    return opt_bytes ? *opt_bytes : nullptr;
}

// Iterator methods
data::iterator data::begin() noexcept {
    return iterator(_store->bytes.empty() ? nullptr : _store->bytes.data());
}

data::iterator data::end() noexcept {
    return iterator(_store->bytes.empty() ? nullptr : _store->bytes.data() + _store->bytes.size());
}

data::const_iterator data::begin() const noexcept {
    return const_iterator(_store->bytes.empty() ? nullptr : _store->bytes.data());
}

data::const_iterator data::end() const noexcept {
    return const_iterator(_store->bytes.empty() ? nullptr : _store->bytes.data() + _store->bytes.size());
}

data::const_iterator data::cbegin() const noexcept {
    return begin();
}

data::const_iterator data::cend() const noexcept {
    return end();
}

// Stream output operator
std::ostream& operator<<(std::ostream& os, const data& d) {
    return os << static_cast<std::string>(d);
}

}
