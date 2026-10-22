/*******************************************************************************
 * bench/format_list.hpp
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <string>
#include <vector>

// returns the strings as a list of quoted strings, e.g. ["a", "b"]
inline std::string format_list(std::vector<std::string> const& strings) {
  std::string list = "[";
  for (size_t i = 0; i < strings.size(); ++i) {
    if (i > 0) list += ", ";
    list += '"' + strings[i] + '"';
  }
  return list + "]";
}
