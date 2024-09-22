#pragma once

#include <memory>
#include <string>

namespace dross {

template <typename T>
concept string_type = std::same_as<T, const char*> || std::same_as<T, std::string>;

class string final {
public:
    string();
    string(const string&);
    string(const std::string&);
    string(const char*);
    ~string();

    bool starts_with(const std::string&) const;

    size_t length() const;
    bool equals(const string&) const;
    bool equals(const std::string&) const;
    bool equals(const char*) const;

    bool operator==(const string&) const;
    bool operator!=(const string&) const;

    bool operator==(const std::string&) const;
    bool operator!=(const std::string&) const;

    bool operator==(const char*) const;
    bool operator!=(const char*) const;

    string& operator=(const string&);
    string& operator=(const std::string&);
    string& operator=(const char*);

    string& operator+=(const string&);
    string& operator+=(const std::string&);
    string& operator+=(const char*);

    operator std::string() const;

private:
    class storage;
    std::unique_ptr<storage> _store;
};

}
