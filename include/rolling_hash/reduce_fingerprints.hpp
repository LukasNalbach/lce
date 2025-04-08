/*******************************************************************************
 * lce/rolling_hash/reduce_fingerprints.hpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <chrono>
#include <cmath>
#include <memory>
#include <vector>

#include "ds/lce_naive_wordwise.hpp"
#include "rolling_hash/string_synchronizing_set.hpp"

#ifdef LCE_BENCHMARK_INTERNAL
#include <fmt/core.h>
#include <fmt/ranges.h>

#include "util/timer.hpp"
#ifdef LCE_BENCHMARK_SPACE
#include <malloc_count/malloc_count.h>
#endif
#endif

namespace lce::ds {

template <typename sss_type>
bool leq_three_tau(uint8_t const* text, size_t text_size, size_t text_pos_i,
                   size_t text_pos_j, sss_type const& sync_set);
template <typename sss_type>
bool eq_three_tau(uint8_t const* text, size_t text_size, size_t text_pos_i,
                  size_t text_pos_j, sss_type const& sync_set);

template <typename sss_type>
std::vector<typename sss_type::index_type> reduce_fps_3tau_lexicographic(
    uint8_t const* text, size_t text_size, sss_type const& sync_set) {
  using index_type = sss_type::index_type;
  static constexpr uint64_t tau = sss_type::tau;

  __extension__ typedef unsigned __int128 uint128_t;
  std::vector<index_type> const& sss = sync_set.get_sss();

  // sort sss-pos by 3tau-infix
  std::vector<index_type> sss_sorted = sss;
  ips4o::parallel::sort(
      sss_sorted.begin(), sss_sorted.end(),
      [&text, &text_size, &sync_set](index_type lhs, index_type rhs) {
        if (lhs == rhs) {
          return false;
        }
        assert(lhs != rhs);
        size_t lce = lce_naive_wordwise_xor<uint8_t>::lce_up_to(text, text_size,
                                                            lhs, rhs, 3 * tau);

        if (std::max(lhs, rhs) + lce == text_size) {
          return lhs > rhs;
        }
        if (text[lhs + lce] != text[rhs + lce]) {
          return text[lhs + lce] < text[rhs + lce];
        }
        if (sync_set.get_run_info(lhs) != sync_set.get_run_info(rhs)) {
          return sync_set.get_run_info(lhs) < sync_set.get_run_info(rhs);
        }
        return false;
      });

  for (size_t idx = 1; idx < sss_sorted.size(); ++idx) {
    size_t i = sss_sorted[idx - 1];
    size_t j = sss_sorted[idx];
    assert(leq_three_tau(text, text_size, i, j, sync_set));
  }

  // Build tuples
  struct index_rank {
    index_type index;
    index_type rank;
  };

  int nt = omp_get_max_threads();
  std::vector<index_rank> rank_tuples(sss_sorted.size());
  // Max rank in block
  std::vector<index_type> max_ranks(nt);
  // The two vectors act as BV; vector<bool> fails in parallel.
  std::vector<uint8_t> all_ranks_equal(nt);  // Are all ranks in block equal?
  // Is first rank in block == last rank in prev block?
  std::vector<uint8_t> rank_extends_prev_block(nt);

#pragma omp parallel
  {
    const int t = omp_get_thread_num();
    const int nt = omp_get_num_threads();
    const size_t slice_size = rank_tuples.size() / nt;
    const index_type begin = t * slice_size;
    const index_type end =
        (t < nt - 1) ? (t + 1) * slice_size : rank_tuples.size();

    index_type cur_rank{1 + begin};
    rank_tuples[begin] = {sss_sorted[begin], cur_rank};
    for (size_t i{begin + 1}; i < end; ++i) {
      if (!eq_three_tau(text, text_size, sss_sorted[i - 1], sss_sorted[i],
                        sync_set)) {
        ++cur_rank;
      }
      rank_tuples[i] = {sss_sorted[i], cur_rank};
    }
    max_ranks[t] = cur_rank;

#pragma omp barrier
    assert(rank_tuples[begin].rank == begin + 1);
    all_ranks_equal[t] = (max_ranks[t] == begin + 1);
    rank_extends_prev_block[t] =
        (begin == 0) ? false
                     : eq_three_tau(text, text_size, sss_sorted[begin - 1],
                                    sss_sorted[begin], sync_set);
#pragma omp barrier
    // Now adjust ranks between blocks
    if (t != 0) {
      if (rank_extends_prev_block[t]) {
        size_t target_t = t - 1;
        while (all_ranks_equal[target_t] && rank_extends_prev_block[target_t]) {
          --target_t;
        }
        index_type target_rank = max_ranks[target_t];

        const uint32_t rank_to_decrease = rank_tuples[begin].rank;
        for (size_t i = begin;
             i < end && rank_tuples[i].rank == rank_to_decrease; ++i) {
          rank_tuples[i].rank = target_rank;
        }
      }
    }
  }

  // Check rank_tuples
  {
    assert(rank_tuples.size() == sss_sorted.size());
    for (size_t i = 0; i < rank_tuples.size(); ++i) {
      assert(rank_tuples[i].index == sss_sorted[i]);
    }
    for (size_t i = 1; i < rank_tuples.size(); ++i) {
      bool neq_neighbors = (rank_tuples[i - 1].rank) < (rank_tuples[i].rank);
      assert(neq_neighbors == !eq_three_tau(text, text_size,
                                            rank_tuples[i - 1].index,
                                            rank_tuples[i].index, sync_set));
    }
  }

  // Sort tuples by pos
  ips4o::parallel::sort(
      rank_tuples.begin(), rank_tuples.end(),
      [](auto lhs, auto rhs) { return lhs.index < rhs.index; });

  // Overwrite
  std::vector<index_type> fps_reduced(sss.size());
  for (size_t i = 0; i < rank_tuples.size(); ++i) {
    fps_reduced[i] = rank_tuples[i].rank;
  }
  // fps.push_back(0) // If using SAIS
  return fps_reduced;
}

template <typename sss_type>
bool leq_three_tau(uint8_t const* text, size_t text_size, size_t text_pos_i,
                   size_t text_pos_j, sss_type const& sync_set) {
  constexpr size_t tau = sync_set.tau;
  size_t const max_length = std::min(
      {text_size - text_pos_i, text_size - text_pos_j, 3 * sync_set.tau});
  size_t text_lce = lce_naive_wordwise_xor<uint8_t>::lce_up_to(
      text, text_size, text_pos_i, text_pos_j, 3 * tau);
  return (text_lce < max_length &&
          text[text_pos_i + text_lce] < text[text_pos_j + text_lce]) ||
         (text_lce == max_length && sync_set.get_run_info(text_pos_i) <=
                                        sync_set.get_run_info(text_pos_j));
}

template <typename sss_type>
bool eq_three_tau(uint8_t const* text, size_t text_size, size_t text_pos_i,
                  size_t text_pos_j, sss_type const& sync_set) {
  assert(text_pos_i != text_pos_j);
  size_t lce = lce_naive_wordwise_xor<uint8_t>::lce_up_to(
      text, text_size, text_pos_i, text_pos_j, 3 * sync_set.tau);

  if (std::max(text_pos_i, text_pos_j) + lce == text_size) {
    return false;
  }
  if (text[text_pos_i + lce] != text[text_pos_j + lce]) {
    return false;
  }
  if (sync_set.get_run_info(text_pos_i) != sync_set.get_run_info(text_pos_j)) {
    return false;
  }
  return true;
}
}  // namespace lce::ds