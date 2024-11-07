add_library(alx_pred INTERFACE)

add_library(alx_pred_binsearch_std INTERFACE)
target_include_directories(alx_pred_binsearch_std INTERFACE ${ALX_INCLUDE_DIR})
target_link_libraries(alx_pred INTERFACE alx_pred_binsearch_std)

add_library(alx_pred_index INTERFACE)
target_include_directories(alx_pred_index INTERFACE ${ALX_INCLUDE_DIR})
target_link_libraries(alx_pred_index INTERFACE OpenMP::OpenMP_CXX)
target_link_libraries(alx_pred INTERFACE alx_pred_index)

add_library(j_index INTERFACE)
target_include_directories(j_index INTERFACE ${ALX_INCLUDE_DIR})
target_link_libraries(j_index INTERFACE OpenMP::OpenMP_CXX)
target_link_libraries(alx_pred INTERFACE j_index)

add_library(alx_pgm_index INTERFACE)
target_link_libraries(alx_pgm_index INTERFACE pgm)
target_link_libraries(alx_pred INTERFACE alx_pgm_index)

find_package(SDSL REQUIRED)
find_package(divsufsort REQUIRED)
find_package(divsufsort64 REQUIRED)

target_link_libraries(alx_pred_index INTERFACE ${SDSL} ${divsufsort} ${divsufsort64})
target_include_directories(alx_pred_index INTERFACE
    ${SDSL_INCLUDE_DIRS}
    ${ALX_INCLUDE_DIR}
)

target_link_libraries(alx_pred_index INTERFACE la_vector)
