cmake_minimum_required(VERSION 3.14)
project(lce-reproducibility-helpers LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

# Only the library targets of lce are needed here. The benchmark tools are
# built from the top-level CMakeLists.txt of the repository.
set(LCE_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. lce EXCLUDE_FROM_ALL)

find_package(OpenMP REQUIRED)

add_executable(text-stats text-stats.cpp)
target_compile_options(text-stats PRIVATE -march=native -mcx16)
target_link_libraries(text-stats PRIVATE lce_string_synchronizing_set util OpenMP::OpenMP_CXX)
