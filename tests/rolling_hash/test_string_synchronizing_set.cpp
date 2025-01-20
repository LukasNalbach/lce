/*******************************************************************************
 * test/rolling_hash/test_string_synchronizing_set.hpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <gtest/gtest.h>
#include <libsais.h>

#include <unordered_set>

#include "pred/pred_index.hpp"
#include "rolling_hash/string_synchronizing_set.hpp"

__extension__ typedef unsigned __int128 uint128_t;

template <typename text_t, typename sss_t>
bool check_string_synchronizing_set(text_t const& text, sss_t const& sss_ds) {
  std::vector<typename sss_t::index_type> const& sss = sss_ds.get_sss();
  std::vector<uint128_t> const& fps = sss_ds.get_fps();

  lce::pred::pred_index<typename sss_t::index_type, 7,
                        typename sss_t::index_type>
      pred(sss);
  const size_t tau = sss_t::tau;

  if (!std::is_sorted(sss.begin(), sss.end())) {
    fmt::print("\nStrings synchronizing set is not sorted.\n");
    fmt::print("{}", sss);

    return false;
  }

  const size_t last_posible_sss_pos = text.size() - 2 * tau;
  if (!sss_ds.has_runs() && sss.back() > last_posible_sss_pos) {
    std::cout << "\nLast string synchronizing set position is too large. "
              << sss.back() << ">" << last_posible_sss_pos << "\n";
    return false;
  }
  if (sss_ds.has_runs() && sss.back() != last_posible_sss_pos + 1) {
    std::cout
        << "\nLast string synchronizing is not included in repetitive text. "
        << sss.back() << ">" << last_posible_sss_pos << "\n";
    return false;
  }

  /* First check consistency:
   * If there are infixes which share 2 tau characters, they must be
   * synchronized, i.e. either both contained in sss or both not contained in
   * sss
   */

  // Build suffix array and longest common prefix array
  std::vector<int32_t> sa(text.size());
  libsais(reinterpret_cast<const uint8_t*>(text.data()), sa.data(), text.size(),
          0, nullptr);

  std::vector<int32_t> plcp(sa.size());
  libsais_plcp(reinterpret_cast<const uint8_t*>(text.data()), sa.data(),
               plcp.data(), text.size());

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
                  << sa[i - 1] << " in sss: " << std::boolalpha << left_in_sss
                  << " - " << sa[i] << " in sss: " << std::boolalpha
                  << right_in_sss << "\n";
        return false;
      }
      if ((static_cast<size_t>(lcp[i]) >= 3 * tau) && left_in_sss &&
          right_in_sss) {
        size_t pos_in_sss_i = pred.predecessor(sa[i - 1]).pos;
        size_t pos_in_sss_j = pred.predecessor(sa[i]).pos;
        if (sss[pos_in_sss_i] != sa[i - 1]) {
          fmt::print("{} should equal {}\n", sss[pos_in_sss_i], sa[i - 1]);
          return false;
        }
        if (sss[pos_in_sss_j] != sa[i]) {
          fmt::print("{} should equal {}\n", sss[pos_in_sss_j], sa[i]);
          return false;
        }
        if (sss_ds.fps_calculated() &&
            (fps[pos_in_sss_i] << 21 != fps[pos_in_sss_j] << 21)) {
          fmt::print(" fingerprints should be equal: {} != {}\n",
                     fps[pos_in_sss_i], fps[pos_in_sss_j]);
          return false;
        }
      }
    }
  }

  // Assert repetitive/distinct string synchronizing set positions store run
  // information
  for (size_t i = 0; i < sss.size() - 1; i++) {
    if (sss[i + 1] - sss[i] > tau) {
      if (sss_ds.get_run_info(sss[i]) == 0) {
        std::cout << "\nsss[" << i << "] should store run information.";
        return false;
      }
    } else {
      if (sss_ds.get_run_info(sss[i] != 0)) {
        std::cout << "\nsss[" << i << "] should NOT store run information.";
      }
    }
  }

  // Now assert that run information is rising.
  int64_t last_run_pos;
  int64_t last_run_info;
  if (sa[0] == 0 || sss_ds.get_run_info(sa[0] - 1) == 0) {
    last_run_pos = -1;
    last_run_info = std::numeric_limits<int64_t>::min();
  } else {
    last_run_pos = 0;
    last_run_info = sss_ds.get_run_info(sa[0] - 1);
  }

  for (size_t i = 1; i < sa.size(); i++) {
    assert(lcp[i] >= 0);
    if (static_cast<size_t>(lcp[i]) >= 3 * tau - 1) {
      if (sa[i] == 0) {
        continue;
      }
      if (sss_ds.get_run_info(sa[i] - 1) ==
          0) {  // sa[i]-1 not in S or sa[i]-1 does not precede run
        continue;
      }

      int64_t run_info = sss_ds.get_run_info(sa[i] - 1);
      if (run_info < last_run_info) {
        std::cout << "\nError in run information: "
                  << " sa[" << last_run_pos << "]=" << sa[last_run_pos]
                  << " sa[" << i << "]=" << sa[i] << " lcp="
                  << *std::min_element(lcp.data() + last_run_pos + 1,
                                       lcp.data() + i)
                  << ": " << last_run_info << " > " << run_info << "!\n";
        return false;
      }
      last_run_pos = i;
      last_run_info = run_info;
    } else {
      last_run_info = -1;
      last_run_info = std::numeric_limits<int64_t>::min();
    }
  }

  // Don't check density condition, because it does not influence
  // correctness
  /* TODO: Then check density condition
   * If there is a gap greater than tau, a repetitive region starts at the
   * beginning of the gap. This region has period up to 1/3 tau and ends at
   * at a specific point. We also check the run information at all
   * positions.
   */
  /*const size_t min_run_length = 3*tau - 2;
  const size_t max_run_period = tau/3;
  const size_t distance_period_end_to_next_sss_pos = 2*tau - 1;
  for(size_t i = 0; i < sss.size(); i++) {
    bool gap_greater_than_tau = (sss[i+1] - sss[i]) > tau;
    bool period_smaller_than_third_tau = (sa[i] < text.size() - 3*tau + 2)
  && period(text.data() + sa[i], 3*tau-1, 1/3*tau) < 1/3*tau;
    if(gap_greater_than_tau != period_smaller_than_3_tau) {
      std::cout << "gap_greater_than_tau: " << std::boolalpha <<
  gap_greater_than_tau << " - "
                << "period_smaller_than_third_tau" = std::boolalpha <<
  period_smaller_than_third_tau << "\n"; return false;
    }
  }*/

  return true;
}

template <typename char_t>
size_t period(char_t* text_p, size_t n, size_t period_up_to) {
  return 0;
}

TEST(StringSynchronizingSet, NonRepetetiveSmall) {
  std::string text =
      "Lorem ipsum dolor sit amet, Lorem ipsum dolor sit amet, Lorem ipsum "
      "dolor sit amet, ";
  {
    lce::rolling_hash::sss<uint32_t, 2> sss(text, true);
    fmt::print("sss_size={} (approx {})\n", sss.size(),
               text.size() * 2 / (sss.tau + 1));
    EXPECT_TRUE(check_string_synchronizing_set(text, sss));
  }
  {
    lce::rolling_hash::sss<uint32_t, 4> sss(text, true);
    fmt::print("sss_size={} (approx {})\n", sss.size(),
               text.size() * 2 / (sss.tau + 1));
    EXPECT_TRUE(check_string_synchronizing_set(text, sss));
  }
  {
    lce::rolling_hash::sss<uint32_t, 8> sss(text, true);
    fmt::print("sss_size={} (approx {})\n", sss.size(),
               text.size() * 2 / (sss.tau + 1));
    EXPECT_TRUE(check_string_synchronizing_set(text, sss));
  }
  {
    lce::rolling_hash::sss<uint32_t, 16> sss(text, true);
    fmt::print("sss_size={} (approx {})\n", sss.size(),
               text.size() * 2 / (sss.tau + 1));
    EXPECT_TRUE(check_string_synchronizing_set(text, sss));
  }
}

TEST(StringSynchronizingSet, NonRepetetive) {
  std::string text =
      "Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam "
      "nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, "
      "sed diam voluptua. At vero eos et accusam et justo duo dolores et ea "
      "rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem "
      "ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing "
      "elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna "
      "aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo "
      "dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus "
      "est Lorem ipsum dolor sit amet.Lorem ipsum dolor sit amet, consetetur "
      "sadipscing elitr, sed diam "
      "nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, "
      "sed diam voluptua. At vero eos et accusam et justo duo dolores et ea "
      "rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem "
      "ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing "
      "elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna "
      "aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo "
      "dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus "
      "est Lorem ipsum dolor sit amet.Lorem ipsum dolor sit amet, consetetur "
      "sadipscing elitr, sed diam "
      "nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, "
      "sed diam voluptua. At vero eos et accusam et justo duo dolores et ea "
      "rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem "
      "ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing "
      "elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna "
      "aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo "
      "dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus "
      "est Lorem ipsum dolor sit amet.";
  {
    lce::rolling_hash::sss<uint32_t, 2> sss(text, true);
    fmt::print("sss_size={} (approx {})\n", sss.size(),
               text.size() * 2 / (sss.tau + 1));
    EXPECT_TRUE(check_string_synchronizing_set(text, sss));
  }
  {
    lce::rolling_hash::sss<uint32_t, 4> sss(text, true);
    fmt::print("sss_size={} (approx {})\n", sss.size(),
               text.size() * 2 / (sss.tau + 1));
    EXPECT_TRUE(check_string_synchronizing_set(text, sss));
  }
  {
    lce::rolling_hash::sss<uint32_t, 8> sss(text, true);
    fmt::print("sss_size={} (approx {})\n", sss.size(),
               text.size() * 2 / (sss.tau + 1));
    EXPECT_TRUE(check_string_synchronizing_set(text, sss));
  }
  {
    lce::rolling_hash::sss<uint32_t, 16> sss(text, true);
    fmt::print("sss_size={} (approx {})\n", sss.size(),
               text.size() * 2 / (sss.tau + 1));
    EXPECT_TRUE(check_string_synchronizing_set(text, sss));
  }
  {
    lce::rolling_hash::sss<uint32_t, 32> sss(text, true);
    fmt::print("sss_size={} (approx {})\n", sss.size(),
               text.size() * 2 / (sss.tau + 1));
    EXPECT_TRUE(check_string_synchronizing_set(text, sss));
  }
}

TEST(StringSynchronizingSet, Repetetive) {
  std::string text =
      "Lorum "
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababab dolor sit amet, "
      "consetetur sadipscing elitr, sed diam "
      "nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, "
      "sed diam voluptua. At vero eos et accusam et justo duo dolores et ea "
      "rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem "
      "ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing "
      "elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna "
      "aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo "
      "dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus "
      "est Lorem ipsum dolor sit amet.Lorem ipsum dolor sit amet, consetetur "
      "sadipscing elitr, sed diam "
      "nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, "
      "sed diam voluptua. At vero eos et accusam et justo duo dolores et ea "
      "rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem "
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "abababababababababababababababababababababababababababababababababababab"
      "nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, "
      "sed diam voluptua. At vero eos et accusam et justo duo dolores et ea "
      "rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem "
      "ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing "
      "elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna "
      "aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo "
      "dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus "
      "est Lorem ipsum dolor sit amet.";
  {
    lce::rolling_hash::sss<uint32_t, 2> sss(text, true);
    EXPECT_TRUE(check_string_synchronizing_set(text, sss));
    fmt::print("sss_size={} (approx {})\n", sss.size(),
               text.size() * 2 / (sss.tau + 1));
  }
  {
    lce::rolling_hash::sss<uint32_t, 4> sss(text, true);
    EXPECT_TRUE(check_string_synchronizing_set(text, sss));
    fmt::print("sss_size={} (approx {})\n", sss.size(),
               text.size() * 2 / (sss.tau + 1));
  }
  {
    lce::rolling_hash::sss<uint32_t, 8> sss(text, true);
    EXPECT_TRUE(check_string_synchronizing_set(text, sss));
    fmt::print("sss_size={} (approx {})\n", sss.size(),
               text.size() * 2 / (sss.tau + 1));
  }
  {
    lce::rolling_hash::sss<uint32_t, 16> sss(text, true);
    EXPECT_TRUE(check_string_synchronizing_set(text, sss));
    fmt::print("sss_size={} (approx {})\n", sss.size(),
               text.size() * 2 / (sss.tau + 1));
    EXPECT_TRUE(sss.has_runs());
    EXPECT_NE(sss.get_run_info(5), 0);
    EXPECT_NE(sss.get_run_info(2059), 0);
    EXPECT_TRUE(sss.get_run_info(5) < sss.get_run_info(2059))
        << fmt::format("{} {}", sss.get_run_info(5), sss.get_run_info(2059));
  }
  {
    lce::rolling_hash::sss<uint32_t, 32> sss(text, true);
    EXPECT_TRUE(check_string_synchronizing_set(text, sss));
    fmt::print("sss_size={} (approx {})\n", sss.size(),
               text.size() * 2 / (sss.tau + 1));
    EXPECT_TRUE(sss.has_runs());
    EXPECT_NE(sss.get_run_info(5), 0);
    EXPECT_NE(sss.get_run_info(2059), 0);
    EXPECT_TRUE(sss.get_run_info(5) < sss.get_run_info(2059))
        << fmt::format("{} {}", sss.get_run_info(5), sss.get_run_info(2059));
  }
}
