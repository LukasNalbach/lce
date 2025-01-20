/*******************************************************************************
 * lce/ds/lce_naive_wordwise.hpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once
#include <assert.h>

#include <cstdint>

namespace lce::ds {

template <typename t_char_type = uint8_t>
class lce_naive_wordwise {
 public:
  typedef t_char_type char_type;
  __extension__ typedef unsigned __int128 uint128_t;

  lce_naive_wordwise() : m_text(nullptr), m_size(0) {
  }

  lce_naive_wordwise(char_type const* text, size_t size)
      : m_text(text), m_size(size) {
  }

  template <typename C>
  lce_naive_wordwise(C const& container)
      : lce_naive_wordwise(container.data(), container.size()) {
  }

  // Return the number of common letters in text[i..] and text[j..].
  size_t lce(size_t i, size_t j) const {
    return lce(m_text, m_size, i, j);
  }

  // Return the number of common letters in text[i..] and text[j..]. Here i and
  // j must be different.
  size_t lce_uneq(size_t i, size_t j) const {
    return lce_uneq(m_text, m_size, i, j);
  }

  // Return the number of common letters in text[i..] and text[j..].
  // Here l must be smaller than r.
  size_t lce_lr(size_t l, size_t r) const {
    return lce_lr(m_text, m_size, l, r);
  }

  // Return {b, lce}, where lce is the number of common letters in text[i..]
  // and text[j..] and b tells whether the lce ends with a mismatch.
  std::pair<bool, size_t> lce_mismatch(size_t i, size_t j) const {
    return lce_mismatch(m_text, m_size, i, j);
  }

  // Return whether text[i..] is lexicographic smaller than text[j..]. Here i
  // and j must be different.
  bool is_leq_suffix(size_t i, size_t j) const {
    return is_leq_suffix(m_text, m_size, i, j);
  }

  // Return the lce of text[i..i+lce) and text[j..j+lce]
  size_t lce_up_to(size_t i, size_t j, size_t up_to) const {
    return lce_up_to(m_text, m_size, i, j, up_to);
  }

  // Return the number of common letters in text[i..] and text[j..].
  static size_t lce(char_type const* text, size_t size, size_t i, size_t j) {
    if (i == j) [[unlikely]] {
      assert(i < size);
      return size - i;
    }
    return lce_uneq(text, size, i, j);
  }

  // Return the number of common letters in text[i..] and text[j..].
  static size_t lce_uneq(char_type const* text, size_t size, size_t i,
                         size_t j) {
    assert(i != j);

    size_t l = std::min(i, j);
    size_t r = std::max(i, j);

    return lce_lr(text, size, l, r);
  }

  // Return the number of common letters in text[i..] and text[j..].
  // Here l must be smaller than r.
  static size_t lce_lr(char_type const* text, size_t size, size_t l, size_t r) {
    assert(l < r);

    const uint64_t max_lce = size - r;
    uint64_t lce = 0;

    // Accelerate search by comparing 16-byte blocks
    uint128_t const* const text_blocks_i =
        reinterpret_cast<uint128_t const*>(text + l);
    uint128_t const* const text_blocks_j =
        reinterpret_cast<uint128_t const*>(text + r);
    size_t lce_val = std::distance(
        text_blocks_j,
        std::mismatch(
            text_blocks_j,
            text_blocks_j + max_lce / (sizeof(uint128_t) / sizeof(char_type)),
            text_blocks_i)
            .first);
    lce_val *= sizeof(uint128_t) / sizeof(char_type);
    // The last block did not match. Here we compare its single characters
    while (lce_val < max_lce && text[l + lce_val] == text[r + lce_val]) {
      ++lce_val;
    }
    return lce_val;
  }

  // Return {b, lce}, where lce is the number of common letters in text[i..]
  // and text[j..] and b tells whether the lce ends with a mismatch.
  static std::pair<bool, size_t> lce_mismatch(char_type const* text,
                                              size_t size, size_t i, size_t j) {
    if (i == j) [[unlikely]] {
      assert(i < size);
      return {false, size - i};
    }

    size_t l = std::min(i, j);
    size_t r = std::max(i, j);

    size_t lce = lce_lr(text, size, l, r);
    return {r + lce != size, lce};
  }

  // Return whether text[i..] is lexicographic smaller than text[j..]. Here i
  // and j must be different.
  static bool is_leq_suffix(char_type const* text, size_t size, size_t i,
                            size_t j) {
    assert(i != j);
    size_t lce_val = lce_uneq(text, size, i, j);
    return (i + lce_val == size ||
            ((j + lce_val != size) && text[i + lce_val] < text[j + lce_val]));
  }

  // Return the lce of text[i..i+lce) and text[j..j+lce]
  static size_t lce_up_to(char_type const* text, size_t size, size_t i,
                          size_t j, size_t up_to) {
    if (i == j) [[unlikely]] {
      assert(i < size);
      return size - i;
    }

    size_t l = std::min(i, j);
    size_t r = std::max(i, j);

    size_t lce_max = std::min(r + up_to, size) - r;
    size_t lce = lce_lr(text, r + lce_max, l, r);
    return lce;
  }

 private:
  char_type const* m_text;
  size_t m_size;
};
}  // namespace lce::ds
