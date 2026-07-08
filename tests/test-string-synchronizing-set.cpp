/*******************************************************************************
 * tests/rolling_hash/test_string_synchronizing_set.cpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <gtest/gtest.h>
#include <libsais.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <unordered_set>

#include "pred/pred_index.hpp"
#include "rolling_hash/string_synchronizing_set.hpp"

#include "test-progress.hpp"
#include "test-strings.hpp"

__extension__ typedef unsigned __int128 uint128_t;

thread_local std::mt19937_64 gen(std::random_device{}());

template <typename text_t, typename sss_t>
static bool check_string_synchronizing_set(text_t const& text, sss_t const& sss_ds) {
  std::vector<typename sss_t::index_type> const& sss = sss_ds.get_sss();
  std::vector<uint128_t> const& fps = sss_ds.get_fps();

  lce::pred::pred_index<typename sss_t::index_type, 7, typename sss_t::index_type> pred(sss);
  const size_t tau = sss_t::tau;

  if (!std::is_sorted(sss.begin(), sss.end())) {
    fmt::print("\nStrings synchronizing set is not sorted.\n");
    fmt::print("{}", sss);
    return false;
  }

  const size_t last_posible_sss_pos = text.size() - 2 * tau;
  if (!sss_ds.has_runs() && sss.back() > last_posible_sss_pos) {
    std::cout << "\nLast string synchronizing set position is too large. " << sss.back() << ">"
              << last_posible_sss_pos << "\n";
    return false;
  }
  if (sss_ds.has_runs() && sss.back() != last_posible_sss_pos + 1) {
    std::cout << "\nLast string synchronizing is not included in repetitive text. " << sss.back() << ">"
              << last_posible_sss_pos << "\n";
    return false;
  }

  std::vector<int32_t> sa(text.size());
  libsais(reinterpret_cast<const uint8_t*>(text.data()), sa.data(), text.size(), 0, nullptr);

  std::vector<int32_t> plcp(sa.size());
  libsais_plcp(reinterpret_cast<const uint8_t*>(text.data()), sa.data(), plcp.data(), text.size());

  std::vector<int32_t> lcp(sa.size());
  libsais_lcp(plcp.data(), sa.data(), lcp.data(), text.size());

  std::unordered_set<size_t> sss_positions;
  for (size_t i = 0; i < sss.size(); i++) {
    sss_positions.insert(sss[i]);
  }

  for (size_t i = 1; i < sa.size(); i++) {
    assert(lcp[i] >= 0);
    if (static_cast<size_t>(lcp[i]) >= 2 * tau) {
      bool left_in_sss = sss_positions.contains(sa[i - 1]);
      bool right_in_sss = sss_positions.contains(sa[i]);
      if (left_in_sss != right_in_sss) {
        std::cout << "\n"
                  << sa[i - 1] << " in sss: " << std::boolalpha << left_in_sss << " - " << sa[i]
                  << " in sss: " << std::boolalpha << right_in_sss << "\n";
        return false;
      }
      if ((static_cast<size_t>(lcp[i]) >= 3 * tau) && left_in_sss && right_in_sss) {
        size_t pos_in_sss_i = pred.predecessor(sa[i - 1]).pos;
        size_t pos_in_sss_j = pred.predecessor(sa[i]).pos;
        if (sss[pos_in_sss_i] != static_cast<size_t>(sa[i - 1])) {
          fmt::print("{} should equal {}\n", sss[pos_in_sss_i], sa[i - 1]);
          return false;
        }
        if (sss[pos_in_sss_j] != static_cast<size_t>(sa[i])) {
          fmt::print("{} should equal {}\n", sss[pos_in_sss_j], sa[i]);
          return false;
        }
        if (sss_ds.fps_calculated() && (fps[pos_in_sss_i] << 21 != fps[pos_in_sss_j] << 21)) {
          fmt::print(" fingerprints should be equal: {}:{} != {}:{}\n",
                     (uint64_t)(fps[pos_in_sss_i] >> 64), (uint64_t)fps[pos_in_sss_i],
                     (uint64_t)(fps[pos_in_sss_j] >> 64), (uint64_t)fps[pos_in_sss_j]);
          return false;
        }
      }
    }
  }

  return true;
}

template <uint64_t tau>
static void verify_sss(std::mt19937_64& g, uint64_t max_size) {
  int sigma = std::uniform_int_distribution<int>(2, 6)(g);
  std::string text = random_repetitive_input<std::string>(g, 5 * tau + 200, max_size, (char)1, (char)sigma);
  size_t n = text.size();

  fuzz_timer tb(fuzz_construct_ns());
  lce::rolling_hash::sss<uint32_t, tau> sss_ds(text.data(), n, true);
  tb.stop();
  fuzz_timer tq(fuzz_query_ns());

  if (sss_ds.get_sss().empty()) {
    ADD_FAILURE() << "empty string synchronizing set, tau=" << tau << " n=" << n;
    return;
  }
  EXPECT_TRUE(check_string_synchronizing_set(text, sss_ds)) << "tau=" << tau << " n=" << n;
}

TEST(test_string_synchronizing_set, all) {
  run_fuzz("string-synchronizing-set", {
    {"verify", [](uint64_t it) {
       switch (it % 5) {
         case 0: verify_sss<2>(gen, 200000); break;
         case 1: verify_sss<4>(gen, 200000); break;
         case 2: verify_sss<8>(gen, 200000); break;
         case 3: verify_sss<16>(gen, 200000); break;
         case 4: verify_sss<32>(gen, 200000); break;
       }
     }, false},
  }, 5000);
}
