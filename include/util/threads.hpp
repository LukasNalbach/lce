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
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

namespace lce::util {

inline uint64_t physical_cores() {
  static const uint64_t cores = [] {
    const uint64_t logical = uint64_t(std::max(1, omp_get_num_procs()));
#ifdef _WIN32
    return logical;
#else
    std::ifstream in("/sys/devices/system/cpu/cpu0/topology/thread_siblings_list");
    std::string list;
    if (!std::getline(in, list)) return logical;
    std::stringstream ranges(list);
    std::string range;
    uint64_t siblings = 0;

    while (std::getline(ranges, range, ',')) {
      const uint64_t dash = range.find('-');
      if (dash == std::string::npos) {
        siblings++;
      } else {
        siblings += std::stoull(range.substr(dash + 1)) - std::stoull(range.substr(0, dash)) + 1;
      }
    }

    return std::max<uint64_t>(1, logical / std::max<uint64_t>(1, siblings));
#endif
  }();

  return cores;
}

inline int sais_threads(int threads = omp_get_max_threads()) {
  return int(std::clamp<uint64_t>(physical_cores(), 1, uint64_t(std::max(1, threads))));
}

}
