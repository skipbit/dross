
enable_testing()

list(APPEND CMAKE_PREFIX_PATH "$ENV{HOMEBREW_PREFIX}/opt/googletest")
find_package(GTest REQUIRED)

set(TEST_NAME ${PROJECT_NAME}_test)

add_executable(${TEST_NAME})

target_sources(${TEST_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_array.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_number.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_string.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_value.cpp
)

target_compile_options(${TEST_NAME}
    PUBLIC -Wall -Wextra -Werror
    $<$<CONFIG:Debug>:-O0 -g3>
    $<$<CONFIG:Release>:-O3>
)
target_compile_features(${TEST_NAME} PUBLIC cxx_std_23)

target_include_directories(${PROJECT_NAME}
    SYSTEM BEFORE
    PRIVATE "$ENV{HOMEBREW_PREFIX}/include" "$ENV{HOMEBREW_PREFIX}/opt/gcc/include/c++/13" "$ENV{HOMEBREW_PREFIX}/opt/glibc/include"
)
target_include_directories(${TEST_NAME}
    PRIVATE ${PROJECT_SOURCE_DIR}/include
)
target_link_directories(${TEST_NAME}
    PRIVATE "$ENV{HOMEBREW_PREFIX}/lib" "$ENV{HOMEBREW_PREFIX}/opt/gcc/lib/gcc/13" "$ENV{HOMEBREW_PREFIX}/opt/glibc/lib" ${dross_BINARY_DIR}/lib
)
target_link_libraries(${TEST_NAME}
    dross
    GTest::gmock
    GTest::gmock_main
)

gtest_discover_tests(${TEST_NAME})
