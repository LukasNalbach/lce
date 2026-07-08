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
option(LCE_USE_MALLOC_COUNT "Track memory usage via malloc_count (overrides malloc/free)" ON)

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

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
add_subdirectory(external/googletest)
enable_testing()
include(GoogleTest)

option(LCE_USE_SDSL "build lce data structure that depends on SDSL" OFF)
option(LCE_BUILD_LA_VECTOR "build lce data structure that depends on SDSL" OFF)
option(LCE_BENCHMARK_INTERNAL "Also benchmark internal data structures" ON)
option(LCE_BENCHMARK_SPACE "Also benchmark memory and memory peak" ON)

set(LCE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(ALX_EXTLIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_library(rk-lce INTERFACE)
target_include_directories(rk-lce INTERFACE ${ALX_EXTLIB_DIR}/rk-lce/internal)

add_library(tlx_clp ${ALX_EXTLIB_DIR}/tlx/tlx/cmdline_parser.cpp ${ALX_EXTLIB_DIR}/tlx/tlx/string/parse_si_iec_units.cpp)
target_include_directories(tlx_clp PUBLIC ${ALX_EXTLIB_DIR}/tlx)

add_subdirectory(external/fmt)

if(LCE_USE_MALLOC_COUNT AND NOT WIN32)
  add_library(malloc_count ${ALX_EXTLIB_DIR}/malloc_count/malloc_count.c)
  target_include_directories(malloc_count PUBLIC ${ALX_EXTLIB_DIR})
  target_link_libraries(malloc_count ${CMAKE_DL_LIBS})
else()
  add_library(malloc_count ${CMAKE_CURRENT_SOURCE_DIR}/cmake/malloc_count_stub.c)
  target_include_directories(malloc_count PUBLIC ${ALX_EXTLIB_DIR})
endif()

set(LIBSAIS_USE_OPENMP ON CACHE BOOL "" FORCE)
add_subdirectory(external/libsais)

add_subdirectory(external/modular_arithmetic)

add_library(parallel-hashmap INTERFACE)
target_include_directories(parallel-hashmap INTERFACE ${ALX_EXTLIB_DIR}/parallel-hashmap/)

add_library(pgm INTERFACE)
target_link_libraries(pgm INTERFACE OpenMP::OpenMP_CXX)
target_include_directories(pgm INTERFACE ${ALX_EXTLIB_DIR}/PGM-index/include/pgm)

option(KEEP_IPS4O_EXTERN_FOLDER OFF)
if(NOT ${KEEP_IPS4O_EXTERN_FOLDER})
file(REMOVE_RECURSE ${ALX_EXTLIB_DIR}/ips4o/extern/cmake-modules)
file(MAKE_DIRECTORY ${ALX_EXTLIB_DIR}/ips4o/extern/cmake-modules)
endif()
add_subdirectory(external/ips4o)
if(TARGET ips4o_example)
  set_target_properties(ips4o_example PROPERTIES EXCLUDE_FROM_ALL 1 EXCLUDE_FROM_DEFAULT_BUILD 1)
endif()

add_library(gsaca_ds INTERFACE)
target_link_libraries(gsaca_ds INTERFACE OpenMP::OpenMP_CXX ips4o)
if(NOT WIN32)
  target_link_libraries(gsaca_ds INTERFACE -latomic)
endif()
target_include_directories(gsaca_ds INTERFACE ${ALX_EXTLIB_DIR}/gsaca-double-sort)

add_subdirectory(external/la_vector)
foreach(_la_target example tests)
  if(TARGET ${_la_target})
    set_target_properties(${_la_target} PROPERTIES EXCLUDE_FROM_ALL 1 EXCLUDE_FROM_DEFAULT_BUILD 1)
  endif()
endforeach()

add_library(util INTERFACE)
target_include_directories(util INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(util INTERFACE fmt::fmt-header-only)

add_library(lce_mersenne_modular_arithmetic INTERFACE)
target_include_directories(lce_mersenne_modular_arithmetic INTERFACE ${LCE_INCLUDE_DIR})

add_library(lce_modular_arithmetic INTERFACE)
target_include_directories(lce_modular_arithmetic INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_modular_arithmetic INTERFACE hurchalla_modular_arithmetic)

add_library(lce_reduce_fingerprints INTERFACE)
target_include_directories(lce_reduce_fingerprints INTERFACE ${LCE_INCLUDE_DIR})

add_library(rolling_hash INTERFACE)
target_include_directories(rolling_hash INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(rolling_hash INTERFACE lce_modular_arithmetic lce_mersenne_modular_arithmetic)

add_library(lce_ring_buffer INTERFACE)
target_include_directories(lce_ring_buffer INTERFACE ${LCE_INCLUDE_DIR})

add_library(lce_string_synchronizing_set INTERFACE)
target_include_directories(lce_string_synchronizing_set INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(lce_string_synchronizing_set INTERFACE lce_ring_buffer rolling_hash OpenMP::OpenMP_CXX parallel-hashmap)

add_library(pred INTERFACE)

add_library(pred_binsearch_std INTERFACE)
target_include_directories(pred_binsearch_std INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(pred INTERFACE pred_binsearch_std)

add_library(pred_index INTERFACE)
target_include_directories(pred_index INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(pred_index INTERFACE OpenMP::OpenMP_CXX)
target_link_libraries(pred INTERFACE pred_index)

add_library(j_index INTERFACE)
target_include_directories(j_index INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(j_index INTERFACE OpenMP::OpenMP_CXX)
target_link_libraries(pred INTERFACE j_index)

add_library(lce_pgm_index INTERFACE)
target_link_libraries(lce_pgm_index INTERFACE pgm)
target_link_libraries(pred INTERFACE lce_pgm_index)

if(LCE_USE_SDSL)
    find_package(SDSL REQUIRED)
    find_package(divsufsort REQUIRED)
    find_package(divsufsort64 REQUIRED)
    target_link_libraries(pred_index INTERFACE ${SDSL} ${divsufsort} ${divsufsort64})
    target_include_directories(pred_index INTERFACE ${SDSL_INCLUDE_DIRS} ${LCE_INCLUDE_DIR})
endif()

if(LCE_BUILD_LA_VECTOR)
    target_link_libraries(pred_index INTERFACE la_vector)
endif()

add_library(rmq INTERFACE)

add_library(rmq_nlgn INTERFACE)
target_include_directories(rmq_nlgn INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(rmq_nlgn INTERFACE OpenMP::OpenMP_CXX)
target_link_libraries(rmq INTERFACE rmq_nlgn)

add_library(rmq_naive INTERFACE)
target_include_directories(rmq_naive INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(rmq_naive INTERFACE OpenMP::OpenMP_CXX)
target_link_libraries(rmq INTERFACE rmq_naive)

add_library(rmq_n INTERFACE)
target_include_directories(rmq_n INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(rmq_n INTERFACE OpenMP::OpenMP_CXX)
target_link_libraries(rmq INTERFACE rmq_n)

add_library(ds INTERFACE)

add_library(ds_naive INTERFACE)
target_include_directories(ds_naive INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(ds INTERFACE ds_naive)

add_library(ds_naive_wordwise INTERFACE)
target_include_directories(ds_naive_wordwise INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(ds INTERFACE ds_naive_wordwise)

add_library(ds_std INTERFACE)
target_include_directories(ds_std INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(ds INTERFACE ds_std)

add_library(lce_memcmp INTERFACE)
target_include_directories(lce_memcmp INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(ds INTERFACE lce_memcmp)

add_library(lce_rk_prezza INTERFACE)
target_link_libraries(lce_rk_prezza INTERFACE rk-lce)
target_link_libraries(ds INTERFACE lce_rk_prezza)

add_library(ds_fp INTERFACE)
target_include_directories(ds_fp INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(ds_fp INTERFACE rolling_hash OpenMP::OpenMP_CXX)
target_link_libraries(ds INTERFACE ds_fp)

add_library(ds_sss_naive INTERFACE)
target_include_directories(ds_sss_naive INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(ds_sss_naive INTERFACE lce_string_synchronizing_set pred_index fmt::fmt-header-only)
target_link_libraries(ds INTERFACE ds_sss_naive)

add_library(ds_sss_noss INTERFACE)
target_include_directories(ds_sss_noss INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(ds_sss_noss INTERFACE lce_string_synchronizing_set pred_index fmt::fmt-header-only)
target_link_libraries(ds INTERFACE ds_sss_noss)

add_library(ds_sss INTERFACE)
target_include_directories(ds_sss INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(ds_sss INTERFACE lce_string_synchronizing_set pred_index fmt::fmt-header-only)
target_link_libraries(ds INTERFACE ds_sss)

add_library(ds_classic INTERFACE)
target_include_directories(ds_classic INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(ds_classic INTERFACE gsaca_ds libsais libsais rmq fmt::fmt-header-only)
target_link_libraries(ds INTERFACE ds_classic)

add_library(ds_classic_for_sss INTERFACE)
target_include_directories(ds_classic_for_sss INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(ds_classic_for_sss INTERFACE gsaca_ds libsais libsais rmq fmt::fmt-header-only)
target_link_libraries(ds INTERFACE ds_classic_for_sss)

if(LCE_USE_SDSL)
    add_library(ds_sdsl_cst INTERFACE)
    target_compile_definitions(ds_sdsl_cst INTERFACE -DLCE_USE_SDSL)
    target_include_directories(ds_sdsl_cst INTERFACE ${SDSL_INCLUDE_DIRS} ${LCE_INCLUDE_DIR})
    target_link_libraries(ds_sdsl_cst INTERFACE ${SDSL} ${divsufsort} ${divsufsort64})
    target_link_libraries(ds INTERFACE ds_sdsl_cst)
endif()

add_library(lce INTERFACE)
target_link_libraries(lce INTERFACE ds rolling_hash pred util rmq)

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

if(LCE_BUILD_BENCHMARKS)
  add_executable(bench-lce bench/bench-lce.cpp)
  target_link_libraries(bench-lce PRIVATE ds tlx_clp fmt::fmt-header-only)
  if(${LCE_BENCHMARK_SPACE})
    target_compile_definitions(bench-lce PRIVATE -DLCE_BENCHMARK_SPACE)
    target_link_libraries(bench-lce PRIVATE malloc_count)
  endif()
  if(${LCE_BENCHMARK_INTERNAL})
    target_compile_definitions(bench-lce PRIVATE -DLCE_BENCHMARK_INTERNAL)
  endif()
  if(LCE_USE_SDSL)
    target_link_libraries(bench-lce PRIVATE ds_sdsl_cst)
  endif()

  add_executable(gen-queries bench/gen-queries.cpp)
  target_link_libraries(gen-queries PRIVATE tlx_clp)

  add_executable(gen-sa-lcp bench/gen-sa-lcp.cpp)
  target_link_libraries(gen-sa-lcp PRIVATE libsais)

  add_executable(bench-pred bench/bench-pred.cpp)
  target_link_libraries(bench-pred PRIVATE pred tlx_clp fmt::fmt-header-only util gsaca_ds)
  if(${LCE_BENCHMARK_SPACE})
    target_compile_definitions(bench-pred PRIVATE -DLCE_BENCHMARK_SPACE)
    target_link_libraries(bench-pred PRIVATE malloc_count)
  endif()
  if(${LCE_BENCHMARK_INTERNAL})
    target_compile_definitions(bench-pred PRIVATE -DLCE_BENCHMARK_INTERNAL)
  endif()

  add_executable(gen-sss bench/gen-sss.cpp)
  target_link_libraries(gen-sss PRIVATE tlx_clp lce_string_synchronizing_set fmt::fmt-header-only util gsaca_ds)

  set_target_properties(bench-lce gen-queries gen-sa-lcp bench-pred gen-sss
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bench)
endif()

foreach(s lce pred rmq rolling-hash string-synchronizing-set)
  add_executable("test-${s}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test-${s}.cpp")
  target_link_libraries("test-${s}" PRIVATE lce libsais GTest::gtest_main OpenMP::OpenMP_CXX)
  target_include_directories("test-${s}" PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  set_target_properties("test-${s}" PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
  gtest_discover_tests("test-${s}")
  foreach(_dll ${LCE_WIN_RUNTIME_DLLS})
    add_custom_command(TARGET "test-${s}" POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "${CMAKE_BINARY_DIR}/tests")
  endforeach()
endforeach()
