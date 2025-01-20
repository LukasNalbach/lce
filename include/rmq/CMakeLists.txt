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