/*******************************************************************************
 * lce/sss/string_synchronizing_set.hpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <omp.h>

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

#include "rolling_hash/rolling_hash.hpp"
#include "text/direct_text.hpp"
#include "util/bit_aligned_vector.hpp"
#include "util/memory.hpp"

namespace lce::sss {

template <typename t_index = uint32_t>
class string_synchronizing_set {
 public:
  typedef t_index index_type;
  __extension__ typedef unsigned __int128 uint128_t;

  string_synchronizing_set() : m_tau(0), m_fps_calculated(false), m_runs_detected(false) {
  }

  template <typename t_char_type>
    requires(sizeof(t_char_type) == 1)
  string_synchronizing_set(t_char_type const* text, size_t size, uint64_t tau, bool calculate_fps = false)
      : string_synchronizing_set(
            lce::text::direct_text<t_char_type>(const_cast<t_char_type*>(text), size), tau, calculate_fps) {
  }

  template <typename t_container>
    requires(!lce::text::text_access<t_container> && !std::is_pointer_v<t_container>)
  string_synchronizing_set(t_container const& container, uint64_t tau, bool calculate_fps = false)
      : string_synchronizing_set(container.data(), container.size(), tau, calculate_fps) {
  }

  template <lce::text::text_access t_text>
  string_synchronizing_set(t_text const& text, uint64_t tau, bool calculate_fps = false)
      : m_tau(tau), m_fps_calculated(calculate_fps), m_runs_detected(false) {
    if (text.size() > 5 * tau) {
      build(text);
    }
  }

  uint64_t tau() const { return m_tau; }

  bool fps_calculated() const {
    return m_fps_calculated;
  }

  lce::util::bit_aligned_vector const& get_sss() const {
    return m_sss;
  }

  std::vector<uint128_t> const& get_fps() const {
    assert(m_fps_calculated);
    return m_fps;
  }

  void free_fps() {
    m_fps = std::vector<uint128_t>{};
    m_fps_calculated = false;
  }

  size_t num_runs() const {
    return m_runs.size();
  }

  bool has_runs() const {
    return m_runs_detected;
  }

  size_t size() const {
    return m_sss.size();
  }

  uint64_t operator[](size_t i) const {
    return m_sss[i];
  }

  int64_t get_run_info(size_t pos) const {
    const uint64_t bucket = uint64_t(pos) >> m_run_shift;
    if (bucket + 1 >= m_run_buckets.size()) return 0;
    for (uint64_t k = m_run_buckets[bucket]; k < m_run_buckets[bucket + 1]; ++k) {
      if (m_runs[k].first == pos) return m_runs[k].second;
    }
    return 0;
  }

  void free_run_info() {
    m_runs = std::vector<std::pair<uint64_t, int64_t>>{};
    m_run_buckets = std::vector<uint64_t>{};
  }

 private:
  struct interval {
    uint64_t beg;
    uint64_t end;
  };

  static constexpr uint64_t parts_per_thread = 8;
  static constexpr uint64_t min_density_saving_percent = 10;

  struct part_t {
    std::vector<t_index> sss;
    std::vector<uint128_t> fps;
    std::vector<t_index> plain_only;
    std::vector<uint128_t> plain_only_fps;
    std::vector<t_index> q_only;
    std::vector<std::pair<uint64_t, int64_t>> runs;
  };

  struct shared_t {
    std::atomic<uint64_t> plain_count{0};
    uint64_t threshold = 0;
  };

  template <typename t_text>
  void build(t_text const& text) {
    const uint64_t tau = m_tau;
    const uint64_t n = text.size();
    const uint64_t sss_end = n - 2 * tau + 1;
    const rolling_hash::rk_mersenne61 rk(tau, 296819);
    const std::optional<rolling_hash::rk_prime<>> rk3 = m_fps_calculated
        ? std::optional<rolling_hash::rk_prime<>>(std::in_place, 3 * tau, 296819) : std::nullopt;

    const uint64_t num_parts = std::clamp<uint64_t>(sss_end / (8 * tau), 1,
                                                    uint64_t(omp_get_max_threads()) * parts_per_thread);
    std::vector<part_t> parts(num_parts);
    shared_t shared;
    shared.threshold = n * 4 / tau;

#pragma omp parallel for schedule(dynamic, 1)
    for (uint64_t t = 0; t < num_parts; ++t) {
      const uint64_t from = (sss_end * t) / num_parts;
      const uint64_t to = (sss_end * (t + 1)) / num_parts;
      scan(text, from, to, parts[t], shared, rk, rk3 ? &*rk3 : nullptr);
    }
    uint64_t density_count = 0;
    for (auto const& part : parts) density_count += part.sss.size();
    const uint64_t plain_count = shared.plain_count.load();
    m_runs_detected = plain_count > shared.threshold ||
                      density_count * 100 <= plain_count * (100 - min_density_saving_percent);
    const bool runs = m_runs_detected;

    std::vector<uint64_t> write_pos{0};
    for (auto const& part : parts) {
      const uint64_t count = runs ? part.sss.size()
                                  : part.sss.size() - part.q_only.size() + part.plain_only.size();
      write_pos.push_back(write_pos.back() + count);
    }
    const uint64_t sss_size = write_pos.back() + (runs ? 1 : 0);

    m_sss = lce::util::bit_aligned_vector(sss_size, n);
    if (m_fps_calculated) {
      lce::util::no_init_resize(m_fps, sss_size);
    }

#pragma omp parallel for schedule(static, 1)
    for (uint64_t t = 0; t < parts.size(); ++t) {
      part_t& part = parts[t];
      const uint64_t beg = write_pos[t];
      if (runs || (part.q_only.empty() && part.plain_only.empty())) {
        m_sss.fill(beg, beg + part.sss.size(),
                   [&](uint64_t i) { return uint64_t(part.sss[i - beg]); });
        if (m_fps_calculated) {
          std::copy(part.fps.begin(), part.fps.end(), m_fps.begin() + beg);
        }
      } else {
        uint64_t a = 0, q = 0, b = 0, at = beg;
        const uint64_t sa = part.sss.size(), sq = part.q_only.size(), sb = part.plain_only.size();
        std::vector<uint64_t> merged;
        merged.reserve(write_pos[t + 1] - beg);
        while (a < sa || b < sb) {
          if (a < sa && q < sq && uint64_t(part.q_only[q]) == uint64_t(part.sss[a])) {
            ++a;
            ++q;
            continue;
          }
          if (b >= sb || (a < sa && uint64_t(part.sss[a]) < uint64_t(part.plain_only[b]))) {
            merged.push_back(part.sss[a]);
            if (m_fps_calculated) m_fps[at] = part.fps[a];
            ++a;
          } else {
            merged.push_back(part.plain_only[b]);
            if (m_fps_calculated) m_fps[at] = part.plain_only_fps[b];
            ++b;
          }
          ++at;
        }
        m_sss.fill(beg, beg + merged.size(), [&](uint64_t i) { return merged[i - beg]; });
      }
      part.sss = std::vector<t_index>{};
      part.fps = std::vector<uint128_t>{};
      part.plain_only = std::vector<t_index>{};
      part.plain_only_fps = std::vector<uint128_t>{};
      part.q_only = std::vector<t_index>{};
    }

    if (runs) {
      m_sss.set(sss_size - 1, n - 2 * tau + 1);
      if (m_fps_calculated) {
        const uint64_t real = sss_size - 1;
#pragma omp parallel for
        for (uint64_t k = 1; k < real; ++k) {
          const uint64_t distance = m_sss[k] - m_sss[k - 1];
          if (distance > tau) {
            m_fps[k - 1] += (uint128_t{distance} << 107);
          }
        }
        m_fps.back() = 1;
      }
      uint64_t num_runs = 0;
      for (auto const& part : parts) num_runs += part.runs.size();
      m_runs.reserve(num_runs);
      for (auto& part : parts) {
        m_runs.insert(m_runs.end(), part.runs.begin(), part.runs.end());
        part.runs = std::vector<std::pair<uint64_t, int64_t>>{};
      }
      if (!m_runs.empty()) {
        m_run_shift = std::bit_width(std::max<uint64_t>(1, n / m_runs.size())) - 1;
        m_run_buckets.assign((n >> m_run_shift) + 2, 0);
        for (auto const& run : m_runs) ++m_run_buckets[(run.first >> m_run_shift) + 1];
        for (uint64_t b = 1; b < m_run_buckets.size(); ++b) m_run_buckets[b] += m_run_buckets[b - 1];
      }
    }
  }

  template <typename t_text>
  uint64_t find_period(t_text const& text, const uint64_t x, const uint64_t block,
                       const uint64_t max_period) const {
    if constexpr (lce::text::is_direct_text_v<t_text> && t_text::is_byte_text) {
      const uint8_t* data = reinterpret_cast<const uint8_t*>(text.data());
#if defined(__AVX2__)
      if (block >= max_period + 40) {
        __m256i head[8];
        for (uint64_t j = 0; j < 8; ++j) head[j] = _mm256_set1_epi8(char(data[x + j]));
        for (uint64_t base = 1; base <= max_period; base += 32) {
          uint32_t hits = max_period - base >= 31
              ? ~uint32_t(0) : (uint32_t(1) << (max_period - base + 1)) - 1;
          for (uint64_t j = 0; j < 8 && hits != 0; ++j) {
            const __m256i v =
                _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + x + base + j));
            hits &= uint32_t(_mm256_movemask_epi8(_mm256_cmpeq_epi8(v, head[j])));
          }
          while (hits != 0) {
            const uint64_t p = base + std::countr_zero(hits);
            if (text.lce(x, x + p, block - p) >= block - p) return p;
            hits &= hits - 1;
          }
        }
        return 0;
      }
#endif
      if (block >= max_period + 8) {
        const uint64_t head = lce::util::load_u64(data + x);
        for (uint64_t p = 1; p <= max_period; ++p) {
          if (lce::util::load_u64(data + x + p) == head &&
              text.lce(x, x + p, block - p) >= block - p) {
            return p;
          }
        }
        return 0;
      }
    }
    for (uint64_t p = 1; p <= max_period; ++p) {
      if (text[x] == text[x + p] && text.lce(x, x + p, block - p) >= block - p) {
        return p;
      }
    }
    return 0;
  }

  template <typename t_text>
  void scan(t_text const& text, const uint64_t from, const uint64_t to, part_t& part, shared_t& shared,
            rolling_hash::rk_mersenne61 const& rk, rolling_hash::rk_prime<> const* rk3) const {
    const uint64_t tau = m_tau;
    const uint64_t n = text.size();
    const uint64_t small_tau = tau / 4;
    const uint64_t max_period = small_tau >= 2 ? small_tau - 1 : small_tau;
    const uint64_t block = std::max<uint64_t>(2 * max_period, 2);
    const uint64_t grid = tau + 1 - block;
    const uint64_t horizon = std::min<uint64_t>(n, to + 2 * tau);
    const uint64_t cap = std::bit_ceil(tau + 2);
    const uint64_t mask = cap - 1;
    const bool calculate_fps = rk3 != nullptr;
    const uint64_t fp3_end = n + 1 > 3 * tau ? n + 1 - 3 * tau : 0;
    const uint128_t tail_fp = (uint128_t{1} << 107) - 2 - n;
    constexpr uint64_t none = std::numeric_limits<uint64_t>::max();
    constexpr uint64_t flush_interval = uint64_t{1} << 14;

    std::vector<uint64_t> phi(cap);
    std::vector<uint8_t> in_q(cap);
    std::vector<uint64_t> dq_value(cap);
    std::vector<uint64_t> dq_pos(cap);
    uint64_t dq_head = 0;
    uint64_t dq_tail = 0;
    std::vector<interval> q;
    uint64_t q_head = 0;
    uint64_t next_block = max_period > 0 ? from : none;
    bool tracking = true;
    bool forked = false;
    uint64_t plain_local = 0;
    uint64_t plain_flushed = 0;
    uint64_t countdown = flush_interval;

    auto detect = [&](uint64_t next_block, const uint64_t limit) {
      while (next_block <= limit && next_block + block <= n) {
        const uint64_t x = next_block;
        const uint64_t period = find_period(text, x, block, max_period);
        if (period == 0) {
          next_block += grid;
          continue;
        }
        const uint64_t beg = x - (x > from ? text.lce_left(x - 1, x + period - 1, x - from) : 0);
        const bool owner = beg > from && beg - 1 < to;
        const uint64_t right_limit = owner ? n : std::max(horizon, x + block);
        const uint64_t end = x + period - 1 + text.lce(x, x + period, right_limit - x - period);
        const uint64_t len = end - beg + 1;
        if (len < tau) {
          next_block += grid;
          continue;
        }
        q.push_back({beg, end - tau + 1});
        if (owner && len >= 3 * tau - 1) {
          const bool greater = end + 1 < n && text[end + 1] > text[end + 1 - period];
          const int64_t info = int64_t(n) - int64_t(end - 2 * tau + 2) + int64_t(beg - 1);
          part.runs.emplace_back(beg - 1, greater ? info : -info);
        }
        if (!owner && end + 1 >= right_limit) {
          next_block = none;
        } else {
          const uint64_t skip = end + 2 > block + x ? end + 2 - block - x : 0;
          next_block = x + std::max<uint64_t>(1, (skip + grid - 1) / grid) * grid;
        }
      }
      return next_block;
    };

    uint64_t q_size = 0;
    auto query_q = [&](const uint64_t j) {
      while (q_head < q_size && q[q_head].end < j) ++q_head;
      return q_head < q_size && q[q_head].beg <= j;
    };

    auto rescan = [&](const uint64_t i, const uint64_t k, const uint64_t last_q) {
      if (last_q == none || last_q < i) {
        uint64_t best = i;
        for (uint64_t j = i + 1; j <= k; ++j) {
          if (phi[j & mask] < phi[best & mask]) best = j;
        }
        return best;
      }
      uint64_t best = none;
      for (uint64_t j = i; j <= k; ++j) {
        if (in_q[j & mask]) continue;
        if (best == none || phi[j & mask] < phi[best & mask]) best = j;
      }
      return best;
    };

    auto push_plain = [&](const uint64_t j) {
      const uint64_t value = phi[j & mask];
      while (dq_tail != dq_head && dq_value[(dq_tail - 1) & mask] > value) --dq_tail;
      dq_value[dq_tail & mask] = value;
      dq_pos[dq_tail & mask] = j;
      ++dq_tail;
    };

    auto fill_window = [&](const uint64_t i) {
      uint64_t f = 0;
      auto c = text.cursor_at(i);
      for (uint64_t j = 0; j < tau; ++j) f = rk.roll_in(f, c.next());
      phi[i & mask] = f;
      auto o = text.cursor_at(i);
      for (uint64_t j = i + 1; j < i + tau; ++j) {
        f = rk.roll(f, o.next(), c.next());
        phi[j & mask] = f;
      }
      return f;
    };

    auto fill_flags = [&](const uint64_t i) {
      uint64_t last = none;
      for (uint64_t j = i; j < i + tau; ++j) {
        const bool jq = query_q(j);
        in_q[j & mask] = jq;
        if (jq) last = j;
      }
      return last;
    };

    auto fp3_at = [&](const uint64_t i) {
      uint128_t f = 0;
      auto c = text.cursor_at(i);
      for (uint64_t j = 0; j < 3 * tau; ++j) f = rk3->roll_in(f, c.next());
      return f;
    };

    next_block = detect(next_block, from + tau + grid);
    q_size = q.size();
    uint64_t fp = fill_window(from);
    uint64_t last_q = fill_flags(from);
    uint64_t first_min = rescan(from, from + tau - 1, last_q);
    uint128_t fp3 = calculate_fps && from < fp3_end ? fp3_at(from) : uint128_t{0};
    if (last_q != none) {
      forked = true;
      for (uint64_t j = from; j < from + tau; ++j) push_plain(j);
    }

    auto out = text.cursor_at(from + tau - 1);
    auto in = text.cursor_at(from + 2 * tau - 1);
    auto out3 = text.cursor_at(from);
    auto in3 = text.cursor_at(std::min<uint64_t>(from + 3 * tau, n - 1));

    for (uint64_t i = from; i < to; ++i) {
      const uint64_t k = i + tau;
      fp = rk.roll(fp, out.next(), in.next());
      phi[k & mask] = fp;
      if (calculate_fps && i > from && i < fp3_end) fp3 = rk3->roll(fp3, out3.next(), in3.next());
      if (next_block <= k + grid - 1) {
        next_block = detect(next_block, k + grid - 1);
        q_size = q.size();
      }

      const bool kq = query_q(k);
      in_q[k & mask] = kq;
      if (kq) {
        if (tracking && !forked) {
          dq_head = dq_tail = 0;
          for (uint64_t j = i; j < k; ++j) push_plain(j);
          forked = true;
        }
        last_q = k;
      } else if (first_min == none || fp < phi[first_min & mask]) {
        first_min = k;
      }
      if (first_min != none && first_min < i) first_min = rescan(i, k, last_q);

      bool member = false;
      if (first_min != none) {
        const uint64_t m = phi[first_min & mask];
        member = phi[i & mask] == m || fp == m;
      }
      if (member) {
        part.sss.push_back(t_index(i));
        if (calculate_fps) part.fps.push_back(i < fp3_end ? fp3 : tail_fp + i);
      }

      if (tracking) {
        if (forked) {
          push_plain(k);
          while (dq_pos[dq_head & mask] < i) ++dq_head;
          const uint64_t m = dq_value[dq_head & mask];
          const bool plain = phi[i & mask] == m || fp == m;
          plain_local += plain;
          if (plain && !member) {
            part.plain_only.push_back(t_index(i));
            if (calculate_fps) part.plain_only_fps.push_back(i < fp3_end ? fp3 : tail_fp + i);
          } else if (member && !plain) {
            part.q_only.push_back(t_index(i));
          }
          if (last_q <= i) forked = false;
        } else {
          plain_local += member;
        }
        if (--countdown == 0) {
          countdown = flush_interval;
          const uint64_t delta = plain_local - plain_flushed;
          plain_flushed = plain_local;
          if (shared.plain_count.fetch_add(delta, std::memory_order_relaxed) + delta > shared.threshold) {
            tracking = false;
            forked = false;
            part.plain_only = std::vector<t_index>{};
            part.plain_only_fps = std::vector<uint128_t>{};
            part.q_only = std::vector<t_index>{};
          }
        }
      } else if (first_min == none && kq && q[q_head].end > k + 2 * tau) {
        const uint64_t target = q[q_head].end - tau + 1;
        if (target >= to) break;
        i = target - 1;
        fp = fill_window(target);
        last_q = fill_flags(target);
        first_min = rescan(target, target + tau - 1, last_q);
        out = text.cursor_at(target + tau - 1);
        in = text.cursor_at(target + 2 * tau - 1);
        if (calculate_fps && target < fp3_end) {
          fp3 = fp3_at(target - 1);
          out3 = text.cursor_at(target - 1);
          in3 = text.cursor_at(target + 3 * tau - 1);
        }
      }
    }
    if (tracking) {
      shared.plain_count.fetch_add(plain_local - plain_flushed, std::memory_order_relaxed);
    }
  }

  uint64_t m_tau;
  lce::util::bit_aligned_vector m_sss;
  std::vector<uint128_t> m_fps;
  bool m_fps_calculated;
  bool m_runs_detected;
  std::vector<std::pair<uint64_t, int64_t>> m_runs;
  std::vector<uint64_t> m_run_buckets;
  uint64_t m_run_shift = 0;
};
}  // namespace lce::sss
