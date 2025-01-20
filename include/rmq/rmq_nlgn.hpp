/*******************************************************************************
 * lce/rmq/rmq_nlgn.hpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <assert.h>
#include <omp.h>

#include <vector>

namespace lce::rmq {

template <typename t_key_type, typename index_type = uint32_t>
class rmq_nlgn {
 public:
  using key_type = t_key_type;
  rmq_nlgn() {
  }

  rmq_nlgn(key_type const* data, size_t size) : m_data(data) {
    assert(size != 0);
    const uint8_t m_num_levels = std::bit_width(size) - 1;
    m_power_rmq.resize(m_num_levels);

    // Build first level
    if (!m_power_rmq.empty()) {
      m_power_rmq[0].resize(size - 1);
    }

#pragma omp parallel for
    for (size_t i = 0; i < size - 1; ++i) {
      m_power_rmq[0][i] = m_data[i] <= data[i + 1] ? i : (i + 1);
    }

    // Build the rest
    for (size_t l = 1; l < m_num_levels; ++l) {
      m_power_rmq[l].resize(size - ((uint64_t{2} << l) - 1));
      uint32_t const span = (uint64_t{1} << l);
#pragma omp parallel for
      for (size_t i = 0; i < m_power_rmq[l].size(); ++i) {
        const uint32_t l_interval_min = m_power_rmq[l - 1][i];
        const uint32_t r_interval_min = m_power_rmq[l - 1][i + span];
        m_power_rmq[l][i] = m_data[l_interval_min] <= m_data[r_interval_min]
                                ? l_interval_min
                                : r_interval_min;
      }
    }
  }

  template <typename C>
  rmq_nlgn(C const& container) : rmq_nlgn(container.data(), container.size()) {
  }

  // Return the index of the smallest element in m_data[left]..m_data[right] for
  // left = std::min(i, j) and right = std::max(i, j).
  size_t rmq(size_t const i, size_t const j) const {
    if (i == j) [[unlikely]] {
      return i;
    }
    return rmq_uneq(i, j);
  }

  // Return the index of the smallest element in m_data[left]..m_data[right] for
  // left = std::min(i, j) and right = std::max(i, j). Here i and j differ.
  size_t rmq_uneq(size_t const i, size_t const j) const {
    assert(i != j);
    size_t const left = std::min(i, j);
    size_t const right = std::max(i, j);
    return rmq_lr(left, right);
  }

  // Return the index of the smallest element in m_data[left]..m_data[right].
  // Here left must be smaller than right.
  size_t rmq_lr(size_t const left, size_t const right) const {
    assert(left < right);
    const uint32_t interval_size = right - left + 1;

    const uint32_t interval_log = std::bit_width(interval_size) - 1;
    const uint32_t max_power_span = (1ULL << interval_log);
    const uint32_t l_interval_min = m_power_rmq[interval_log - 1][left];
    const uint32_t r_interval_min =
        m_power_rmq[interval_log - 1][right + 1 - max_power_span];

    return m_data[l_interval_min] <= m_data[r_interval_min] ? l_interval_min
                                                            : r_interval_min;
  }

  // Return the index of the smallest element in m_data[left+1]..m_data[right]
  // for left = std::min(i, j) and right = std::max(i, j). Useful for the LCP
  // array.
  size_t rmq_shifted(size_t const i, size_t const j) const {
    assert(i != j);
    size_t const left = std::min(i, j) + 1;
    size_t const right = std::max(i, j);

    // We can not use rmq_lr because interval_size could be one.
    const uint32_t interval_size = right - left + 1;
    if (interval_size <= 2) {
      return m_data[left] <= m_data[right] ? left : right;
    }

    const uint32_t interval_log = std::bit_width(interval_size) - 1;
    const uint32_t max_power_span = (1ULL << interval_log);
    const uint32_t l_interval_min = m_power_rmq[interval_log - 1][left];
    const uint32_t r_interval_min =
        m_power_rmq[interval_log - 1][right + 1 - max_power_span];

    return m_data[l_interval_min] <= m_data[r_interval_min] ? l_interval_min
                                                            : r_interval_min;
  }

 private:
  key_type const* m_data = nullptr;
  std::vector<std::vector<index_type>> m_power_rmq;
};
}  // namespace lce::rmq