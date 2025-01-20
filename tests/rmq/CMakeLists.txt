add_executable(
  test_rmq
  test_rmq.cpp
)
target_link_libraries(
  test_rmq
  GTest::gtest_main
  rmq
)

include(GoogleTest)
gtest_discover_tests(test_rmq)
