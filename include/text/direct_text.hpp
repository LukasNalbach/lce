/**
 * part of LukasNalbach/lce
 *
 * MIT License
 *
 * Copyright (c) Lukas Nalbach
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <omp.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>

#include "util/hash.hpp"
#include "util/memory.hpp"

namespace lce::text {

template <typename t_char_type = char>
class direct_text {
  static_assert(sizeof(t_char_type) == 1);

 public:
  using char_type = t_char_type;

  class cursor {
   public:
    cursor() = default;
    explicit cursor(const uint8_t* at) : m_at(at) {}
    uint8_t next() { return *m_at++; }

   private:
    const uint8_t* m_at = nullptr;
  };

  direct_text() = default;
  direct_text(char_type* data, uint64_t size) : m_data(data), m_size(size) {}

  char_type* data() const { return m_data; }
  uint64_t size() const { return m_size; }
  uint64_t sigma() const { return 256; }
  uint64_t size_in_bytes() const { return m_size; }

  uint8_t operator[](uint64_t i) const { return uint8_t(m_data[i]); }
  char_type char_at(uint64_t i) const { return m_data[i]; }
  bool less_char(uint64_t i, uint64_t j) const { return m_data[i] < m_data[j]; }
  uint8_t to_char(uint8_t symbol) const { return symbol; }
  cursor cursor_at(uint64_t i) const { return cursor(bytes() + i); }

  uint64_t lce(uint64_t i, uint64_t j,
               uint64_t max = std::numeric_limits<uint64_t>::max()) const {
    if (i == j) [[unlikely]] return std::min(max, m_size - i);
    const uint64_t l = std::min(i, j);
    const uint64_t r = std::max(i, j);
    const uint64_t limit = std::min(max, m_size - r);
    const uint8_t* a = bytes() + l;
    const uint8_t* b = bytes() + r;
    uint64_t k = 0;

    while (k + 8 <= limit) {
      const uint64_t diff = util::load_u64(a + k) ^ util::load_u64(b + k);
      if (diff != 0) return k + std::countr_zero(diff) / 8;
      k += 8;
    }

    while (k < limit && a[k] == b[k]) ++k;
    return k;
  }

  uint64_t lce_left(uint64_t i, uint64_t j,
                    uint64_t max = std::numeric_limits<uint64_t>::max()) const {
    if (i == j) [[unlikely]] return std::min(max, i + 1);
    const uint64_t limit = std::min({max, i + 1, j + 1});
    const uint8_t* a = bytes() + i + 1;
    const uint8_t* b = bytes() + j + 1;
    uint64_t k = 0;

    while (k + 8 <= limit) {
      const uint64_t diff = util::load_u64(a - k - 8) ^ util::load_u64(b - k - 8);
      if (diff != 0) return k + std::countl_zero(diff) / 8;
      k += 8;
    }

    while (k < limit && a[-1 - int64_t(k)] == b[-1 - int64_t(k)]) ++k;
    return k;
  }

  bool equal(uint64_t i, uint64_t j, uint64_t len) const {
    return std::memcmp(m_data + i, m_data + j, len) == 0;
  }

  uint64_t hash(uint64_t i, uint64_t len) const {
    const uint8_t* p = bytes() + i;
    const uint64_t full = len >> 3;
    const uint64_t h = util::hash_words(full, [p](uint64_t k) { return util::load_u64(p + 8 * k); });
    if ((len & 7) == 0) return h;
    uint64_t tail = 0;
    std::memcpy(&tail, p + 8 * full, len & 7);
    return util::hash_mix(h, tail);
  }

  void reverse(int threads = omp_get_max_threads()) {
    const uint64_t half = m_size / 2;
#pragma omp parallel for num_threads(threads)
    for (uint64_t k = 0; k < half; ++k) std::swap(m_data[k], m_data[m_size - 1 - k]);
  }

 private:
  const uint8_t* bytes() const { return reinterpret_cast<const uint8_t*>(m_data); }

  char_type* m_data = nullptr;
  uint64_t m_size = 0;
};

template <typename t_text>
concept text_access = requires(t_text const& text) {
  text.size();
  text[0];
  text.cursor_at(0);
  text.lce(0, 0, 0);
};

template <typename t_type>
struct text_of {
  using type = t_type;
};

template <typename t_type>
  requires(std::is_integral_v<t_type> && sizeof(t_type) == 1)
struct text_of<t_type> {
  using type = direct_text<t_type>;
};

template <typename t_type>
using text_of_t = typename text_of<t_type>::type;

template <typename t_text>
inline constexpr bool is_direct_text_v = false;

template <typename t_char_type>
inline constexpr bool is_direct_text_v<direct_text<t_char_type>> = true;

}
