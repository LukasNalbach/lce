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

option(LCE_BUILD_LA_VECTOR "build lce data structure that depends on SDSL" OFF)
if(LCE_BUILD_LA_VECTOR)
    target_link_libraries(pred_index INTERFACE la_vector)
endif()