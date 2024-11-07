add_executable(benchmark_lce benchmark.cpp)
target_link_libraries(benchmark_lce PRIVATE alx_lce tlx_clp fmt::fmt-header-only)

if(${ALX_BENCHMARK_SPACE})
  target_compile_definitions(benchmark_lce PRIVATE -DALX_BENCHMARK_SPACE)
  target_link_libraries(benchmark_lce PRIVATE malloc_count)
endif()
if(${ALX_BENCHMARK_INTERNAL})
  target_compile_definitions(benchmark_lce PRIVATE -DALX_BENCHMARK_INTERNAL)
endif()
target_link_libraries(benchmark_lce PRIVATE alx_lce_sdsl_cst)

add_executable(genqueries genqueries.cpp)
target_link_libraries(genqueries PRIVATE tlx_clp)