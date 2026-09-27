/*******************************************************************************
 * lce/pred/pred_index.hpp
 *
 * Copyright (C) 2020 Patrick Dinklage <patrick.dinklage@tu-dortmund.de>
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <omp.h>

#include <algorithm>

#include "pred_result.hpp"
#include "util/memory.hpp"

namespace lce::pred {

// the "idx" data structure for successor queries
template <typename T, typename index_type, typename t_array = const T*>
class pred_index {
 public:
  typedef T data_type;
  inline pred_index() : m_size(0), m_min(0), m_max(0), m_lo_bits(0) {
  }

  template <typename C>
  pred_index(C const& container, uint8_t lo_bits)
      : pred_index(array_of(container), container.size(), lo_bits) {
  }

  inline pred_index(t_array data, size_t size, uint8_t lo_bits)
      : m_data(data), m_size(size),
        m_min(size == 0 ? 0 : uint64_t(data[0])),
        m_max(size == 0 ? 0 : uint64_t(data[size - 1])),
        m_lo_bits(lo_bits) {
    if (size == 0) {
      return;
    }

#ifndef NDEBUG
    for (size_t i = 1; i < size; ++i) assert(at(i - 1) <= at(i));
#endif

    // build an index for high bits
    lce::util::no_init_resize(m_hi_idx, (m_max >> m_lo_bits) + 2);
    uint64_t p = std::min<uint64_t>(omp_get_max_threads(), m_size);

#pragma omp parallel num_threads(p)
    {
      const int t = omp_get_thread_num();
      const int nt = omp_get_num_threads();
      const size_t slice_size = m_size / nt;
      const size_t start_i = t * slice_size;
      const size_t end_i = (t < nt - 1) ? (t + 1) * slice_size : m_size;

      if (t == 0) {
        m_hi_idx[0] = 0;
      }
      uint64_t prev_key = (t == 0) ? 0 : hi(data[start_i - 1]);
      for (size_t i = start_i; i < end_i; ++i) {
        const uint64_t cur_key = hi(data[i]);
        if (cur_key > prev_key) {
          for (uint64_t key = prev_key + 1; key <= cur_key; key++) {
            m_hi_idx[key] = i;
          }
          prev_key = cur_key;
        }
      }
    }
    m_hi_idx[hi(m_max) + 1] = m_size;
  }

 private:
  inline uint64_t hi(uint64_t x) const { return x >> m_lo_bits; }

  template <typename C>
  static t_array array_of(const C& container) {
    if constexpr (requires { container.view(); }) return container.view();
    else return container.data();
  }

  inline uint64_t at(size_t i) const { return uint64_t(m_data[i]); }

  inline size_t upper_bound(size_t p, size_t q, uint64_t x) const {
    while (p < q) {
      const size_t m = p + (q - p) / 2;
      if (at(m) <= x) p = m + 1;
      else q = m;
    }

    return p;
  }

  inline size_t lower_bound(size_t p, size_t q, uint64_t x) const {
    while (p < q) {
      const size_t m = p + (q - p) / 2;
      if (at(m) < x) p = m + 1;
      else q = m;
    }

    return p;
  }

  t_array m_data {};
  size_t m_size;
  uint64_t m_min;
  uint64_t m_max;
  uint8_t m_lo_bits;

  std::vector<index_type> m_hi_idx;

 public:
  // finds the greatest element less than OR equal to x
  inline result predecessor(const uint64_t x) const {
    if (m_size == 0 || x < m_min) [[unlikely]]
      return result{false, 0};
    if (x >= m_max) [[unlikely]]
      return result{true, m_size - 1};

    const uint64_t key = hi(x);
    const size_t p = m_hi_idx[key];
    const size_t q = m_hi_idx[key + 1];
    return {true, upper_bound(p, q, x) - 1};
  }

  // finds the smallest element greater than OR equal to x
  inline result successor(const uint64_t x) const {
    if (m_size == 0 || x > m_max) [[unlikely]]
      return result{false, 0};
    if (x <= m_min) [[unlikely]]
      return result{true, 0};

    const uint64_t key = hi(x);
    const size_t p = m_hi_idx[key];
    const size_t q = m_hi_idx[key + 1];
    return {true, lower_bound(p, q, x)};
  }
};
}  // namespace lce::pred