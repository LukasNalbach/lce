/*******************************************************************************
 * tests/rolling_hash/test_rolling_hash.cpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <vector>

#include "rolling_hash/mersenne_modular_arithmetic.hpp"
#include "rolling_hash/rolling_hash.hpp"

#include "test-progress.hpp"
#include "test-strings.hpp"

thread_local std::mt19937_64 gen(std::random_device{}());

using u128 = __uint128_t;

static constexpr uint64_t P61 = (uint64_t{1} << 61) - 1;
static constexpr u128 P107 = (u128{1} << 107) - 1;

static u128 rand_u128(std::mt19937_64& g) {
  std::uniform_int_distribution<uint64_t> d64;
  return ((u128)d64(g) << 64) | d64(g);
}

static void verify_modular_arithmetic(std::mt19937_64& g, int reps) {
  std::uniform_int_distribution<uint64_t> d64;

  for (int rep = 0; rep < reps; ++rep) {
    const uint64_t full64 = d64(g);
    EXPECT_EQ((lce::mersenne::mod<uint64_t, P61>(full64)), full64 % P61);

    const uint64_t small64 = d64(g) % (2 * (P61 - 1) + 1);
    EXPECT_EQ((lce::mersenne::small_num_mod<uint64_t, P61>(small64)), small64 % P61);
    EXPECT_EQ((lce::mersenne::small_num_mod_alt<uint64_t, P61>(small64)), small64 % P61);

    const uint64_t a64 = d64(g) % P61;
    const uint64_t b64 = d64(g) % P61;
    EXPECT_EQ((lce::mersenne::add_mod<uint64_t, P61>(a64, b64)), (a64 + b64) % P61);
    EXPECT_EQ((lce::mersenne::additive_inverse_mod<uint64_t, P61>(a64)), (P61 - a64) % P61);

    const u128 full128 = rand_u128(g);
    EXPECT_TRUE((lce::mersenne::mod<u128, P107>(full128)) == full128 % P107);

    const u128 small128 = rand_u128(g) % (2 * (P107 - 1) + 1);
    EXPECT_TRUE((lce::mersenne::small_num_mod<u128, P107>(small128)) == small128 % P107);
    EXPECT_TRUE((lce::mersenne::small_num_mod_alt<u128, P107>(small128)) == small128 % P107);

    const u128 a128 = rand_u128(g) % P107;
    const u128 b128 = rand_u128(g) % P107;
    EXPECT_TRUE((lce::mersenne::add_mod<u128, P107>(a128, b128)) == (a128 + b128) % P107);
    EXPECT_TRUE((lce::mersenne::additive_inverse_mod<u128, P107>(a128)) == (P107 - a128) % P107);
  }
}

template <size_t prime_exp>
static void verify_rolling_hash(std::mt19937_64& g, uint64_t max_size) {
  std::vector<uint8_t> text = random_repetitive_input<std::vector<uint8_t>>(g, 4, max_size);
  const size_t n = text.size();
  const size_t tau = std::uniform_int_distribution<size_t>(1, std::min<size_t>(n - 1, 1024))(g);
  const u128 base = std::uniform_int_distribution<uint64_t>(257, (1u << 20) - 1)(g);

  fuzz_timer tb(fuzz_construct_ns());
  lce::rolling_hash::rk_prime<prime_exp> roller(tau, base);
  lce::rolling_hash::rk_prime<prime_exp> fresh(tau, base);
  tb.stop();
  fuzz_timer tq(fuzz_query_ns());

  auto check_window = [&](size_t start) {
    fresh.reset();
    for (size_t k = start; k < start + tau; ++k) fresh.roll_in(text[k]);
    EXPECT_TRUE(roller.get_fp() == fresh.get_fp())
        << "window [" << start << "," << (start + tau) << ") n=" << n << " tau=" << tau
        << " prime_exp=" << prime_exp;
  };

  const size_t num_windows = n - tau + 1;
  const double check_prob = std::min(1.0, 50.0 / (double)num_windows);
  std::uniform_real_distribution<double> coin(0.0, 1.0);

  for (size_t i = 0; i < tau; ++i) roller.roll_in(text[i]);
  if (coin(g) < check_prob) check_window(0);
  for (size_t i = tau; i < n; ++i) {
    roller.roll(text[i - tau], text[i]);
    if (coin(g) < check_prob) check_window(i - tau + 1);
  }
}

static void verify_rolling_hash_mersenne61(std::mt19937_64& g, uint64_t max_size) {
  std::vector<uint8_t> text = random_repetitive_input<std::vector<uint8_t>>(g, 4, max_size);
  const size_t n = text.size();
  const size_t tau = std::uniform_int_distribution<size_t>(1, std::min<size_t>(n - 1, 1024))(g);
  const uint64_t max_base = std::uniform_int_distribution<int>(0, 1)(g) == 0 ? (uint64_t{1} << 32) - 1 : (uint64_t{1} << 61) - 2;
  const uint64_t base = std::uniform_int_distribution<uint64_t>(257, max_base)(g);

  fuzz_timer tb(fuzz_construct_ns());
  lce::rolling_hash::rk_mersenne61 roller(tau, base);
  tb.stop();
  fuzz_timer tq(fuzz_query_ns());

  auto fresh = [&](size_t start) {
    uint64_t fp = 0;
    for (size_t k = start; k < start + tau; ++k) fp = roller.roll_in(fp, text[k]);
    return fp;
  };

  auto reference = [&](size_t start) {
    u128 fp = 0;
    for (size_t k = start; k < start + tau; ++k) fp = (fp * base + text[k]) % P61;
    return uint64_t(fp);
  };

  const size_t num_windows = n - tau + 1;
  const double check_prob = std::min(1.0, 50.0 / (double)num_windows);
  std::uniform_real_distribution<double> coin(0.0, 1.0);

  uint64_t fp = fresh(0);
  EXPECT_EQ(fp, reference(0)) << "n=" << n << " tau=" << tau;
  for (size_t i = tau; i < n; ++i) {
    fp = roller.roll(fp, text[i - tau], text[i]);
    if (coin(g) < check_prob) {
      EXPECT_EQ(fp, reference(i - tau + 1)) << "window [" << (i - tau + 1) << "," << (i + 1) << ") n=" << n << " tau=" << tau;
    }
  }
}

TEST(test_rolling_hash, modular_arithmetic) {
  run_fuzz("modular-arithmetic", {
    {"verify", [](uint64_t) { verify_modular_arithmetic(gen, 4000); }, true},
  }, fuzz_iterations(200000));
}

TEST(test_rolling_hash, rolling_hash) {
  run_fuzz("rolling-hash", {
    {"verify", [](uint64_t it) {
       switch (it % 3) {
         case 0: verify_rolling_hash<61>(gen, 400000); break;
         case 1: verify_rolling_hash<89>(gen, 400000); break;
         case 2: verify_rolling_hash<107>(gen, 400000); break;
       }
     }, true},
  }, fuzz_iterations(100000));
}

TEST(test_rolling_hash, rolling_hash_mersenne61) {
  run_fuzz("rolling-hash-mersenne61", {
    {"verify", [](uint64_t) { verify_rolling_hash_mersenne61(gen, 400000); }, true},
  }, fuzz_iterations(200000));
}
