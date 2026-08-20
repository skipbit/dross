#include <gtest/gtest.h>

#include "dross/platform/path.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unistd.h>

namespace {

// Process-unique scratch directory (CWE-377: never a predictable fixed
// name). Removes itself and everything created inside it on destruction,
// including when a test exits early via an ASSERT_* failure: ASSERT_*
// returns from the test function rather than throwing, so the stack still
// unwinds normally and the destructor still runs.
class scoped_temp_dir {
public:
    scoped_temp_dir()
        : _path(std::filesystem::temp_directory_path()
              / ("dross_path_test_" + std::to_string(::getpid()) + "_" + std::to_string(_next_id++)))
    {
        std::filesystem::create_directories(_path);
    }

    ~scoped_temp_dir()
    {
        std::error_code ignored;
        std::filesystem::remove_all(_path, ignored);
    }

    scoped_temp_dir(const scoped_temp_dir&) = delete;
    scoped_temp_dir& operator=(const scoped_temp_dir&) = delete;

    const std::filesystem::path& path() const { return _path; }

private:
    std::filesystem::path _path;
    static inline unsigned _next_id = 0;
};

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

private:
    std::string _name;
    std::optional<std::string> _original;
};

void write_regular_file(const std::filesystem::path& p)
{
    std::ofstream out(p);
    out << "content";
}

}

// --- path::mkdir -------------------------------------------------------

TEST(path_test, mkdir_creates_a_new_directory)
{
    const scoped_temp_dir base;
    const auto target = base.path() / "created";

    const auto result = dross::path::mkdir(target.string());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->string(), target.string());
    EXPECT_TRUE(std::filesystem::is_directory(target));
}

TEST(path_test, mkdir_creates_nested_parent_directories)
{
    const scoped_temp_dir base;
    const auto target = base.path() / "a" / "b" / "c";

    const auto result = dross::path::mkdir(target.string());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::is_directory(target));
}

TEST(path_test, mkdir_on_an_existing_directory_succeeds)
{
    const scoped_temp_dir base;

    const auto result = dross::path::mkdir(base.path().string());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->string(), base.path().string());
}

TEST(path_test, mkdir_called_twice_on_the_same_directory_succeeds_both_times)
{
    const scoped_temp_dir base;
    const auto target = base.path() / "repeat";

    const auto first = dross::path::mkdir(target.string());
    const auto second = dross::path::mkdir(target.string());

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->string(), target.string());
}

TEST(path_test, mkdir_fails_when_a_path_component_is_a_regular_file)
{
    // The true-failure case is exercised through ENOTDIR (a parent path
    // component that is a regular file) rather than through a permission
    // failure, so the assertion still holds when the suite runs as root,
    // where permission checks are bypassed.
    const scoped_temp_dir base;
    const auto blocking_file = base.path() / "not_a_directory";
    write_regular_file(blocking_file);
    const auto target = blocking_file / "child";

    const auto result = dross::path::mkdir(target.string());

    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().code().value(), 0);
}

TEST(path_test, mkdir_target_is_an_existing_regular_file)
{
    // Boundary case: the target itself already exists but is not a
    // directory. This must not be folded into the "existing directory"
    // success case.
    const scoped_temp_dir base;
    const auto target = base.path() / "already_a_file";
    write_regular_file(target);

    const auto result = dross::path::mkdir(target.string());

    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().code().value(), 0);
}

TEST(path_test, mkdir_with_an_empty_path)
{
    // Boundary case: an empty path must not be silently accepted as an
    // "already exists" success.
    const auto result = dross::path::mkdir(std::string{});

    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().code().value(), 0);
}

// --- path::expand --------------------------------------------------------

TEST(path_test, expand_of_a_tilde_path_matches_a_freshly_computed_canonical_path)
{
    const dross::path p{std::string{"~"}};

    const auto result = p.expand();

    ASSERT_TRUE(result.has_value());
    const auto home = dross::path::home();
    ASSERT_TRUE(home.has_value());
    const auto expected = std::filesystem::canonical(std::filesystem::path{home->string()});
    EXPECT_EQ(result->string(), expected.string());
}

TEST(path_test, expand_of_a_tilde_path_resolves_through_a_symlinked_home)
{
    const scoped_temp_dir base;
    const auto real_home = base.path() / "real_home";
    std::filesystem::create_directories(real_home);
    const auto home_link = base.path() / "home_link";
    std::filesystem::create_directory_symlink(real_home, home_link);

    scoped_env_var home("HOME");
    home.set(home_link.string());

    const dross::path p{std::string{"~"}};
    const auto result = p.expand();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->string(), std::filesystem::canonical(real_home).string());
}

TEST(path_test, expand_of_a_tilde_path_to_a_nonexistent_target_does_not_terminate)
{
    // Regression test: std::filesystem::canonical() throws on a missing
    // path. expand() must report that as std::unexpected instead of
    // letting the exception escape, which previously terminated the
    // process.
    const std::string tilde_path = "~/dross_test_nonexistent_" + std::to_string(::getpid());
    const dross::path p{tilde_path};

    const auto result = p.expand();

    EXPECT_FALSE(result.has_value());
}

TEST(path_test, expand_of_a_non_tilde_path_is_returned_unchanged)
{
    const std::string original = "relative/does/not/exist";
    const dross::path p{original};

    const auto result = p.expand();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->string(), original);
}

// --- path::resolve ---------------------------------------------------------

TEST(path_test, resolve_of_an_existing_path_succeeds_in_canonical_form)
{
    const scoped_temp_dir base;
    const dross::path p{base.path()};

    const auto result = p.resolve();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->string(), std::filesystem::canonical(base.path()).string());
}

TEST(path_test, resolve_of_a_missing_path_returns_unexpected)
{
    const scoped_temp_dir base;
    const dross::path p{base.path() / "does_not_exist"};

    const auto result = p.resolve();

    EXPECT_FALSE(result.has_value());
}

// --- path::exists / default constructor (pinning current, unmodified behavior) ---

TEST(path_test, exists_is_false_for_a_missing_path)
{
    const scoped_temp_dir base;
    const dross::path p{base.path() / "does_not_exist"};

    EXPECT_FALSE(p.exists());
}

TEST(path_test, exists_is_true_for_a_present_path)
{
    const scoped_temp_dir base;
    const dross::path p{base.path()};

    EXPECT_TRUE(p.exists());
}

TEST(path_test, default_constructor_refers_to_the_current_working_directory)
{
    const dross::path p;

    EXPECT_TRUE(std::filesystem::equivalent(std::filesystem::path(p), std::filesystem::current_path()));
}
