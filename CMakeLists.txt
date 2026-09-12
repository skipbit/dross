cmake_minimum_required(VERSION 3.20)

# Git tags are the source of truth for the version, so it is resolved here
# rather than written down. See cmake/ProjectVersion.cmake.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ProjectVersion.cmake)
project_version_detect(DROSS_VERSION DROSS_GIT_DESCRIBE)

project(dross VERSION ${DROSS_VERSION} LANGUAGES CXX)

message(STATUS "Version: ${PROJECT_VERSION} (${DROSS_GIT_DESCRIBE})")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Modern CMake package support
include(CMakePackageConfigHelpers)

add_subdirectory(${PROJECT_SOURCE_DIR}/src)

# enable_testing() registers the test tree with ctest only when it runs at the
# top level, so it lives here rather than in test/CMakeLists.txt.
enable_testing()
add_subdirectory(${PROJECT_SOURCE_DIR}/test)

# Generate and install package config files
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/drossConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/drossConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dross
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/drossConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/drossConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/drossConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dross
)

# Export targets for find_package
install(EXPORT drossTargets
    FILE drossTargets.cmake
    NAMESPACE dross::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dross
)

# pkg-config support
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dross.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/dross.pc"
    @ONLY
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/dross.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
