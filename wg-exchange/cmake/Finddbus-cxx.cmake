message(STATUS "Using dbus-cxx via add_subdirectory (FetchContent).")
include(FetchContent)

FetchContent_Declare(
  dbus-cxx
  GIT_REPOSITORY https://github.com/dbus-cxx/dbus-cxx.git
  GIT_TAG        2.5.2
  GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable(dbus-cxx)

set(_DBUS_CXX dbus-cxx)