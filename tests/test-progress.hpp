/*******************************************************************************
 * tests/test-progress.hpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <omp.h>

inline uint64_t fuzz_iterations(uint64_t default_iterations = 1000) {
  if (const char* env = std::getenv("LCE_TEST_ITERATIONS")) {
    uint64_t value = std::strtoull(env, nullptr, 10);
    if (value > 0) return value;
  }
  return default_iterations;
}

inline std::atomic<uint64_t>& fuzz_construct_ns() {
  static std::atomic<uint64_t> value{0};
  return value;
}

inline std::atomic<uint64_t>& fuzz_query_ns() {
  static std::atomic<uint64_t> value{0};
  return value;
}

struct fuzz_timer {
  std::atomic<uint64_t>& acc;
  std::chrono::steady_clock::time_point t0;
  bool stopped = false;
  explicit fuzz_timer(std::atomic<uint64_t>& a)
      : acc(a), t0(std::chrono::steady_clock::now()) {}
  void stop() {
    if (stopped) return;
    stopped = true;
    acc.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(
                      std::chrono::steady_clock::now() - t0).count(),
                  std::memory_order_relaxed);
  }
  ~fuzz_timer() { stop(); }
};

struct fuzz_functionality {
  std::string name;
  std::function<void(uint64_t)> run;
  bool parallel = false;
};

inline void run_fuzz(
    const std::string& structure, const std::vector<fuzz_functionality>& functionalities,
    uint64_t iterations = fuzz_iterations()) {
  using clock = std::chrono::steady_clock;
#ifdef _WIN32
  const bool tty = _isatty(_fileno(stderr)) != 0;
#else
  const bool tty = isatty(fileno(stderr)) != 0;
#endif
  const char* green = tty ? "\033[0;32m" : "";
  const char* reset = tty ? "\033[m" : "";

  const uint64_t total = std::max<uint64_t>(1, iterations);

  auto seconds_since = [](clock::time_point t) {
    return std::chrono::duration<double>(clock::now() - t).count();
  };

  auto draw = [&](const std::string& label, uint64_t done, double elapsed) {
    constexpr int width = 10;
    double fraction = (double)done / total;
    int filled = (int)(fraction * width + 0.5);
    if (filled > width) filled = width;
    std::string bar(filled, '=');
    bar.resize(width, ' ');
    std::fprintf(stderr, "%s%s[%s]%s %-30s %8llu/%-8llu %5.1fs   ",
        tty ? "\r" : "\n", green, bar.c_str(), reset, (structure + "/" + label).c_str(),
        (unsigned long long)done, (unsigned long long)total, elapsed);
    std::fflush(stderr);
  };

  for (const fuzz_functionality& functionality : functionalities) {
    const auto slice_start = clock::now();
    int last_pct = -1;

    if (functionality.parallel) {
      std::atomic<uint64_t> done{0};
#pragma omp parallel for schedule(dynamic)
      for (int64_t it = 0; it < (int64_t)total; ++it) {
        functionality.run((uint64_t)it);
        uint64_t d = done.fetch_add(1) + 1;
        if (omp_get_thread_num() == 0) {
          int pct = (int)(100.0 * d / total);
          if (pct != last_pct && (tty || pct % 10 == 0)) {
            last_pct = pct;
            draw(functionality.name, d, seconds_since(slice_start));
          }
        }
      }
    } else {
      for (uint64_t it = 0; it < total; ++it) {
        functionality.run(it);
        int pct = (int)(100.0 * (it + 1) / total);
        if (pct != last_pct && (tty || pct % 10 == 0)) {
          last_pct = pct;
          draw(functionality.name, it + 1, seconds_since(slice_start));
        }
      }
    }

    draw(functionality.name, total, seconds_since(slice_start));
    std::fputc('\n', stderr);
  }
}
