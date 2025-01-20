add_library(util INTERFACE)
target_include_directories(util INTERFACE ${LCE_INCLUDE_DIR})
target_link_libraries(util INTERFACE fmt::fmt-header-only)