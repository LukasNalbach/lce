/*******************************************************************************
 * lce/rolling_hash/modular_arithmetic.hpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once
#include <assert.h>
#include <hurchalla/modular_arithmetic/modular_multiplication.h>
#include <hurchalla/modular_arithmetic/modular_pow.h>

#include <bit>
#include <iterator>
#include <random>

namespace lce::modular {

// Return base^exp % prime.
template <typename T>
inline T pow_mod(T base, T exp, T prime) {
  return hurchalla::modular_pow(base, exp, prime);
}

// Return base*exp % prime.
template <typename T>
inline T mult_mod(T a, T b, T prime) {
  return hurchalla::modular_multiplication_prereduced_inputs(a, b, prime);
}

// Return base^exp % prime.
template <typename T>
inline T mod(T a, T prime) {
  return a % prime;
}
}  // namespace lce::modular
