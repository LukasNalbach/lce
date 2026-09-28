/*******************************************************************************
 * lce/sss/rank_3tau_substrings.hpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <omp.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

#include "sss/string_synchronizing_set.hpp"

#include <ips4o.hpp>
#include "util/hash.hpp"
#include "util/memory.hpp"

#include <libsais40_types.hpp>

#ifdef LCE_BENCHMARK_INTERNAL
#include <fmt/core.h>
#include <fmt/ranges.h>

#include "util/timer.hpp"
#ifdef LCE_BENCHMARK_SPACE
#include <malloc_count/malloc_count.h>
#endif
#endif

namespace lce::sss {

template <typename t_rank, typename t_key_index, typename t_text, typename sss_type>
std::vector<t_rank> rank_3tau_substrings_impl(t_text const& text, sss_type const& sync_set) {
  using index_type = typename sss_type::index_type;
  const uint64_t n = text.size();
  const uint64_t len = 3 * sync_set.tau();
  auto const& sss = sync_set.get_sss();
  const uint64_t s = sss.size();

  if (s == 0) {
    return {};
  }

  auto less = [&](uint64_t i, uint64_t j) {
    const uint64_t lhs = sss[i];
    const uint64_t rhs = sss[j];
    if (lhs == rhs) return false;
    const uint64_t lce = text.lce(lhs, rhs, len);
    if (std::max(lhs, rhs) + lce == n) return lhs > rhs;
    if (lce < len) return text[lhs + lce] < text[rhs + lce];
    return sync_set.get_run_info(lhs) < sync_set.get_run_info(rhs);
  };

  auto equal = [&](uint64_t i, uint64_t j) {
    const uint64_t lhs = sss[i];
    const uint64_t rhs = sss[j];
    return text.equal(lhs, rhs, len) && sync_set.get_run_info(lhs) == sync_set.get_run_info(rhs);
  };

  uint64_t m = s;
  while (m > 0 && sss[m - 1] + len >= n) --m;

  struct __attribute__((packed)) key_t {
    uint64_t hash;
    t_key_index index;
  };

  std::vector<key_t> keys;
  lce::util::no_init_resize(keys, m);

#pragma omp parallel for schedule(static)
  for (uint64_t i = 0; i < m; ++i) {
    const uint64_t pos = sss[i];
    const uint64_t run = uint64_t(sync_set.get_run_info(pos));
    keys[i] = key_t{lce::util::hash_mix(text.hash(pos, len), run), t_key_index(i)};
  }

  ips4o::parallel::sort(keys.begin(), keys.end(), [](const key_t& lhs, const key_t& rhs) {
    const uint64_t l = lhs.hash;
    const uint64_t r = rhs.hash;
    return l < r || (l == r && uint64_t(lhs.index) < uint64_t(rhs.index));
  });

  const uint64_t p = std::max<uint64_t>(1, std::min<uint64_t>(omp_get_max_threads(), m));
  constexpr uint64_t none = std::numeric_limits<uint64_t>::max();
  std::vector<uint64_t> chunk(p + 1);
  for (uint64_t c = 0; c <= p; ++c) chunk[c] = m * c / p;
  std::vector<uint64_t> last_start(p, none);
  std::vector<uint64_t> prev_hash(p, 0);
  std::vector<uint8_t> first_is_start(p, 1);

#pragma omp parallel for num_threads(p) schedule(static, 1)
  for (uint64_t c = 0; c < p; ++c) {
    const uint64_t beg = chunk[c];
    const uint64_t end = chunk[c + 1];
    if (beg == end) continue;
    if (beg > 0) {
      prev_hash[c] = keys[beg - 1].hash;
      first_is_start[c] = keys[beg].hash != prev_hash[c];
    }
    for (uint64_t j = end; j-- > beg;) {
      if (j == 0 || keys[j].hash != keys[j - 1].hash) {
        last_start[c] = j;
        break;
      }
    }
  }

  std::vector<uint64_t> carry(p, 0);
  for (uint64_t c = 1; c < p; ++c) {
    if (first_is_start[c]) carry[c] = chunk[c];
    else carry[c] = last_start[c - 1] != none ? last_start[c - 1] : carry[c - 1];
  }

#pragma omp parallel for num_threads(p) schedule(static, 1)
  for (uint64_t c = 0; c < p; ++c) {
    uint64_t cur = carry[c];
    uint64_t prev = prev_hash[c];
    for (uint64_t j = chunk[c]; j < chunk[c + 1]; ++j) {
      const uint64_t h = keys[j].hash;
      if (j == 0 || h != prev) cur = j;
      prev = h;
      keys[j].hash = cur;
    }
  }

  std::vector<std::vector<uint64_t>> impure_parts(p);

#pragma omp parallel for num_threads(p) schedule(dynamic, 4096)
  for (uint64_t j = 0; j < m; ++j) {
    const uint64_t start = keys[j].hash;
    if (start != j && !equal(uint64_t(keys[start].index), uint64_t(keys[j].index))) {
      impure_parts[omp_get_thread_num()].push_back(start);
    }
  }

  std::vector<uint64_t> impure;
  for (auto& part : impure_parts) impure.insert(impure.end(), part.begin(), part.end());
  impure_parts = decltype(impure_parts)();
  std::sort(impure.begin(), impure.end());
  impure.erase(std::unique(impure.begin(), impure.end()), impure.end());

  constexpr uint64_t skipped = none;
  std::vector<std::pair<uint64_t, uint64_t>> impure_ranges;
  impure_ranges.reserve(impure.size());
  for (uint64_t start : impure) {
    uint64_t end = start + 1;
    while (end < m && keys[end].hash == start) ++end;
    for (uint64_t j = start; j < end; ++j) keys[j].hash = skipped;
    impure_ranges.emplace_back(start, end);
  }

  std::vector<uint64_t> num_starts(p + 1, 0);

#pragma omp parallel for num_threads(p) schedule(static, 1)
  for (uint64_t c = 0; c < p; ++c) {
    uint64_t count = 0;
    for (uint64_t j = chunk[c]; j < chunk[c + 1]; ++j) count += keys[j].hash == j;
    num_starts[c + 1] = count;
  }

  for (uint64_t c = 0; c < p; ++c) num_starts[c + 1] += num_starts[c];

  std::vector<t_rank> ranks;
  lce::util::no_init_resize(ranks, s);
  std::vector<uint64_t> rep_index;
  lce::util::no_init_resize(rep_index, num_starts[p]);

#pragma omp parallel for num_threads(p) schedule(static, 1)
  for (uint64_t c = 0; c < p; ++c) {
    uint64_t id = num_starts[c];
    for (uint64_t j = chunk[c]; j < chunk[c + 1]; ++j) {
      if (keys[j].hash != j) continue;
      const uint64_t i = uint64_t(keys[j].index);
      rep_index[id] = i;
      ranks[i] = t_rank(int64_t(id));
      ++id;
    }
  }

#pragma omp parallel for num_threads(p) schedule(static)
  for (uint64_t j = 0; j < m; ++j) {
    const uint64_t start = keys[j].hash;
    if (start == skipped || start == j) continue;
    ranks[uint64_t(keys[j].index)] = ranks[uint64_t(keys[start].index)];
  }

  std::vector<uint64_t> members;
  for (auto [start, end] : impure_ranges) {
    members.clear();
    for (uint64_t j = start; j < end; ++j) members.push_back(uint64_t(keys[j].index));
    std::sort(members.begin(), members.end(), less);

    for (uint64_t k = 0; k < members.size(); ++k) {
      if (k == 0 || !equal(members[k - 1], members[k])) rep_index.push_back(members[k]);
      ranks[members[k]] = t_rank(int64_t(rep_index.size() - 1));
    }
  }

  keys = decltype(keys)();

  for (uint64_t i = m; i < s; ++i) {
    rep_index.push_back(i);
    ranks[i] = t_rank(int64_t(rep_index.size() - 1));
  }

  const uint64_t u = rep_index.size();

  struct __attribute__((packed)) rep_t {
    index_type pos;
    t_key_index id;
  };

  std::vector<rep_t> reps;
  lce::util::no_init_resize(reps, u);

#pragma omp parallel for num_threads(p) schedule(static)
  for (uint64_t k = 0; k < u; ++k) reps[k] = rep_t{index_type(sss[rep_index[k]]), t_key_index(k)};

  rep_index = decltype(rep_index)();

  ips4o::parallel::sort(reps.begin(), reps.end(), [&](const rep_t& a, const rep_t& b) {
    const uint64_t lhs = a.pos;
    const uint64_t rhs = b.pos;
    const uint64_t lce = text.lce(lhs, rhs, len);
    if (std::max(lhs, rhs) + lce == n) return lhs > rhs;
    if (lce < len) return text[lhs + lce] < text[rhs + lce];
    return sync_set.get_run_info(lhs) < sync_set.get_run_info(rhs);
  });

  std::vector<t_key_index> rank_of_id;
  lce::util::no_init_resize(rank_of_id, u);

#pragma omp parallel for num_threads(p) schedule(static)
  for (uint64_t k = 0; k < u; ++k) rank_of_id[uint64_t(reps[k].id)] = t_key_index(k + 1);

  reps = decltype(reps)();

#pragma omp parallel for num_threads(p) schedule(static)
  for (uint64_t i = 0; i < s; ++i) ranks[i] = t_rank(int64_t(uint64_t(rank_of_id[int64_t(ranks[i])])));

  return ranks;
}

template <typename t_rank, typename t_text, typename sss_type>
std::vector<t_rank> rank_3tau_substrings(t_text const& text, sss_type const& sync_set) {
  if (sync_set.get_sss().size() <= uint64_t(std::numeric_limits<uint32_t>::max())) {
    return rank_3tau_substrings_impl<t_rank, uint32_t>(text, sync_set);
  }

  return rank_3tau_substrings_impl<t_rank, typename sss_type::index_type>(text, sync_set);
}
}  // namespace lce::sss