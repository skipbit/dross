#include "dross/platform/path.h"
#include "dross/platform/xdg.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <pwd.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>

namespace {

// Saves and restores a single environment variable across a test, even
// when the test exits early via an ASSERT_* failure. Restores "unset" as
// unset rather than as an empty string.
class scoped_env_var {
public:
    explicit scoped_env_var(std::string name)
        : _name(std::move(name))
    {
        if (const char* v = std::getenv(_name.c_str())) {
            _original = std::string(v);
        }
    }

    ~scoped_env_var()
    {
        if (_original) {
            setenv(_name.c_str(), _original->c_str(), 1);
        } else {
            unsetenv(_name.c_str());
        }
    }

    scoped_env_var(const scoped_env_var&) = delete;
    scoped_env_var& operator=(const scoped_env_var&) = delete;

    void set(const std::string& value) const
    {
        setenv(_name.c_str(), value.c_str(), 1);
    }

    void unset() const
    {
        unsetenv(_name.c_str());
    }

private:
    std::string _name;
    std::optional<std::string> _original;
};

}  // namespace

// --- config_home -----------------------------------------------------------

TEST(xdg_test, config_home_uses_xdg_config_home_when_set)
{
    scoped_env_var xdg_config_home("XDG_CONFIG_HOME");
    xdg_config_home.set("/tmp/dross_xdg_test/xdg_config_home");

    const dross::xdg app{ "myapp" };
    const auto result = app.config_home();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "/tmp/dross_xdg_test/xdg_config_home/myapp");
}

TEST(xdg_test, config_home_falls_back_to_home_dot_config_when_unset)
{
    scoped_env_var xdg_config_home("XDG_CONFIG_HOME");
    xdg_config_home.unset();
    scoped_env_var home("HOME");
    home.set("/tmp/dross_xdg_test/home");

    const dross::xdg app{ "myapp" };
    const auto result = app.config_home();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "/tmp/dross_xdg_test/home/.config/myapp");
}

TEST(xdg_test, config_home_falls_back_when_xdg_config_home_is_empty)
{
    scoped_env_var xdg_config_home("XDG_CONFIG_HOME");
    xdg_config_home.set("");
    scoped_env_var home("HOME");
    home.set("/tmp/dross_xdg_test/home");

    const dross::xdg app{ "myapp" };
    const auto result = app.config_home();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "/tmp/dross_xdg_test/home/.config/myapp");
}

TEST(xdg_test, config_home_falls_back_when_xdg_config_home_is_relative)
{
    scoped_env_var xdg_config_home("XDG_CONFIG_HOME");
    xdg_config_home.set("relative/xdg_config_home");
    scoped_env_var home("HOME");
    home.set("/tmp/dross_xdg_test/home");

    const dross::xdg app{ "myapp" };
    const auto result = app.config_home();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "/tmp/dross_xdg_test/home/.config/myapp");
}

TEST(xdg_test, config_home_falls_back_to_the_password_database_when_home_is_relative)
{
    scoped_env_var xdg_config_home("XDG_CONFIG_HOME");
    xdg_config_home.unset();
    scoped_env_var home("HOME");
    home.set("relative/home");
    const struct passwd* pw = getpwuid(getuid());
    if (pw == nullptr || pw->pw_dir == nullptr || ! std::filesystem::path{ pw->pw_dir }.is_absolute()) {
        GTEST_SKIP() << "the password database has no absolute home directory for this user";
    }
    const std::string pw_dir = pw->pw_dir;

    const dross::xdg app{ "myapp" };
    const auto result = app.config_home();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, dross::path{ pw_dir }.append(".config").append("myapp").string());
}

// --- data_home ---------------------------------------------------------

TEST(xdg_test, data_home_uses_xdg_data_home_when_set)
{
    scoped_env_var xdg_data_home("XDG_DATA_HOME");
    xdg_data_home.set("/tmp/dross_xdg_test/xdg_data_home");

    const dross::xdg app{ "myapp" };
    const auto result = app.data_home();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "/tmp/dross_xdg_test/xdg_data_home/myapp");
}

TEST(xdg_test, data_home_falls_back_to_home_dot_local_share_when_unset)
{
    scoped_env_var xdg_data_home("XDG_DATA_HOME");
    xdg_data_home.unset();
    scoped_env_var home("HOME");
    home.set("/tmp/dross_xdg_test/home");

    const dross::xdg app{ "myapp" };
    const auto result = app.data_home();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "/tmp/dross_xdg_test/home/.local/share/myapp");
}

TEST(xdg_test, data_home_falls_back_when_xdg_data_home_is_empty)
{
    scoped_env_var xdg_data_home("XDG_DATA_HOME");
    xdg_data_home.set("");
    scoped_env_var home("HOME");
    home.set("/tmp/dross_xdg_test/home");

    const dross::xdg app{ "myapp" };
    const auto result = app.data_home();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "/tmp/dross_xdg_test/home/.local/share/myapp");
}

TEST(xdg_test, data_home_falls_back_when_xdg_data_home_is_relative)
{
    scoped_env_var xdg_data_home("XDG_DATA_HOME");
    xdg_data_home.set("relative/xdg_data_home");
    scoped_env_var home("HOME");
    home.set("/tmp/dross_xdg_test/home");

    const dross::xdg app{ "myapp" };
    const auto result = app.data_home();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "/tmp/dross_xdg_test/home/.local/share/myapp");
}

// --- cache_home --------------------------------------------------------

TEST(xdg_test, cache_home_uses_xdg_cache_home_when_set)
{
    scoped_env_var xdg_cache_home("XDG_CACHE_HOME");
    xdg_cache_home.set("/tmp/dross_xdg_test/xdg_cache_home");

    const dross::xdg app{ "myapp" };
    const auto result = app.cache_home();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "/tmp/dross_xdg_test/xdg_cache_home/myapp");
}

TEST(xdg_test, cache_home_falls_back_to_home_dot_cache_when_unset)
{
    scoped_env_var xdg_cache_home("XDG_CACHE_HOME");
    xdg_cache_home.unset();
    scoped_env_var home("HOME");
    home.set("/tmp/dross_xdg_test/home");

    const dross::xdg app{ "myapp" };
    const auto result = app.cache_home();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "/tmp/dross_xdg_test/home/.cache/myapp");
}

TEST(xdg_test, cache_home_falls_back_when_xdg_cache_home_is_empty)
{
    scoped_env_var xdg_cache_home("XDG_CACHE_HOME");
    xdg_cache_home.set("");
    scoped_env_var home("HOME");
    home.set("/tmp/dross_xdg_test/home");

    const dross::xdg app{ "myapp" };
    const auto result = app.cache_home();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "/tmp/dross_xdg_test/home/.cache/myapp");
}

TEST(xdg_test, cache_home_falls_back_when_xdg_cache_home_is_relative)
{
    scoped_env_var xdg_cache_home("XDG_CACHE_HOME");
    xdg_cache_home.set("relative/xdg_cache_home");
    scoped_env_var home("HOME");
    home.set("/tmp/dross_xdg_test/home");

    const dross::xdg app{ "myapp" };
    const auto result = app.cache_home();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "/tmp/dross_xdg_test/home/.cache/myapp");
}

// --- state_home --------------------------------------------------------

TEST(xdg_test, state_home_uses_xdg_state_home_when_set)
{
    scoped_env_var xdg_state_home("XDG_STATE_HOME");
    xdg_state_home.set("/tmp/dross_xdg_test/xdg_state_home");

    const dross::xdg app{ "myapp" };
    const auto result = app.state_home();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "/tmp/dross_xdg_test/xdg_state_home/myapp");
}

TEST(xdg_test, state_home_falls_back_to_home_dot_local_state_when_unset)
{
    scoped_env_var xdg_state_home("XDG_STATE_HOME");
    xdg_state_home.unset();
    scoped_env_var home("HOME");
    home.set("/tmp/dross_xdg_test/home");

    const dross::xdg app{ "myapp" };
    const auto result = app.state_home();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "/tmp/dross_xdg_test/home/.local/state/myapp");
}

TEST(xdg_test, state_home_falls_back_when_xdg_state_home_is_empty)
{
    scoped_env_var xdg_state_home("XDG_STATE_HOME");
    xdg_state_home.set("");
    scoped_env_var home("HOME");
    home.set("/tmp/dross_xdg_test/home");

    const dross::xdg app{ "myapp" };
    const auto result = app.state_home();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "/tmp/dross_xdg_test/home/.local/state/myapp");
}

TEST(xdg_test, state_home_falls_back_when_xdg_state_home_is_relative)
{
    scoped_env_var xdg_state_home("XDG_STATE_HOME");
    xdg_state_home.set("relative/xdg_state_home");
    scoped_env_var home("HOME");
    home.set("/tmp/dross_xdg_test/home");

    const dross::xdg app{ "myapp" };
    const auto result = app.state_home();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "/tmp/dross_xdg_test/home/.local/state/myapp");
}
