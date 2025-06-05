cmake_minimum_required(VERSION 3.30)

project(wg-exchange VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
message(STATUS "using cmake install prefix: ${CMAKE_INSTALL_PREFIX}")

find_package(Threads REQUIRED)
find_package(gRPC REQUIRED)
find_package(dbus-cxx REQUIRED)
find_package(Boost REQUIRED COMPONENTS program_options thread chrono filesystem)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
add_subdirectory("src")
