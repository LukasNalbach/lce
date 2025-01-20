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

namespace lce::pred {

// the "idx" data structure for successor queries
template <typename T, size_t m_lo_bits, typename index_type>
class pred_index {
 public:
  typedef T data_type;
  inline pred_index() : m_data(nullptr), m_size(0), m_min(0), m_max(0) {
  }

  template <typename C>
  pred_index(C const& container)
      : pred_index(container.data(), container.size()) {
  }

  inline pred_index(T const* data, size_t size)
      : m_data(data), m_size(size), m_min(data[0]), m_max(data[size - 1]) {
    assert(std::is_sorted(m_data, m_data + size));

    // build an index for high bits
    m_hi_idx.resize((uint64_t(m_max) >> m_lo_bits) + 2);
#pragma omp parallel
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
  static constexpr size_t m_hi_bits = 8 * sizeof(T) - m_lo_bits;

  static constexpr uint64_t hi(uint64_t x) {
    return x >> m_lo_bits;
  }

  const T* m_data;
  size_t m_size;
  T m_min;
  T m_max;

  std::vector<index_type> m_hi_idx;

 public:
  // finds the greatest element less than OR equal to x
  inline result predecessor(const T x) const {
    if ((x < m_min)) [[unlikely]]
      return result{false, 0};
    if ((x >= m_max)) return result{true, m_size - 1};

    const uint64_t key = hi(x);
    const size_t p = m_hi_idx[key];
    const size_t q = m_hi_idx[key + 1];
    return {true, static_cast<size_t>(
                      std::distance(
                          m_data, std::upper_bound(m_data + p, m_data + q, x)) -
                      1)};
  }

  // finds the smallest element greater than OR equal to x
  inline result successor(const T x) const {
    if ((x <= m_min)) [[unlikely]]
      return result{true, 0};
    if ((x > m_max)) [[unlikely]]
      return result{false, 0};

    const uint64_t key = hi(x);
    const size_t p = m_hi_idx[key];
    const size_t q = m_hi_idx[key + 1];
    return {true, static_cast<size_t>(std::distance(
                      m_data, std::lower_bound(m_data + p, m_data + q, x)))};
  }
};
}  // namespace lce::pred