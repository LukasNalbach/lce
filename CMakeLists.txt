cmake_minimum_required(VERSION 3.14)
project(lce LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)

if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()
message("Building in ${CMAKE_BUILD_TYPE} mode")

option(LCE_MARCH_NATIVE "Tune the build for the host CPU (-march=native); turn OFF for portable binaries" ON)
option(LCE_BUILD_BENCHMARKS "Build benchmark cli programs" ON)
option(LCE_BUILD_TESTS "Build tests" ON)
option(LCE_USE_MALLOC_COUNT "Track memory usage via malloc_count (overrides malloc/free)" ON)
option(LCE_ENABLE_LTO "Enable link-time optimization (LTO/IPO) for lce executables" ON)

if(LCE_MARCH_NATIVE)
  set(lce_arch_flags "-march=native")
else()
  set(lce_arch_flags "")
endif()

set(lce_debug_flags   "-Wall -Wextra -Wpedantic -ggdb")
set(lce_release_flags "-w ${lce_arch_flags} -DNDEBUG")

set(CMAKE_CXX_FLAGS_DEBUG          "${lce_debug_flags}   -O0")
set(CMAKE_CXX_FLAGS_RELEASE        "${lce_release_flags} -O3")
set(CMAKE_CXX_FLAGS_MINSIZEREL     "${lce_release_flags} -Os")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${lce_debug_flags}   -O3")

add_compile_options(-mcx16 -pthread)

if(WIN32)
  set(LCE_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)

  include_directories(BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/winshim")
  add_compile_definitions(_REENTRANT)
  set(IPS4O_OPTIMIZE_FOR_NATIVE OFF CACHE BOOL "" FORCE)
  set(IPS4O_DISABLE_PARALLEL ON CACHE BOOL "" FORCE)

  if(NOT MINGW)
    add_compile_definitions(MSVC_COMPILER _HAS_DEPRECATED_RESULT_OF=1)
  else()
    get_filename_component(_mingw_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_mingw_root "${_mingw_bin}" DIRECTORY)
    if(EXISTS "${_mingw_root}/lib/libgomp.a")
      set(OpenMP_gomp_LIBRARY "${_mingw_root}/lib/libgomp.a" CACHE FILEPATH "" FORCE)
    endif()
  endif()

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_definitions(HURCHALLA_DISALLOW_INLINE_ASM_MODMUL)
    add_compile_options(-fms-runtime-lib=static)
    add_link_options("LINKER:/defaultlib:libcmt,/defaultlib:libcpmt,/defaultlib:libvcruntime,/defaultlib:libucrt,/defaultlib:oldnames")
    add_link_options("LINKER:/STACK:0x10000000")
    get_filename_component(_llvm_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_llvm_root "${_llvm_bin}" DIRECTORY)
    file(GLOB _lce_clang_rt "${_llvm_root}/lib/clang/*/lib/windows/clang_rt.builtins-x86_64.lib")
    if(_lce_clang_rt)
      list(GET _lce_clang_rt 0 LCE_WIN_CLANG_RT)
    endif()
    if(EXISTS "${_llvm_root}/lib/libomp.lib")
      foreach(_lang C CXX)
        set(OpenMP_${_lang}_FLAGS "-fopenmp=libomp" CACHE STRING "" FORCE)
        set(OpenMP_${_lang}_LIB_NAMES "libomp" CACHE STRING "" FORCE)
      endforeach()
      set(OpenMP_libomp_LIBRARY "${_llvm_root}/lib/libomp.lib" CACHE FILEPATH "" FORCE)
      if(EXISTS "${_llvm_root}/bin/libomp.dll")
        list(APPEND LCE_WIN_RUNTIME_DLLS "${_llvm_root}/bin/libomp.dll")
      endif()
    endif()
  endif()

  set(LCE_WINDOWS_TBB_ROOT "" CACHE PATH "Root of an extracted oneAPI TBB (Windows) release")
  if(LCE_WINDOWS_TBB_ROOT)
    include_directories(BEFORE "${LCE_WINDOWS_TBB_ROOT}/include")
    find_library(LCE_WIN_TBB_LIB NAMES tbb12 tbb
      PATHS "${LCE_WINDOWS_TBB_ROOT}/lib/intel64/vc14" "${LCE_WINDOWS_TBB_ROOT}/lib" NO_DEFAULT_PATH)
    file(GLOB _tbb_dll "${LCE_WINDOWS_TBB_ROOT}/redist/intel64/vc14/tbb12.dll"
                       "${LCE_WINDOWS_TBB_ROOT}/redist/intel64/vc14/tbb.dll"
                       "${LCE_WINDOWS_TBB_ROOT}/bin/*tbb12*.dll"
                       "${LCE_WINDOWS_TBB_ROOT}/bin/*tbb.dll")
    if(_tbb_dll)
      list(APPEND LCE_WIN_RUNTIME_DLLS ${_tbb_dll})
    endif()
  else()
    message(WARNING "LCE_WINDOWS_TBB_ROOT is not set: ips4o's parallel sort needs Intel "
      "oneTBB on Windows. Download an 'oneapi-tbb-*-win' release and set LCE_WINDOWS_TBB_ROOT.")
  endif()
endif()

find_package(OpenMP REQUIRED)

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${CMAKE_CURRENT_SOURCE_DIR}/patched-files" "${CMAKE_CURRENT_SOURCE_DIR}"
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

option(LCE_USE_SDSL "Build the LCE data structures that depend on SDSL" OFF)
option(LCE_BUILD_LA_VECTOR "Build the predecessor data structure that depends on SDSL" OFF)
option(LCE_BENCHMARK_INTERNAL "Also benchmark internal data structures" ON)
option(LCE_BENCHMARK_SPACE "Also benchmark memory and memory peak" ON)

set(LCE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(LCE_EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_library(rk-lce INTERFACE)
target_include_directories(rk-lce INTERFACE ${LCE_EXTERNAL_DIR}/rk-lce/internal)

add_library(tlx_clp ${LCE_EXTERNAL_DIR}/tlx/tlx/cmdline_parser.cpp ${LCE_EXTERNAL_DIR}/tlx/tlx/string/parse_si_iec_units.cpp)
target_include_directories(tlx_clp PUBLIC ${LCE_EXTERNAL_DIR}/tlx)

add_subdirectory(external/fmt)

if(LCE_USE_MALLOC_COUNT AND NOT WIN32)
  add_library(malloc_count ${LCE_EXTERNAL_DIR}/malloc_count/malloc_count.c)
  target_include_directories(malloc_count PUBLIC ${LCE_EXTERNAL_DIR})
  target_link_libraries(malloc_count ${CMAKE_DL_LIBS})
else()
  add_library(malloc_count ${CMAKE_CURRENT_SOURCE_DIR}/cmake/malloc_count_stub.c)
  target_include_directories(malloc_count PUBLIC ${LCE_EXTERNAL_DIR})
endif()

set(LIBSAIS_USE_OPENMP ON CACHE BOOL "" FORCE)
add_subdirectory(external/libsais)

if(NOT TARGET libsais40)
  add_library(libsais40 STATIC "${CMAKE_CURRENT_SOURCE_DIR}/external/libsais/src/libsais40.cpp")
  target_include_directories(libsais40 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external/libsais/include")
  target_compile_definitions(libsais40 PUBLIC LIBSAIS_OPENMP)
  target_compile_options(libsais40 PRIVATE -fno-strict-aliasing)
  target_link_libraries(libsais40 PUBLIC OpenMP::OpenMP_CXX)
  set_target_properties(libsais40 PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

add_subdirectory(external/modular_arithmetic)

option(LCE_KEEP_IPS4O_EXTERN_FOLDER "Keep the cmake-modules bundled in ips4o's extern folder" OFF)
if(NOT LCE_KEEP_IPS4O_EXTERN_FOLDER)
file(REMOVE_RECURSE ${LCE_EXTERNAL_DIR}/ips4o/extern/cmake-modules)
file(MAKE_DIRECTORY ${LCE_EXTERNAL_DIR}/ips4o/extern/cmake-modules)
endif()
add_subdirectory(external/ips4o)
if(TARGET ips4o_example)
  set_target_properties(ips4o_example PROPERTIES EXCLUDE_FROM_ALL 1 EXCLUDE_FROM_DEFAULT_BUILD 1)
endif()


add_subdirectory(external/la_vector)
foreach(_la_target example tests)
  if(TARGET ${_la_target})
    set_target_properties(${_la_target} PROPERTIES EXCLUDE_FROM_ALL 1 EXCLUDE_FROM_DEFAULT_BUILD 1)
  endif()
endforeach()

add_library(lce_util INTERFACE)
target_include_directories(lce_util INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_util INTERFACE fmt::fmt-header-only)

add_library(lce_mersenne_modular_arithmetic INTERFACE)
target_include_directories(lce_mersenne_modular_arithmetic INTERFACE ${LCE_INCLUDE_DIR})

add_library(lce_modular_arithmetic INTERFACE)
target_include_directories(lce_modular_arithmetic INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_modular_arithmetic INTERFACE hurchalla_modular_arithmetic)

add_library(lce_rolling_hash INTERFACE)
target_include_directories(lce_rolling_hash INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_rolling_hash INTERFACE lce_modular_arithmetic lce_mersenne_modular_arithmetic)

add_library(lce_string_synchronizing_set INTERFACE)
target_include_directories(lce_string_synchronizing_set INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_string_synchronizing_set INTERFACE lce_rolling_hash OpenMP::OpenMP_CXX)

add_library(lce_rank_3tau_substrings INTERFACE)
target_include_directories(lce_rank_3tau_substrings INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_rank_3tau_substrings INTERFACE lce_string_synchronizing_set libsais40 ips4o)

add_library(lce_pred INTERFACE)

add_library(lce_binsearch_std INTERFACE)
target_include_directories(lce_binsearch_std INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_pred INTERFACE lce_binsearch_std)

add_library(lce_pred_index INTERFACE)
target_include_directories(lce_pred_index INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_pred_index INTERFACE OpenMP::OpenMP_CXX)
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
    find_package(SDSL REQUIRED)
    find_package(divsufsort REQUIRED)
    find_package(divsufsort64 REQUIRED)
    target_link_libraries(lce_pred_index INTERFACE ${SDSL} ${divsufsort} ${divsufsort64})
    target_include_directories(lce_pred_index INTERFACE ${SDSL_INCLUDE_DIRS} ${LCE_INCLUDE_DIR})
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
target_link_libraries(lce_rmq_n INTERFACE OpenMP::OpenMP_CXX)
target_link_libraries(lce_rmq INTERFACE lce_rmq_n)

add_library(lce_ds INTERFACE)

add_library(lce_naive INTERFACE)
target_include_directories(lce_naive INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_ds INTERFACE lce_naive)

add_library(lce_naive_wordwise INTERFACE)
target_include_directories(lce_naive_wordwise INTERFACE ${LCE_INCLUDE_DIR})
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
target_link_libraries(lce_classic INTERFACE libsais ips4o lce_rmq fmt::fmt-header-only)
target_link_libraries(lce_ds INTERFACE lce_classic)

add_library(lce_classic_for_sss INTERFACE)
target_include_directories(lce_classic_for_sss INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_classic_for_sss INTERFACE libsais40 libsais lce_rmq fmt::fmt-header-only)
target_link_libraries(lce_ds INTERFACE lce_classic_for_sss)

if(LCE_USE_SDSL)
    add_library(lce_sdsl_cst INTERFACE)
    target_compile_definitions(lce_sdsl_cst INTERFACE -DLCE_USE_SDSL)
    target_include_directories(lce_sdsl_cst INTERFACE ${SDSL_INCLUDE_DIRS} ${LCE_INCLUDE_DIR})
    target_link_libraries(lce_sdsl_cst INTERFACE ${SDSL} ${divsufsort} ${divsufsort64})
    target_link_libraries(lce_ds INTERFACE lce_sdsl_cst)
endif()

add_library(lce INTERFACE)
target_link_libraries(lce INTERFACE lce_ds lce_rolling_hash lce_pred lce_util lce_rmq)

if(WIN32 AND LCE_WIN_TBB_LIB)
  target_link_libraries(lce INTERFACE "${LCE_WIN_TBB_LIB}")
endif()

if(WIN32 AND LCE_WIN_CLANG_RT)
  target_link_libraries(lce INTERFACE "${LCE_WIN_CLANG_RT}")
endif()

if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_library(lce_win_atomic STATIC "${CMAKE_CURRENT_SOURCE_DIR}/cmake/win_atomic_shim.c")
  target_link_libraries(lce INTERFACE lce_win_atomic)
endif()

if(MINGW)
  target_link_libraries(lce INTERFACE atomic dl)
  target_link_options(lce INTERFACE -static)
endif()

if(LCE_ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT LCE_IPO_SUPPORTED OUTPUT LCE_IPO_ERROR LANGUAGES CXX)
  if(NOT LCE_IPO_SUPPORTED)
    message(WARNING "LCE_ENABLE_LTO is ON but LTO/IPO is not supported by the toolchain: ${LCE_IPO_ERROR}")
  endif()
endif()

function(lce_add_tool name src outdir)
  add_executable(${name} "${src}")
  target_link_libraries(${name} PRIVATE ${ARGN})
  set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${outdir}")
  if(LCE_ENABLE_LTO AND LCE_IPO_SUPPORTED)
    set_target_properties(${name} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
  endif()
  foreach(_dll ${LCE_WIN_RUNTIME_DLLS})
    add_custom_command(TARGET ${name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "${outdir}")
  endforeach()
endfunction()

if(LCE_BUILD_BENCHMARKS)
  set(lce_bench_dir "${CMAKE_BINARY_DIR}/bench")
  lce_add_tool(bench-lce bench/bench-lce.cpp "${lce_bench_dir}" lce_ds tlx_clp fmt::fmt-header-only)
  if(${LCE_BENCHMARK_SPACE})
    target_compile_definitions(bench-lce PRIVATE -DLCE_BENCHMARK_SPACE)
    target_link_libraries(bench-lce PRIVATE malloc_count)
  endif()
  if(${LCE_BENCHMARK_INTERNAL})
    target_compile_definitions(bench-lce PRIVATE -DLCE_BENCHMARK_INTERNAL)
  endif()
  if(LCE_USE_SDSL)
    target_link_libraries(bench-lce PRIVATE lce_sdsl_cst)
  endif()

  lce_add_tool(gen-queries bench/gen-queries.cpp "${lce_bench_dir}" tlx_clp)
  lce_add_tool(gen-sa-lcp bench/gen-sa-lcp.cpp "${lce_bench_dir}" libsais)
  lce_add_tool(bench-pred bench/bench-pred.cpp "${lce_bench_dir}" lce_pred tlx_clp fmt::fmt-header-only lce_util)
  if(${LCE_BENCHMARK_SPACE})
    target_compile_definitions(bench-pred PRIVATE -DLCE_BENCHMARK_SPACE)
    target_link_libraries(bench-pred PRIVATE malloc_count)
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

  lce_add_tool(gen-sss bench/gen-sss.cpp "${lce_bench_dir}" tlx_clp lce_string_synchronizing_set fmt::fmt-header-only lce_util)
endif()

if(LCE_BUILD_TESTS)
  if(NOT TARGET gtest)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    add_subdirectory(external/googletest)
  endif()
  enable_testing()
  include(GoogleTest)

  foreach(s lce pred rmq rolling-hash string-synchronizing-set text)
    lce_add_tool("test-${s}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test-${s}.cpp" "${CMAKE_BINARY_DIR}/tests"
      lce libsais GTest::gtest_main OpenMP::OpenMP_CXX)
    target_include_directories("test-${s}" PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    gtest_discover_tests("test-${s}")
  endforeach()
endif()
