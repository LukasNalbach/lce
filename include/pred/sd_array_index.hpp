/*******************************************************************************
 * lce/pred/sd_array_index.hpp
 *
 * Copyright (C) 2024 Lukas Nalbach <lukas.nalbach@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <omp.h>

#include <algorithm>
#include <sdsl/bit_vectors.hpp>

#include "pred_result.hpp"

namespace lce::pred {

template <typename T, uint64_t sample_rate>
class sd_array_index {
 public:
  inline sd_array_index() = default;

  template <typename C>
  sd_array_index(C const& container)
      : sd_array_index(container.data(), container.size()) {
  }

  inline static uint64_t div_ceil(uint64_t x, uint64_t y) {
      return x == 0 ? 0 : (1 + (x - 1) / y);
  }

  inline sd_array_index(T const* data, size_t size)
      : m_data(data), m_size(size), m_min(data[0]), m_max(data[size - 1]) {
    assert(std::is_sorted(m_data, m_data + size));
    uint64_t num_samples = div_ceil(m_size, sample_rate);
    sdsl::sd_vector_builder sdvb(m_max + 1, num_samples);
    for (uint64_t i = 0; i < m_size; i += sample_rate) {
      sdvb.set(m_data[i]);
    }
    m_sd_vec = sdsl::sd_vector<>(sdvb);
    m_rank_1_supp.set_vector(&m_sd_vec);
  }

 private:

  const T* m_data;
  size_t m_size;
  T m_min;
  T m_max;

  sdsl::sd_vector<> m_sd_vec;
  sdsl::sd_vector<>::rank_1_type m_rank_1_supp;

 public:
  // finds the greatest element less than OR equal to x
  inline result predecessor(const T x) const {
    if ((x < m_min)) [[unlikely]]
      return result{false, 0};
    if ((x >= m_max)) return result{true, m_size - 1};
    uint64_t rnk = sample_rate * (m_rank_1_supp.rank(x + 1) - 1);
    while (m_data[rnk + 1] <= x) rnk++;
    return {true, rnk};
  }

  // finds the smallest element greater than OR equal to x
  inline result successor(const T x) const {
    if ((x <= m_min)) [[unlikely]]
      return result{true, 0};
    if ((x > m_max)) [[unlikely]]
      return result{false, 0};
    uint64_t rnk = sample_rate * (m_rank_1_supp.rank(x + 1) - 1);
    while (m_data[rnk] < x) rnk++;
    return {true, rnk};
  }
};
}  // namespace lce::pred