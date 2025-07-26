#include <dross/type.h>

#include <numeric>
#include <ranges>

namespace dross {

std::vector<std::string> split(const std::string& s, const std::string& delimiter)
{
    std::vector<std::string> tokens;

    auto range = s | std::views::split(delimiter) | std::views::transform([](auto&& p) {
        return std::string(p.begin(), p.end());
    });

    for (const auto& token : range) {
        tokens.push_back(token);
    }

    return tokens;
}

std::string join(const std::vector<std::string>& c, const std::string& delimiter)
{
    if (c.empty()) {
        return "";
    }

    return std::accumulate(std::next(c.begin()), c.end(), c.front(), [&delimiter](const std::string& a, const std::string& b) {
        return a + delimiter + b;
    });
}

std::string to_string(const boolean& b)
{
    return static_cast<std::string>(b);
}

std::string to_string(const number& n)
{
    return static_cast<std::string>(n);
}

std::string to_string(const string& s)
{
    return static_cast<std::string>(s);
}

std::string to_string(const timestamp& ts)
{
    return static_cast<std::string>(ts);
}

std::string to_string(const timezone& tz)
{
    return static_cast<std::string>(tz);
}

}
