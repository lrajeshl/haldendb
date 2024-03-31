cmake_minimum_required(VERSION 3.10)

project(test_all VERSION 1.0)


add_library(haldendb_compiler_flags INTERFACE)
target_compile_features(haldendb_compiler_flags  INTERFACE cxx_std_20)

#configure_file(TutorialConfig.h.in TutorialConfig.h)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_subdirectory(../libcache ${PROJECT_BINARY_DIR}/libcache)
add_subdirectory(../libbtree ${PROJECT_BINARY_DIR}/libbtree)

add_executable(test_all 
               BPlusStore_DRAMLRU_Suite_1.cpp 
               BPlusStore_DRAMLRU_Suite_2.cpp 
               BPlusStore_NoCache_Suite_1.cpp 
               BPlusStore_NVRAMLRU_Suite_1.cpp
               main.cpp 
)

target_link_libraries(test_all PUBLIC libcache haldendb_compiler_flags)
target_link_libraries(test_all PUBLIC libbtree haldendb_compiler_flags)

target_include_directories(test_all PUBLIC
                           "${PROJECT_BINARY_DIR}"
                           "${PROJECT_SOURCE_DIR}/../libcache"
                           "${PROJECT_SOURCE_DIR}/../libbtree"
                           )

enable_testing()



target_link_libraries(test_all PUBLIC  GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(test_all)