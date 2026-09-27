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

#include <cstdint>

namespace lce::util {

inline uint64_t hash_mix(uint64_t h, uint64_t w) {
  h = (h ^ w) * 0x9E3779B97F4A7C15ull;
  return h ^ (h >> 32);
}

template <typename word_at_t>
inline uint64_t hash_words(uint64_t num_words, word_at_t word_at) {
  uint64_t a = 0x243F6A8885A308D3ull;
  uint64_t b = 0x13198A2E03707344ull;
  uint64_t c = 0xA4093822299F31D0ull;
  uint64_t d = 0x082EFA98EC4E6C89ull;
  uint64_t k = 0;

  for (; k + 4 <= num_words; k += 4) {
    a = hash_mix(a, word_at(k));
    b = hash_mix(b, word_at(k + 1));
    c = hash_mix(c, word_at(k + 2));
    d = hash_mix(d, word_at(k + 3));
  }

  for (; k < num_words; ++k) a = hash_mix(a, word_at(k));
  return hash_mix(hash_mix(hash_mix(a, b), c), d ^ num_words);
}

}
