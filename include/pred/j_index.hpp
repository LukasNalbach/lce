/*******************************************************************************
 * lce/pred/j_index.hpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <algorithm>

#include "pred_result.hpp"

namespace lce::pred {

// predecessor data structure that uses a linear function to approximate entries
template <typename T>
class j_index {
 public:
  typedef T data_type;
  j_index() : m_data(nullptr), m_size(0), m_min(0), m_max(0) {
  }

  template <typename C>
  j_index(C const& container) : j_index(container.data(), container.size()) {
  }

  j_index(T const* data, size_t size)
      : m_data(data), m_size(size), m_min(data[0]), m_max(data[m_size - 1]) {
    assert(std::is_sorted(data, data + size));
    slope = static_cast<double>(m_max) / static_cast<double>(m_size);

#pragma omp parallel for reduction(min : max_l_error) reduction(max : max_r_error)
    for (size_t i = 0; i < m_size; i++) {
      int64_t apprx_pos = static_cast<int64_t>(1.0 * m_data[i] / slope);
      int64_t error = static_cast<int64_t>(i) - apprx_pos;
      max_l_error = std::min(error, max_l_error);
      max_r_error = std::max(error, max_r_error);
    }
    --max_l_error;
    ++max_r_error;
  }

  // finds the greatest element less than OR equal to x
  inline result predecessor_lin(const T x) const {
    if (x < m_min) [[unlikely]]
      return result{false, 0};
    if (x >= m_max) [[unlikely]]
      return result{true, m_size - 1};

    // linear_scan (left, then right)
    size_t aprx_pos = (1.0 * x) / slope;
    size_t scan_pos = aprx_pos;

    // scan left
    while (m_data[scan_pos] > x) {
      --scan_pos;
    }
    // scan right
    while (m_data[scan_pos + 1] <= x) {
      ++scan_pos;
    }
    return {true, scan_pos};
  }

  // finds the greatest element less than OR equal to x
  inline result predecessor(const T x) const {
    if (x < m_min) [[unlikely]]
      return result{false, 0};
    if (x >= m_max) [[unlikely]]
      return result{true, m_size - 1};

    int64_t aprx_pos = (1.0 * x) / slope;
    int64_t left_border = std::max(aprx_pos + max_l_error, int64_t{0});
    int64_t right_border =
        std::min(aprx_pos + max_r_error + 1, static_cast<int64_t>(m_size));
    size_t scan_pos =
        std::distance(m_data, std::upper_bound(m_data + left_border,
                                               m_data + right_border, x)) -
        1;

    return {true, scan_pos};
  }

  // finds the smallest element greater than OR equal to x
  inline result successor_lin(const T x) const {
    if (x <= m_min) [[unlikely]]
      return result{true, 0};
    if (x > m_max) [[unlikely]]
      return result{false, m_size - 1};

    size_t aprx_pos =
        std::min((1.0 * x) / slope, static_cast<double>(m_size - 1));
    size_t scan_pos = aprx_pos;
    // scan left
    while (m_data[scan_pos] >= x) {
      --scan_pos;
    }
    // scan right
    while (m_data[scan_pos] < x) {
      ++scan_pos;
    }
    return {true, scan_pos};
  }

  // finds the greatest element less than OR equal to x
  inline result successor(const T x) const {
    if (x <= m_min) [[unlikely]]
      return result{true, 0};
    if (x > m_max) [[unlikely]]
      return result{false, m_size - 1};

    int64_t aprx_pos = (1.0 * x) / slope;
    int64_t left_border = std::max(aprx_pos + max_l_error, int64_t{0});
    int64_t right_border =
        std::min(aprx_pos + max_r_error + 1, static_cast<int64_t>(m_size));
    size_t scan_pos = std::distance(
        m_data,
        std::lower_bound(m_data + left_border, m_data + right_border, x));

    return {true, scan_pos};
  }

 private:
  const T* m_data;
  size_t m_size;
  T m_min;
  T m_max;

  int64_t max_l_error = 0;
  int64_t max_r_error = 0;
  double slope;
  mutable std::vector<int64_t> err;
};

}  // namespace lce::pred