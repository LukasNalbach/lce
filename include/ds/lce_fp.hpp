/*******************************************************************************
 * lce/ds/lce_fp.hpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once
#include <assert.h>
#include <omp.h>

#include <array>
#include <bit>
#include <cstdint>

#include "rolling_hash/modular_arithmetic.hpp"

namespace lce::ds {

template <typename t_char_type = uint8_t, size_t t_naive_scan = 32>
class lce_fp {
 public:
  typedef t_char_type char_type;
  __extension__ typedef unsigned __int128 uint128_t;

  lce_fp() : m_block_fps(nullptr), m_size(0) {
  }

  lce_fp(char_type* text, size_t size)
      : m_block_fps(reinterpret_cast<uint64_t*>(text)), m_size(size) {
    assert(size % 8 == 0);
    size_t size_in_blocks{size / 8};
    assert(sizeof(t_char_type) == 1);
    std::vector<uint64_t> superblock_fps(omp_get_max_threads());
    // Partition text for threads in superblocks.

    // For small endian systems we need to swap the order of bytes in order to
    // calculate fingerprints. Luckily this step is fast.
    if constexpr (std::endian::native == std::endian::little) {
#pragma omp parallel for
      for (size_t i = 0; i < size_in_blocks; ++i) {
        m_block_fps[i] =
            __builtin_bswap64(m_block_fps[i]);  // C++23 std::byteswap!
      }
    }

#pragma omp parallel
    {
      int t = omp_get_thread_num();
      int nt = omp_get_num_threads();
      uint64_t slice_size = size_in_blocks / nt;
      size_t begin = t * slice_size;
      size_t end = (t < nt - 1) ? (t + 1) * slice_size : size_in_blocks;

      // First calculate FP of superblock.
      if (t != nt - 1) {
        uint128_t fingerprint = 0;
        for (size_t i = begin; i < end; ++i) {
          uint128_t current_block = m_block_fps[i];
          fingerprint <<= 64;
          fingerprint += current_block;
          fingerprint %= m_prime;
        }
        superblock_fps[t + 1] = fingerprint;
      }
    }

    // Prefix sum over fingerprints of superblocks.
    uint64_t slice_size = size_in_blocks / omp_get_max_threads();
    uint128_t shift_influence =
        modular::pow_mod(uint128_t{1} << 64, uint128_t{slice_size}, m_prime);
    for (size_t i = 1; i < superblock_fps.size(); ++i) {
      uint128_t last_block_influence = shift_influence * superblock_fps[i - 1];
      uint128_t cur_block_influence = superblock_fps[i];
      superblock_fps[i] =
          (last_block_influence + cur_block_influence) % m_prime;
    }

#pragma omp parallel
    {
      int t = omp_get_thread_num();
      int nt = omp_get_num_threads();
      uint64_t slice_size = size_in_blocks / nt;
      size_t begin = t * slice_size;
      size_t end = (t < nt - 1) ? (t + 1) * slice_size : size_in_blocks;

      // Overwrite text with fingerprints.
      uint128_t fingerprint = superblock_fps[t];
      for (size_t i = begin; i < end; ++i) {
        uint128_t current_block = m_block_fps[i];
        fingerprint <<= 64;
        fingerprint += current_block;
        fingerprint %= m_prime;
        assert(fingerprint < 0x8000000000000000ULL);
        m_block_fps[i] = static_cast<uint64_t>(fingerprint) +
                         (current_block >= m_prime) * 0x8000000000000000ULL;
      }
    }
  }

  template <typename C>
  lce_fp(C& container) : lce_fp(container.data(), container.size()) {
  }

  // destructor
  ~lce_fp() {
    retransform_text();
  }

  // copy constructor
  lce_fp(const lce_fp& other) = delete;

  // move constructor
  lce_fp(lce_fp&& other)
      : m_block_fps(other.m_block_fps), m_size(other.m_size) {
  }

  // copy assignment
  lce_fp& operator=(const lce_fp& other) = delete;

  // move assignment
  lce_fp& operator=(lce_fp&& other) noexcept {
    m_block_fps = other.m_block_fps;
    m_size = other.m_size;
    return *this;
  }

  void retransform_text() {
    if (m_block_fps != nullptr) {
      for (size_t i{(m_size / 8) - 1}; i > 0; --i) {
        m_block_fps[i] = get_block_not_first(i);
        m_block_fps[i] = __builtin_bswap64(m_block_fps[i]);
      }
      m_block_fps[0] &= 0x7FFFFFFFFFFFFFFFULL;
      m_block_fps[0] = __builtin_bswap64(m_block_fps[0]);
    }
  }

  char_type operator[](size_t pos) const {
    uint64_t block_number = pos / 8;
    uint64_t offset = 7 - (pos % 8);
    return (get_block(block_number)) >> (8 * offset) & 0xff;
  }

  // Return the number of common letters in text[i..] and text[j..].
  size_t lce(size_t i, size_t j) const {
    if (i == j) [[unlikely]] {
      assert(i < m_size);
      return m_size - i;
    }
    return lce_uneq(i, j);
  }

  // Return the number of common letters in text[i..] and text[j..]. Here i and
  // j must be different.
  size_t lce_uneq(size_t i, size_t j) const {
    assert(i != j);

    size_t l = std::min(i, j);
    size_t r = std::max(i, j);

    return lce_lr(l, r);
  }

  // Return the number of common letters in text[i..] and text[j..].
  // Here l must be smaller than r.
  size_t lce_lr(size_t l, size_t r) const {
    uint64_t max_lce = m_size - r;
    uint64_t lce = lce_scan(l, r, max_lce);
    if (lce < t_naive_scan) {
      return lce;
    }
    // Exponential search
    uint64_t dist = t_naive_scan * 2;
    int exp = std::countr_zero(dist);

    const uint128_t fingerprint_to_l = (l != 0) ? fp_to(l - 1) : 0;
    const uint128_t fingerprint_to_r = (r != 0) ? fp_to(r - 1) : 0;

    while (2 * dist <= max_lce && fp_exp(fingerprint_to_l, l, exp) ==
                                  fp_exp(fingerprint_to_r, r, exp)) {
      ++exp;
      dist *= 2;
    }

    // Binary search. We start it at i2 and j2, because we know that up until
    // i2 and j2 everything matched.
    --exp;
    dist /= 2;
    uint64_t add = dist;

    while (dist > t_naive_scan) {
      --exp;
      dist /= 2;
      if (fp_exp(l + add, exp) == fp_exp(r + add, exp)) {
        add += dist;
      }
    }
    max_lce -= add;
    return add + lce_scan_to_end(l + add, r + add, max_lce);
  }

  // Return {b, lce}, where lce is the number of common letters in text[i..]
  // and text[j..] and b tells whether the lce ends with a mismatch.
  std::pair<bool, size_t> lce_mismatch(size_t i, size_t j) const {
    if (i == j) [[unlikely]] {
      assert(i < m_size);
      return {false, m_size - i};
    }

    size_t l = std::min(i, j);
    size_t r = std::max(i, j);

    size_t lce = lce_lr(l, r);
    return {r + lce != m_size, lce};
  }

  // Return whether text[i..] is lexicographic smaller than text[j..]. Here i
  // and j must be different.
  bool is_leq_suffix(size_t i, size_t j) const {
    assert(i != j);
    size_t lce_val = lce_uneq(i, j);
    return i + lce_val == m_size ||
           ((j + lce_val != m_size) && operator[](i + lce_val) < operator[](
                                                                     j +
                                                                     lce_val));
  }

  // Alternative: Calculate influence, and compare fp - influence until mismatch
  uint64_t lce_scan(const uint64_t i, const uint64_t j,
                    uint64_t max_lce) const {
    uint64_t lce = 0;
    // Naive part of lce query. Compare blockwise.
    const int offset_lce1 = (i % 8) * 8;
    const int offset_lce2 = (j % 8) * 8;
    uint64_t block_i = get_block(i / 8);
    uint64_t block_i2 = get_block_not_first(i / 8 + 1);
    uint64_t block_j = get_block(j / 8);
    uint64_t block_j2 = get_block_not_first(j / 8 + 1);
    uint64_t comp_block_i =
        (block_i << offset_lce1) + ((block_i2 >> 1) >> (63 - offset_lce1));
    uint64_t comp_block_j =
        (block_j << offset_lce2) + ((block_j2 >> 1) >> (63 - offset_lce2));

    const uint64_t max_block_naive = std::min(t_naive_scan, max_lce) / 8;
    while (lce < max_block_naive) {
      if (comp_block_i != comp_block_j) {
        break;
      }
      ++lce;
      block_i = block_i2;
      block_i2 = get_block_not_first((i / 8) + lce + 1);
      block_j = block_j2;
      block_j2 = get_block_not_first((j / 8) + lce + 1);
      comp_block_i =
          (block_i << offset_lce1) + ((block_i2 >> 1) >> (63 - offset_lce1));
      comp_block_j =
          (block_j << offset_lce2) + ((block_j2 >> 1) >> (63 - offset_lce2));
    }
    lce *= 8;
    // If everything except the stub matches, we compare the stub character-wise
    // and return the result
    if (lce != t_naive_scan) {
      uint64_t max_stub = std::min((max_lce - lce), uint64_t{8});
      return lce + std::min<uint64_t>(
                       ((std::countl_zero(comp_block_i ^ comp_block_j)) / 8),
                       max_stub);
    }
    return t_naive_scan;
  }

  uint64_t lce_scan_to_end(const uint64_t i, const uint64_t j,
                           uint64_t max_lce) const {
    uint64_t lce = 0;
    // Naive part of lce query. Compare blockwise.
    const int offset_lce1 = (i % 8) * 8;
    const int offset_lce2 = (j % 8) * 8;
    uint64_t block_i = get_block(i / 8);
    uint64_t block_i2 = get_block_not_first(i / 8 + 1);
    uint64_t block_j = get_block(j / 8);
    uint64_t block_j2 = get_block_not_first(j / 8 + 1);
    uint64_t comp_block_i =
        (block_i << offset_lce1) + ((block_i2 >> 1) >> (63 - offset_lce1));
    uint64_t comp_block_j =
        (block_j << offset_lce2) + ((block_j2 >> 1) >> (63 - offset_lce2));

    while (lce < max_lce) {
      if (comp_block_i != comp_block_j) {
        break;
      }
      ++lce;
      block_i = block_i2;
      block_i2 = get_block_not_first((i / 8) + lce + 1);
      block_j = block_j2;
      block_j2 = get_block_not_first((j / 8) + lce + 1);
      comp_block_i =
          (block_i << offset_lce1) + ((block_i2 >> 1) >> (63 - offset_lce1));
      comp_block_j =
          (block_j << offset_lce2) + ((block_j2 >> 1) >> (63 - offset_lce2));
    }
    lce *= 8;
    // If everything except the stub matches, we compare the stub character-wise
    // and return the result.

    uint64_t max_stub = std::min((max_lce - lce), uint64_t{8});
    return lce +
           std::min<uint64_t>(
               ((std::countl_zero(comp_block_i ^ comp_block_j)) / 8), max_stub);
  }

  // Return the lce of text[i..i+lce) and text[j..j+lce]
  size_t lce_up_to(size_t i, size_t j, size_t up_to) const {
    if (i == j) [[unlikely]] {
      assert(i < m_size);
      m_size - i;
    }

    size_t l = std::min(i, j);
    size_t r = std::max(i, j);

    uint64_t max_lce = std::min(m_size - r, up_to);
    uint64_t lce = lce_scan(l, r, max_lce);
    if (lce < t_naive_scan) {
      return lce;
    }
    // Exponential search
    uint64_t dist = t_naive_scan * 2;
    int exp = std::countr_zero(dist);

    const uint128_t fingerprint_to_l = (l != 0) ? fp_to(l - 1) : 0;
    const uint128_t fingerprint_to_r = (r != 0) ? fp_to(r - 1) : 0;

    while (dist <= max_lce && fp_exp(fingerprint_to_l, l, exp) ==
                                  fp_exp(fingerprint_to_r, r, exp)) {
      ++exp;
      dist *= 2;
    }

    // Binary search. We start it at i2 and j2, because we know that up until
    // i2 and j2 everything matched.
    --exp;
    dist /= 2;
    uint64_t add = dist;

    while (dist > t_naive_scan) {
      --exp;
      dist /= 2;
      if (fp_exp(l + add, exp) == fp_exp(r + add, exp)) {
        add += dist;
      }
    }
    max_lce -= add;
    lce = add + lce_scan_to_end(l + add, r + add, max_lce);

    return lce;
  }

 private:
  uint64_t* m_block_fps = nullptr;
  size_t m_size = 0;
  static constexpr uint128_t m_prime{0x800000000000001d};

  // Calculates the powers of 2. This supports LCE queries and reduces the time
  // from polylogarithmic to logarithmic.
  static constexpr std::array<uint64_t, 70> calculate_power_table() {
    std::array<uint64_t, 70> powers;
    uint128_t x = 256;
    powers[0] = static_cast<uint64_t>(x);
    for (size_t i = 1; i < powers.size(); ++i) {
      x = (x * x) % m_prime;
      powers[i] = static_cast<uint64_t>(x);
    }
    return powers;
  }
  static constexpr std::array<uint64_t, 70> m_power_table =
      calculate_power_table();

  // Return the i'th block. A block contains 8 character.
  uint64_t get_block(const uint64_t i) const {
    uint128_t x = (i != 0) ? m_block_fps[i - 1] & 0x7FFFFFFFFFFFFFFFULL : 0;
    x <<= 64;
    x %= m_prime;

    uint64_t current_fingerprint = m_block_fps[i];
    uint64_t s_bit = current_fingerprint >> 63;
    current_fingerprint &= 0x7FFFFFFFFFFFFFFFULL;

    uint64_t y = static_cast<uint64_t>(x);

    y = y <= current_fingerprint ? current_fingerprint - y
                                 : m_prime - (y - current_fingerprint);
    return y + s_bit * static_cast<uint64_t>(m_prime);
  }

  // Return the i'th block for i > 0.
  uint64_t get_block_not_first(const uint64_t i) const {
    assert(i >= 1);
    if (i >= m_size / 8) {
      return 0;
    }
    uint128_t x = m_block_fps[i - 1] & 0x7FFFFFFFFFFFFFFFULL;
    x <<= 64;
    x %= m_prime;

    uint64_t current_fingerprint = m_block_fps[i];
    uint64_t s_bit = current_fingerprint >> 63;
    current_fingerprint &= 0x7FFFFFFFFFFFFFFFULL;

    uint64_t y = static_cast<uint64_t>(x);

    y = y <= current_fingerprint ? current_fingerprint - y
                                 : m_prime - (y - current_fingerprint);
    return y + s_bit * static_cast<uint64_t>(m_prime);
  }

  uint128_t fp_to(size_t i) const {
    uint128_t fingerprint = 0;
    int pad = ((i + 1) & 7) * 8;
    if (pad == 0) {
      // This fingerprints is already saved.
      // We only have to remove the helping bit.
      return m_block_fps[i / 8] & 0x7FFFFFFFFFFFFFFFULL;
    }
    /* Add fingerprint from previous block */
    if (i > 7) [[likely]] {
      fingerprint = m_block_fps[(i / 8) - 1] & 0x7FFFFFFFFFFFFFFFULL;
      fingerprint <<= pad;
      uint64_t y = get_block_not_first(i / 8);
      fingerprint += (y >> (64 - pad));

    } else {
      const uint64_t current_fingerprint = m_block_fps[0];
      const uint64_t s_bit = current_fingerprint >> 63;

      fingerprint = current_fingerprint & 0x7FFFFFFFFFFFFFFFULL;
      fingerprint += (s_bit * m_prime);
      fingerprint >>= (64 - pad);
    }

    fingerprint %= m_prime;
    return static_cast<uint64_t>(fingerprint);
  };

  // Calculate the fingerprint of T[from, from + 2^exp).
  uint64_t fp_exp(const uint64_t from, const int exp) const {
    uint128_t fingerprint_to_i = (from != 0) ? fp_to(from - 1) : 0;
    uint128_t fingerprint_to_j = fp_to(from + (size_t{1} << exp) - 1);
    fingerprint_to_i *= m_power_table[exp];
    fingerprint_to_i %= m_prime;

    return fingerprint_to_j >= fingerprint_to_i
               ? static_cast<uint64_t>(fingerprint_to_j - fingerprint_to_i)
               : static_cast<uint64_t>(m_prime -
                                       (fingerprint_to_i - fingerprint_to_j));
  }

  // Calculates the fingerprint of T[from, from + 2^exp) when the fingerprint of
  // T[1, from) is already knwon */
  uint64_t fp_exp(uint128_t fingerprint_to_i, const uint64_t from,
                  const int exp) const {
    uint128_t fingerprint_to_j = fp_to(from + (size_t{1} << exp) - 1);
    fingerprint_to_i *= m_power_table[exp];
    fingerprint_to_i %= m_prime;

    return fingerprint_to_j >= fingerprint_to_i
               ? static_cast<uint64_t>(fingerprint_to_j - fingerprint_to_i)
               : static_cast<uint64_t>(m_prime -
                                       (fingerprint_to_i - fingerprint_to_j));
  }
};
}  // namespace lce::ds
