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

#include <atomic>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>

#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif

#if defined(__GLIBC__)
#include <malloc.h>
#endif

namespace lce::util {

struct uint40_t {
  static_assert(std::endian::native == std::endian::little);

  static constexpr uint8_t width = 5;
  static constexpr uint64_t max = (uint64_t{1} << (8 * width)) - 1;

  uint8_t bytes[width];

  uint40_t() = default;

  uint40_t(uint64_t value) {
    assert(value <= max);
    uint32_t low = uint32_t(value);
    std::memcpy(bytes, &low, 4);
    bytes[4] = uint8_t(value >> 32);
  }

  operator uint64_t() const {
    uint32_t low;
    std::memcpy(&low, bytes, 4);
    return uint64_t(low) | (uint64_t(bytes[4]) << 32);
  }
};

static_assert(sizeof(uint40_t) == uint40_t::width);
static_assert(alignof(uint40_t) == 1);

inline uint64_t load_u64(const void* data) {
  uint64_t value;
  std::memcpy(&value, data, sizeof(uint64_t));
  return value;
}

template <typename value_t>
inline constexpr uint64_t largest_value() {
  if constexpr (requires { value_t::max; }) return uint64_t(value_t::max);
  else return uint64_t(std::numeric_limits<value_t>::max());
}

template <typename T>
class no_init {
  static_assert(std::is_trivial_v<T>);
  T m_value;

 public:
  no_init() noexcept {}
  constexpr no_init(T value) noexcept : m_value{value} {}
  constexpr operator T() const noexcept { return m_value; }
};

template <typename T>
inline void no_init_resize(std::vector<T>& vec, size_t size) {
  reinterpret_cast<std::vector<no_init<T>>&>(vec).resize(size);
  std::atomic_signal_fence(std::memory_order_seq_cst);
}

inline void parallel_memset(void* data, int value, uint64_t bytes, int threads = omp_get_max_threads()) {
#pragma omp parallel num_threads(threads)
  {
    const uint64_t t = omp_get_thread_num();
    const uint64_t nt = omp_get_num_threads();
    const uint64_t beg = bytes * t / nt;
    const uint64_t end = bytes * (t + 1) / nt;
    std::memset(static_cast<uint8_t*>(data) + beg, value, end - beg);
  }
}

inline void release_free_memory() {
#if defined(__GLIBC__)
  malloc_trim(0);
#endif
}

static constexpr uint64_t huge_page_min_bytes = 4 * 1024 * 1024;

inline void advise_huge_pages([[maybe_unused]] void* data, [[maybe_unused]] uint64_t bytes) {
#if defined(MADV_HUGEPAGE)
  static const int mode = [] {
    const char* value = std::getenv("LCE_HUGE_PAGES");
    return value == nullptr ? 1 : std::atoi(value);
  }();

  if (mode == 0 || bytes < huge_page_min_bytes) return;

  uint64_t page = uint64_t(sysconf(_SC_PAGESIZE));
  uintptr_t beg = uintptr_t(data) & ~(page - 1);
  uintptr_t end = (uintptr_t(data) + bytes + page - 1) & ~(page - 1);
  madvise(reinterpret_cast<void*>(beg), end - beg, mode > 0 ? MADV_HUGEPAGE : MADV_NOHUGEPAGE);
#endif
}

}
