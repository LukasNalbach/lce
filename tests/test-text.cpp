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

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <random>
#include <vector>

#include "ds/lce_sss.hpp"
#include "text/direct_text.hpp"
#include "text/packed_text.hpp"
#include "text/split_text.hpp"
#include "util/memory.hpp"

#include "test-progress.hpp"
#include "test-strings.hpp"

thread_local std::mt19937_64 gen(std::random_device{}());

static std::vector<uint8_t> random_text(uint64_t max_size) {
  const uint64_t sigma = random_log_uniform_size(1, 256, gen);
  const uint64_t low = std::uniform_int_distribution<uint64_t>(0, 256 - sigma)(gen);
  std::vector<uint8_t> text = random_repetitive_input<std::vector<uint8_t>>(
      gen, 1, max_size, uint8_t(low), uint8_t(low + sigma - 1));
  text.reserve(text.size() + 64);
  return text;
}

static std::vector<uint8_t> random_skewed_text(uint64_t max_size) {
  std::vector<uint8_t> text = random_text(max_size);
  std::uniform_real_distribution<double> prob(0.0, 1.0);
  const double rare_share = prob(gen) * 0.1;
  const uint64_t rare_sigma = random_log_uniform_size(1, 256, gen);
  const uint64_t rare_low = std::uniform_int_distribution<uint64_t>(0, 256 - rare_sigma)(gen);
  std::uniform_int_distribution<uint64_t> rare(rare_low, rare_low + rare_sigma - 1);

  for (uint8_t& c : text) {
    if (prob(gen) < rare_share) c = uint8_t(rare(gen));
  }

  return text;
}

static uint64_t naive_lce(const std::vector<uint8_t>& t, uint64_t i, uint64_t j, uint64_t max) {
  uint64_t k = 0;
  while (k < max && std::max(i, j) + k < t.size() && t[i + k] == t[j + k]) ++k;
  return k;
}

static uint64_t naive_lce_left(const std::vector<uint8_t>& t, uint64_t i, uint64_t j, uint64_t max) {
  uint64_t k = 0;
  while (k < max && k <= std::min(i, j) && t[i - k] == t[j - k]) ++k;
  return k;
}

static void test_packed_access() {
  std::vector<uint8_t> t = random_text(200000);
  const uint64_t n = t.size();
  lce::text::packed_text p(reinterpret_cast<const char*>(t.data()), n, random_num_threads(gen));

  std::vector<bool> used(256, false);
  for (uint8_t c : t) used[c] = true;
  const uint64_t sigma = std::count(used.begin(), used.end(), true);
  EXPECT_EQ(p.sigma(), sigma);
  EXPECT_EQ(p.width(), std::max<int>(1, std::bit_width(sigma - 1)));

  for (uint64_t i = 0; i < n; ++i) {
    ASSERT_EQ(p.to_char(p[i]), t[i]) << "i=" << i << " n=" << n;
    ASSERT_EQ(uint8_t(p.char_at(i)), t[i]);
  }

  std::uniform_int_distribution<uint64_t> pos(0, n - 1);
  for (uint64_t q = 0; q < 1000; ++q) {
    const uint64_t i = pos(gen);
    const uint64_t j = pos(gen);
    EXPECT_EQ(p[i] < p[j], t[i] < t[j]);
    EXPECT_EQ(p.less_char(i, j), t[i] < t[j]);
  }

  const uint64_t start = pos(gen);
  auto cursor = p.cursor_at(start);
  for (uint64_t i = start; i < n; ++i) ASSERT_EQ(cursor.next(), p[i]) << "cursor at " << i;
}

template <typename text_t>
static void check_lce(const text_t& text, const std::vector<uint8_t>& t) {
  const uint64_t n = t.size();
  std::uniform_int_distribution<uint64_t> pos(0, n - 1);
  std::uniform_int_distribution<uint64_t> max_distrib(0, n + 1);

  for (uint64_t q = 0; q < 300; ++q) {
    uint64_t i = pos(gen);
    uint64_t j = pos(gen);

    if (q % 2 == 0 && n > 1) {
      const uint64_t shift = random_log_uniform_size(1, n - 1, gen);
      j = (i + shift) % n;
    }

    const uint64_t max = q % 3 == 0 ? std::numeric_limits<uint64_t>::max() : max_distrib(gen);
    EXPECT_EQ(text.lce(i, j, max), naive_lce(t, i, j, max))
        << "lce(" << i << "," << j << "," << max << ") n=" << n;
    EXPECT_EQ(text.lce_left(i, j, max), naive_lce_left(t, i, j, max))
        << "lce_left(" << i << "," << j << "," << max << ") n=" << n;
  }
}

static void test_lce() {
  std::vector<uint8_t> t = random_text(50000);
  lce::text::direct_text<uint8_t> d(t.data(), t.size());
  lce::text::packed_text p(reinterpret_cast<const char*>(t.data()), t.size(), random_num_threads(gen));
  check_lce(d, t);
  check_lce(p, t);
}

static std::vector<uint8_t> random_split_text(uint64_t max_size) {
  return std::uniform_int_distribution<int>(0, 1)(gen) == 0 ? random_text(max_size) : random_skewed_text(max_size);
}

template <typename text_t>
static text_t build_split(const std::vector<uint8_t>& t, uint16_t threads) {
  const char* data = reinterpret_cast<const char*>(t.data());
  const uint64_t n = t.size();
  if (std::uniform_int_distribution<int>(0, 3)(gen) == 0) return text_t(data, n, threads);
  typename text_t::histogram_t histogram{};
  lce::text::packed_text::add_counts(histogram, data, n, threads);
  text_t text(histogram, n);
  const uint64_t block = text_t::chunk_symbols * random_log_uniform_size(1, n / text_t::chunk_symbols + 1, gen);
  std::vector<uint64_t> starts;
  for (uint64_t at = 0; at < n; at += block) starts.push_back(at);
  std::shuffle(starts.begin(), starts.end(), gen);

#pragma omp parallel for num_threads(threads) schedule(dynamic, 1)
  for (uint64_t k = 0; k < starts.size(); ++k) {
    text.pack(data + starts[k], std::min(block, n - starts[k]), starts[k], 1);
  }

  text.finish(threads);
  return text;
}

template <typename fnc_t>
static void with_random_split(const std::vector<uint8_t>& t, uint16_t threads, fnc_t fnc) {
  if (std::uniform_int_distribution<int>(0, 1)(gen) == 0) {
    fnc(build_split<lce::text::split_text<0>>(t, threads));
  } else {
    fnc(build_split<lce::text::split_text<2>>(t, threads));
  }
}

static void test_split_access() {
  std::vector<uint8_t> t = random_split_text(200000);
  const uint64_t n = t.size();

  with_random_split(t, random_num_threads(gen), [&](const auto& s) {
    std::vector<bool> used(256, false);
    for (uint8_t c : t) used[c] = true;
    EXPECT_EQ(s.sigma(), uint64_t(std::count(used.begin(), used.end(), true)));
    const auto histogram = lce::text::packed_text::count_symbols(reinterpret_cast<const char*>(t.data()), n, 1);
    EXPECT_EQ(std::decay_t<decltype(s)>::size_in_bytes_for(histogram, n), s.size_in_bytes());

    for (uint64_t i = 0; i < n; ++i) {
      ASSERT_EQ(s.to_char(s[i]), t[i]) << "i=" << i << " n=" << n << " width=" << int(s.width())
                                       << " payloads=" << s.has_payloads();
      ASSERT_EQ(uint8_t(s.char_at(i)), t[i]);
    }

    std::uniform_int_distribution<uint64_t> pos(0, n - 1);

    for (uint64_t q = 0; q < 1000; ++q) {
      const uint64_t i = pos(gen);
      const uint64_t j = pos(gen);
      EXPECT_EQ(s[i] < s[j], t[i] < t[j]);
      EXPECT_EQ(s.less_char(i, j), t[i] < t[j]);
    }

    std::vector<uint8_t> out;

    for (uint64_t q = 0; q < 100; ++q) {
      const uint64_t i = pos(gen);
      const uint64_t len = random_log_uniform_size(1, n - i, gen);
      out.assign(len, 0);
      s.extract(i, len, out.data());
      ASSERT_TRUE(std::equal(out.begin(), out.end(), t.begin() + i)) << "extract(" << i << "," << len << ") n=" << n;
    }

    const uint64_t start = pos(gen);
    auto cursor = s.cursor_at(start);
    for (uint64_t i = start; i < n; ++i) ASSERT_EQ(cursor.next(), s[i]) << "cursor at " << i;
  });
}

static void test_split_lce() {
  std::vector<uint8_t> t = random_split_text(50000);
  const uint64_t n = t.size();

  with_random_split(t, random_num_threads(gen), [&](const auto& s) {
    check_lce(s, t);
    std::uniform_int_distribution<uint64_t> pos(0, n - 1);

    for (uint64_t q = 0; q < 300; ++q) {
      const uint64_t i = pos(gen);
      const uint64_t j = pos(gen);
      const uint64_t len = std::min<uint64_t>(n - std::max(i, j), random_log_uniform_size(1, n, gen));
      const bool same = naive_lce(t, i, j, len) == len;
      EXPECT_EQ(s.equal(i, j, len), same) << "equal(" << i << "," << j << "," << len << ") n=" << n;
      if (same) EXPECT_EQ(s.hash(i, len), s.hash(j, len)) << "hash(" << i << "," << j << "," << len << ") n=" << n;
    }
  });
}

static void test_reverse() {
  std::vector<uint8_t> t = random_text(200000);
  const uint16_t threads = random_num_threads(gen);
  lce::text::packed_text p(reinterpret_cast<const char*>(t.data()), t.size(), threads);
  std::vector<uint8_t> r = t;
  lce::text::direct_text<uint8_t> d(r.data(), r.size());
  p.reverse(threads);
  d.reverse(threads);
  std::reverse(t.begin(), t.end());
  ASSERT_EQ(r, t);

  for (uint64_t i = 0; i < t.size(); ++i) ASSERT_EQ(p.to_char(p[i]), t[i]) << "i=" << i;
}

template <typename text_t>
static void check_lce_sss(const text_t& text, const std::vector<uint8_t>& t, uint64_t tau) {
  scoped_num_threads threads(random_num_threads(gen));
  lce::ds::lce_sss<text_t, lce::util::uint40_t> ds(text, tau);
  std::uniform_int_distribution<uint64_t> pos(0, t.size() - 1);

  for (uint64_t q = 0; q < 100; ++q) {
    const uint64_t i = pos(gen);
    const uint64_t j = pos(gen);
    EXPECT_EQ(ds.lce(i, j), naive_lce(t, i, j, std::numeric_limits<uint64_t>::max()))
        << "lce(" << i << "," << j << ") n=" << t.size() << " tau=" << tau;
  }
}

static void test_lce_sss_packed() {
  const uint64_t tau = uint64_t{1} << std::uniform_int_distribution<uint64_t>(2, 6)(gen);
  std::vector<uint8_t> t = random_text(150000);
  const uint16_t threads = random_num_threads(gen);
  check_lce_sss(lce::text::packed_text(reinterpret_cast<const char*>(t.data()), t.size(), threads), t, tau);
}

static void test_lce_sss_split() {
  const uint64_t tau = uint64_t{1} << std::uniform_int_distribution<uint64_t>(2, 6)(gen);
  std::vector<uint8_t> t = random_split_text(150000);
  with_random_split(t, random_num_threads(gen), [&](const auto& s) { check_lce_sss(s, t, tau); });
}

TEST(test_text, packed_access) {
  run_fuzz("text", {{"packed-access", [](uint64_t) { test_packed_access(); }, false}}, fuzz_iterations(3000));
}

TEST(test_text, lce) {
  run_fuzz("text", {{"lce", [](uint64_t) { test_lce(); }, false}}, fuzz_iterations(3000));
}

TEST(test_text, reverse) {
  run_fuzz("text", {{"reverse", [](uint64_t) { test_reverse(); }, false}}, fuzz_iterations(2000));
}

TEST(test_text, lce_sss_packed) {
  run_fuzz("text", {{"lce-sss-packed", [](uint64_t) { test_lce_sss_packed(); }, false}}, fuzz_iterations(1000));
}

TEST(test_text, split_access) {
  run_fuzz("text", {{"split-access", [](uint64_t) { test_split_access(); }, false}}, fuzz_iterations(2000));
}

TEST(test_text, split_lce) {
  run_fuzz("text", {{"split-lce", [](uint64_t) { test_split_lce(); }, false}}, fuzz_iterations(3000));
}

TEST(test_text, lce_sss_split) {
  run_fuzz("text", {{"lce-sss-split", [](uint64_t) { test_lce_sss_split(); }, false}}, fuzz_iterations(1000));
}
