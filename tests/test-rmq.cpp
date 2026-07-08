/*******************************************************************************
 * tests/rmq/test_rmq.cpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <vector>

#include "rmq/rmq_n.hpp"
#include "rmq/rmq_naive.hpp"
#include "rmq/rmq_nlgn.hpp"

#include "test-progress.hpp"
#include "test-strings.hpp"

thread_local std::mt19937_64 gen(std::random_device{}());

using u128 = __uint128_t;

template <typename key_t, typename gen_t>
static std::vector<key_t> random_rmq_input(gen_t& g, uint64_t min_size, uint64_t max_size) {
  int sigma = std::uniform_int_distribution<int>(2, 8)(g);
  std::vector<uint8_t> bytes =
      random_repetitive_input<std::vector<uint8_t>>(g, min_size, max_size, 0, (uint8_t)(sigma - 1));
  std::vector<key_t> data(bytes.size());
  for (size_t i = 0; i < bytes.size(); ++i) data[i] = (key_t)bytes[i];
  return data;
}

template <typename rmq_ds_t, typename key_t, typename gen_t>
static void verify_rmq(gen_t& g, uint64_t max_size, size_t num_queries) {
  std::vector<key_t> data = random_rmq_input<key_t>(g, 1, max_size);
  const size_t n = data.size();

  lce::rmq::rmq_naive<key_t> ref(data);
  fuzz_timer tb(fuzz_construct_ns());
  rmq_ds_t ds(data);
  tb.stop();
  fuzz_timer tq(fuzz_query_ns());

  std::uniform_int_distribution<size_t> pos_distrib(0, n - 1);
  for (size_t q = 0; q < num_queries; ++q) {
    size_t i = pos_distrib(g);
    size_t j = pos_distrib(g);
    EXPECT_EQ(ds.rmq(i, j), ref.rmq(i, j)) << "rmq(" << i << "," << j << ") n=" << n;
    if (i != j) {
      EXPECT_EQ(ds.rmq_shifted(i, j), ref.rmq_shifted(i, j))
          << "rmq_shifted(" << i << "," << j << ") n=" << n;
    }
  }
}

TEST(test_rmq, all) {
  run_fuzz("rmq", {
    {"rmq-n", [](uint64_t it) {
       switch (it % 8) {
         case 0: verify_rmq<lce::rmq::rmq_n<uint8_t>, uint8_t>(gen, 2000000, 2000); break;
         case 1: verify_rmq<lce::rmq::rmq_n<int8_t>, int8_t>(gen, 2000000, 2000); break;
         case 2: verify_rmq<lce::rmq::rmq_n<uint16_t>, uint16_t>(gen, 2000000, 2000); break;
         case 3: verify_rmq<lce::rmq::rmq_n<int16_t>, int16_t>(gen, 2000000, 2000); break;
         case 4: verify_rmq<lce::rmq::rmq_n<uint32_t>, uint32_t>(gen, 2000000, 2000); break;
         case 5: verify_rmq<lce::rmq::rmq_n<int32_t>, int32_t>(gen, 2000000, 2000); break;
         case 6: verify_rmq<lce::rmq::rmq_n<uint64_t>, uint64_t>(gen, 2000000, 2000); break;
         case 7: verify_rmq<lce::rmq::rmq_n<u128>, u128>(gen, 2000000, 2000); break;
       }
     }, true},
    {"rmq-nlgn", [](uint64_t it) {
       switch (it % 8) {
         case 0: verify_rmq<lce::rmq::rmq_nlgn<uint8_t>, uint8_t>(gen, 2000000, 2000); break;
         case 1: verify_rmq<lce::rmq::rmq_nlgn<int8_t>, int8_t>(gen, 2000000, 2000); break;
         case 2: verify_rmq<lce::rmq::rmq_nlgn<uint16_t>, uint16_t>(gen, 2000000, 2000); break;
         case 3: verify_rmq<lce::rmq::rmq_nlgn<int16_t>, int16_t>(gen, 2000000, 2000); break;
         case 4: verify_rmq<lce::rmq::rmq_nlgn<uint32_t>, uint32_t>(gen, 2000000, 2000); break;
         case 5: verify_rmq<lce::rmq::rmq_nlgn<int32_t>, int32_t>(gen, 2000000, 2000); break;
         case 6: verify_rmq<lce::rmq::rmq_nlgn<uint64_t>, uint64_t>(gen, 2000000, 2000); break;
         case 7: verify_rmq<lce::rmq::rmq_nlgn<u128>, u128>(gen, 2000000, 2000); break;
       }
     }, true},
  }, 1200);
}
