

set(PB_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/protos")

add_subdirectory("generated")

foreach(_target
  wg_exchange_client wg_exchange_server)
  add_executable(${_target} "${_target}.cxx" "service_dbus_handle.cxx")
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