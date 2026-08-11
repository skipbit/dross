#include <dross/type.h>

#include <numeric>
#include <ranges>
#include <string_view>

namespace dross {

std::vector<std::string> split(const std::string& s, const std::string& delimiter)
{
    std::vector<std::string> tokens;

    // Pipe over views rather than over the arguments themselves. On some of
    // the compiler and standard library pairings this project supports, a
    // const std::string is not accepted as the left operand of the pipe.
    const std::string_view sv{s};
    const std::string_view dv{delimiter};

    auto range = sv | std::views::split(dv) | std::views::transform([](auto&& p) {
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

std::string to_string(const data& d)
{
    return static_cast<std::string>(d);
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
