

set(PB_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/protos")

add_subdirectory("generated")

# add_executable(wg-exchange-server)
foreach(_target
  wg-exchange-client wg-exchange-server)
  add_executable(${_target} "${_target}.cxx")
  target_include_directories(${_target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  target_link_libraries(${_target}
    wg_proto
    absl::check
    absl::flags
    absl::flags_parse
    absl::log
    absl::log_initialize
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
    ${_DBUS_CXX})
endforeach()