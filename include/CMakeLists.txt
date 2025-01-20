set(LCE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_library(lce INTERFACE)

add_subdirectory(ds)
target_link_libraries(lce INTERFACE ds)

add_subdirectory(rolling_hash)
target_link_libraries(lce INTERFACE rolling_hash)

add_subdirectory(pred)
target_link_libraries(lce INTERFACE pred)

add_subdirectory(util)
target_link_libraries(lce INTERFACE util)

add_subdirectory(rmq)
target_link_libraries(lce INTERFACE rmq)