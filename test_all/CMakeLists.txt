cmake_minimum_required(VERSION 3.10)

project(test_all VERSION 1.0)

find_package (glog 0.7.0 REQUIRED)

add_library(haldendb_compiler_flags INTERFACE)
target_compile_features(haldendb_compiler_flags  INTERFACE cxx_std_20)

add_compile_definitions(__TRACK_CACHE_FOOTPRINT__)
add_compile_definitions(__CONCURRENT__)
add_compile_definitions(__TREE_WITH_CACHE__)
add_compile_definitions(__VALIDITY_CHECK__)

#configure_file(TutorialConfig.h.in TutorialConfig.h)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.12.1
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_subdirectory(../libcache ${PROJECT_BINARY_DIR}/libcache)
add_subdirectory(../libbtree ${PROJECT_BINARY_DIR}/libbtree)

set(CMAKE_CXX_COMPILER g++-11)

add_executable(test_all 
	       BPlusStore_LRUCache_FileStorage_Suite_1.cpp 
	#       BPlusStore_LRUCache_FileStorage_Suite_2.cpp 
	#       BPlusStore_LRUCache_FileStorage_Suite_3.cpp
               BPlusStore_LRUCache_VolatileStorage_Suite_1.cpp
               BPlusStore_LRUCache_VolatileStorage_Suite_2.cpp
               BPlusStore_LRUCache_VolatileStorage_Suite_3.cpp
               BPlusStore_NoCache_Suite_1.cpp 
               BPlusStore_NoCache_Suite_2.cpp 
               BPlusStore_NoCache_Suite_3.cpp 
               main.cpp 
)

set_target_properties(test_all PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

target_link_libraries(test_all PUBLIC libcache haldendb_compiler_flags)
target_link_libraries(test_all PUBLIC libbtree haldendb_compiler_flags)
target_link_libraries(test_all PUBLIC glog::glog)

target_include_directories(test_all PUBLIC
                           "${PROJECT_BINARY_DIR}"
                           "${PROJECT_SOURCE_DIR}/../libcache"
                           "${PROJECT_SOURCE_DIR}/../libbtree"
                           )
#enable_testing()

target_link_libraries(test_all PUBLIC  GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(test_all)
