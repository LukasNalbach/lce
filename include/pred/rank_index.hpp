#pragma once

#include "../bit_vector/bit_access.hpp"
#include "../bit_vector/bit_rank.hpp"
#include "../bit_vector/bit_select.hpp"
#include "../bit_vector/util.hpp"

#include <assert.h>
#include <algorithm>
#include <cstddef>
#include <iterator>

#include "pred_result.hpp"

namespace lce::pred {

template<typename T>
class rank_index {
public:
  inline rank_index(T const* data, size_t size)
    : m_data(data), m_size(size), m_min(data[0]), m_max(data[size - 1]) {
    assert(std::is_sorted(m_data, m_data + size));
    m_bv = stash::bit_vector(m_max - m_min + 1);

    for(size_t i = 0; i < m_size; i++) {
      m_bv[data[i] - m_min] = 1;
    }

    assert(m_bv[0] == 1);
    assert(m_bv[m_max - m_min] == 1);

    m_rank = stash::bit_rank(m_bv);
  }

  template <typename C>
  rank_index(C const& container)
      : rank_index(container.data(), container.size()) {
  }

  // finds the greatest element less than OR equal to x
  inline result predecessor(T x) const {
    if(unlikely(x < m_min))  return result { false, 0 };
    if(unlikely(x >= m_max)) return result { true, m_size-1 };

    const size_t p = m_rank(x - m_min);
    assert(p > 0);
    return result { true, p - 1 };
  }

  // finds the smallest element greater than OR equal to x
  inline result successor(T x) const {
    if(unlikely(x <= m_min))  return result { true, 0 };
    if(unlikely(x > m_max)) return result { false, 0 };

    const size_t p = m_rank(x - m_min);
    assert(p > 0);
    return result { true, p - 1 + (x > m_data[p - 1]) };
  }
  
private:
  const T* m_data;
  size_t m_size;
  T m_min;
  T m_max;
  stash::bit_vector m_bv;
  stash::bit_rank m_rank;
};
};  // namespace lce::pred