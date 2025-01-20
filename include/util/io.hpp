/*******************************************************************************
 * lce/util/io.hpp
 *
 * Copyright (C) 2019 Florian Kurpicz <florian.kurpicz@tu-dortmund.de>
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once
#include <fmt/core.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace lce::util {
namespace fs = std::filesystem;

template <typename t_char_type>
std::vector<t_char_type> load_vector(
    fs::path file_path, size_t prefix_size = std::numeric_limits<size_t>::max(),
    size_t excess = 0, size_t block_size = 1) {
  if (!fs::is_regular_file(file_path)) {
    fmt::print("Text file {} does not exist.\n", file_path.string());
    return std::vector<t_char_type>();
  }

  prefix_size =
      std::min(prefix_size, fs::file_size(file_path) / sizeof(t_char_type));
  size_t vector_len = prefix_size;
  vector_len += ((vector_len % block_size) == 0)
                    ? 0
                    : block_size - (vector_len % block_size);

  size_t bytes_to_read = prefix_size * sizeof(t_char_type);

  std::vector<t_char_type> vec(vector_len + excess);
  vec.resize(vector_len);

  std::ifstream stream(file_path, std::ios::in | std::ios::binary);
  stream.read((char*)vec.data(), bytes_to_read);
  return vec;
}

template <typename C>
void write_vector(fs::path file_path, C container,
                  size_t prefix_size = std::numeric_limits<size_t>::max()) {
  prefix_size = std::min(prefix_size, container.size());
  size_t bytes_to_write = prefix_size * sizeof(typename C::value_type);

  std::ofstream stream(file_path, std::ios::out | std::ios::binary);
  stream.write((char*)container.data(), bytes_to_write);
}

}  // namespace lce::util