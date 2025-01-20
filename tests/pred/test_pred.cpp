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

#include "pred/binsearch_std.hpp"
#include "pred/j_index.hpp"
#include "pred/pgm_index.hpp"
#include "pred/pred_index.hpp"

template <typename pred_ds_type>
void test_empty_constructor() {
  pred_ds_type ds;
}

template <typename pred_ds_type>
void test_simple() {
  typedef typename pred_ds_type::data_type data_type;
  std::vector<data_type> data(100);
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = i * 2 + 1;
  }

  std::vector<data_type> data_copy = data;

  // Test pointer constructor
  {
    pred_ds_type ds(data.data(), data.size());
    EXPECT_EQ(ds.template predecessor(0).exists, false);

    EXPECT_EQ(ds.template predecessor(1), lce::pred::result(true, size_t{0}));
    EXPECT_EQ(ds.template predecessor(0).exists, false);

    EXPECT_EQ(ds.template predecessor(98), lce::pred::result(true, size_t{48}));
    EXPECT_EQ(ds.template predecessor(99), lce::pred::result(true, size_t{49}));
    EXPECT_EQ(ds.template predecessor(100),
              lce::pred::result(true, size_t{49}));

    EXPECT_EQ(ds.template predecessor(198),
              lce::pred::result(true, size_t{98}));
    EXPECT_EQ(ds.template predecessor(199),
              lce::pred::result(true, size_t{99}));
    EXPECT_EQ(ds.template predecessor(200),
              lce::pred::result(true, size_t{99}));

    EXPECT_EQ(ds.template successor(0), lce::pred::result(true, size_t{0}));
    EXPECT_EQ(ds.template successor(1), lce::pred::result(true, size_t{0}));
    EXPECT_EQ(ds.template successor(2), lce::pred::result(true, size_t{1}));

    EXPECT_EQ(ds.template successor(99), lce::pred::result(true, size_t{49}));
    EXPECT_EQ(ds.template successor(100), lce::pred::result(true, size_t{50}));
    EXPECT_EQ(ds.template successor(101), lce::pred::result(true, size_t{50}));

    EXPECT_EQ(ds.template successor(199), lce::pred::result(true, size_t{99}));
    EXPECT_EQ(ds.template successor(200).exists, false);
    EXPECT_EQ(ds.template successor(201).exists, false);

    // Methods without bounds check
    EXPECT_EQ(ds.template predecessor_unsafe(1), 0);

    EXPECT_EQ(ds.template predecessor_unsafe(98), 48);
    EXPECT_EQ(ds.template predecessor_unsafe(99), 49);
    EXPECT_EQ(ds.template predecessor_unsafe(100), 49);

    EXPECT_EQ(ds.template predecessor_unsafe(198), 98);
    EXPECT_EQ(ds.template predecessor_unsafe(199), 99);
    EXPECT_EQ(ds.template predecessor_unsafe(200), 99);

    EXPECT_EQ(ds.template successor_unsafe(0), 0);
    EXPECT_EQ(ds.template successor_unsafe(1), 0);
    EXPECT_EQ(ds.template successor_unsafe(2), 1);

    EXPECT_EQ(ds.template successor_unsafe(99), 49);
    EXPECT_EQ(ds.template successor_unsafe(100), 50);
    EXPECT_EQ(ds.template successor_unsafe(101), 50);

    EXPECT_EQ(ds.template successor_unsafe(199), 99);

    // Exist method
    EXPECT_EQ(ds.contains(0), false);
    EXPECT_EQ(ds.contains(1), true);
    EXPECT_EQ(ds.contains(99), true);
    EXPECT_EQ(ds.contains(100), false);
    EXPECT_EQ(ds.contains(199), true);
    EXPECT_EQ(ds.contains(200), false);
  }
  data = data_copy;
}

template <typename pred_ds_type>
void test_simple_safe() {
  typedef typename pred_ds_type::data_type data_type;
  std::vector<data_type> data(100);
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = i * 2 + 1;
  }

  std::vector<data_type> data_copy = data;

  // Test pointer constructor
  {
    pred_ds_type ds(data.data(), data.size());
    EXPECT_EQ(ds.template predecessor(0).exists, false);

    EXPECT_EQ(ds.template predecessor(1), lce::pred::result(true, size_t{0}));
    EXPECT_EQ(ds.template predecessor(0).exists, false);

    EXPECT_EQ(ds.template predecessor(98), lce::pred::result(true, size_t{48}));
    EXPECT_EQ(ds.template predecessor(99), lce::pred::result(true, size_t{49}));
    EXPECT_EQ(ds.template predecessor(100),
              lce::pred::result(true, size_t{49}));

    EXPECT_EQ(ds.template predecessor(198),
              lce::pred::result(true, size_t{98}));
    EXPECT_EQ(ds.template predecessor(199),
              lce::pred::result(true, size_t{99}));
    EXPECT_EQ(ds.template predecessor(200),
              lce::pred::result(true, size_t{99}));

    EXPECT_EQ(ds.template successor(0), lce::pred::result(true, size_t{0}));
    EXPECT_EQ(ds.template successor(1), lce::pred::result(true, size_t{0}));
    EXPECT_EQ(ds.template successor(2), lce::pred::result(true, size_t{1}));

    EXPECT_EQ(ds.template successor(99), lce::pred::result(true, size_t{49}));
    EXPECT_EQ(ds.template successor(100), lce::pred::result(true, size_t{50}));
    EXPECT_EQ(ds.template successor(101), lce::pred::result(true, size_t{50}));

    EXPECT_EQ(ds.template successor(199), lce::pred::result(true, size_t{99}));
    EXPECT_EQ(ds.template successor(200).exists, false);
    EXPECT_EQ(ds.template successor(201).exists, false);
  }
  data = data_copy;
}

TEST(PredBinsearchStd, All) {
  test_empty_constructor<lce::pred::binsearch_std<uint64_t>>();
  test_simple<lce::pred::binsearch_std<unsigned char>>();
  test_simple<lce::pred::binsearch_std<uint8_t>>();
  test_simple<lce::pred::binsearch_std<uint32_t>>();
  test_simple<lce::pred::binsearch_std<int32_t>>();
  test_simple<lce::pred::binsearch_std<uint64_t>>();
  test_simple<lce::pred::binsearch_std<int64_t>>();
  test_simple<lce::pred::binsearch_std<__uint128_t>>();
}

TEST(PredIndex, Safe) {
  test_empty_constructor<lce::pred::pred_index<uint32_t, 7, uint32_t>>();
  test_simple_safe<lce::pred::pred_index<uint32_t, 7, uint32_t>>();
  test_simple_safe<lce::pred::pred_index<uint32_t, 7, uint32_t>>();
  test_simple_safe<lce::pred::pred_index<uint32_t, 7, uint32_t>>();
  test_simple_safe<lce::pred::pred_index<uint32_t, 7, uint32_t>>();
  test_simple_safe<lce::pred::pred_index<uint32_t, 7, uint32_t>>();
  test_simple_safe<lce::pred::pred_index<uint32_t, 7, uint32_t>>();
  test_simple_safe<lce::pred::pred_index<uint32_t, 7, uint32_t>>();
}

TEST(JIndex, Safe) {
  test_empty_constructor<lce::pred::j_index<uint64_t>>();
  test_simple_safe<lce::pred::j_index<unsigned char>>();
  test_simple_safe<lce::pred::j_index<uint8_t>>();
  test_simple_safe<lce::pred::j_index<uint32_t>>();
  test_simple_safe<lce::pred::j_index<int32_t>>();
  test_simple_safe<lce::pred::j_index<uint64_t>>();
  test_simple_safe<lce::pred::j_index<int64_t>>();
  test_simple_safe<lce::pred::j_index<__uint128_t>>();
}

TEST(PGMIndex, Safe) {
  test_empty_constructor<lce::pred::pgm_index<uint64_t, 32>>();
  test_simple_safe<lce::pred::pgm_index<unsigned char, 32>>();
  test_simple_safe<lce::pred::pgm_index<uint8_t, 32>>();
  test_simple_safe<lce::pred::pgm_index<uint32_t, 32>>();
  test_simple_safe<lce::pred::pgm_index<int32_t, 32>>();
  test_simple_safe<lce::pred::pgm_index<uint64_t, 32>>();
  test_simple_safe<lce::pred::pgm_index<int64_t, 32>>();
  //test_simple_safe<lce::pred::pgm_index<__uint128_t, 32>>();
}