/*******************************************************************************
 * test/rolling_hash/test_rolling_hash.hpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include <gtest/gtest.h>
#include <hurchalla/modular_arithmetic/modular_multiplication.h>
#include <hurchalla/modular_arithmetic/modular_pow.h>

#include "rolling_hash/mersenne_modular_arithmetic.hpp"
#include "rolling_hash/modular_arithmetic.hpp"
#include "rolling_hash/rolling_hash.hpp"

__extension__ typedef unsigned __int128 uint128_t;

TEST(ModularArithmetic, MulMod64) {
  uint64_t mod63 = (uint64_t{1} << 63) - 1;
  uint64_t a_64{16'000'000'000'000'000'000ULL % mod63};
  uint64_t b_64{15'000'000'000'000'000'000ULL % mod63};
  // 16000000000000000000 * 15000000000000000000 mod (2^63)-1
  EXPECT_EQ(
      hurchalla::modular_multiplication_prereduced_inputs(a_64, b_64, mod63),
      494952449394867);

  uint32_t mod31 = (uint32_t{1} << 31) - 1;
  uint32_t a_32 = 4'000'000'000 % mod31;
  uint32_t b_32 = 3'500'000'000 % mod31;
  // 4000000000 * 3500000000 mod (2^31)-1
  EXPECT_EQ(
      hurchalla::modular_multiplication_prereduced_inputs(a_32, b_32, mod31),
      738982825);
}

TEST(ModularArithmetic, MulMod128) {
  uint128_t mod127 = (uint128_t{1} << 127) - 1;
  uint128_t a_128 = (uint128_t{16'000'000'000'000'000'000ULL} << 64) +
                    uint128_t{15'000'000'000'000'000'000ULL};
  a_128 %= mod127;
  uint128_t b_128 = (uint128_t{15'000'000'000'000'000'000ULL} << 64) +
                    uint128_t{16'000'000'000'000'000'000ULL};
  b_128 %= mod127;
  uint128_t result = (uint128_t{9'133'530'719'038'205'195} << 64) +
                     uint128_t{1'759'169'045'508'956'047};
  //(16000000000000000000 * 2^64 + 15000000000000000000) * (15000000000000000000
  //* 2^64 + 16000000000000000000) mod ((2^127) - 1)
  EXPECT_EQ(
      hurchalla::modular_multiplication_prereduced_inputs(a_128, b_128, mod127),
      result);
}

TEST(ModularArithmetic, MulPow64) {
  uint64_t mod63 = (uint64_t{1} << 63) - 1;
  uint64_t a_64{16'000'000'000'000'000'000ULL};
  uint64_t b_64{15'000'000'000'000'000'000ULL};
  // 16000000000000000000 ^ 15000000000000000000 mod (2^63)-1
  EXPECT_EQ(hurchalla::modular_pow(a_64, b_64, mod63),
            6'500'969'394'908'058'554ULL);
}

TEST(ModularArithmetic, MulPow128) {
  uint128_t mod127 = (uint128_t{1} << 127) - 1;
  uint128_t a_128 = (uint128_t{16'000'000'000'000'000'000ULL} << 64) +
                    uint128_t{15'000'000'000'000'000'000ULL};
  uint128_t b_128 = (uint128_t{15'000'000'000'000'000'000ULL} << 64) +
                    uint128_t{16'000'000'000'000'000'000ULL};
  uint128_t result = (uint128_t{8'277'472'356'650'270'234} << 64) +
                     uint128_t{2'777'364'698'120'919'522};
  // 295147905179352825871000000000000000000 ^
  // 276701161105643274256000000000000000000 mod ((2^127) - 1)
  // Result: 152692414140333008462721715371492317666
  EXPECT_EQ(hurchalla::modular_pow(a_128, b_128, mod127), result);
}

TEST(ModularArithmetic, SmallMod64) {
  constexpr uint64_t mod61 = (uint64_t{1} << 61) - 1;

  std::array<uint64_t, 5> nums{mod61 / 2, mod61 - 1, mod61, mod61 + 1,
                               (mod61 - 1) * 2};

  for (auto num : nums) {
    uint64_t res0 = num % mod61;
    uint64_t res1 = lce::mersenne::mod_naive<uint64_t, mod61>(num);
    EXPECT_EQ(res1, res0);

    uint64_t res2 = lce::mersenne::small_num_mod<uint64_t, mod61>(num);
    EXPECT_EQ(res2, res0);

    uint64_t res3 = lce::mersenne::small_num_mod_alt<uint64_t, mod61>(num);
    EXPECT_EQ(res3, res0);
  }
}

TEST(ModularArithmetic, SmallMod128) {
  constexpr uint128_t mod107 = (uint128_t{1} << 107) - 1;

  std::array<uint128_t, 5> nums{mod107 / 2, mod107 - 1, mod107, mod107 + 1,
                                (mod107 - 1) * 2};

  for (auto num : nums) {
    uint128_t res0 = num % mod107;
    uint128_t res1 = lce::mersenne::mod_naive<uint128_t, mod107>(num);
    EXPECT_EQ(res1, res0);

    uint128_t res2 = lce::mersenne::small_num_mod<uint128_t, mod107>(num);
    EXPECT_EQ(res2, res0);

    uint128_t res3 = lce::mersenne::small_num_mod_alt<uint128_t, mod107>(num);
    EXPECT_EQ(res3, res0);
  }
}

TEST(ModularArithmetic, Mod64) {
  constexpr uint64_t mod61 = (uint64_t{1} << 61) - 1;
  constexpr uint64_t mod63 = (uint64_t{1} << 63) - 1;

  std::array<uint64_t, 5> nums{mod63 / 2, mod63 - 1, mod63, mod63 + 1,
                               (mod63 - 1) * 2};

  for (auto num : nums) {
    uint64_t res0 = num % mod61;
    uint64_t res1 = lce::mersenne::mod_naive<uint64_t, mod61>(num);
    EXPECT_EQ(res1, res0);

    uint64_t res2 = lce::mersenne::mod<uint64_t, mod61>(num);
    EXPECT_EQ(res2, res0);
  }
}

TEST(ModularArithmetic, Mod128) {
  constexpr uint128_t mod107 = (uint128_t{1} << 107) - 1;
  constexpr uint128_t mod127 = (uint128_t{1} << 127) - 1;

  std::array<uint128_t, 5> nums{mod127 / 2, mod127 - 1, mod127, mod127 + 1,
                                (mod127 - 1) * 2};

  for (auto num : nums) {
    uint128_t res0 = num % mod107;
    uint128_t res1 = lce::mersenne::mod_naive<uint128_t, mod107>(num);
    EXPECT_EQ(res1, res0);

    uint64_t res2 = lce::mersenne::mod<uint128_t, mod107>(num);
    EXPECT_EQ(res2, res0);
  }
}

TEST(RollingHash, Roll) {
  std::string text =
      "Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam "
      "nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, "
      "sed diam voluptua. At vero eos et accusam et justo duo dolores et ea "
      "rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem "
      "ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing "
      "elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna "
      "aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo "
      "dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus "
      "est Lorem ipsum dolor sit amet.";
  size_t tau = 16;
  lce::rolling_hash::rk_prime rolling_hasher(tau, 123123);
  // roll
  for (size_t i = 0; i < tau; ++i) {
    rolling_hasher.roll_in(text[i]);
  }
  for (size_t i = tau; i < text.size(); ++i) {
    rolling_hasher.roll(text[i-tau], text[i]);
  }
  // check fp
  lce::rolling_hash::rk_prime rolling_hasher_end(tau, 123123);
  for (size_t i = text.size() - tau; i < text.size(); ++i) {
    rolling_hasher_end.roll_in(text[i]);
  }
  EXPECT_EQ(rolling_hasher.get_fp(), rolling_hasher_end.get_fp());
}