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
    find_package(SDSL REQUIRED)
    find_package(divsufsort REQUIRED)
    find_package(divsufsort64 REQUIRED)

    add_library(ds_sdsl_cst INTERFACE)
    target_compile_definitions(ds_sdsl_cst INTERFACE -DLCE_USE_SDSL)
    target_include_directories(ds_sdsl_cst INTERFACE ${SDSL_INCLUDE_DIRS} ${LCE_INCLUDE_DIR})
    target_link_libraries(ds_sdsl_cst INTERFACE ${SDSL} ${divsufsort} ${divsufsort64})
    target_link_libraries(ds INTERFACE ds_sdsl_cst)
endif()