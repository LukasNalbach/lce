/*******************************************************************************
 * lce/pred/pgm_index.hpp
 *
 * Copyright (C) 2022 Patrick Dinklage <patrick.dinklage@tu-dortmund.de>
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <algorithm>
#include <pgm_index.hpp>

#include "pred_result.hpp"

namespace lce::pred {

// a wrapper around the PGM index for successor queries
template <typename T, size_t m_epsilon>
class pgm_index {
 public:
  typedef T data_type;
  inline pgm_index() {}

  template <typename C>
  pgm_index(C const& container)
      : pgm_index(container.begin(), container.end()) {}

  inline pgm_index(T const* data, size_t size) : pgm_index(data, data + size) {}

  template <typename I>
  inline pgm_index(I begin, I end)
      : m_data(&(*begin)),
        m_num(std::distance(begin, end)),
        m_min(*begin),
        m_max(*(end - 1)),
        m_pgm(begin, end) {
    static_assert(sizeof(T) <= 8);  // warning deep down in pgm index
  }

  pgm_index(pgm_index const&) = default;
  pgm_index& operator=(pgm_index const&) = default;
  pgm_index(pgm_index&&) = default;
  pgm_index& operator=(pgm_index&&) = default;

  // finds the greatest element less than OR equal to x
  inline result predecessor(const T x) const {
    if (x < m_min) [[unlikely]]
      return result{false, 0};

    auto range = m_pgm.search(x);
    auto lo = m_data + range.lo;
    auto hi = m_data + range.hi;
    return {true, static_cast<size_t>(std::distance(m_data, std::upper_bound(lo, hi, x)) - 1)};
    // nb: the PGM index returns the interval that would contain x if it
    // were contained the predecessor and successor may thus be the items
    // just outside the interval!
  }

  // finds the smallest element greater than OR equal to x
  inline result successor(const T x) const {
    if (x > m_max) [[unlikely]]
      return result{false, 0};

    auto range = m_pgm.search(x);
    auto lo = m_data + range.lo;
    auto hi = m_data + range.hi;
    return {true, static_cast<size_t>(std::distance(m_data, std::lower_bound(lo, hi, x)))};
    // nb: the PGM index returns the interval that would contain x if it
    // were contained the predecessor and successor may thus be the items
    // just outside the interval!
  }

 private:
  const T* m_data;
  size_t m_num;
  T m_min;
  T m_max;

  pgm::PGMIndex<T, m_epsilon> m_pgm;
};
}  // namespace lce::pred