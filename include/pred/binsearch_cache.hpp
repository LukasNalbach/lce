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

template<typename T, size_t m_cache_num = 512ULL / sizeof(T)>
class binsearch_cache {
public:
  inline binsearch_cache() : m_data(nullptr), m_size(0), m_min(), m_max() {
  }

  inline binsearch_cache(T const* data, size_t size) : m_data(data), m_size(size) {
    assert(std::is_sorted(data, data + size));
    if (m_size != 0) {
      m_min = data[0];
      m_max = data[size - 1];
    }
  }

  template <typename C>
  binsearch_cache(C const& container)
      : binsearch_cache(container.data(), container.size()) {
  }

  // finds the smallest element greater than OR equal to x
  // seeded using a start interval
  inline result predecessor_seeded(const T x, size_t p, size_t q) const {
    assert(x >= m_min && x < m_max);
    while(q - p > m_cache_num) {
      assert(x >= m_data[p]);

      const size_t m = (p + q) >> 1ULL;
      const bool le = (m_data[m] <= x);

      /*
        the following is a fast form of:
        if(le) p = m; else q = m;
      */
      const size_t le_mask = -size_t(le);
      const size_t gt_mask = ~le_mask;

      p = (le_mask & m) | (gt_mask & p);
      q = (gt_mask & m) | (le_mask & q);
    }

    // linear search
    while(m_data[p] <= x) ++p;
    assert(m_data[p-1] <= x);

    return result { true, p-1 };
  }

  // finds the greatest element less than OR equal to x
  inline result predecessor(const T x) const {
    if(x < m_min)  [[unlikely]] return result { false, 0 };
    if(x >= m_max) [[unlikely]] return result { true, m_size-1 };
    return predecessor_seeded(x, 0, m_size - 1);
  }

  // finds the smallest element greater than OR equal to x
  // seeded using a start interval
  inline result successor_seeded(const T x, size_t p, size_t q) const {
    assert(x >= m_min && x < m_max);
    while(q - p > m_cache_num) {
      assert(x > m_data[p]);
      assert(x <= m_data[q]);

      const size_t m = (p + q) >> 1ULL;
      const bool lt = (m_data[m] < x);

      /*
        the following is a fast form of:
        if(lt) p = m; else q = m;
      */
      const size_t lt_mask = -size_t(lt);
      const size_t ge_mask = ~lt_mask;

      p = (lt_mask & m) | (ge_mask & p);
      q = (ge_mask & m) | (lt_mask & q);
    }

    // linear search
    while(m_data[p] < x) ++p;
    assert(m_data[p] >= x);

    return result { true, p };
  }

  // finds the smallest element greater than OR equal to x
  inline result successor(const T x) const {
    if(x <= m_min) [[unlikely]] return result { true, 0 };
    if(x > m_max)  [[unlikely]] return result { false, 0 };
    return successor_seeded(x, 0, m_size - 1);
  }

protected:
  T const* m_data = nullptr;
  size_t m_size = 0;
  T m_min = 0;
  T m_max = 0;
};
};  // namespace lce::pred