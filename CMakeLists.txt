cmake_minimum_required(VERSION 3.20)
project(dross VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Modern CMake package support
include(CMakePackageConfigHelpers)

add_subdirectory(${PROJECT_SOURCE_DIR}/src)
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
