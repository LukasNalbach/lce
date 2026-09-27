/*******************************************************************************
 * lce/ds/lce_sss.hpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <bit>
#include <limits>
#include <vector>

#include "ds/lce_classic_for_sss.hpp"
#include "text/direct_text.hpp"
#include "pred/pred_index.hpp"
#include "rolling_hash/reduce_fingerprints.hpp"
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

template <typename t_text = uint8_t, typename t_index_type = uint32_t,
          bool t_prefer_long = false>
class lce_sss {
 public:
  using text_type = lce::text::text_of_t<t_text>;
  using char_type = typename text_type::char_type;
  __extension__ typedef unsigned __int128 uint128_t;
  lce_sss() : m_size(0), m_tau(0) {}

  lce_sss(char_type const* text, size_t size, uint64_t tau)
    requires(lce::text::is_direct_text_v<text_type>)
      : lce_sss(text_type(const_cast<char_type*>(text), size), tau) {}

  lce_sss(text_type const& text, uint64_t tau)
      : m_text(text), m_size(text.size()), m_tau(tau) {
#ifdef LCE_BENCHMARK_INTERNAL
    lce::util::timer t;
#ifdef LCE_BENCHMARK_SPACE
    size_t mem_before = malloc_count_current();
    malloc_count_reset_peak();
#endif
#endif

    m_sync_set = rolling_hash::sss<t_index_type>(m_text, m_tau, false);
    // check_string_synchronizing_set(text, m_sync_set);

#ifdef LCE_BENCHMARK_INTERNAL
    fmt::print(" sss_time={}", t.get_and_reset());
    fmt::print(" sss_size={}", m_sync_set.size());
    fmt::print(" sss_runs={}", m_sync_set.num_runs());
#ifdef LCE_BENCHMARK_SPACE
    fmt::print(" sss_mem={}", malloc_count_current() - mem_before);
    fmt::print(" sss_mem_peak={}", malloc_count_peak() - mem_before);
    mem_before = malloc_count_current();
    malloc_count_reset_peak();
#endif
#endif

    m_pred = lce::pred::pred_index<t_index_type, t_index_type, lce::util::bit_aligned_view>(
        m_sync_set.get_sss(), uint8_t(std::bit_width(m_tau) + 1));

#ifdef LCE_BENCHMARK_INTERNAL
    fmt::print(" pred_time={}", t.get_and_reset());
#ifdef LCE_BENCHMARK_SPACE
    fmt::print(" pred_mem={}", malloc_count_current() - mem_before);
    fmt::print(" pred_mem_peak={}", malloc_count_peak() - mem_before);
    mem_before = malloc_count_current();
    malloc_count_reset_peak();
#endif
#endif

    auto const& sss = m_sync_set.get_sss();

    auto build_fp_lce = [&](auto reduced_fps) {
      m_sync_set.free_run_info();

#ifdef LCE_BENCHMARK_INTERNAL
      fmt::print(" alphabet_reduction_time={}", t.get_and_reset());
#ifdef LCE_BENCHMARK_SPACE
      fmt::print(" alphabet_reduction_mem={}", malloc_count_current() - mem_before);
      fmt::print(" alphabet_reduction_mem_peak={}", malloc_count_peak() - mem_before);
#endif
#endif

      m_fp_lce = lce::ds::lce_classic_for_sss<t_index_type>(m_text, reduced_fps, sss, m_tau);
    };

    if (sss.size() < uint64_t(std::numeric_limits<int32_t>::max())) {
      build_fp_lce(reduce_fps_3tau_lexicographic<int32_t>(m_text, m_sync_set));
    } else {
      build_fp_lce(reduce_fps_3tau_lexicographic<sa_int40_t>(m_text, m_sync_set));
    }
  }

  template <typename C>
    requires(!std::is_same_v<std::remove_cvref_t<C>, text_type>)
  lce_sss(C const& container, uint64_t tau)
      : lce_sss(container.data(), container.size(), tau) {}

  // Return the number of common letters in text[i..] and text[j..].
  size_t lce(size_t i, size_t j) const {
    if (i == j) [[unlikely]] {
      assert(i < m_size);
      return m_size - i;
    }
    return lce_uneq(i, j);
  }

  // Return the number of common letters in text[i..] and text[j..]. Here i
  // and j must be different.
  size_t lce_uneq(size_t i, size_t j) const {
    assert(i != j);

    size_t l = std::min(i, j);
    size_t r = std::max(i, j);

    return lce_lr(l, r);
  }

  // Return the number of common letters in text[i..] and text[j..].
  // Here l must be smaller than r.
  inline uint64_t lce_lr(size_t l, size_t r) const {
    auto const& sss = m_sync_set.get_sss();
    size_t l_, r_;

    if constexpr (t_prefer_long) {
      // Only scan until synchronizing position
      size_t lce_max{m_size - r};
      size_t lce_local_max{std::min<size_t>(3 * m_tau, lce_max)};

      pred::result l_res = m_pred.successor(l);
      pred::result r_res = m_pred.successor(r);
      l_ = l_res.pos;
      r_ = r_res.pos;
      if (l_res.exists && r_res.exists && (sss[l_] - l == sss[r_] - r)) {
        lce_local_max =
            std::min(lce_local_max, static_cast<size_t>(sss[l_] - l));
      }

      size_t lce_local = m_text.lce(l, r, lce_local_max);

      // Case 0: Mismatch at first 3*tau symbols
      if (lce_local < lce_local_max || lce_local == lce_max) {
        return lce_local;
      }

      if (!l_res.exists || !r_res.exists) {
        return m_text.lce(l, r, lce_max);
      }
    } else {
      // Naive part until synchronizing position
      size_t lce_max{m_size - r};
      size_t lce_local_max{std::min<size_t>(3 * m_tau, lce_max)};
      size_t lce_local = m_text.lce(l, r, lce_local_max);

      // Case 0: Mismatch at first 3*tau symbols
      if (lce_local < lce_local_max || lce_local == lce_max) {
        return lce_local;
      }
      pred::result l_res = m_pred.successor(l);
      pred::result r_res = m_pred.successor(r);

      if (!l_res.exists || !r_res.exists) {
        return m_text.lce(l, r, lce_max);
      }

      l_ = l_res.pos;
      r_ = r_res.pos;
    }

    if (sss[l_] - l != sss[r_] - r) {
      // Case 1: Positions l' and r' don't sync, (because they are at the end of
      // runs).
      size_t final_lce = std::min<size_t>(sss[l_] - l, sss[r_] - r) + 2 * m_tau - 1;
      assert(final_lce == m_text.lce(l, r));
      return final_lce;
    } else {
      // Case 2: Positions l' and r' are synchronized.
      size_t final_lce = (sss[l_] - l) + m_fp_lce.lce_lr(l_, r_);
      assert(final_lce == m_text.lce(l, r));
      return final_lce;
    }
  }

  // Return {b, lce}, where lce is the number of common letters in text[i..]
  // and text[j..] and b tells whether the lce ends with a mismatch.
  std::pair<bool, size_t> lce_mismatch(size_t i, size_t j) const {
    if (i == j) [[unlikely]] {
      assert(i < m_size);
      return {false, m_size - i};
    }

    size_t l = std::min(i, j);
    size_t r = std::max(i, j);

    size_t lce = lce_lr(l, r);
    return {r + lce != m_size, lce};
  }

  // Return whether text[i..] is lexicographic smaller than text[j..]. Here i
  // and j must be different.
  bool is_leq_suffix(size_t i, size_t j) const {
    assert(i != j);
    size_t lce_val = lce_uneq(i, j);
    return (
        i + lce_val == m_size ||
        ((j + lce_val != m_size) && m_text.less_char(i + lce_val, j + lce_val)));
  }

  char_type operator[](size_t i) const { return m_text.char_at(i); }

  size_t size() const { return m_size; }

  bool has_runs() const {
    return m_sync_set.has_runs();
  }

  uint64_t tau() const { return m_tau; }

  const lce::util::bit_aligned_vector& get_sync_set() const { return m_sync_set.get_sss(); }

 private:
  text_type m_text;
  size_t m_size;
  uint64_t m_tau;

  lce::pred::pred_index<t_index_type, t_index_type, lce::util::bit_aligned_view>
      m_pred;
  rolling_hash::sss<t_index_type> m_sync_set;
  lce::ds::lce_classic_for_sss<t_index_type> m_fp_lce;
};
}  // namespace lce::ds