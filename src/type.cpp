#include <dross/type.h>

#include <ranges>

namespace dross {

std::vector<std::string> split(const std::string& s, const char& delimiter)
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

}
