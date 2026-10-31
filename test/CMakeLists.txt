
# Use FetchContent to get GoogleTest if not found in system
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.18.0
    )
    # GoogleTest is fetched for this project's own tests. Installing dross
    # must not carry it into the user's prefix.
    set(INSTALL_GTEST OFF)
    FetchContent_MakeAvailable(googletest)
endif()

function(dross_add_test_executable name)
    add_executable(${name} ${ARGN})
    target_compile_options(${name}
        PUBLIC -Wall -Wextra -Werror
        $<$<CONFIG:Debug>:-O0 -g3>
        $<$<CONFIG:Release>:-O3>
    )
    target_compile_features(${name} PUBLIC cxx_std_23)
    target_link_libraries(${name}
        dross
        GTest::gmock_main
    )
    gtest_discover_tests(${name})
endfunction()

set(TEST_NAME ${PROJECT_NAME}_test)

dross_add_test_executable(${TEST_NAME})

target_sources(${TEST_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/test_environment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/test_path.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/test_xdg.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_array.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_boolean.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_data.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_dictionary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_dictionary_header.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_error.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_locale_independence.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_number.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_string.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_timestamp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_timezone.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_to_string.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_value.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_type.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/format/test_toml.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/test_operation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/test_operation_queue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/test_runloop.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/test_thread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/test_timer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/test_timer_schedule.cpp
    # Hidden in the shared library, and used by the internal headers the
    # tests include.
    ${PROJECT_SOURCE_DIR}/src/thread/deadline.cpp
)

target_include_directories(${TEST_NAME}
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src
)

# Replaces operator new to count allocations, which would reach every test in
# the executable above. A runtime that defines operator new and delete itself,
# such as ThreadSanitizer's with Clang, keeps the replacement from linking, so
# the test is built only where a program replacing the same functions links.
include(CheckCXXSourceCompiles)
check_cxx_source_compiles([[
#include <cstdlib>
#include <new>

void* operator new(std::size_t size) { return std::malloc(size); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return std::malloc(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }

int main()
{
    delete new int(0);
}
]] DROSS_OPERATOR_NEW_REPLACEABLE)
if(DROSS_OPERATOR_NEW_REPLACEABLE)
    dross_add_test_executable(${PROJECT_NAME}_allocation_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test_allocations.cpp
    )
endif()

# --- CMake module tests (run in script mode) ---
#
# The version is resolved at configure time, so the logic behind it cannot be
# reached from a C++ test.
add_test(NAME ProjectVersionTest
    COMMAND ${CMAKE_COMMAND}
        -D "SCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/version-test"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ProjectVersionTest.cmake
)
