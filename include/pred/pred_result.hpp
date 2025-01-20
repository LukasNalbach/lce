/*******************************************************************************
 * lce/pred/binsearch_std.hpp
 * Copyright (C) 2019 Patrick Dinklage <patrick.dinklage@tu-dortmund.de>
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <cstddef>

namespace lce::pred {

struct result {
  bool exists;
  size_t pos;

  inline operator bool() const {
    return exists;
  }

  inline operator size_t() const {
    return pos;
  }

  inline operator long long int() const {
    return pos;
  }
};

bool operator==(const result& lhs, const result& rhs) {
  return lhs.exists == rhs.exists && lhs.pos == rhs.pos;
}
}  // namespace lce::pred
