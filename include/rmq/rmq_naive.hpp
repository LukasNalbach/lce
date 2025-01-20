/*******************************************************************************
 * lce/rmq/rmq_naive.hpp
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

template <typename t_key_type>
class rmq_naive {
 public:
  using key_type = t_key_type;
  rmq_naive() {
  }

  rmq_naive(key_type const* data, size_t size) : m_data(data), m_size(size) {
  }

  template <typename C>
  rmq_naive(C const& container)
      : rmq_naive(container.data(), container.size()) {
  }

  // Return the index of the smallest element in m_data[left]..m_data[right]
  // for left = std::min(i, j) and right = std::max(i, j).
  size_t rmq(size_t const i, size_t const j) const {
    size_t const left = std::min(i, j);
    size_t const right = std::max(i, j);
    return rmq_lr(left, right);
  }

  // Return the index of the smallest element in m_data[left]..m_data[right].
  // Here left must be no more than right.
  size_t rmq_lr(size_t const left, size_t const right) const {
    assert(left <= right);
    size_t min = left;
    for (size_t i{left + 1}; i <= right; ++i) {
      min = m_data[min] <= m_data[i] ? min : i;
    }
    return min;
  };

  // Return the index of the smallest element in m_data[left+1]..m_data[right]
  // for left = std::min(i, j) and right = std::max(i, j). Useful for the
  // LCP array.
  size_t rmq_shifted(size_t const i, size_t const j) const {
    assert(i != j);
    size_t const left = std::min(i, j) + 1;
    size_t const right = std::max(i, j);
    return rmq_lr(left, right);
  }

 private:
  key_type const* m_data = nullptr;
  size_t m_size;
};
}  // namespace lce::rmq