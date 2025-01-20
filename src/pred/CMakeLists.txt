add_executable(benchmark_pred benchmark.cpp)
target_link_libraries(benchmark_pred PRIVATE pred tlx_clp fmt::fmt-header-only util gsaca_ds)

if(${LCE_BENCHMARK_SPACE})
  target_compile_definitions(benchmark_pred PRIVATE -DLCE_BENCHMARK_SPACE)
  target_link_libraries(benchmark_pred PRIVATE malloc_count)
endif()
if(${LCE_BENCHMARK_INTERNAL})
  target_compile_definitions(benchmark_pred PRIVATE -DLCE_BENCHMARK_INTERNAL)
endif()

add_executable(gen_sss gen_sss.cpp)
target_link_libraries(gen_sss PRIVATE tlx_clp lce_string_synchronizing_set fmt::fmt-header-only util gsaca_ds)