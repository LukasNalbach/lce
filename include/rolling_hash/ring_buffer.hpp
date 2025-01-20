/*******************************************************************************
 * lce/rolling_hash/ring_buffer.hpp
 *
 * Copyright (C) 2019 Florian Kurpicz <florian.kurpicz@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once
#include <bit>

namespace lce::rolling_hash {

template <typename T>
class ring_buffer {
 public:
  ring_buffer(uint64_t const buffer_size)
      : m_buffer_size(std::bit_ceil(buffer_size)),
        m_mod_mask(m_buffer_size - 1),
        m_size(0),
        m_data(m_buffer_size) {
  }

  void push_back(T const data) {
    uint64_t const insert_pos = m_size++ & m_mod_mask;
    m_data[insert_pos] = data;
  }

  size_t size() const {
    return m_size;
  }

  void resize(size_t s) {
    m_size = s;
  }

  T operator[](size_t const index) {
    return m_data[index & m_mod_mask];
  }

  T operator[](size_t const index) const {
    return m_data[index & m_mod_mask];
  }

 private:
  uint64_t const m_buffer_size;
  uint64_t const m_mod_mask;

  uint64_t m_size;

  std::vector<T> m_data;

};
}  // namespace lce::rolling_hash