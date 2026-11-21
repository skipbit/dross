#pragma once

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

namespace dross_test {

// A directory of its own under /tmp, removed at the end of the test. A
// socket path has to fit in about a hundred bytes, which the usual
// temporary directory on macOS does not leave room for.
class socket_directory_test : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::string pattern = "/tmp/dross-XXXXXX";
        ASSERT_NE(::mkdtemp(pattern.data()), nullptr);
        _directory = pattern;
    }

    void TearDown() override
    {
        std::error_code ignored;
        std::filesystem::remove_all(_directory, ignored);
    }

    std::string at(const std::string& name) const
    {
        return (_directory / name).string();
    }

private:
    std::filesystem::path _directory;
};

}  // namespace dross_test
