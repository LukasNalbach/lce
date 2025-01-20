add_executable(
  test_lce
  test_lce.cpp
)
target_link_libraries(
  test_lce
  GTest::gtest_main
  ds
)

include(GoogleTest)
gtest_discover_tests(test_lce)
