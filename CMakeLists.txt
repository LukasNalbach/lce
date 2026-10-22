cmake_minimum_required(VERSION 3.21)
project(lce LANGUAGES C CXX)

option(LCE_BUILD_BENCHMARKS "Build benchmark cli programs" OFF)
option(LCE_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})

if(WIN32)
  set(LCE_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
endif()

# add external/string-utils, if the parent project has not added it
if(NOT TARGET string_utils)
  file(GLOB lce_string_utils_contents "${CMAKE_CURRENT_SOURCE_DIR}/external/string-utils/*")

  if(lce_string_utils_contents STREQUAL "")
    find_package(Git REQUIRED)
    message(STATUS "Downloading the git submodule external/string-utils")
    execute_process(COMMAND "${GIT_EXECUTABLE}" submodule update --init -- external/string-utils
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()

  # the prefix of the options and environment variables (LCE_MARCH_NATIVE, LCE_TEST_ITERATIONS, ...)
  if(PROJECT_IS_TOP_LEVEL)
    set(STRING_UTILS_PREFIX LCE)
    option(LCE_ENABLE_LTO "Enable link-time optimization (LTO/IPO) for the executables" ON)
    include("${CMAKE_CURRENT_SOURCE_DIR}/external/string-utils/cmake/settings.cmake")
  endif()

  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/string-utils")
endif()

# the text classes
if(NOT TARGET compact_texts)
  string_utils_fetch_submodules("${CMAKE_CURRENT_SOURCE_DIR}" PATHS external/compact-texts)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/compact-texts")
endif()

string_utils_fetch_submodules("${CMAKE_CURRENT_SOURCE_DIR}" RECURSIVE PATHS
  external/tlx external/fmt external/modular_arithmetic external/rk-lce external/PGM-index external/la_vector)

execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_CURRENT_SOURCE_DIR}/patched-files" "${CMAKE_CURRENT_SOURCE_DIR}"
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

option(LCE_USE_SDSL "Build the LCE data structures that depend on SDSL" OFF)
option(LCE_BUILD_LA_VECTOR "Build the predecessor data structure that depends on SDSL" OFF)
option(LCE_BENCHMARK_INTERNAL "Also benchmark internal data structures" ON)
option(LCE_BENCHMARK_SPACE "Also benchmark memory and memory peak" ON)

set(LCE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(LCE_EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external)

add_library(rk-lce INTERFACE)
target_include_directories(rk-lce INTERFACE ${LCE_EXTERNAL_DIR}/rk-lce/internal)

add_library(tlx_clp ${LCE_EXTERNAL_DIR}/tlx/tlx/cmdline_parser.cpp ${LCE_EXTERNAL_DIR}/tlx/tlx/string/parse_si_iec_units.cpp)
target_include_directories(tlx_clp PUBLIC ${LCE_EXTERNAL_DIR}/tlx)

if(NOT TARGET fmt::fmt-header-only)
  add_subdirectory(external/fmt)
endif()

add_subdirectory(external/modular_arithmetic)

add_subdirectory(external/la_vector)
foreach(_la_target example tests)
  if(TARGET ${_la_target})
    set_target_properties(${_la_target} PROPERTIES EXCLUDE_FROM_ALL 1 EXCLUDE_FROM_DEFAULT_BUILD 1)
  endif()
endforeach()

add_library(lce_util INTERFACE)
target_include_directories(lce_util INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_util INTERFACE string_utils compact_texts fmt::fmt-header-only)

add_library(lce_mersenne_modular_arithmetic INTERFACE)
target_include_directories(lce_mersenne_modular_arithmetic INTERFACE ${LCE_INCLUDE_DIR})

add_library(lce_modular_arithmetic INTERFACE)
target_include_directories(lce_modular_arithmetic INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_modular_arithmetic INTERFACE hurchalla_modular_arithmetic)

if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_definitions(lce_modular_arithmetic INTERFACE HURCHALLA_DISALLOW_INLINE_ASM_MODMUL)
endif()

add_library(lce_rolling_hash INTERFACE)
target_include_directories(lce_rolling_hash INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_rolling_hash INTERFACE lce_util lce_modular_arithmetic lce_mersenne_modular_arithmetic)

add_library(lce_string_synchronizing_set INTERFACE)
target_include_directories(lce_string_synchronizing_set INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_string_synchronizing_set INTERFACE lce_util lce_rolling_hash OpenMP::OpenMP_CXX)

add_library(lce_rank_3tau_substrings INTERFACE)
target_include_directories(lce_rank_3tau_substrings INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_rank_3tau_substrings INTERFACE lce_string_synchronizing_set libsais40 ips4o)

add_library(lce_pred INTERFACE)

add_library(lce_binsearch_std INTERFACE)
target_include_directories(lce_binsearch_std INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_pred INTERFACE lce_binsearch_std)

add_library(lce_pred_index INTERFACE)
target_include_directories(lce_pred_index INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_pred_index INTERFACE lce_util OpenMP::OpenMP_CXX)
target_link_libraries(lce_pred INTERFACE lce_pred_index)

add_library(lce_j_index INTERFACE)
target_include_directories(lce_j_index INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_j_index INTERFACE OpenMP::OpenMP_CXX)
target_link_libraries(lce_pred INTERFACE lce_j_index)

add_library(lce_pgm_index INTERFACE)
target_include_directories(lce_pgm_index INTERFACE ${LCE_EXTERNAL_DIR}/PGM-index/include/pgm)
target_link_libraries(lce_pgm_index INTERFACE OpenMP::OpenMP_CXX)
target_link_libraries(lce_pred INTERFACE lce_pgm_index)

if(LCE_USE_SDSL)
  target_link_libraries(lce_pred_index INTERFACE sdsl)
endif()

if(LCE_BUILD_LA_VECTOR)
  target_link_libraries(lce_pred_index INTERFACE la_vector)
endif()

add_library(lce_rmq INTERFACE)

add_library(lce_rmq_nlgn INTERFACE)
target_include_directories(lce_rmq_nlgn INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_rmq_nlgn INTERFACE OpenMP::OpenMP_CXX)
target_link_libraries(lce_rmq INTERFACE lce_rmq_nlgn)

add_library(lce_rmq_naive INTERFACE)
target_include_directories(lce_rmq_naive INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_rmq_naive INTERFACE OpenMP::OpenMP_CXX)
target_link_libraries(lce_rmq INTERFACE lce_rmq_naive)

add_library(lce_rmq_n INTERFACE)
target_include_directories(lce_rmq_n INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_rmq_n INTERFACE lce_util OpenMP::OpenMP_CXX)
target_link_libraries(lce_rmq INTERFACE lce_rmq_n)

add_library(lce_ds INTERFACE)

add_library(lce_naive INTERFACE)
target_include_directories(lce_naive INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_ds INTERFACE lce_naive)

add_library(lce_naive_wordwise INTERFACE)
target_include_directories(lce_naive_wordwise INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_naive_wordwise INTERFACE lce_util)
target_link_libraries(lce_ds INTERFACE lce_naive_wordwise)

add_library(lce_naive_std INTERFACE)
target_include_directories(lce_naive_std INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_ds INTERFACE lce_naive_std)

add_library(lce_memcmp INTERFACE)
target_include_directories(lce_memcmp INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_ds INTERFACE lce_memcmp)

add_library(lce_rk_prezza INTERFACE)
target_link_libraries(lce_rk_prezza INTERFACE rk-lce)
target_link_libraries(lce_ds INTERFACE lce_rk_prezza)

add_library(lce_fp INTERFACE)
target_include_directories(lce_fp INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_fp INTERFACE lce_rolling_hash OpenMP::OpenMP_CXX)
target_link_libraries(lce_ds INTERFACE lce_fp)

add_library(lce_sss_naive INTERFACE)
target_include_directories(lce_sss_naive INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_sss_naive INTERFACE lce_string_synchronizing_set lce_pred_index fmt::fmt-header-only)
target_link_libraries(lce_ds INTERFACE lce_sss_naive)

add_library(lce_sss_noss INTERFACE)
target_include_directories(lce_sss_noss INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_sss_noss INTERFACE lce_string_synchronizing_set lce_pred_index fmt::fmt-header-only)
target_link_libraries(lce_ds INTERFACE lce_sss_noss)

add_library(lce_sss INTERFACE)
target_include_directories(lce_sss INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_sss INTERFACE lce_string_synchronizing_set lce_rank_3tau_substrings lce_pred_index lce_classic_for_sss ips4o fmt::fmt-header-only)
target_link_libraries(lce_ds INTERFACE lce_sss)

add_library(lce_classic INTERFACE)
target_include_directories(lce_classic INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_classic INTERFACE lce_util libsais ips4o lce_rmq fmt::fmt-header-only)
target_link_libraries(lce_ds INTERFACE lce_classic)

add_library(lce_classic_for_sss INTERFACE)
target_include_directories(lce_classic_for_sss INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_classic_for_sss INTERFACE lce_util libsais40 libsais lce_rmq fmt::fmt-header-only)
target_link_libraries(lce_ds INTERFACE lce_classic_for_sss)

if(LCE_USE_SDSL)
  add_library(lce_sdsl_cst INTERFACE)
  target_compile_definitions(lce_sdsl_cst INTERFACE -DLCE_USE_SDSL)
  target_include_directories(lce_sdsl_cst INTERFACE ${LCE_INCLUDE_DIR})
  target_link_libraries(lce_sdsl_cst INTERFACE sdsl)
  target_link_libraries(lce_ds INTERFACE lce_sdsl_cst)
endif()

add_library(lce INTERFACE)
target_link_libraries(lce INTERFACE lce_ds lce_rolling_hash lce_pred lce_util lce_rmq)

if(LCE_BUILD_BENCHMARKS)
  set(lce_bench_dir "${CMAKE_BINARY_DIR}/bench")
  string_utils_add_executable(bench-lce bench/bench-lce.cpp "${lce_bench_dir}" lce_ds tlx_clp fmt::fmt-header-only)
  if(${LCE_BENCHMARK_SPACE})
    target_compile_definitions(bench-lce PRIVATE -DLCE_BENCHMARK_SPACE)
  endif()
  if(${LCE_BENCHMARK_INTERNAL})
    target_compile_definitions(bench-lce PRIVATE -DLCE_BENCHMARK_INTERNAL)
  endif()
  if(LCE_USE_SDSL)
    target_link_libraries(bench-lce PRIVATE lce_sdsl_cst)
  endif()

  string_utils_add_executable(gen-queries bench/gen-queries.cpp "${lce_bench_dir}" tlx_clp)
  string_utils_add_executable(gen-sa-lcp bench/gen-sa-lcp.cpp "${lce_bench_dir}" string_utils)
  string_utils_add_executable(bench-pred bench/bench-pred.cpp "${lce_bench_dir}" lce_pred tlx_clp fmt::fmt-header-only lce_util)
  if(${LCE_BENCHMARK_SPACE})
    target_compile_definitions(bench-pred PRIVATE -DLCE_BENCHMARK_SPACE)
  endif()
  if(${LCE_BENCHMARK_INTERNAL})
    target_compile_definitions(bench-pred PRIVATE -DLCE_BENCHMARK_INTERNAL)
  endif()
  if(LCE_USE_SDSL)
    target_compile_definitions(bench-pred PRIVATE -DLCE_USE_SDSL)
  endif()
  if(LCE_BUILD_LA_VECTOR)
    target_compile_definitions(bench-pred PRIVATE -DLCE_BUILD_LA_VECTOR)
  endif()

  string_utils_add_executable(gen-sss bench/gen-sss.cpp "${lce_bench_dir}" tlx_clp lce_string_synchronizing_set fmt::fmt-header-only lce_util)
endif()

if(LCE_BUILD_TESTS)
  enable_testing()
  include(GoogleTest)

  foreach(s lce pred rmq rolling-hash string-synchronizing-set text)
    string_utils_add_executable("test-${s}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test-${s}.cpp" "${CMAKE_BINARY_DIR}/tests"
      lce string_utils_test)
    target_include_directories("test-${s}" PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    gtest_discover_tests("test-${s}")
  endforeach()
endif()
