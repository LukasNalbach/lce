/*******************************************************************************
 * lce/rolling_hash/string_synchronizing_set.hpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <omp.h>
#include <parallel_hashmap/phmap.h>

#include <mutex>

#include "ring_buffer.hpp"
#include "rolling_hash.hpp"
namespace lce::rolling_hash {

template <typename t_index = uint32_t, uint64_t t_tau = 1024>
class sss {
 public:
  typedef t_index index_type;
  static constexpr uint64_t tau = t_tau;
  __extension__ typedef unsigned __int128 uint128_t;

  sss() : m_fps_calculated(false) {
  }

  template <typename t_char_type>
  sss(t_char_type const* text, size_t size, bool calculate_fps = false)
      : m_fps_calculated(calculate_fps) {
    assert(size > 5 * t_tau);
    std::vector<std::vector<t_index>> sss_part(omp_get_max_threads());
    std::vector<std::vector<uint128_t>> fps_part(omp_get_max_threads());
#pragma omp parallel
    {
      const size_t sss_end = size - 2 * t_tau + 1;

      const int t = omp_get_thread_num();
      const int nt = omp_get_num_threads();
      const size_t slice_size = sss_end / nt;

      const size_t begin = t * slice_size;
      const size_t end = (t < nt - 1) ? (t + 1) * slice_size : sss_end;

      std::tie(sss_part[t], fps_part[t]) =
          fill_synchronizing_set(text, begin, end);
    }

    // Merge SSS parts
    std::vector<size_t> write_pos{0};
    for (auto& part : sss_part) {
      write_pos.push_back(write_pos.back() + part.size());
    }
    size_t sss_size = write_pos.back();
    m_runs_detected = sss_size > size * 4 / t_tau;

    // If the text contains long runs, the sss inflates. We the then use a
    // algorithm which detects runs.
    if (m_runs_detected) {
#pragma omp parallel
      {
        const size_t sss_end = size - 2 * t_tau + 1;

        const int t = omp_get_thread_num();
        const int nt = omp_get_num_threads();
        const size_t slice_size = sss_end / nt;

        const size_t begin = t * slice_size;
        const size_t end = (t < nt - 1) ? (t + 1) * slice_size : sss_end;
        sss_part[t] = std::vector<t_index>{};
        std::tie(sss_part[t], fps_part[t]) =
            fill_synchronizing_set_runs(text, size, begin, end);
      }
      write_pos = {0};
      for (auto& part : sss_part) {
        write_pos.push_back(write_pos.back() + part.size());
      }
      sss_size = write_pos.back() + 1;  //+1 for sentinel
    }

    m_sss.resize(sss_size);
    if (m_fps_calculated) {
      m_fps.resize(sss_size);
    }
#pragma omp parallel
    {
      const int t = omp_get_thread_num();
      std::copy(sss_part[t].begin(), sss_part[t].end(),
                m_sss.begin() + write_pos[t]);
      if (m_fps_calculated) {
        std::copy(fps_part[t].begin(), fps_part[t].end(),
                  m_fps.begin() + write_pos[t]);
#pragma omp barrier
        // add distance to positions that start periodic area
        if (m_runs_detected && m_fps_calculated && !fps_part[t].empty() &&
            write_pos[t] != 0) {
          size_t i = m_sss[write_pos[t]];
          size_t prev_i = m_sss[write_pos[t] - 1];
          size_t distance = i - prev_i;
          assert(distance < (size_t{1} << 20));
          if (distance > t_tau) {
            m_fps[write_pos[t] - 1] += (uint128_t{distance} << 107);
          }
        }
      }
    }
    if (m_runs_detected) {
      m_sss.back() =
          size - 2 * t_tau + 1;  // sentinel needed for text with runs
      if (m_fps_calculated) {
        m_fps.back() = 1;
      }
    }
  }

  template <typename t_char_type>
  std::pair<std::vector<t_index>, std::vector<uint128_t>>
  fill_synchronizing_set(t_char_type const* text, const size_t from,
                         const size_t to) const {
    // calculate SSS
    std::vector<t_index> sss;
    std::vector<uint128_t> fps;

    rk_prime rk(t_tau, 296819);
    rk_prime rk3(3 * t_tau, 296819);

    for (size_t i = 0; i < t_tau; ++i) {
      rk.roll_in(text[from + i]);
    }
    for (size_t i = 0; i < 3 * t_tau; ++i) {
      rk3.roll_in(text[from + i]);
    }

    ring_buffer<uint128_t> fingerprints(4 * t_tau);
    fingerprints.resize(from);
    fingerprints.push_back(rk.get_fp());

    ring_buffer<uint128_t> fingerprints3(4 * t_tau);
    fingerprints3.resize(from);
    fingerprints3.push_back(rk3.get_fp());

    // Loop:
    t_index first_min = 0;
    for (size_t i = from; i < to; ++i) {
      for (size_t j = fingerprints.size(); j <= i + 3 * t_tau; ++j) {
        fingerprints.push_back(rk.roll(text[j - 1], text[j + t_tau - 1]));
        fingerprints3.push_back(rk3.roll(text[j - 1], text[j + 3 * t_tau - 1]));
      }

      if (first_min == 0 || first_min < i) {
        first_min = i;
        for (size_t j = i; j <= i + t_tau; ++j) {
          if (fingerprints[j] < fingerprints[first_min]) {
            first_min = j;
          }
        }
      } else if (fingerprints[i + t_tau] < fingerprints[first_min]) {
        first_min = i + t_tau;
      }

      if (fingerprints[first_min] == fingerprints[i] ||
          fingerprints[first_min] == fingerprints[i + t_tau]) {
        sss.push_back(i);
        if (m_fps_calculated) {
          fps.push_back(fingerprints3[i]);
        }
      }
    }
    return {sss, fps};
  }
  template <typename t_char_type>
  std::pair<std::vector<t_index>, std::vector<uint128_t>>
  fill_synchronizing_set_runs(const t_char_type* text, size_t size,
                              const size_t from, const size_t to) {
    // calculate Q
    std::vector<std::pair<t_index, t_index>> qset =
        calculate_q(text, size, from, to);
    qset.push_back(std::make_pair(std::numeric_limits<t_index>::max(),
                                  std::numeric_limits<t_index>::max()));
    auto it_q = qset.begin();

    // calculate SSS
    std::vector<t_index> sss;
    std::vector<uint128_t> fps;

    rk_prime rk(t_tau, 296819);
    rk_prime rk3(3 * t_tau, 296819);
    for (size_t i = 0; i < t_tau; ++i) {
      rk.roll_in(text[from + i]);
    }
    for (size_t i = 0; i < 3 * t_tau; ++i) {
      rk3.roll_in(text[from + i]);
    }

    ring_buffer<uint128_t> fingerprints(4 * t_tau);
    fingerprints.resize(from);
    fingerprints.push_back(rk.get_fp());

    ring_buffer<uint128_t> fingerprints3(4 * t_tau);
    fingerprints3.resize(from);
    fingerprints3.push_back(rk3.get_fp());

    t_index MIN_UNKNOWN = std::numeric_limits<t_index>::max();
    t_index first_min = MIN_UNKNOWN;
    // Loop:
    for (size_t i = from; i < to; ++i) {
      for (size_t j = fingerprints.size(); j <= i + t_tau; ++j) {
        fingerprints.push_back(rk.roll(text[j - 1], text[j + t_tau - 1]));
        fingerprints3.push_back(rk3.roll(text[j - 1], text[j + 3 * t_tau - 1]));
      }
      while (it_q->second < i) {
        std::advance(it_q, 1);
      }

      // If then minimum in the current range is not known, we need to find one
      if (first_min == MIN_UNKNOWN || first_min < i) {
        auto it_qt = it_q;
        for (size_t j = i; j <= i + t_tau; ++j) {
          // advance q pointer
          if (it_qt->second < j) {
            std::advance(it_qt, 1);
          }
          // don't compare values from q
          if (it_qt->first <= j) {
            // first_min = it_qt->second + 1;
            // j = first_min;
            j = it_qt->second;
            continue;
          }
          // take first fingerprint not in q
          if (first_min == MIN_UNKNOWN || first_min < i) {
            first_min = j;
          }
          // compare values that are not in q
          if (fingerprints[j] < fingerprints[first_min]) {
            first_min = j;
          }
        }
        // If no minimum exists, we jump to the next position, which may be part
        // of sss
        if (first_min == MIN_UNKNOWN || first_min < i) {
          i = it_qt->second - t_tau;
          continue;
        }
      }
      // If the minimum of the range is already known, we only need to compare
      // with the new fingerprint
      else if (first_min <= i + t_tau) {
        auto it_qt = it_q;
        while (it_qt->second < i + t_tau) {
          std::advance(it_qt, 1);
        }
        if (it_qt->first > i + t_tau &&
            fingerprints[i + t_tau] < fingerprints[first_min]) {
          first_min = i + t_tau;
        }
      }
      // maybe_add(i);
      if (fingerprints[first_min] == fingerprints[i] ||
          fingerprints[first_min] == fingerprints[i + t_tau]) {
        sss.push_back(i);
        if (m_fps_calculated) {
          fps.push_back(fingerprints3[i]);
          // add distance to positions that start periodic area
          if (sss.size() > 1) {
            size_t prev_i = sss[sss.size() - 2];
            size_t distance = i - prev_i;
            // we only have 20 empty bits in fingerprint
            assert(distance < (size_t{1} << 20));
            if (distance > t_tau) {
              fps[sss.size() - 2] += (uint128_t{distance} << 107);
            }
          }
        }
      }
    }

    return {sss, fps};
  }

  template <typename t_char_type>
  std::vector<std::pair<t_index, t_index>> calculate_q(t_char_type* const text,
                                                       size_t size,
                                                       const size_t from,
                                                       const size_t to) {
    std::vector<std::pair<t_index, t_index>> qset{};  // inclusive intervals
    constexpr size_t small_tau = t_tau / 4;

    rk_prime rk(small_tau, 296819);
    for (size_t i = 0; i < small_tau; ++i) {
      rk.roll_in(text[from + i]);
    }

    ring_buffer<uint128_t> fingerprints(4 * t_tau);
    fingerprints.resize(from);
    fingerprints.push_back(rk.get_fp());

    for (size_t i = from; i < to + t_tau; ++i) {  //++i correct?
      for (size_t j = fingerprints.size(); j < i + t_tau; ++j) {
        fingerprints.push_back(rk.roll(text[j - 1], text[j + small_tau - 1]));
      }
      // find first minimum
      size_t first_min = i;
      for (size_t j = first_min; j < i + small_tau; ++j) {
        if (fingerprints[j] < fingerprints[first_min]) {
          first_min = j;
        }
      }
      // find next minimum
      size_t next_min = first_min + 1;
      for (size_t j = next_min; j < first_min + small_tau; ++j) {
        if (fingerprints[j] < fingerprints[next_min]) {
          next_min = j;
        }
      }

      // if minimum fps match, look for run
      if (fingerprints[next_min] != fingerprints[first_min]) {
        i = next_min - 1;
      } else {
        // if matching fingerprint exists, extend the run and add it to q
        size_t const period = next_min - first_min;
        // now extend run naivly to the left
        size_t run_start = first_min;
        while (run_start > from &&
               text[run_start - 1] == text[run_start + period - 1]) {
          --run_start;
        }

        // extend run naivly to the right
        size_t run_end = next_min;  // inclusive
        // while (run_end < to + 2 * t_tau - 2 &&
        while (run_end < (size - 1) &&
               text[run_end + 1] == text[run_end - period + 1]) {
          ++run_end;
        }

        // add run to set q
        if (run_end - run_start + 1 >= t_tau) {
          qset.push_back(std::make_pair(run_start, run_end - t_tau + 1));
          i = run_end - small_tau;

          if (run_end - run_start + 1 >= 3 * t_tau - 1) {
            if (run_start == 0) {
              continue;  // Run starts at 0, no run information needed
            }
            if (text[run_start - 1] == text[run_start + period - 1]) {
              continue;  // Run starts at previous PE, we are not responsible
            }

            while (run_end < size - 1 &&
                   text[run_end + 1] == text[run_end - period + 1]) {
              ++run_end;
            }

            size_t const sss_pos1 = run_start - 1;
            size_t const sss_pos2 = run_end - (2 * t_tau) + 2;
            int64_t const run_info = int64_t{1} * size - sss_pos2 + sss_pos1;
            m_run_info[sss_pos1] =
                text[run_end + 1] > text[run_end - period + 1]
                    ? run_info
                    : run_info * (-1);
          }
        } else {
          i = next_min - 1;
        }
      }
    }
    return qset;
  }

  template <typename C>
  sss(C const& container, bool calculate_fps = false)
      : sss(container.data(), container.size(), calculate_fps) {
  }

  bool fps_calculated() const {
    return m_fps_calculated;
  }

  std::vector<t_index> const& get_sss() const {
    return m_sss;
  }

  std::vector<uint128_t> const& get_fps() const {
    assert(m_fps_calculated);
    return m_fps;
  }
  void free_fps() {
    m_fps = std::vector<uint128_t>{};
    m_fps_calculated = false;
  }

  size_t num_runs() const {
    return m_run_info.size();
  }
  size_t has_runs() const {
    return m_runs_detected;
  }

  size_t size() const {
    return m_sss.size();
  }

  uint64_t operator[](size_t i) const {
    return m_sss[i];
  }

  int64_t get_run_info(size_t pos) const {
    auto run_info_entry = m_run_info.find(pos);
    return run_info_entry == m_run_info.end() ? 0 : run_info_entry->second;
  }

 private:
  std::vector<t_index> m_sss;
  std::vector<uint128_t> m_fps;
  bool m_fps_calculated;

  phmap::parallel_flat_hash_map<
      t_index, int64_t, phmap::priv::hash_default_hash<t_index>,
      phmap::priv::hash_default_eq<t_index>,
      phmap::priv::Allocator<std::pair<const t_index, int64_t>>, 4, std::mutex>
      m_run_info;
  bool m_runs_detected;
};
}  // namespace lce::rolling_hash
