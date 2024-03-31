# CMakeList.txt : CMake project for abc, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project(
    sandbox
    VERSION 0.1
    DESCRIPTION "Small CMake example built with VSCode")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET abc PROPERTY CXX_STANDARD 20)
endif()

//add_subdirectory(hello)   # look in hello subdirectory for CMakeLists.txt to process
//add_subdirectory(apps)    # look in apps subdirectory for CMakeLists.txt to process

# TODO: Add tests and install targets if needed.
