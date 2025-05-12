
# Proto file
get_filename_component(proto "${PROTO_DIR}/wg_exchange.proto" ABSOLUTE)
get_filename_component(proto_path "${proto}" DIRECTORY)

# Generated sources
set(proto_srcs "${PB_GEN_DIR}/wg_exchange.pb.cc")
set(proto_hdrs "${PB_GEN_DIR}/wg_exchange.pb.h")
set(grpc_srcs "${PB_GEN_DIR}/wg_exchange.grpc.pb.cc")
set(grpc_hdrs "${PB_GEN_DIR}/wg_exchange.grpc.pb.h")

add_custom_command(
      OUTPUT "${proto_srcs}" "${proto_hdrs}" "${grpc_srcs}" "${grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${PB_GEN_DIR}"
        -I "${proto_path}"
        --cpp_out "${PB_GEN_DIR}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${proto}"
      DEPENDS "${proto}")

add_library(wg_proto
  ${proto_srcs}
  ${proto_hdrs}
  ${grpc_srcs}
  ${grpc_hdrs})

target_link_libraries(wg_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})
