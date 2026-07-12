cmake_minimum_required(VERSION 3.10)
project(noros_examples CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)

# noros is header-only (single header at include/noros.hpp). The implementation
# is compiled into exactly ONE translation unit: noros_impl.cpp, which does
#   #define NOROS_IMPLEMENTATION
#   #include "noros.hpp"
# We build that once as a static lib and link it into every example. In your own
# project you don't need CMake at all -- just copy include/noros.hpp, put those
# two lines in one of your .cpp files, and compile with -std=c++17 -pthread.
add_library(noros STATIC noros_impl.cpp)
target_include_directories(noros PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(noros PUBLIC Threads::Threads)

foreach(ex talker listener custom_msg md5_discovery
           add_two_ints_server add_two_ints_client
           stamped_pub stamped_sub
           fibonacci_client fibonacci_server
           params_example udp_listener nr_roscore)
  add_executable(${ex} examples/${ex}.cpp)
  target_include_directories(${ex} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(${ex} noros)
endforeach()

# Optional OpenCV demos: webcam_pub + image_viewer. OpenCV is NOT a dependency of
# noros -- the core links zero third-party libs. These two examples build ONLY if
# OpenCV happens to be installed; the rest of the library never needs it.
find_package(OpenCV QUIET)
if(OpenCV_FOUND)
  foreach(ex webcam_pub image_viewer)
    add_executable(${ex} examples/${ex}.cpp)
    target_include_directories(${ex} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(${ex} noros ${OpenCV_LIBS})
  endforeach()
  message(STATUS "noros: OpenCV found -> building webcam_pub + image_viewer")
else()
  message(STATUS "noros: OpenCV not found -> skipping webcam_pub + image_viewer (optional)")
endif()
