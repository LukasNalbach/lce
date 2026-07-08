/*******************************************************************************
 * tests/test-strings.hpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <type_traits>
#include <vector>

template <typename gen_t>
inline uint64_t random_log_uniform_size(uint64_t min_size, uint64_t max_size, gen_t& gen) {
  std::uniform_real_distribution<double> log_distrib(
      std::log((double)std::max<uint64_t>(1, min_size)),
      std::log((double)std::max<uint64_t>(1, max_size)));
  return std::clamp<uint64_t>((uint64_t)std::llround(std::exp(log_distrib(gen))), min_size, max_size);
}

template <typename T>
struct sym_range {
  static constexpr T min() {
    if constexpr (std::is_same_v<T, __uint128_t>) return 0;
    else return std::numeric_limits<T>::min();
  }
  static constexpr T max() {
    if constexpr (std::is_same_v<T, __uint128_t>) return ~static_cast<__uint128_t>(0);
    else return std::numeric_limits<T>::max();
  }
};

template <typename sym_t, typename gen_t>
inline sym_t draw_symbol(sym_t min_sym, sym_t max_sym, gen_t& gen) {
  if constexpr (sizeof(sym_t) <= 8) {
    using sym_dist_t = std::conditional_t<sizeof(sym_t) == 1,
        std::conditional_t<std::is_signed_v<sym_t>, int, unsigned int>, sym_t>;
    return (sym_t)std::uniform_int_distribution<sym_dist_t>(
        (sym_dist_t)min_sym, (sym_dist_t)max_sym)(gen);
  } else {
    const __uint128_t span = (__uint128_t)max_sym - (__uint128_t)min_sym;
    std::uniform_int_distribution<uint64_t> d64;
    const __uint128_t hi = (__uint128_t)d64(gen) << 64;
    __uint128_t r = hi | (__uint128_t)d64(gen);
    if (span != ~static_cast<__uint128_t>(0)) r %= (span + 1);
    return (sym_t)((__uint128_t)min_sym + r);
  }
}

template <typename inp_t, typename gen_t>
inp_t random_repetitive_input(
    gen_t& gen, uint64_t min_size, uint64_t max_size,
    typename inp_t::value_type min_sym = sym_range<typename inp_t::value_type>::min(),
    typename inp_t::value_type max_sym = sym_range<typename inp_t::value_type>::max()) {
  using sym_t = typename inp_t::value_type;
  std::uniform_real_distribution<double> prob_distrib(0.0, 1.0);

  const uint64_t target_input_size = random_log_uniform_size(min_size, max_size, gen);
  enum construction_operation { new_symbol = 0, repetition = 1, run = 2 };
  const double repetition_repetitiveness = prob_distrib(gen);
  const double run_repetitiveness = prob_distrib(gen);

  std::uniform_int_distribution<uint64_t> repetition_length_distrib(
      1, std::max<double>(1.0, (repetition_repetitiveness * target_input_size) / 100));
  std::uniform_int_distribution<uint64_t> run_length_distrib(
      1, std::max<double>(1.0, (run_repetitiveness * target_input_size) / 200));
  std::discrete_distribution<uint32_t> next_operation_distrib({
      2 - (repetition_repetitiveness + run_repetitiveness),
      repetition_repetitiveness,
      run_repetitiveness});

  inp_t input;
  input.reserve(target_input_size);
  input.push_back(draw_symbol<sym_t>(min_sym, max_sym, gen));

  while (input.size() < target_input_size) {
    switch (next_operation_distrib(gen)) {
      case new_symbol: {
        input.push_back(draw_symbol<sym_t>(min_sym, max_sym, gen));
        break;
      }
      case repetition: {
        uint64_t repetition_length =
            std::min<uint64_t>(target_input_size - input.size(), repetition_length_distrib(gen));
        uint64_t repetition_source = std::uniform_int_distribution<uint64_t>(0, input.size() - 1)(gen);
        for (uint64_t i = 0; i < repetition_length; i++)
          input.push_back(input[repetition_source + i]);
        break;
      }
      case run: {
        uint64_t run_length =
            std::min<uint64_t>(target_input_size - input.size(), run_length_distrib(gen));
        sym_t run_sym = draw_symbol<sym_t>(min_sym, max_sym, gen);
        for (uint64_t i = 0; i < run_length; i++)
          input.push_back(run_sym);
        break;
      }
    }
  }

  return input;
}
