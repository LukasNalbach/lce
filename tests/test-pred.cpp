/*******************************************************************************
 * tests/pred/test_pred.cpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include "pred/binsearch_std.hpp"
#include "pred/j_index.hpp"
#include "pred/pred_index.hpp"

#include "test-progress.hpp"
#include "test-strings.hpp"

thread_local std::mt19937_64 gen(std::random_device{}());

using u128 = __uint128_t;

template <typename T>
static uint64_t value_ceiling() {
  uint64_t type_max = sizeof(T) >= 8 ? std::numeric_limits<uint64_t>::max() : (uint64_t)std::numeric_limits<T>::max();
  return std::min<uint64_t>(type_max - 1, 4000000);
}

template <typename T, typename gen_t>
static std::vector<T> random_sorted_distinct(gen_t& g, uint64_t max_n) {
  const uint64_t vmax = value_ceiling<T>();
  const uint64_t n = random_log_uniform_size(1, std::min<uint64_t>(max_n, vmax), g);

  std::vector<T> v;
  v.reserve(n);
  uint64_t slack = vmax - n;
  uint64_t cur = 1;
  for (uint64_t i = 0; i < n; ++i) {
    uint64_t extra = slack == 0 ? 0 : std::uniform_int_distribution<uint64_t>(0, slack / (n - i))(g);
    cur += extra;
    slack -= extra;
    v.push_back((T)cur);
    cur += 1;
  }
  return v;
}

template <typename T>
static lce::pred::result ref_predecessor(const std::vector<T>& d, T x) {
  if (x < d[0]) return {false, 0};
  return {true, (size_t)(std::upper_bound(d.begin(), d.end(), x) - d.begin() - 1)};
}

template <typename T>
static lce::pred::result ref_successor(const std::vector<T>& d, T x) {
  if (x > d.back()) return {false, 0};
  return {true, (size_t)(std::lower_bound(d.begin(), d.end(), x) - d.begin())};
}

static void expect_pred_eq(lce::pred::result got, lce::pred::result exp, const char* what, uint64_t x,
                           size_t n) {
  EXPECT_EQ(got.exists, exp.exists) << what << " exists, x=" << x << " n=" << n;
  if (got.exists && exp.exists) {
    EXPECT_EQ(got.pos, exp.pos) << what << " pos, x=" << x << " n=" << n;
  }
}

template <typename pred_t, typename T, typename gen_t>
static void verify_pred(gen_t& g, uint64_t max_n, size_t num_queries) {
  std::vector<T> data = random_sorted_distinct<T>(g, max_n);
  const size_t n = data.size();

  fuzz_timer tb(fuzz_construct_ns());
  pred_t ds(data.data(), n);
  tb.stop();
  fuzz_timer tq(fuzz_query_ns());

  const uint64_t back = (uint64_t)data.back();
  const uint64_t front = (uint64_t)data[0];
  const uint64_t xhi = std::min<uint64_t>(value_ceiling<T>(), back + 3);

  for (size_t q = 0; q < num_queries; ++q) {
    const uint64_t xu = std::uniform_int_distribution<uint64_t>(0, xhi)(g);
    const T x = (T)xu;

    lce::pred::result exp_pred = ref_predecessor(data, x);
    lce::pred::result exp_succ = ref_successor(data, x);
    expect_pred_eq(ds.predecessor(x), exp_pred, "predecessor", xu, n);
    expect_pred_eq(ds.successor(x), exp_succ, "successor", xu, n);

    if constexpr (requires { ds.contains(x); }) {
      EXPECT_EQ(ds.contains(x), std::binary_search(data.begin(), data.end(), x))
          << "contains x=" << xu << " n=" << n;
    }
    if constexpr (requires { ds.predecessor_unsafe(x); }) {
      if (xu >= front) {
        EXPECT_EQ((size_t)ds.predecessor_unsafe(x), exp_pred.pos) << "predecessor_unsafe x=" << xu;
      }
    }
    if constexpr (requires { ds.successor_unsafe(x); }) {
      if (xu <= back) {
        EXPECT_EQ((size_t)ds.successor_unsafe(x), exp_succ.pos) << "successor_unsafe x=" << xu;
      }
    }
  }
}

TEST(test_pred, binsearch_std) {
  run_fuzz("pred-binsearch-std", {
    {"verify", [](uint64_t it) {
       switch (it % 9) {
         case 0: verify_pred<lce::pred::binsearch_std<uint8_t>, uint8_t>(gen, 200000, 4000); break;
         case 1: verify_pred<lce::pred::binsearch_std<int8_t>, int8_t>(gen, 200000, 4000); break;
         case 2: verify_pred<lce::pred::binsearch_std<uint16_t>, uint16_t>(gen, 200000, 4000); break;
         case 3: verify_pred<lce::pred::binsearch_std<int16_t>, int16_t>(gen, 200000, 4000); break;
         case 4: verify_pred<lce::pred::binsearch_std<uint32_t>, uint32_t>(gen, 200000, 4000); break;
         case 5: verify_pred<lce::pred::binsearch_std<int32_t>, int32_t>(gen, 200000, 4000); break;
         case 6: verify_pred<lce::pred::binsearch_std<uint64_t>, uint64_t>(gen, 200000, 4000); break;
         case 7: verify_pred<lce::pred::binsearch_std<int64_t>, int64_t>(gen, 200000, 4000); break;
         case 8: verify_pred<lce::pred::binsearch_std<u128>, u128>(gen, 200000, 4000); break;
       }
     }, true},
  }, 170000);
}

TEST(test_pred, pred_index) {
  run_fuzz("pred-index", {
    {"verify", [](uint64_t it) {
       switch (it % 3) {
         case 0: verify_pred<lce::pred::pred_index<uint16_t, 7, uint32_t>, uint16_t>(gen, 60000, 4000); break;
         case 1: verify_pred<lce::pred::pred_index<uint32_t, 7, uint32_t>, uint32_t>(gen, 4000000, 4000); break;
         case 2: verify_pred<lce::pred::pred_index<uint64_t, 7, uint32_t>, uint64_t>(gen, 4000000, 4000); break;
       }
     }, true},
  }, 110000);
}

TEST(test_pred, j_index) {
  run_fuzz("pred-j-index", {
    {"verify", [](uint64_t it) {
       switch (it % 6) {
         case 0: verify_pred<lce::pred::j_index<uint8_t>, uint8_t>(gen, 200000, 4000); break;
         case 1: verify_pred<lce::pred::j_index<uint16_t>, uint16_t>(gen, 200000, 4000); break;
         case 2: verify_pred<lce::pred::j_index<uint32_t>, uint32_t>(gen, 4000000, 4000); break;
         case 3: verify_pred<lce::pred::j_index<int32_t>, int32_t>(gen, 4000000, 4000); break;
         case 4: verify_pred<lce::pred::j_index<uint64_t>, uint64_t>(gen, 4000000, 4000); break;
         case 5: verify_pred<lce::pred::j_index<int64_t>, int64_t>(gen, 4000000, 4000); break;
       }
     }, true},
  }, 105000);
}
