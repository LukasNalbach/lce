/*******************************************************************************
 * lce/ds/lce_memcmp.hpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once
#include <assert.h>

#include <cstdint>
#include <cstring>

namespace lce::ds {

class lce_memcmp {
 public:
  typedef uint8_t char_type;

  lce_memcmp() : m_text(nullptr), m_size(0) {
  }

  lce_memcmp(char_type const* text, size_t size) : m_text(text), m_size(size) {
  }

  template <typename C>
  lce_memcmp(C const& container)
      : lce_memcmp(container.data(), container.size()) {
  }

  // Return whether text[i..] is lexicographic smaller than text[j..]. Here i
  // and j must be different.
  bool is_leq_suffix(size_t i, size_t j) const {
    return is_leq_suffix(m_text, m_size, i, j);
  }

  // Return whether text[i..] is lexicographic smaller than text[j..]. Here i
  // and j must be different.
  static bool is_leq_suffix(char_type const* text, size_t size, size_t i,
                            size_t j) {
    assert(i != j);
    size_t r = std::max(i, j);
    size_t max_lce = (size - r);
    int result = std::memcmp(text + i, text + j, max_lce);
    return (result < 0) || (result == 0 && (i > j));
  }

 private:
  char_type const* m_text;
  size_t m_size;
};
}  // namespace lce::ds
