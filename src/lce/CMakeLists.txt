add_executable(benchmark_lce benchmark.cpp)
target_link_libraries(benchmark_lce PRIVATE ds tlx_clp fmt::fmt-header-only)

if(${LCE_BENCHMARK_SPACE})
  target_compile_definitions(benchmark_lce PRIVATE -DLCE_BENCHMARK_SPACE)
  target_link_libraries(benchmark_lce PRIVATE malloc_count)
endif()
if(${LCE_BENCHMARK_INTERNAL})
  target_compile_definitions(benchmark_lce PRIVATE -DLCE_BENCHMARK_INTERNAL)
endif()

if(LCE_USE_SDSL)
  target_link_libraries(benchmark_lce PRIVATE ds_sdsl_cst)
endif()

add_executable(gen_queries gen_queries.cpp)
target_link_libraries(gen_queries PRIVATE tlx_clp)

add_executable(gen_sa_lcp gen_sa_lcp.cpp)
target_link_libraries(gen_sa_lcp PRIVATE libsais)