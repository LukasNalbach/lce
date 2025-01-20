/*******************************************************************************
 * test/lce/test_lce.cpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include <gtest/gtest.h>

#include <limits>
#include <numeric>

#include "ds/lce_classic.hpp"
#include "ds/lce_fp.hpp"
#include "ds/lce_memcmp.hpp"
#include "ds/lce_naive.hpp"
#include "ds/lce_naive_std.hpp"
#include "ds/lce_naive_wordwise.hpp"
#include "ds/lce_naive_wordwise_xor.hpp"
#include "ds/lce_rk_prezza.hpp"
#include "ds/lce_sss.hpp"
#include "ds/lce_sss_naive.hpp"
#include "ds/lce_sss_noss.hpp"

template <typename ds_type>
void test_empty_constructor() {
  ds_type ds;
}

template <typename ds_type>
void test_simple() {
  typedef typename ds_type::char_type char_typee;
  std::vector<char_typee> text(2000);
  std::iota(text.begin(), text.begin() + 1000,
            std::numeric_limits<char_typee>::max() / 2);
  std::iota(text.begin() + 1000, text.end(),
            std::numeric_limits<char_typee>::max() / 2);
  for (auto& c : text) {
    if (c == std::numeric_limits<char_typee>::max()) {
      c = 0;
    }
  }
  std::vector<char_typee> text_copy = text;
  // Test pointer constructor
  {
    ds_type ds(text.data(), text.size());
    EXPECT_EQ(ds.lce(0, 0), 2000);
    EXPECT_EQ(ds.lce(0, 1000), 1000);
    EXPECT_EQ(ds.lce(500, 1000), 0);
  }
  text = text_copy;

  // Test container constructor
  {
    ds_type ds(text);
    EXPECT_EQ(ds.lce(0, 0), 2000);
    EXPECT_EQ(ds.lce(0, 1000), 1000);
    EXPECT_EQ(ds.lce(500, 1000), 0);
  }
  text = text_copy;
}

template <typename ds_type>
void test_simple_classic_for_sss() {
  std::vector<uint8_t> text(2000);
  std::iota(text.begin(), text.begin() + 1000,
            std::numeric_limits<uint8_t>::max() / 2);
  std::iota(text.begin() + 1000, text.end(),
            std::numeric_limits<uint8_t>::max() / 2);
  for (auto& c : text) {
    if (c == std::numeric_limits<uint8_t>::max()) {
      c = 0;
    }
  }
  std::vector<uint32_t> sss{0, 500, 1000};
  std::vector<uint32_t> fps_reduced{2, 0, 1};
  std::vector<uint8_t> text_copy = text;
  // Test pointer constructor
  {
    ds_type ds(text.data(), text.size(), fps_reduced.data(),
                   fps_reduced.size(), sss);
    EXPECT_EQ(ds.lce_uneq(0, 2), 1000);
    EXPECT_EQ(ds.lce_uneq(1, 2), 0);
  }
  text = text_copy;
}
template <typename ds_type>
void test_suffix_sorting() {
  typedef typename ds_type::char_type char_typee;
  std::vector<char_typee> text(200);
  std::iota(text.begin(), text.begin() + 100,
            std::numeric_limits<char_typee>::max() / 2);
  std::iota(text.begin() + 100, text.end(),
            std::numeric_limits<char_typee>::max() / 2);

  ds_type ds(text);
  EXPECT_EQ(ds.is_leq_suffix(50, 150), false);
  EXPECT_EQ(ds.is_leq_suffix(150, 50), true);
  EXPECT_EQ(ds.is_leq_suffix(0, 50), true);
  EXPECT_EQ(ds.is_leq_suffix(50, 0), false);
}

template <typename ds_type, bool lr = true, bool mm = true, bool suf = true,
          bool upto = true>
void test_variants() {
  typedef typename ds_type::char_type char_typee;
  std::vector<char_typee> text(200);
  std::iota(text.begin(), text.begin() + 100,
            std::numeric_limits<char_typee>::max() / 2);
  std::iota(text.begin() + 100, text.end(),
            std::numeric_limits<char_typee>::max() / 2);
  for (auto& c : text) {
    if (c == std::numeric_limits<char_typee>::max()) {
      c = 0;
    }
  }
  ds_type ds(text);
  if constexpr (lr) {
    EXPECT_EQ(ds.lce_lr(0, 100), 100);
    EXPECT_EQ(ds.lce(50, 100), 0);
  }

  if constexpr (mm) {
    EXPECT_EQ(ds.lce_mismatch(100, 0), std::make_pair(false, size_t{100}));
    EXPECT_EQ(ds.lce_mismatch(100, 50), std::make_pair(true, size_t{0}));
  }
  if constexpr (suf) {
    EXPECT_EQ(ds.is_leq_suffix(50, 150), false);
    EXPECT_EQ(ds.is_leq_suffix(150, 50), true);
    EXPECT_EQ(ds.is_leq_suffix(0, 50), true);
    EXPECT_EQ(ds.is_leq_suffix(50, 0), false);
  }
  if constexpr (upto) {
    EXPECT_EQ(ds.lce_up_to(100, 0, 20), size_t{20});
    EXPECT_EQ(ds.lce_up_to(100, 50, 20), size_t{0});
  }
}

template <typename ds_type>
void test_retransform() {
  typedef typename ds_type::char_type char_typee;
  std::vector<char_typee> text(2000);
  std::iota(text.begin(), text.begin() + 1000,
            std::numeric_limits<char_typee>::max() / 2);
  std::iota(text.begin() + 1000, text.end(),
            std::numeric_limits<char_typee>::max() / 2);

  std::vector<char_typee> text_copy = text;
  ds_type ds(text);

  for (size_t i = 0; i < text.size(); ++i) {
    ASSERT_EQ(ds[i], text_copy[i]) << i;
  }
  ds.retransform_text();
  for (size_t i = 0; i < text.size(); ++i) {
    ASSERT_EQ(text[i], text_copy[i]) << i;
  }
}

TEST(LceNaive, All) {
  test_empty_constructor<lce::ds::lce_naive<uint8_t>>();

  test_simple<lce::ds::lce_naive<uint8_t>>();
  test_simple<lce::ds::lce_naive<int8_t>>();
  test_simple<lce::ds::lce_naive<uint16_t>>();
  test_simple<lce::ds::lce_naive<int16_t>>();
  test_simple<lce::ds::lce_naive<uint32_t>>();
  test_simple<lce::ds::lce_naive<int32_t>>();
  test_simple<lce::ds::lce_naive<uint64_t>>();
  test_simple<lce::ds::lce_naive<int64_t>>();
  test_simple<lce::ds::lce_naive<__uint128_t>>();
  test_simple<lce::ds::lce_naive<__int128_t>>();

  test_variants<lce::ds::lce_naive<uint8_t>>();
  test_variants<lce::ds::lce_naive<int8_t>>();
  test_variants<lce::ds::lce_naive<uint16_t>>();
  test_variants<lce::ds::lce_naive<int16_t>>();
  test_variants<lce::ds::lce_naive<uint32_t>>();
  test_variants<lce::ds::lce_naive<int32_t>>();
  test_variants<lce::ds::lce_naive<uint64_t>>();
  test_variants<lce::ds::lce_naive<int64_t>>();
  test_variants<lce::ds::lce_naive<__uint128_t>>();
  test_variants<lce::ds::lce_naive<__int128_t>>();
}

TEST(LceNaiveStd, All) {
  test_empty_constructor<lce::ds::lce_naive_std<uint8_t>>();

  test_simple<lce::ds::lce_naive_std<uint8_t>>();
  test_simple<lce::ds::lce_naive_std<int8_t>>();
  test_simple<lce::ds::lce_naive_std<uint16_t>>();
  test_simple<lce::ds::lce_naive_std<int16_t>>();
  test_simple<lce::ds::lce_naive_std<uint32_t>>();
  test_simple<lce::ds::lce_naive_std<int32_t>>();
  test_simple<lce::ds::lce_naive_std<uint64_t>>();
  test_simple<lce::ds::lce_naive_std<int64_t>>();
  test_simple<lce::ds::lce_naive_std<__uint128_t>>();
  test_simple<lce::ds::lce_naive_std<__int128_t>>();

  test_variants<lce::ds::lce_naive_std<uint8_t>>();
  test_variants<lce::ds::lce_naive_std<int8_t>>();
  test_variants<lce::ds::lce_naive_std<uint16_t>>();
  test_variants<lce::ds::lce_naive_std<int16_t>>();
  test_variants<lce::ds::lce_naive_std<uint32_t>>();
  test_variants<lce::ds::lce_naive_std<int32_t>>();
  test_variants<lce::ds::lce_naive_std<uint64_t>>();
  test_variants<lce::ds::lce_naive_std<int64_t>>();
  test_variants<lce::ds::lce_naive_std<__uint128_t>>();
  test_variants<lce::ds::lce_naive_std<__int128_t>>();
}

TEST(LceNaiveWordwise, All) {
  test_empty_constructor<lce::ds::lce_naive_wordwise<uint8_t>>();

  test_simple<lce::ds::lce_naive_wordwise<uint8_t>>();
  test_simple<lce::ds::lce_naive_wordwise<int8_t>>();
  test_simple<lce::ds::lce_naive_wordwise<uint16_t>>();
  test_simple<lce::ds::lce_naive_wordwise<int16_t>>();
  test_simple<lce::ds::lce_naive_wordwise<uint32_t>>();
  test_simple<lce::ds::lce_naive_wordwise<int32_t>>();
  test_simple<lce::ds::lce_naive_wordwise<uint64_t>>();
  test_simple<lce::ds::lce_naive_wordwise<int64_t>>();
  test_simple<lce::ds::lce_naive_wordwise<__uint128_t>>();
  test_simple<lce::ds::lce_naive_wordwise<__int128_t>>();

  test_variants<lce::ds::lce_naive_wordwise<uint8_t>>();
  test_variants<lce::ds::lce_naive_wordwise<int8_t>>();
  test_variants<lce::ds::lce_naive_wordwise<uint16_t>>();
  test_variants<lce::ds::lce_naive_wordwise<int16_t>>();
  test_variants<lce::ds::lce_naive_wordwise<uint32_t>>();
  test_variants<lce::ds::lce_naive_wordwise<int32_t>>();
  test_variants<lce::ds::lce_naive_wordwise<uint64_t>>();
  test_variants<lce::ds::lce_naive_wordwise<int64_t>>();
  test_variants<lce::ds::lce_naive_wordwise<__uint128_t>>();
  test_variants<lce::ds::lce_naive_wordwise<__int128_t>>();
}

TEST(LceNaiveWordwiseXor, All) {
  test_empty_constructor<lce::ds::lce_naive_wordwise_xor<uint8_t>>();

  test_simple<lce::ds::lce_naive_wordwise_xor<uint8_t>>();
  test_simple<lce::ds::lce_naive_wordwise_xor<int8_t>>();
  test_simple<lce::ds::lce_naive_wordwise_xor<uint16_t>>();
  test_simple<lce::ds::lce_naive_wordwise_xor<int16_t>>();
  test_simple<lce::ds::lce_naive_wordwise_xor<uint32_t>>();
  test_simple<lce::ds::lce_naive_wordwise_xor<int32_t>>();
  test_simple<lce::ds::lce_naive_wordwise_xor<uint64_t>>();
  test_simple<lce::ds::lce_naive_wordwise_xor<int64_t>>();
  test_simple<lce::ds::lce_naive_wordwise_xor<__uint128_t>>();
  test_simple<lce::ds::lce_naive_wordwise_xor<__int128_t>>();

  test_variants<lce::ds::lce_naive_wordwise_xor<uint8_t>>();
  test_variants<lce::ds::lce_naive_wordwise_xor<int8_t>>();
  test_variants<lce::ds::lce_naive_wordwise_xor<uint16_t>>();
  test_variants<lce::ds::lce_naive_wordwise_xor<int16_t>>();
  test_variants<lce::ds::lce_naive_wordwise_xor<uint32_t>>();
  test_variants<lce::ds::lce_naive_wordwise_xor<int32_t>>();
  test_variants<lce::ds::lce_naive_wordwise_xor<uint64_t>>();
  test_variants<lce::ds::lce_naive_wordwise_xor<int64_t>>();
  test_variants<lce::ds::lce_naive_wordwise_xor<__uint128_t>>();
  test_variants<lce::ds::lce_naive_wordwise_xor<__int128_t>>();
}

TEST(LceClassic, All) {
  test_empty_constructor<lce::ds::lce_classic<unsigned char>>();

  test_simple<lce::ds::lce_classic<uint8_t>>();
  // test_simple<lce::ds::lce_classic<int8_t>>();
  test_simple<lce::ds::lce_classic<uint16_t>>();
  // test_simple<lce::ds::lce_classic<int16_t>>();
  test_simple<lce::ds::lce_classic<uint32_t>>();
  // test_simple<lce::ds::lce_classic<int32_t>>();
  test_simple<lce::ds::lce_classic<uint64_t>>();
  // test_simple<lce::ds::lce_classic<int64_t>>();
  test_simple<lce::ds::lce_classic<__uint128_t>>();
  // test_simple<lce::ds::lce_classic<__int128_t>>();

  test_variants<lce::ds::lce_classic<uint8_t>, true, true, true, false>();
  // test_variants<lce::ds::lce_classic<int8_t>, true, true, true, false>();
  test_variants<lce::ds::lce_classic<uint16_t>, true, true, true, false>();
  // test_variants<lce::ds::lce_classic<int16_t>, true, true, true, false>();
  test_variants<lce::ds::lce_classic<uint32_t>, true, true, true, false>();
  // test_variants<lce::ds::lce_classic<int32_t>, true, true, true, false>();
  test_variants<lce::ds::lce_classic<uint64_t>, true, true, true, false>();
  // test_variants<lce::ds::lce_classic<int64_t>, true, true, true, false>();
  test_variants<lce::ds::lce_classic<__uint128_t>, true, true, true, false>();
  // test_variants<lce::ds::lce_classic<__int128_t>, true, true, true,
  // false>();
}
TEST(LceClassicSss, All) {
  test_empty_constructor<lce::ds::lce_classic_for_sss<uint32_t, 1024>>();
  test_simple_classic_for_sss<lce::ds::lce_classic_for_sss<uint32_t, 1024>>();
}

TEST(LceSssNaive, All) {
  test_empty_constructor<lce::ds::lce_sss_naive<uint8_t, 16>>();

  test_simple<lce::ds::lce_sss_naive<uint8_t, 16, uint32_t, false>>();
  test_simple<lce::ds::lce_sss_naive<uint8_t, 16, uint32_t, false>>();
  // test_simple<lce::ds::lce_sss_naive<uint16_t, 16>>();
  // test_simple<lce::ds::lce_sss_naive<int16_t, 16>>();
  // test_simple<lce::ds::lce_sss_naive<uint32_t, 16>>();
  // test_simple<lce::ds::lce_sss_naive<int32_t, 16>>();
  // test_simple<lce::ds::lce_sss_naive<uint64_t, 16>>();
  // test_simple<lce::ds::lce_sss_naive<int64_t, 16>>();
  // test_simple<lce::ds::lce_sss_naive<__uint128_t, 16>>();
  // test_simple<lce::ds::lce_sss_naive<__int128_t, 16>>();

  test_variants<lce::ds::lce_sss_naive<uint8_t, 16, uint32_t, false>>();
  test_variants<lce::ds::lce_sss_naive<uint8_t, 16, uint32_t, false>>();
  // test_variants<lce::ds::lce_sss_naive<uint16_t, 16>>();
  // test_variants<lce::ds::lce_sss_naive<int16_t, 16>>();
  // test_variants<lce::ds::lce_sss_naive<uint32_t, 16>>();
  // test_variants<lce::ds::lce_sss_naive<int32_t, 16>>();
  // test_variants<lce::ds::lce_sss_naive<uint64_t, 16>>();
  // test_variants<lce::ds::lce_sss_naive<int64_t, 16>>();
  // test_variants<lce::ds::lce_sss_naive<__uint128_t, 16>>();
  // test_variants<lce::ds::lce_sss_naive<__int128_t, 16>>();
}
TEST(LceSssNaivePL, All) {
  test_empty_constructor<lce::ds::lce_sss_naive<uint8_t, 16>>();

  test_simple<lce::ds::lce_sss_naive<uint8_t, 16, uint32_t, true>>();
  test_simple<lce::ds::lce_sss_naive<uint8_t, 16, uint32_t, true>>();
  // test_simple<lce::ds::lce_sss_naive<uint16_t, 16>>();
  // test_simple<lce::ds::lce_sss_naive<int16_t, 16>>();
  // test_simple<lce::ds::lce_sss_naive<uint32_t, 16>>();
  // test_simple<lce::ds::lce_sss_naive<int32_t, 16>>();
  // test_simple<lce::ds::lce_sss_naive<uint64_t, 16>>();
  // test_simple<lce::ds::lce_sss_naive<int64_t, 16>>();
  // test_simple<lce::ds::lce_sss_naive<__uint128_t, 16>>();
  // test_simple<lce::ds::lce_sss_naive<__int128_t, 16>>();

  test_variants<lce::ds::lce_sss_naive<uint8_t, 16, uint32_t, true>>();
  test_variants<lce::ds::lce_sss_naive<uint8_t, 16, uint32_t, true>>();
  // test_variants<lce::ds::lce_sss_naive<uint16_t, 16>>();
  // test_variants<lce::ds::lce_sss_naive<int16_t, 16>>();
  // test_variants<lce::ds::lce_sss_naive<uint32_t, 16>>();
  // test_variants<lce::ds::lce_sss_naive<int32_t, 16>>();
  // test_variants<lce::ds::lce_sss_naive<uint64_t, 16>>();
  // test_variants<lce::ds::lce_sss_naive<int64_t, 16>>();
  // test_variants<lce::ds::lce_sss_naive<__uint128_t, 16>>();
  // test_variants<lce::ds::lce_sss_naive<__int128_t, 16>>();
}

TEST(LceSssNoSs, All) {
  test_empty_constructor<lce::ds::lce_sss_noss<uint8_t, 16>>();

  test_simple<lce::ds::lce_sss_noss<uint8_t, 16, uint32_t, false>>();
  test_simple<lce::ds::lce_sss_noss<int8_t, 16, uint32_t, false>>();
  // test_simple<lce::ds::lce_sss_noss<uint16_t, 16>>();
  // test_simple<lce::ds::lce_sss_noss<int16_t, 16>>();
  // test_simple<lce::ds::lce_sss_noss<uint32_t, 16>>();
  // test_simple<lce::ds::lce_sss_noss<int32_t, 16>>();
  // test_simple<lce::ds::lce_sss_noss<uint64_t, 16>>();
  // test_simple<lce::ds::lce_sss_noss<int64_t, 16>>();
  // test_simple<lce::ds::lce_sss_noss<__uint128_t, 16>>();
  // test_simple<lce::ds::lce_sss_noss<__int128_t, 16>>();

  test_variants<lce::ds::lce_sss_noss<uint8_t, 16, uint32_t, false>, true,
                true, true, false>();
  test_variants<lce::ds::lce_sss_noss<int8_t, 16, uint32_t, false>, true, true,
                true, false>();
  // test_variants<lce::ds::lce_sss_noss<uint16_t, 16>>();
  // test_variants<lce::ds::lce_sss_noss<int16_t, 16>>();
  // test_variants<lce::ds::lce_sss_noss<uint32_t, 16>>();
  // test_variants<lce::ds::lce_sss_noss<int32_t, 16>>();
  // test_variants<lce::ds::lce_sss_noss<uint64_t, 16>>();
  // test_variants<lce::ds::lce_sss_noss<int64_t, 16>>();
  // test_variants<lce::ds::lce_sss_noss<__uint128_t, 16>>();
  // test_variants<lce::ds::lce_sss_noss<__int128_t, 16>>();
}
TEST(LceSssNoSsPL, All) {
  test_empty_constructor<lce::ds::lce_sss_noss<uint8_t, 16>>();

  test_simple<lce::ds::lce_sss_noss<uint8_t, 16, uint32_t, true>>();
  test_simple<lce::ds::lce_sss_noss<int8_t, 16, uint32_t, true>>();
  // test_simple<lce::ds::lce_sss_noss<uint16_t, 16>>();
  // test_simple<lce::ds::lce_sss_noss<int16_t, 16>>();
  // test_simple<lce::ds::lce_sss_noss<uint32_t, 16>>();
  // test_simple<lce::ds::lce_sss_noss<int32_t, 16>>();
  // test_simple<lce::ds::lce_sss_noss<uint64_t, 16>>();
  // test_simple<lce::ds::lce_sss_noss<int64_t, 16>>();
  // test_simple<lce::ds::lce_sss_noss<__uint128_t, 16>>();
  // test_simple<lce::ds::lce_sss_noss<__int128_t, 16>>();

  test_variants<lce::ds::lce_sss_noss<uint8_t, 16, uint32_t, true>, true, true,
                true, false>();
  test_variants<lce::ds::lce_sss_noss<int8_t, 16, uint32_t, true>, true, true,
                true, false>();
  // test_variants<lce::ds::lce_sss_noss<uint16_t, 16>>();
  // test_variants<lce::ds::lce_sss_noss<int16_t, 16>>();
  // test_variants<lce::ds::lce_sss_noss<uint32_t, 16>>();
  // test_variants<lce::ds::lce_sss_noss<int32_t, 16>>();
  // test_variants<lce::ds::lce_sss_noss<uint64_t, 16>>();
  // test_variants<lce::ds::lce_sss_noss<int64_t, 16>>();
  // test_variants<lce::ds::lce_sss_noss<__uint128_t, 16>>();
  // test_variants<lce::ds::lce_sss_noss<__int128_t, 16>>();
}

TEST(LceSss, All) {
  test_empty_constructor<lce::ds::lce_sss<uint8_t, 16>>();

  test_simple<lce::ds::lce_sss<uint8_t, 16, uint32_t, false>>();
  test_simple<lce::ds::lce_sss<int8_t, 16, uint32_t, false>>();
  // test_simple<lce::ds::lce_sss<uint16_t, 16>>();
  // test_simple<lce::ds::lce_sss<int16_t, 16>>();
  // test_simple<lce::ds::lce_sss<uint32_t, 16>>();
  // test_simple<lce::ds::lce_sss<int32_t, 16>>();
  // test_simple<lce::ds::lce_sss<uint64_t, 16>>();
  // test_simple<lce::ds::lce_sss<int64_t, 16>>();
  // test_simple<lce::ds::lce_sss<__uint128_t, 16>>();
  // test_simple<lce::ds::lce_sss<__int128_t, 16>>();

  test_variants<lce::ds::lce_sss<uint8_t, 16, uint32_t, false>, true, true,
                true, false>();
  test_variants<lce::ds::lce_sss<int8_t, 16, uint32_t, false>, true, true,
                true, false>();
  // test_variants<lce::ds::lce_sss<uint16_t, 16>>();
  // test_variants<lce::ds::lce_sss<int16_t, 16>>();
  // test_variants<lce::ds::lce_sss<uint32_t, 16>>();
  // test_variants<lce::ds::lce_sss<int32_t, 16>>();
  // test_variants<lce::ds::lce_sss<uint64_t, 16>>();
  // test_variants<lce::ds::lce_sss<int64_t, 16>>();
  // test_variants<lce::ds::lce_sss<__uint128_t, 16>>();
  // test_variants<lce::ds::lce_sss<__int128_t, 16>>();
}

TEST(LceSssPL, All) {
  test_empty_constructor<lce::ds::lce_sss<uint8_t, 16, uint32_t, true>>();

  test_simple<lce::ds::lce_sss<uint8_t, 16, uint32_t, true>>();
  test_simple<lce::ds::lce_sss<int8_t, 16, uint32_t, true>>();
  // test_simple<lce::ds::lce_sss<uint16_t, 16>>();
  // test_simple<lce::ds::lce_sss<int16_t, 16>>();
  // test_simple<lce::ds::lce_sss<uint32_t, 16>>();
  // test_simple<lce::ds::lce_sss<int32_t, 16>>();
  // test_simple<lce::ds::lce_sss<uint64_t, 16>>();
  // test_simple<lce::ds::lce_sss<int64_t, 16>>();
  // test_simple<lce::ds::lce_sss<__uint128_t, 16>>();
  // test_simple<lce::ds::lce_sss<__int128_t, 16>>();

  test_variants<lce::ds::lce_sss<uint8_t, 16, uint32_t, true>, true, true,
                true, false>();
  test_variants<lce::ds::lce_sss<int8_t, 16, uint32_t, true>, true, true, true,
                false>();
  // test_variants<lce::ds::lce_sss<uint16_t, 16>>();
  // test_variants<lce::ds::lce_sss<int16_t, 16>>();
  // test_variants<lce::ds::lce_sss<uint32_t, 16>>();
  // test_variants<lce::ds::lce_sss<int32_t, 16>>();
  // test_variants<lce::ds::lce_sss<uint64_t, 16>>();
  // test_variants<lce::ds::lce_sss<int64_t, 16>>();
  // test_variants<lce::ds::lce_sss<__uint128_t, 16>>();
  // test_variants<lce::ds::lce_sss<__int128_t, 16>>();
}

TEST(LceMemcmp, SS) {
  test_empty_constructor<lce::ds::lce_memcmp>();
  test_suffix_sorting<lce::ds::lce_memcmp>();
}

TEST(LceFP, All) {
  test_empty_constructor<lce::ds::lce_fp<unsigned char>>();
  test_retransform<lce::ds::lce_fp<unsigned char>>();

  test_simple<lce::ds::lce_fp<uint8_t>>();
  test_simple<lce::ds::lce_fp<int8_t>>();
  // test_simple<lce::ds::lce_fp<uint16_t>>();
  // test_simple<lce::ds::lce_fp<int16_t>>();
  // test_simple<lce::ds::lce_fp<uint32_t>>();
  // test_simple<lce::ds::lce_fp<int32_t>>();
  // test_simple<lce::ds::lce_fp<uint64_t>>();
  // test_simple<lce::ds::lce_fp<int64_t>>();
  // test_simple<lce::ds::lce_fp<__uint128_t>>();
  // test_simple<lce::ds::lce_fp<__int128_t>>();

  test_variants<lce::ds::lce_fp<uint8_t>>();
  test_variants<lce::ds::lce_fp<int8_t>>();
  // test_variants<lce::ds::lce_fp<uint16_t>>();
  // test_variants<lce::ds::lce_fp<int16_t>>();
  // test_variants<lce::ds::lce_fp<uint32_t>>();
  // test_variants<lce::ds::lce_fp<int32_t>>();
  // test_variants<lce::ds::lce_fp<uint64_t>>();
  // test_variants<lce::ds::lce_fp<int64_t>>();
  // test_variants<lce::ds::lce_fp<__uint128_t>>();
  // test_variants<lce::ds::lce_fp<__int128_t>>();
}

TEST(LceRkPrezza, All) {
  test_empty_constructor<rklce::lce_rk_prezza>();
  // test_retransform<rklce::lce_rk_prezza>();

  test_simple<rklce::lce_rk_prezza>();
  // test_variants<rklce::lce_rk_prezza>();
}