/*******************************************************************************
 * lce/pred/binsearch_std.hpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <assert.h>

#include <algorithm>
#include <cstddef>
#include <iterator>

#include "pred_result.hpp"

namespace lce::pred {
template <typename T>
class binsearch_std {
 public:
  typedef T data_type;
  binsearch_std() : m_data(nullptr), m_size(0), m_min(0), m_max(0) {
  }

  binsearch_std(T const* data, size_t size) : m_data(data), m_size(size) {
    assert(std::is_sorted(data, data + size));
    if (m_size != 0) {
      m_min = data[0];
      m_max = data[size - 1];
    }
  }

  template <typename C>
  binsearch_std(C const& container)
      : binsearch_std(container.data(), container.size()) {
  }

  result predecessor(T x) const {
    if (x < m_min) {
      return result{false, 0};
    }
    return result{true, predecessor_unsafe(x)};
  }

  size_t predecessor_unsafe(T x) const {
    assert(x >= m_min);
    return std::distance(m_data, std::upper_bound(m_data, m_data + m_size, x)) -
           1;
  }

  result successor(T x) const {
    if (x > m_max) {
      return result{false, 0};
    }
    return result{true, successor_unsafe(x)};
  }

  size_t successor_unsafe(T x) const {
    assert(x <= m_max);
    return std::distance(m_data, std::lower_bound(m_data, m_data + m_size, x));
  }

  bool contains(T x) const {
    auto const it = std::lower_bound(m_data, m_data + m_size, x);
    return (it != m_data + m_size) && (*it == x);
  }

 private:
  T const* m_data = nullptr;
  size_t m_size = 0;
  T m_min = 0;
  T m_max = 0;
};
};  // namespace lce::pred