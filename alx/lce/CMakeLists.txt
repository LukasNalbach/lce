add_library(alx_lce INTERFACE)

add_library(alx_lce_naive INTERFACE)
target_include_directories(alx_lce_naive INTERFACE ${ALX_INCLUDE_DIR})
target_link_libraries(alx_lce INTERFACE alx_lce_naive)

add_library(alx_lce_naive_wordwise INTERFACE)
target_include_directories(alx_lce_naive_wordwise INTERFACE ${ALX_INCLUDE_DIR})
target_link_libraries(alx_lce INTERFACE alx_lce_naive_wordwise)

add_library(alx_lce_std INTERFACE)
target_include_directories(alx_lce_std INTERFACE ${ALX_INCLUDE_DIR})
target_link_libraries(alx_lce INTERFACE alx_lce_std)

add_library(alx_memcmp INTERFACE)
target_include_directories(alx_memcmp INTERFACE ${ALX_INCLUDE_DIR})
target_link_libraries(alx_lce INTERFACE alx_memcmp)

add_library(alx_rk_prezza INTERFACE)
target_link_libraries(alx_rk_prezza INTERFACE rk-lce)
target_link_libraries(alx_lce INTERFACE alx_rk_prezza)

add_library(alx_lce_fp INTERFACE)
target_include_directories(alx_lce_fp INTERFACE ${ALX_INCLUDE_DIR})
target_link_libraries(alx_lce_fp INTERFACE alx_rolling_hash OpenMP::OpenMP_CXX)
target_link_libraries(alx_lce INTERFACE alx_lce_fp)

add_library(alx_lce_sss_naive INTERFACE)
target_include_directories(alx_lce_sss_naive INTERFACE ${ALX_INCLUDE_DIR})
target_link_libraries(alx_lce_sss_naive INTERFACE alx_string_synchronizing_set alx_pred_index fmt::fmt-header-only)
target_link_libraries(alx_lce INTERFACE alx_lce_sss_naive)

add_library(alx_lce_sss_noss INTERFACE)
target_include_directories(alx_lce_sss_noss INTERFACE ${ALX_INCLUDE_DIR})
target_link_libraries(alx_lce_sss_noss INTERFACE alx_string_synchronizing_set alx_pred_index fmt::fmt-header-only)
target_link_libraries(alx_lce INTERFACE alx_lce_sss_noss)

add_library(alx_lce_sss INTERFACE)
target_include_directories(alx_lce_sss INTERFACE ${ALX_INCLUDE_DIR})
target_link_libraries(alx_lce_sss INTERFACE alx_string_synchronizing_set alx_pred_index fmt::fmt-header-only)
target_link_libraries(alx_lce INTERFACE alx_lce_sss)

add_library(alx_lce_classic INTERFACE)
target_include_directories(alx_lce_classic INTERFACE ${ALX_INCLUDE_DIR})
target_link_libraries(alx_lce_classic INTERFACE gsaca_ds libsais64 libsais alx_rmq fmt::fmt-header-only)
target_link_libraries(alx_lce INTERFACE alx_lce_classic)

add_library(alx_lce_classic_for_sss INTERFACE)
target_include_directories(alx_lce_classic_for_sss INTERFACE ${ALX_INCLUDE_DIR})
target_link_libraries(alx_lce_classic_for_sss INTERFACE gsaca_ds libsais64 libsais alx_rmq fmt::fmt-header-only)
target_link_libraries(alx_lce INTERFACE alx_lce_classic_for_sss)

find_package(SDSL REQUIRED)
find_package(divsufsort REQUIRED)
find_package(divsufsort64 REQUIRED)

add_library(alx_lce_sdsl_cst INTERFACE)
target_include_directories(alx_lce_sdsl_cst INTERFACE
${SDSL_INCLUDE_DIRS}
${ALX_INCLUDE_DIR}
)
target_link_libraries(alx_lce_sdsl_cst INTERFACE ${SDSL} ${divsufsort} ${divsufsort64})
target_link_libraries(alx_lce INTERFACE alx_lce_sdsl_cst)