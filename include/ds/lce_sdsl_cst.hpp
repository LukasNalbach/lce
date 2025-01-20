/*******************************************************************************
 * lce/ds/lce_sdsl_cst.hpp
 *
 * Copyright (C) 2023 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <filesystem>
#include <sdsl/cst_sada.hpp>
#include <sdsl/cst_sct3.hpp>

namespace lce::ds {
namespace fs = std::filesystem;

class lce_sdsl_cst {
 public:
  

  lce_sdsl_cst(std::vector<uint8_t> const path) {
    sdsl::construct(m_cst, std::string(path.begin(), path.end()), 1);
    m_size = m_cst.size();
  }

  // Return the number of common letters in text[i..] and text[j..].
  size_t lce(size_t i, size_t j) const {
    if (i == j) [[unlikely]] {
      return m_size - i - 1; // -1 because of internal sentinel
    }
    uint64_t const ip = m_cst.csa.isa[i];
    uint64_t const jp = m_cst.csa.isa[j];

    return m_cst.depth(m_cst.node(std::min(ip, jp), std::max(ip, jp)));
  }

  size_t size() { return m_size; }

 private:
  size_t m_size;
  sdsl::cst_sada<> m_cst;
};
}  // namespace lce::ds