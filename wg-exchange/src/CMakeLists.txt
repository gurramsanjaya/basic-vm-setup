

set(PB_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/protos")

add_subdirectory("ext_hacl_star")
add_subdirectory("generated")

add_executable(wg-exchange-server "wg_exchange_server.cxx" "service_dbus_handle.cxx")
target_include_directories(wg-exchange-server PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(wg-exchange-server
  hacl_curve25519
  wg_proto
  Boost::program_options
  Boost::thread
  Boost::filesystem
  Boost::chrono
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF}
  ${_DBUS_CXX})

add_executable(wg-exchange-client "wg_exchange_client.cxx")
target_include_directories(wg-exchange-client PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(wg-exchange-client
  wg_proto
  Boost::program_options
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})