/*******************************************************************************
 * lce/ds/lce_classic_for_sss.hpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once
#include <assert.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <libsais.h>
#include <libsais40.h>

#include <stdexcept>
#include <type_traits>
#include <utility>

#include "rmq/rmq_n.hpp"
#include "util/bit_aligned_vector.hpp"
#include "util/memory.hpp"
#include "util/threads.hpp"

#ifdef LCE_BENCHMARK_INTERNAL
#include <fmt/core.h>
#include <fmt/ranges.h>

#include "util/timer.hpp"
#ifdef LCE_BENCHMARK_SPACE
#include <malloc_count/malloc_count.h>
#endif
#endif

namespace lce::ds {

template <typename t_index_type>
class lce_classic_for_sss {
 public:
  lce_classic_for_sss() : m_size{0}, m_tau{0} {
  }

  template <typename t_text, typename t_sss, typename t_rank>
  lce_classic_for_sss(t_text const& text, std::vector<t_rank>& reduced_text,
                      t_sss const& sss, uint64_t tau)
      : m_size(reduced_text.size()), m_tau(tau) {
    const size_t reduced_size = reduced_text.size();
    if (reduced_size == 0) {
      return;
    }
    // sort sa
#ifdef LCE_BENCHMARK_INTERNAL
    lce::util::timer t;
#ifdef LCE_BENCHMARK_SPACE
    size_t mem_before = malloc_count_current();
    malloc_count_reset_peak();
#endif
#endif
    int64_t max_rank = 0;
#pragma omp parallel for reduction(max : max_rank)
    for (size_t i = 0; i < reduced_size; ++i) {
      max_rank = std::max<int64_t>(max_rank, int64_t(reduced_text[i]));
    }

    auto build = [&](auto& sa) {
#ifdef LCE_BENCHMARK_INTERNAL
      fmt::print(" sa_time={}", t.get_and_reset());
#ifdef LCE_BENCHMARK_SPACE
      fmt::print(" sa_mem={}", malloc_count_current() - mem_before);
      fmt::print(" sa_mem_peak={}", malloc_count_peak() - mem_before);
      mem_before = malloc_count_current();
      malloc_count_reset_peak();
#endif
#endif

      // build isa
      m_isa = lce::util::bit_aligned_vector(reduced_size, reduced_size);
#pragma omp parallel for
      for (size_t i = 0; i < sa.size(); ++i) {
        m_isa.set_parallel(uint64_t(int64_t(sa[i])), i);
      }

#ifdef LCE_BENCHMARK_INTERNAL
      fmt::print(" isa_time={}", t.get_and_reset());
#ifdef LCE_BENCHMARK_SPACE
      fmt::print(" isa_mem={}", malloc_count_current() - mem_before);
      fmt::print(" isa_mem_peak={}", malloc_count_peak() - mem_before);
      mem_before = malloc_count_current();
      malloc_count_reset_peak();
#endif
#endif

      // build lcp
      m_lcp = lce::util::bit_aligned_vector(sa.size(), text.size());
      m_lcp.set(0, 0);
      uint64_t p = std::min<uint64_t>(omp_get_max_threads(), reduced_size);

#pragma omp parallel num_threads(p)
      {
        const int t = omp_get_thread_num();
        const int nt = omp_get_num_threads();
        const size_t slice_size = reduced_size / nt;

        const size_t begin = t * slice_size;
        const size_t end = (t < nt - 1) ? (t + 1) * slice_size : reduced_size;

        size_t current_lcp = 0;
        for (size_t i{begin}; i < end; ++i) {
          size_t suffix_array_pos = m_isa[i];
          if (suffix_array_pos == 0) {
            continue;
          }
          assert(suffix_array_pos != 0);

          size_t preceding_suffix_pos = int64_t(sa[suffix_array_pos - 1]);
          current_lcp += text.lce(sss[i] + current_lcp,
                                  sss[preceding_suffix_pos] + current_lcp);
          m_lcp.set_parallel(suffix_array_pos, current_lcp);
          assert(text.lce(sss[i], sss[preceding_suffix_pos]) == current_lcp);

          if (i == end - 1) break;
          uint64_t diff = sss[i + 1] - sss[i];
          if (current_lcp < 2 * m_tau + diff) {
            current_lcp = 0;
          } else {
            current_lcp -= diff;
          }
        }
      }

#ifdef LCE_BENCHMARK_INTERNAL
      fmt::print(" lcp_time={}", t.get_and_reset());
#ifdef LCE_BENCHMARK_SPACE
      fmt::print(" lcp_mem={}", malloc_count_current() - mem_before);
      fmt::print(" lcp_mem_peak={}", malloc_count_peak() - mem_before);
      mem_before = malloc_count_current();
      malloc_count_reset_peak();
#endif
#endif

      sa = std::decay_t<decltype(sa)>();

      uint64_t max_lcp = 0;
#pragma omp parallel for reduction(max : max_lcp)
      for (size_t i = 0; i < m_lcp.size(); ++i) {
        max_lcp = std::max<uint64_t>(max_lcp, m_lcp[i]);
      }

      if (std::max<uint8_t>(1, std::bit_width(max_lcp)) < m_lcp.width()) {
        lce::util::bit_aligned_vector lcp(m_lcp.size(), max_lcp);

#pragma omp parallel
        {
          const uint64_t threads = omp_get_num_threads();
          const uint64_t chunk = (m_lcp.size() + threads - 1) / threads;
          const uint64_t beg = std::min<uint64_t>(m_lcp.size(), omp_get_thread_num() * chunk);
          const uint64_t end = std::min<uint64_t>(m_lcp.size(), beg + chunk);
          lcp.fill(beg, end, [&](uint64_t i) { return m_lcp[i]; });
        }

        m_lcp = std::move(lcp);
      }

      // build rmq
      m_rmq = lce::rmq::rmq_n<t_index_type, uint32_t, 128, lce::util::bit_aligned_view>(m_lcp);

#ifdef LCE_BENCHMARK_INTERNAL
      fmt::print(" rmq_time={}", t.get_and_reset());
#ifdef LCE_BENCHMARK_SPACE
      fmt::print(" rmq_mem={}", malloc_count_current() - mem_before);
      fmt::print(" rmq_mem_peak={}", malloc_count_peak() - mem_before);
#endif
#endif
    };

    constexpr uint64_t max_int32 = uint64_t(std::numeric_limits<int32_t>::max());

    if constexpr (std::is_same_v<t_rank, int32_t>) {
      std::vector<int32_t> sa;
      lce::util::no_init_resize(sa, reduced_size);
      if (libsais_int_omp(reduced_text.data(), sa.data(), int32_t(reduced_size), int32_t(max_rank + 1), 0,
                          lce::util::sais_threads()) != 0) {
        throw std::runtime_error("libsais_int_omp failed");
      }
      reduced_text = std::vector<int32_t>();
      build(sa);
    } else if (reduced_size < max_int32 && uint64_t(max_rank) < max_int32) {
      std::vector<int32_t> text32;
      lce::util::no_init_resize(text32, reduced_size);
#pragma omp parallel for
      for (size_t i = 0; i < reduced_size; ++i) text32[i] = int32_t(int64_t(reduced_text[i]));
      reduced_text = std::vector<t_rank>();
      std::vector<int32_t> sa;
      lce::util::no_init_resize(sa, reduced_size);
      if (libsais_int_omp(text32.data(), sa.data(), int32_t(reduced_size), int32_t(max_rank + 1), 0,
                          lce::util::sais_threads()) != 0) {
        throw std::runtime_error("libsais_int_omp failed");
      }
      text32 = std::vector<int32_t>();
      build(sa);
    } else {
      std::vector<sa_int40_t> sa;
      lce::util::no_init_resize(sa, reduced_size);
      if (libsais40_impl::libsais40_long_omp(reduced_text.data(), sa.data(), int64_t(reduced_size),
                                             max_rank + 1, 0, lce::util::sais_threads()) != 0) {
        throw std::runtime_error("libsais40_long_omp failed");
      }
      reduced_text = std::vector<t_rank>();
      build(sa);
    }
  }

  // Return the number of common letters in text[i..] and text[j..]. Here i and
  // j must be different.
  size_t lce_uneq(size_t i, size_t j) const {
    assert(i != j);
    return lce_lr(i, j);
  }

  // Return the number of common letters in text[i..] and text[j..].
  // Here l must be smaller than r.
  size_t lce_lr(size_t l, size_t r) const {
    return m_lcp[m_rmq.rmq_shifted(m_isa[l], m_isa[r])];
  }

 private:
  size_t m_size;
  uint64_t m_tau;
  lce::util::bit_aligned_vector m_isa;
  lce::util::bit_aligned_vector m_lcp;
  lce::rmq::rmq_n<t_index_type, uint32_t, 128, lce::util::bit_aligned_view> m_rmq;
};
}  // namespace lce::ds
