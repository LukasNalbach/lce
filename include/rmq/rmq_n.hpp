/*******************************************************************************
 * lce/rmq/rmq_n.hpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <assert.h>
#include <omp.h>

#include <cstdint>
#include <vector>

#include "rmq_nlgn.hpp"

namespace lce::rmq {

template <typename t_key_type, typename index_type = uint32_t,
          uint64_t t_block_size = 64, typename t_array = const t_key_type*>
class rmq_n {
 public:
  using key_type = t_key_type;
  rmq_n() {
  }

  rmq_n(t_array data, size_t size) : m_data(data), m_size(size) {
    const uint64_t num_sampled_elements = (m_size - 1) / t_block_size + 1;
    m_sampled_indexes.resize(num_sampled_elements);
    m_sampled_minimas.resize(num_sampled_elements);

// Get the minimal elements from the blocks.
#pragma omp parallel for
    for (size_t block = 0; block < num_sampled_elements; ++block) {
      index_type min_index = block * t_block_size;
      index_type end = std::min(((1 + block) * t_block_size), m_size);
      for (size_t i = min_index; i < end; ++i) {
        min_index = data[min_index] <= data[i] ? min_index : i;
      }
      m_sampled_indexes[block] = min_index;
      m_sampled_minimas[block] = at(min_index);
    }

    // Build an RMQ data structure for these block minimas.
    m_sampled_rmq = rmq_nlgn<key_type>(m_sampled_minimas);
  }

  template <typename C>
  rmq_n(C const& container) : rmq_n(array_of(container), container.size()) {
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
    if (right - left <= 3 * t_block_size) {
      return scan_min(left, right + 1);
    }
    // Min in left block
    size_t const check_left_until = (1 + left / t_block_size) * t_block_size;
    assert(check_left_until < m_size);  // Because we scanned 3*t_block_size
    size_t const min_beg = scan_min(left, check_left_until);

    // Min in right block
    size_t const check_right_from = (right / t_block_size) * t_block_size;
    size_t const min_end = scan_min(check_right_from, right + 1);

    // Now look for min in middle part.
    size_t const l_block = (left / t_block_size) + 1;
    size_t const r_block = (right / t_block_size) - 1;
    assert(l_block < r_block);  // Because we scanned 3*t_block_size before

    size_t const min_mid =
        m_sampled_indexes[m_sampled_rmq.rmq_lr(l_block, r_block)];
    
    size_t min = min_beg;
    min = at(min) <= at(min_mid) ? min : min_mid;
    min = at(min) <= at(min_end) ? min : min_end;
    return min; 
  }

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
  template <typename C>
  static t_array array_of(const C& container)
  {
    if constexpr (requires { container.view(); }) return container.view();
    else return container.data();
  }

  inline uint64_t at(uint64_t i) const { return uint64_t(m_data[i]); }

  inline size_t scan_min(size_t beg, size_t end) const {
    size_t min = beg;
    uint64_t min_val = at(beg);
    for (size_t i{beg + 1}; i < end; ++i) {
      const uint64_t val = at(i);
      if (val < min_val) {
        min = i;
        min_val = val;
      }
    }
    return min;
  }

  t_array m_data {};
  size_t m_size;

  std::vector<index_type> m_sampled_indexes;
  std::vector<key_type> m_sampled_minimas;
  rmq_nlgn<key_type, index_type> m_sampled_rmq;
};
}  // namespace lce::rmq