
# Include necessary modules
include(GNUInstallDirs)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/type/datetime.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/dictionary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/error.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/number.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/string.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/timezone.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/value.cpp
)

target_compile_options(${PROJECT_NAME}
    PUBLIC -Wall -Wextra -Werror
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
    ${PROJECT_SOURCE_DIR}/include/dross/platform/environment.h
    ${PROJECT_SOURCE_DIR}/include/dross/platform/path.h
    ${PROJECT_SOURCE_DIR}/include/dross/platform/xdg.h
    ${PROJECT_SOURCE_DIR}/include/dross/type.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/array.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/boolean.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/data.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/datetime.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/dictionary.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/error.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/number.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/string.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/timezone.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/value.h
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
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
