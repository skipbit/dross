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

}
