
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
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_value_traits.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/test_type.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/format/test_cbor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/format/test_toml.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc/test_accept_pause.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc/test_backoff.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc/test_error.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc/test_frame.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc/test_message_place.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc/test_outbox.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc/test_part_calls.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc/test_poller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc/test_unix_place.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc/test_unix_socket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc/test_unix_socket_calls.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc/test_unix_transport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/file_local_peer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/test_connector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/test_broadcaster.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/test_notification.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/test_observable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/test_own_place.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/test_published_broadcaster.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/test_publisher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/test_remote_listener.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/test_subscriber_table.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/test_subscription.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/test_wire.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/test_operation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/test_operation_queue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/test_runloop.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/test_thread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/test_timer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/test_timer_schedule.cpp
    # Hidden in the shared library, and used by the internal headers the
    # tests include.
    ${PROJECT_SOURCE_DIR}/src/format/cbor.cpp
    ${PROJECT_SOURCE_DIR}/src/ipc/frame.cpp
    ${PROJECT_SOURCE_DIR}/src/ipc/outbox.cpp
    ${PROJECT_SOURCE_DIR}/src/ipc/part_calls.cpp
    ${PROJECT_SOURCE_DIR}/src/ipc/unix_socket.cpp
    ${PROJECT_SOURCE_DIR}/src/ipc/unix_transport.cpp
    ${PROJECT_SOURCE_DIR}/src/notification/connector.cpp
    ${PROJECT_SOURCE_DIR}/src/notification/publisher.cpp
    ${PROJECT_SOURCE_DIR}/src/notification/subscriber_table.cpp
    ${PROJECT_SOURCE_DIR}/src/notification/wire.cpp
    ${PROJECT_SOURCE_DIR}/src/thread/deadline.cpp
    ${DROSS_PLATFORM_SOURCES}
)

target_include_directories(${TEST_NAME}
    PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}
)

# The gRPC part, built only when DROSS_WITH_GRPC is on.
if(DROSS_WITH_GRPC)
    dross_add_test_executable(${PROJECT_NAME}_grpc_test
        ${CMAKE_CURRENT_SOURCE_DIR}/ipc/grpc/test_grpc_place.cpp
        # Hidden in the shared libraries, and used by the internal headers
        # the tests include.
        ${PROJECT_SOURCE_DIR}/src/ipc/part_calls.cpp
        ${PROJECT_SOURCE_DIR}/src/ipc/grpc/grpc_acceptor.cpp
        ${PROJECT_SOURCE_DIR}/src/ipc/grpc/grpc_connector.cpp
        ${PROJECT_SOURCE_DIR}/src/ipc/grpc/reaction_scope.cpp
        ${PROJECT_SOURCE_DIR}/src/ipc/grpc/stream_state.cpp
    )
    target_link_libraries(${PROJECT_NAME}_grpc_test dross_grpc)
    target_include_directories(${PROJECT_NAME}_grpc_test
        PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}
        $<TARGET_PROPERTY:dross_grpc,BINARY_DIR>
    )
endif()

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
