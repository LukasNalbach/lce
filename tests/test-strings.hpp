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

enum repetitiveness_kind_t : uint8_t {
  _repetitive_periodic = 0,
  _repetitive_versioned = 1,
  _repetitive_indel = 2,
  _repetitive_blockmove = 3,
  _repetitive_markov = 4,
  _repetitive_lz = 5,
  _repetitive_fibonacci = 6,
  _repetitive_runs = 7,
  _repetitive_num_kinds = 8
};

struct repetitive_params_t {
  repetitiveness_kind_t kind = _repetitive_versioned;
  uint64_t size = 1 << 20;
  uint64_t base_length = 4096;
  uint64_t block_length = 256;
  double mutation_rate = 0.001;
};

template <typename gen_t>
inline repetitive_params_t random_repetitive_params(uint64_t size, gen_t& gen) {
  std::uniform_int_distribution<uint32_t> kinds(0, _repetitive_num_kinds - 1);
  std::uniform_real_distribution<double> chance(0.0, 1.0);
  repetitive_params_t params;

  params.kind = repetitiveness_kind_t(kinds(gen));
  params.size = size;

  std::uniform_int_distribution<uint64_t> bases(1, std::max<uint64_t>(1, size));
  params.base_length = std::max<uint64_t>(1, bases(gen) / (1 + kinds(gen)));
  params.block_length = std::max<uint64_t>(1, params.base_length / (1 + kinds(gen)));
  params.mutation_rate = chance(gen) * 0.05;
  return params;
}

template <typename inp_t, typename gen_t>
inline void append_runs_input(inp_t& input, uint64_t size, gen_t& gen,
                              typename inp_t::value_type min_sym, typename inp_t::value_type max_sym) {
  using sym_t = typename inp_t::value_type;
  std::uniform_real_distribution<double> prob_distrib(0.0, 1.0);
  enum construction_operation { new_symbol = 0, repetition = 1, run = 2 };
  const double repetition_repetitiveness = prob_distrib(gen);
  const double run_repetitiveness = prob_distrib(gen);

  std::uniform_int_distribution<uint64_t> repetition_length_distrib(
      1, std::max<double>(1.0, (repetition_repetitiveness * size) / 100));
  std::uniform_int_distribution<uint64_t> run_length_distrib(
      1, std::max<double>(1.0, (run_repetitiveness * size) / 200));
  std::discrete_distribution<uint32_t> next_operation_distrib({
      2 - (repetition_repetitiveness + run_repetitiveness),
      repetition_repetitiveness,
      run_repetitiveness});

  input.push_back(draw_symbol<sym_t>(min_sym, max_sym, gen));

  while (input.size() < size) {
    switch (next_operation_distrib(gen)) {
      case new_symbol: {
        input.push_back(draw_symbol<sym_t>(min_sym, max_sym, gen));
        break;
      }
      case repetition: {
        uint64_t repetition_length =
            std::min<uint64_t>(size - input.size(), repetition_length_distrib(gen));
        uint64_t repetition_source = std::uniform_int_distribution<uint64_t>(0, input.size() - 1)(gen);
        for (uint64_t i = 0; i < repetition_length; i++)
          input.push_back(input[repetition_source + i]);
        break;
      }
      case run: {
        uint64_t run_length = std::min<uint64_t>(size - input.size(), run_length_distrib(gen));
        sym_t run_sym = draw_symbol<sym_t>(min_sym, max_sym, gen);
        for (uint64_t i = 0; i < run_length; i++)
          input.push_back(run_sym);
        break;
      }
    }
  }
}

template <typename inp_t, typename gen_t>
inline inp_t generate_repetitive_input(
    repetitive_params_t params, gen_t& gen,
    typename inp_t::value_type min_sym = sym_range<typename inp_t::value_type>::min(),
    typename inp_t::value_type max_sym = sym_range<typename inp_t::value_type>::max()) {
  using sym_t = typename inp_t::value_type;
  std::uniform_real_distribution<double> chance(0.0, 1.0);

  params.size = std::max<uint64_t>(1, params.size);
  params.base_length = std::clamp<uint64_t>(params.base_length, 1, params.size);
  params.block_length = std::clamp<uint64_t>(params.block_length, 1, params.base_length);

  inp_t input;
  input.reserve(params.size);
  auto draw = [&]() { return draw_symbol<sym_t>(min_sym, max_sym, gen); };

  if (params.kind == _repetitive_runs) {
    append_runs_input(input, params.size, gen, min_sym, max_sym);
    input.resize(params.size);
    return input;
  }

  if (params.kind == _repetitive_fibonacci) {
    inp_t previous, current;
    previous.push_back(min_sym);
    current.push_back(max_sym == min_sym ? min_sym : sym_t(min_sym + 1));

    while (current.size() < params.size) {
      inp_t next = current;
      next.insert(next.end(), previous.begin(), previous.end());
      previous = std::move(current);
      current = std::move(next);
    }

    current.resize(params.size);
    return current;
  }

  if (params.kind == _repetitive_markov) {
    uint64_t least = std::max<uint64_t>(1, params.base_length / 4);

    while (input.size() < params.size) {
      if (input.size() > params.base_length && chance(gen) < 0.5) {
        std::uniform_int_distribution<uint64_t> from(0, input.size() - params.base_length - 1);
        std::uniform_int_distribution<uint64_t> length(least, params.base_length);
        uint64_t start = from(gen);
        uint64_t take = std::min(length(gen), params.size - input.size());
        for (uint64_t i = 0; i < take; i++) input.push_back(input[start + i]);
      } else {
        for (uint64_t i = 0; i < least && input.size() < params.size; i++)
          input.push_back(draw());
      }
    }

    input.resize(params.size);
    return input;
  }

  if (params.kind == _repetitive_lz) {
    input.push_back(draw());
    std::uniform_int_distribution<uint64_t> length(1, params.base_length);

    while (input.size() < params.size) {
      if (chance(gen) < 0.98) {
        std::uniform_int_distribution<uint64_t> from(0, input.size() - 1);
        uint64_t start = from(gen);
        uint64_t take = std::min({length(gen), input.size() - start, params.size - input.size()});
        for (uint64_t i = 0; i < take; i++) input.push_back(input[start + i]);
      } else {
        input.push_back(draw());
      }
    }

    input.resize(params.size);
    return input;
  }

  inp_t base;
  base.reserve(params.base_length);
  for (uint64_t i = 0; i < params.base_length; i++) base.push_back(draw());

  while (input.size() < params.size) {
    switch (params.kind) {
      case _repetitive_periodic:
        input.insert(input.end(), base.begin(),
                     base.begin() + std::min<uint64_t>(base.size(), params.size - input.size()));
        break;

      case _repetitive_versioned:
        for (uint64_t i = 0; i < base.size() && input.size() < params.size; i++)
          input.push_back(chance(gen) < params.mutation_rate ? draw() : base[i]);
        break;

      case _repetitive_indel:
        for (uint64_t i = 0; i < base.size() && input.size() < params.size; i++) {
          double roll = chance(gen);
          if (roll < params.mutation_rate / 2) continue;
          if (roll < params.mutation_rate) input.push_back(draw());
          input.push_back(base[i]);
        }
        break;

      default: {
        uint64_t blocks = std::max<uint64_t>(1, base.size() / params.block_length);
        std::vector<uint64_t> order(blocks);
        for (uint64_t i = 0; i < blocks; i++) order[i] = i;
        std::shuffle(order.begin(), order.end(), gen);

        for (uint64_t block : order) {
          if (input.size() >= params.size) break;
          uint64_t start = block * params.block_length;
          uint64_t take = std::min({params.block_length, base.size() - start, params.size - input.size()});
          input.insert(input.end(), base.begin() + start, base.begin() + start + take);
        }
        break;
      }
    }
  }

  input.resize(params.size);
  return input;
}

template <typename inp_t, typename gen_t>
inline inp_t random_repetitive_input(
    gen_t& gen, uint64_t min_size, uint64_t max_size,
    typename inp_t::value_type min_sym = sym_range<typename inp_t::value_type>::min(),
    typename inp_t::value_type max_sym = sym_range<typename inp_t::value_type>::max()) {
  const uint64_t size = random_log_uniform_size(min_size, max_size, gen);
  return generate_repetitive_input<inp_t>(random_repetitive_params(size, gen), gen, min_sym, max_sym);
}
