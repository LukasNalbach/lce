/*******************************************************************************
 * tests/lce/test_lce.cpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <vector>

#include "ds/lce_classic.hpp"
#include "ds/lce_fp.hpp"
#include "ds/lce_memcmp.hpp"
#include "ds/lce_naive.hpp"
#include "ds/lce_naive_std.hpp"
#include "ds/lce_naive_wordwise.hpp"
#include "ds/lce_naive_wordwise_xor.hpp"
#include "ds/lce_sss.hpp"
#include "ds/lce_sss_naive.hpp"
#include "ds/lce_sss_noss.hpp"

#include "test-progress.hpp"
#include "test-strings.hpp"

thread_local std::mt19937_64 gen(std::random_device{}());

using u128 = __uint128_t;
using i128 = __int128_t;


template <typename ds_t, typename ref_char_t, typename gen_t>
static void run_lce_checks(ds_t& ds, const lce::ds::lce_naive<ref_char_t>& ref, size_t n,
                           size_t num_queries, gen_t& g) {
  if (n == 0) return;
  std::uniform_int_distribution<size_t> pos_distrib(0, n - 1);

  for (size_t q = 0; q < num_queries; ++q) {
    size_t i = pos_distrib(g);
    size_t j = pos_distrib(g);

    if constexpr (requires { ds.lce(i, j); }) {
      EXPECT_EQ(ds.lce(i, j), ref.lce(i, j)) << "lce(" << i << "," << j << ") n=" << n;
    }

    if (i != j) {
      size_t l = std::min(i, j);
      size_t r = std::max(i, j);
      if constexpr (requires { ds.lce_lr(l, r); }) {
        EXPECT_EQ(ds.lce_lr(l, r), ref.lce_lr(l, r)) << "lce_lr(" << l << "," << r << ") n=" << n;
      }
      if constexpr (requires { ds.lce_mismatch(i, j); }) {
        EXPECT_EQ(ds.lce_mismatch(i, j), ref.lce_mismatch(i, j))
            << "lce_mismatch(" << i << "," << j << ") n=" << n;
      }
      if constexpr (requires { ds.is_leq_suffix(i, j); }) {
        EXPECT_EQ(ds.is_leq_suffix(i, j), ref.is_leq_suffix(i, j))
            << "is_leq_suffix(" << i << "," << j << ") n=" << n;
      }
      if constexpr (requires { ds.lce_up_to(i, j, size_t{0}); }) {
        size_t up_to = std::uniform_int_distribution<size_t>(0, n)(g);
        EXPECT_EQ(ds.lce_up_to(i, j, up_to), ref.lce_up_to(i, j, up_to))
            << "lce_up_to(" << i << "," << j << "," << up_to << ") n=" << n;
      }
    }
  }
}

template <typename char_t, typename gen_t>
static void verify_naive_variants(gen_t& g, uint64_t max_size, size_t num_queries) {
  std::vector<char_t> text = random_repetitive_input<std::vector<char_t>>(g, 1, max_size);
  size_t n = text.size();
  lce::ds::lce_naive<char_t> ref(text.data(), n);
  {
    fuzz_timer tb(fuzz_construct_ns());
    lce::ds::lce_naive_std<char_t> ds(text.data(), n);
    tb.stop();
    fuzz_timer tq(fuzz_query_ns());
    run_lce_checks(ds, ref, n, num_queries, g);
  }
  {
    lce::ds::lce_naive_wordwise<char_t> ds(text.data(), n);
    fuzz_timer tq(fuzz_query_ns());
    run_lce_checks(ds, ref, n, num_queries, g);
  }
  {
    lce::ds::lce_naive_wordwise_xor<char_t> ds(text.data(), n);
    fuzz_timer tq(fuzz_query_ns());
    run_lce_checks(ds, ref, n, num_queries, g);
  }
}

TEST(test_lce, naive_variants) {
  run_fuzz("lce-naive-variants", {
    {"verify", [](uint64_t it) {
       switch (it % 10) {
         case 0: verify_naive_variants<uint8_t>(gen, 1000000, 1000); break;
         case 1: verify_naive_variants<int8_t>(gen, 1000000, 1000); break;
         case 2: verify_naive_variants<uint16_t>(gen, 1000000, 1000); break;
         case 3: verify_naive_variants<int16_t>(gen, 1000000, 1000); break;
         case 4: verify_naive_variants<uint32_t>(gen, 1000000, 1000); break;
         case 5: verify_naive_variants<int32_t>(gen, 1000000, 1000); break;
         case 6: verify_naive_variants<uint64_t>(gen, 1000000, 1000); break;
         case 7: verify_naive_variants<int64_t>(gen, 1000000, 1000); break;
         case 8: verify_naive_variants<u128>(gen, 1000000, 1000); break;
         case 9: verify_naive_variants<i128>(gen, 1000000, 1000); break;
       }
     }, true},
  }, 640000);
}

TEST(test_lce, memcmp) {
  run_fuzz("lce-memcmp", {
    {"verify", [](uint64_t) {
       std::vector<uint8_t> text = random_repetitive_input<std::vector<uint8_t>>(gen, 2, 1000000);
       size_t n = text.size();
       lce::ds::lce_naive<uint8_t> ref(text.data(), n);
       fuzz_timer tb(fuzz_construct_ns());
       lce::ds::lce_memcmp ds(text.data(), n);
       tb.stop();
       fuzz_timer tq(fuzz_query_ns());
       run_lce_checks(ds, ref, n, 1000, gen);
     }, true},
  }, 1900000);
}

template <typename char_t, typename gen_t>
static void verify_classic(gen_t& g, uint64_t max_size, size_t num_queries) {
  std::vector<char_t> text = random_repetitive_input<std::vector<char_t>>(g, 2, max_size);
  size_t n = text.size();
  lce::ds::lce_naive<char_t> ref(text.data(), n);
  fuzz_timer tb(fuzz_construct_ns());
  lce::ds::lce_classic<char_t, uint32_t> ds(text.data(), n);
  tb.stop();
  fuzz_timer tq(fuzz_query_ns());
  run_lce_checks(ds, ref, n, num_queries, g);
}

TEST(test_lce, classic) {
  run_fuzz("lce-classic", {
    {"verify", [](uint64_t it) {
       switch (it % 5) {
         case 0: verify_classic<uint8_t>(gen, 200000, 400); break;
         case 1: verify_classic<uint16_t>(gen, 200000, 400); break;
         case 2: verify_classic<uint32_t>(gen, 200000, 400); break;
         case 3: verify_classic<uint64_t>(gen, 200000, 400); break;
         case 4: verify_classic<u128>(gen, 200000, 400); break;
       }
     }, false},
  }, 2800);
}

template <template <typename, uint64_t, typename, bool> class sss_ds_t, typename char_t, bool prefer_long,
          typename gen_t>
static void verify_sss(gen_t& g, uint64_t max_size, size_t num_queries) {
  constexpr uint64_t tau = 16;
  std::vector<char_t> text = random_repetitive_input<std::vector<char_t>>(g, 5 * tau + 100, max_size);
  size_t n = text.size();
  lce::ds::lce_naive<char_t> ref(text.data(), n);
  fuzz_timer tb(fuzz_construct_ns());
  sss_ds_t<char_t, tau, uint32_t, prefer_long> ds(text.data(), n);
  tb.stop();
  fuzz_timer tq(fuzz_query_ns());
  run_lce_checks(ds, ref, n, num_queries, g);
}

TEST(test_lce, sss) {
  run_fuzz("lce-sss", {
    {"lce_sss", [](uint64_t it) {
       switch (it % 4) {
         case 0: verify_sss<lce::ds::lce_sss, uint8_t, false>(gen, 500000, 400); break;
         case 1: verify_sss<lce::ds::lce_sss, int8_t, false>(gen, 500000, 400); break;
         case 2: verify_sss<lce::ds::lce_sss, uint8_t, true>(gen, 500000, 400); break;
         case 3: verify_sss<lce::ds::lce_sss, int8_t, true>(gen, 500000, 400); break;
       }
     }, false},
    {"lce_sss_naive", [](uint64_t it) {
       switch (it % 4) {
         case 0: verify_sss<lce::ds::lce_sss_naive, uint8_t, false>(gen, 500000, 400); break;
         case 1: verify_sss<lce::ds::lce_sss_naive, int8_t, false>(gen, 500000, 400); break;
         case 2: verify_sss<lce::ds::lce_sss_naive, uint8_t, true>(gen, 500000, 400); break;
         case 3: verify_sss<lce::ds::lce_sss_naive, int8_t, true>(gen, 500000, 400); break;
       }
     }, false},
    {"lce_sss_noss", [](uint64_t it) {
       switch (it % 4) {
         case 0: verify_sss<lce::ds::lce_sss_noss, uint8_t, false>(gen, 500000, 400); break;
         case 1: verify_sss<lce::ds::lce_sss_noss, int8_t, false>(gen, 500000, 400); break;
         case 2: verify_sss<lce::ds::lce_sss_noss, uint8_t, true>(gen, 500000, 400); break;
         case 3: verify_sss<lce::ds::lce_sss_noss, int8_t, true>(gen, 500000, 400); break;
       }
     }, false},
  }, 3600);
}

TEST(test_lce, fp) {
  run_fuzz("lce-fp", {
    {"verify", [](uint64_t) {
       std::vector<uint8_t> text = random_repetitive_input<std::vector<uint8_t>>(gen, 8, 30000000);
       size_t n = text.size() & ~size_t(7);
       if (n < 8) n = 8;
       std::vector<uint8_t> original(text.begin(), text.begin() + n);
       lce::ds::lce_naive<uint8_t> ref(original.data(), n);
       fuzz_timer tb(fuzz_construct_ns());
       lce::ds::lce_fp<uint8_t> ds(text.data(), n);
       tb.stop();
       fuzz_timer tq(fuzz_query_ns());
       run_lce_checks(ds, ref, n, 1500, gen);
     }, false},
  }, 3800);
}
