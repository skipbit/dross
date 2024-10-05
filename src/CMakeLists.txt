
option(BUILD_SHARED_LIBS "build dross as a shared library" ON)
if (BUILD_SHARED_LIBS)
    add_library(${PROJECT_NAME} SHARED)
else()
    add_library(${PROJECT_NAME} STATIC)
endif()

target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/type/array.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/data.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/dictionary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/error.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/number.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/string.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/type/value.cpp
)

target_compile_options(${PROJECT_NAME}
    PUBLIC -Wall -Wextra -Werror
    $<$<CONFIG:Debug>:-O0 -g3>
    $<$<CONFIG:Release>:-O3>
)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

target_include_directories(${PROJECT_NAME}
    SYSTEM BEFORE
    PRIVATE "$ENV{HOMEBREW_PREFIX}/include" "$ENV{HOMEBREW_PREFIX}/opt/gcc/include/c++/13" "$ENV{HOMEBREW_PREFIX}/opt/glibc/include"
)
target_include_directories(${PROJECT_NAME}
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_directories(${PROJECT_NAME}
    PRIVATE "$ENV{HOMEBREW_PREFIX}/lib" "$ENV{HOMEBREW_PREFIX}/opt/gcc/lib/gcc/13" "$ENV{HOMEBREW_PREFIX}/opt/glibc/lib"
)

set(DROSS_HEADERS
    ${PROJECT_SOURCE_DIR}/include/dross/dross.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/array.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/data.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/dictionary.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/error.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/number.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/string.h
    ${PROJECT_SOURCE_DIR}/include/dross/type/value.h
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    PUBLIC_HEADER "${PROJECT_SOURCE_DIR}/include/dross/dross.h"
    POSITION_INDEPENDENT_CODE ON
)

include(GNUInstallDirs)
install(TARGETS dross
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dross
)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/dross DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
