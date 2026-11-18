
# Include necessary modules
include(GNUInstallDirs)

# The thread module runs on std::thread, which needs the platform's threading
# support linked in. PUBLIC, because a consumer linking libdross statically has
# to link the thread library itself; see drossConfig.cmake.in and dross.pc.in.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# What differs between platforms lives in files named for the platform, and
# the build picks them here; the sources themselves do not test the platform.
# The tests compile the hidden ones too, so the list reaches the parent scope.
if (APPLE)
    set(DROSS_PLATFORM_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/notification/poller_kqueue.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/notification/unix_socket_macos.cpp
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(DROSS_PLATFORM_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/notification/poller_epoll.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/notification/unix_socket_linux.cpp
    )
else()
    message(FATAL_ERROR "dross builds for Linux and macOS")
endif()
set(DROSS_PLATFORM_SOURCES ${DROSS_PLATFORM_SOURCES} PARENT_SCOPE)

option(BUILD_SHARED_LIBS "build dross as a shared library" ON)
if (BUILD_SHARED_LIBS)
    add_library(${PROJECT_NAME} SHARED)
else()
    add_library(${PROJECT_NAME} STATIC)
endif()

target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/environment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/path.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/xdg.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/array.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/boolean.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/data.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/dictionary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/error.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/number.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/string.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/timestamp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/timezone.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/value.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/value_traits.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/format/cbor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/format/toml.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/broadcaster.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/delivery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/error.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/listener.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/notification.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/observable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/subscription.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/topic_address.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/notification/unix_socket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/deadline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/native.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/operation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/operation_queue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/runloop.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/thread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/time_source.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread/timer.cpp
    ${DROSS_PLATFORM_SOURCES}
)

target_link_libraries(${PROJECT_NAME} PUBLIC Threads::Threads)

# PRIVATE keeps these out of the exported target: a consumer decides its own
# warning policy and optimization level.
target_compile_options(${PROJECT_NAME}
    PRIVATE -Wall -Wextra -Werror
    $<$<CONFIG:Debug>:-O0 -g3>
    $<$<CONFIG:Release>:-O3>
)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

target_include_directories(${PROJECT_NAME}
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

set(DROSS_HEADERS
    ${PROJECT_SOURCE_DIR}/include/dross/dross.h
    ${PROJECT_SOURCE_DIR}/include/dross/platform.h
    ${PROJECT_SOURCE_DIR}/include/dross/platform/environment.h
    ${PROJECT_SOURCE_DIR}/include/dross/platform/path.h
    ${PROJECT_SOURCE_DIR}/include/dross/platform/xdg.h
    ${PROJECT_SOURCE_DIR}/include/dross/type.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/array.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/boolean.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/data.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/dictionary.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/error.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/number.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/string.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/timestamp.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/timezone.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/value.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/value_traits.h
    ${PROJECT_SOURCE_DIR}/include/dross/format.h
    ${PROJECT_SOURCE_DIR}/include/dross/format/toml.h
    ${PROJECT_SOURCE_DIR}/include/dross/notification.h
    ${PROJECT_SOURCE_DIR}/include/dross/notification/broadcaster.h
    ${PROJECT_SOURCE_DIR}/include/dross/notification/delivery.h
    ${PROJECT_SOURCE_DIR}/include/dross/notification/error.h
    ${PROJECT_SOURCE_DIR}/include/dross/notification/listener.h
    ${PROJECT_SOURCE_DIR}/include/dross/notification/notification.h
    ${PROJECT_SOURCE_DIR}/include/dross/notification/subscription.h
    ${PROJECT_SOURCE_DIR}/include/dross/notification/topic.h
    ${PROJECT_SOURCE_DIR}/include/dross/thread.h
    ${PROJECT_SOURCE_DIR}/include/dross/thread/operation.h
    ${PROJECT_SOURCE_DIR}/include/dross/thread/operation_queue.h
    ${PROJECT_SOURCE_DIR}/include/dross/thread/runloop.h
    ${PROJECT_SOURCE_DIR}/include/dross/thread/thread.h
    ${PROJECT_SOURCE_DIR}/include/dross/thread/timer.h
)

# While the major version is 0, a minor bump can change the ABI, so the soname
# carries the minor as well. Once 1.0 is out, the major alone names the ABI.
if (PROJECT_VERSION_MAJOR EQUAL 0)
    set(DROSS_SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
else()
    set(DROSS_SOVERSION "${PROJECT_VERSION_MAJOR}")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${DROSS_SOVERSION}
    PUBLIC_HEADER "${PROJECT_SOURCE_DIR}/include/dross/dross.h"
    POSITION_INDEPENDENT_CODE ON
)

# Create alias for consistent usage
add_library(dross::dross ALIAS ${PROJECT_NAME})

# Modern CMake install with export
install(TARGETS ${PROJECT_NAME}
    EXPORT drossTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dross
)

# Install all headers
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/dross
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)
