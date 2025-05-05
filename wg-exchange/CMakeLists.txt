cmake_minimum_required(VERSION 3.16)

project(wg-exchange VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

find_package(Threads REQUIRED)
find_package(gRPC REQUIRED)
find_package(absl REQUIRED)
find_package(dbus-cxx REQUIRED)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
add_subdirectory("src")
