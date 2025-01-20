/*******************************************************************************
 * src/pred/gen_sss.cpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <gsaca-double-sort/uint_types.hpp>
#include <tlx/cmdline_parser.hpp>

#include "rolling_hash/string_synchronizing_set.hpp"
#include "util/io.hpp"

namespace fs = std::filesystem;

namespace std {
template <>
struct hash<gsaca_lyndon::uint40_t> {
  auto operator()(const gsaca_lyndon::uint40_t& xyz) const -> size_t {
    return hash<uint64_t>{}(xyz.u64());
  }
};
}  // namespace std

int main(int argc, char** argv) {
  std::vector<std::string> algorithms{"all", "sss256", "sss512", "sss1024",
                                      "sss2048"};

  std::filesystem::path text_path;
  std::vector<uint8_t> text;
  std::filesystem::path output_path;
  std::string algorithm{"sss512"};

  tlx::CmdlineParser cp;
  cp.set_description(
      "This program writes the string synchronizing set of a text to disk. "
      "This can be used as input for predecessor data structures.");
  cp.set_author("Alexander Herlez <alexander.herlez@tu-dortmund.de>");

  cp.add_param_path("text_path", text_path, "The path to the text.");
  cp.add_string(
      'a', "algorithm", algorithm,
      fmt::format("Name of data structure which is benchmarked. Options: {}",
                  algorithms));
  cp.add_path('o', "output_folder", output_path, "The output folder.");
  if (!cp.process(argc, argv)) {
    std::exit(EXIT_FAILURE);
  }

  // Check parameters
  {
    if (!fs::is_regular_file(text_path) || fs::file_size(text_path) == 0) {
      fmt::print("Text file {} is empty or does not exist.\n",
                 text_path.string());
      return -1;
    }
    if (std::find(algorithms.begin(), algorithms.end(), algorithm) ==
        algorithms.end()) {
      fmt::print("Algorithm {} is not specified.\n Use one of {}\n", algorithm,
                 algorithms);
      return -1;
    }
    if(!output_path.empty() && !fs::is_directory(output_path)) {
      fmt::print("Folder {} does not exist.\n",
                 output_path.string());
      return -1;
    }
  }

  text = lce::util::load_vector<uint8_t>(text_path,
    std::numeric_limits<size_t>::max(), 4096 * 4);
  using gsaca_lyndon::uint40_t;

  if(output_path == "") {
    output_path = text_path;
    output_path += ".sss";
  } else {
    output_path /= text_path.filename();
    output_path += ".sss";
  }
  
  if (algorithm == "sss256" || algorithm == "all") {
    output_path.replace_extension("sss256");
    lce::rolling_hash::sss<uint40_t, 256> sss(text);
    lce::util::write_vector(output_path, sss.get_sss());
  }
  if (algorithm == "sss512" || algorithm == "all") {
    output_path.replace_extension("sss512");
    lce::rolling_hash::sss<uint40_t, 512> sss(text);
    lce::util::write_vector(output_path, sss.get_sss());
  }
  if (algorithm == "sss1024" || algorithm == "all") {
    output_path.replace_extension(".sss1024");
    lce::rolling_hash::sss<uint40_t, 1024> sss(text);
    lce::util::write_vector(output_path, sss.get_sss());
  }
  if (algorithm == "sss2048" || algorithm == "all") {
    output_path.replace_extension(".sss2048");
    lce::rolling_hash::sss<uint40_t, 2048> sss(text);
    lce::util::write_vector(output_path, sss.get_sss());
  }
  return 0;
}
