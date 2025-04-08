/*******************************************************************************
 * lce/rolling_hash/mersenne_modular_arithmetic.hpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once
#include <assert.h>

#include <bit>
#include <iterator>
#include <random>
#include <algorithm>

// Return whether num is a mersenne prime.
namespace lce::mersenne {
template <typename T>
constexpr bool is_mersenne_prime(T num) {
  std::array<size_t, 12> mersenne_exponents{2,  3,  5,  7,  13,  17,
                                            19, 31, 61, 89, 107, 127};
  size_t exp = std::countr_one(num);
  return (exp == std::popcount(num) &&
          std::find(mersenne_exponents.begin(), mersenne_exponents.end(),
                    exp) != mersenne_exponents.end());
}

// For num < (2*(m_prime-1)) return num % prime.
template <typename T, T t_mersenne_prime>
inline T small_num_mod(T num) {
  static_assert(is_mersenne_prime(t_mersenne_prime));
  constexpr size_t mersenne_exp = std::bit_width(t_mersenne_prime);
  assert(num <= (t_mersenne_prime - 1) * 2);

  T const z = (num + 1) >> mersenne_exp;
  return (num + z) & t_mersenne_prime;
}

// Return num % prime.
template <typename T, T t_mersenne_prime>
inline T mod(T num) {
  static_assert(is_mersenne_prime(t_mersenne_prime));
  constexpr size_t mersenne_exp = std::bit_width(t_mersenne_prime);

  num = (num & t_mersenne_prime) + (num >> mersenne_exp);
  return (num >= t_mersenne_prime) ? (num - t_mersenne_prime) : num;
}

// For num < (2*(m_prime-1)) return num % prime.
template <typename T, T t_mersenne_prime>
inline T small_num_mod_alt(T num) {
  static_assert(is_mersenne_prime(t_mersenne_prime));
  constexpr size_t mersenne_exp = std::bit_width(t_mersenne_prime);
  assert(num <= (t_mersenne_prime - 1) * 2);

  num = (num & t_mersenne_prime) + (num >> mersenne_exp);
  assert(num <= t_mersenne_prime);
  return (num == t_mersenne_prime) ? 0 : num;
}

// Return num % prime.
template <typename T, T t_prime>
inline T mod_naive(T num) {
  return num % t_prime;
}

// Return a+b % prime. The two integers must already be reduced.
template <typename T, T t_mersenne_prime>
inline T add_mod(T a, T b) {
  static_assert(is_mersenne_prime(t_mersenne_prime));
  assert(a < t_mersenne_prime && b < t_mersenne_prime);

  return small_num_mod<T, t_mersenne_prime>(a + b);
}

// Return -a % prime. The integer a must already be reduced.
template <typename T, T t_mersenne_prime>
inline T additive_inverse_mod(T a) {
  static_assert(is_mersenne_prime(t_mersenne_prime));
  assert(a < t_mersenne_prime);
  return small_num_mod<T, t_mersenne_prime>(t_mersenne_prime - a);
}
}  // namespace lce::mersenne
