

add_library(hacl_curve25519 "Hacl_Curve25519_51.c")
target_include_directories(hacl_curve25519 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
set_target_properties(hacl_curve25519
                  PROPERTIES
                  CXX_STANDARD 11
                  CXX_EXTENSIONS off
)