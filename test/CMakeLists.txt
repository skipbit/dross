
enable_testing()

# Use FetchContent to get GoogleTest if not found in system
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
    )
    FetchContent_MakeAvailable(googletest)
endif()

set(TEST_NAME ${PROJECT_NAME}_test)

add_executable(${TEST_NAME})

target_sources(${TEST_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/test_environment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/test_path.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/test_xdg.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_array.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_boolean.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_data.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_dictionary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_error.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_number.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_string.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_timestamp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_timezone.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_to_string.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_value.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_type.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/format/test_toml.cpp
)

target_compile_options(${TEST_NAME}
    PUBLIC -Wall -Wextra -Werror
    $<$<CONFIG:Debug>:-O0 -g3>
    $<$<CONFIG:Release>:-O3>
)
target_compile_features(${TEST_NAME} PUBLIC cxx_std_23)

target_include_directories(${TEST_NAME}
    PRIVATE ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(${TEST_NAME}
    dross
    GTest::gmock_main
)

gtest_discover_tests(${TEST_NAME})
