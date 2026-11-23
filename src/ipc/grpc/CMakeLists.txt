# dross::grpc, the gRPC part, built only when DROSS_WITH_GRPC is on. gRPC
# comes from the user's environment: dross neither fetches nor builds it.

# gRPC's own package finds protobuf, through protobuf's package or CMake's
# FindProtobuf, depending on how gRPC was installed.
find_package(gRPC ${DROSS_GRPC_MINIMUM_VERSION} CONFIG)
if (NOT gRPC_FOUND)
    message(FATAL_ERROR
        "DROSS_WITH_GRPC is on, but gRPC ${DROSS_GRPC_MINIMUM_VERSION} or later was not found. "
        "Install gRPC, or point CMAKE_PREFIX_PATH at where it is installed.")
endif()

# Protobuf's own package accepts a version asked of find_package() only with
# the major of its C++ numbering, so the minimum is compared here instead.
if (Protobuf_VERSION VERSION_LESS DROSS_PROTOBUF_MINIMUM_VERSION)
    message(FATAL_ERROR
        "DROSS_WITH_GRPC needs protobuf ${DROSS_PROTOBUF_MINIMUM_VERSION} or later, "
        "but gRPC ${gRPC_VERSION} came with protobuf ${Protobuf_VERSION}.")
endif()

message(STATUS "gRPC part: gRPC ${gRPC_VERSION}, protobuf ${Protobuf_VERSION}")

add_library(dross_grpc)

target_sources(dross_grpc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/cleanup_thread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/grpc_acceptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/grpc_address.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/grpc_connector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/grpc_target.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/messages.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/reaction_scope.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/stream_state.cpp
)

get_target_property(DROSS_GRPC_CPP_PLUGIN gRPC::grpc_cpp_plugin LOCATION)
protobuf_generate(TARGET dross_grpc LANGUAGE cpp)
protobuf_generate(TARGET dross_grpc LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    PLUGIN "protoc-gen-grpc=${DROSS_GRPC_CPP_PLUGIN}"
)

target_link_libraries(dross_grpc
    PUBLIC dross gRPC::grpc++
    PRIVATE protobuf::libprotobuf
)

target_compile_options(dross_grpc PRIVATE
    -Wall -Wextra -Werror
    $<$<CONFIG:Debug>:-O0 -g3>
    $<$<CONFIG:Release>:-O3>
)
target_compile_features(dross_grpc PUBLIC cxx_std_23)

target_include_directories(dross_grpc
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${PROJECT_SOURCE_DIR}/src
)

set_target_properties(dross_grpc PROPERTIES
    EXPORT_NAME grpc
    VERSION ${PROJECT_VERSION}
    SOVERSION ${DROSS_SOVERSION}
    POSITION_INDEPENDENT_CODE ON
)

add_library(dross::grpc ALIAS dross_grpc)

install(TARGETS dross_grpc
    EXPORT drossGrpcTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/dross/ipc/grpc
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dross/ipc
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT drossGrpcTargets
    FILE drossGrpcTargets.cmake
    NAMESPACE dross::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dross
)
