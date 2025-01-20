/*******************************************************************************
 * test/lce/test_rmq.cpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include <gtest/gtest.h>

#include <limits>
#include <numeric>

#include "rmq/rmq_n.hpp"
#include "rmq/rmq_naive.hpp"
#include "rmq/rmq_nlgn.hpp"

template <typename rmq_ds_type>
void test_empty_constructor() {
  rmq_ds_type ds;
}

template <typename rmq_ds_type>
void test_simple() {
  using key_type = rmq_ds_type::key_type;
  std::vector<key_type> random_data(10'000);
  std::generate(random_data.begin(), random_data.end(), std::rand);

  rmq_ds_type rmq(random_data);

  for (size_t i = 0; i < random_data.size() - 100; ++i) {
    size_t res = rmq.rmq(i + 99, i);
    size_t check = i;
    for (size_t j = 1; j < 100; ++j) {
      if (random_data[check] > random_data[i + j]) {
        check = i + j;
      }
    }
    ASSERT_EQ(res, check);
  }

  for (size_t i = 0; i < random_data.size() - 1000; ++i) {
    size_t res = rmq.rmq(i + 999, i);
    size_t check = i;
    for (size_t j = 1; j < 1000; ++j) {
      if (random_data[check] > random_data[i + j]) {
        check = i + j;
      }
    }
    ASSERT_EQ(res, check);
  }

  for (size_t i = 0; i < random_data.size() - 1000; ++i) {
    size_t res = rmq.rmq_shifted(i + 999, i);
    size_t check = i + 1;
    for (size_t j = 2; j < 1000; ++j) {
      if (random_data[check] > random_data[i + j]) {
        check = i + j;
      }
    }
    ASSERT_EQ(res, check);
  }
}

TEST(RmqNaive, All) {
  test_empty_constructor<lce::rmq::rmq_naive<uint64_t>>();
  test_simple<lce::rmq::rmq_naive<unsigned char>>();
  test_simple<lce::rmq::rmq_naive<uint8_t>>();
  test_simple<lce::rmq::rmq_naive<uint32_t>>();
  test_simple<lce::rmq::rmq_naive<int32_t>>();
  test_simple<lce::rmq::rmq_naive<uint64_t>>();
  test_simple<lce::rmq::rmq_naive<int64_t>>();
  test_simple<lce::rmq::rmq_naive<__uint128_t>>();
}

TEST(RmqNlgn, All) {
  test_empty_constructor<lce::rmq::rmq_nlgn<uint64_t>>();
  test_simple<lce::rmq::rmq_nlgn<unsigned char>>();
  test_simple<lce::rmq::rmq_nlgn<uint8_t>>();
  test_simple<lce::rmq::rmq_nlgn<uint32_t>>();
  test_simple<lce::rmq::rmq_nlgn<int32_t>>();
  test_simple<lce::rmq::rmq_nlgn<uint64_t>>();
  test_simple<lce::rmq::rmq_nlgn<int64_t>>();
  test_simple<lce::rmq::rmq_nlgn<__uint128_t>>();
}

TEST(RmqN, All) {
  test_empty_constructor<lce::rmq::rmq_n<uint64_t>>();
  test_simple<lce::rmq::rmq_n<unsigned char>>();
  test_simple<lce::rmq::rmq_n<uint8_t>>();
  test_simple<lce::rmq::rmq_n<uint32_t>>();
  test_simple<lce::rmq::rmq_n<int32_t>>();
  test_simple<lce::rmq::rmq_n<uint64_t>>();
  test_simple<lce::rmq::rmq_n<int64_t>>();
  test_simple<lce::rmq::rmq_n<__uint128_t>>();
}
