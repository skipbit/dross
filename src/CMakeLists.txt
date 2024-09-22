
option(DROSS_BUILD_SHARED_LIBS "build dross as a shared library" ON)
if (DROSS_BUILD_SHARED_LIBS)
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
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_directories(${PROJECT_NAME}
    PRIVATE "$ENV{HOMEBREW_PREFIX}/lib" "$ENV{HOMEBREW_PREFIX}/opt/gcc/lib/gcc/13" "$ENV{HOMEBREW_PREFIX}/opt/glibc/lib"
)

set(PUBLIC_HEADERS
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
    LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib
    PUBLIC_HEADER "${PUBLIC_HEADERS}"
    POSITION_INDEPENDENT_CODE ON
)

foreach(header ${PUBLIC_HEADERS})
    file(RELATIVE_PATH relative_path "${PROJECT_SOURCE_DIR}/include/dross" ${header})
    get_filename_component(header_directory, ${relative_path} DIRECTORY)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/include/${header_directory}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${header} ${PROJECT_BINARY_DIR}/include/dross/${relative_path}
        COMMENT "Copying ${header} to ${PROJECT_BINARY_DIR}/include/dross/${relative_path}"
        DEPENDS ${PROJECT_SOURCE_DIR}/include/dross/${relative_path}
    )
endforeach()

