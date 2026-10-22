/*******************************************************************************
 * bench/gen-sss.cpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/


#include <filesystem>
#include <fstream>
#include <iostream>
#include <misc/packed_integers.hpp>
#include <misc/files.hpp>
#include <misc/memory.hpp>
#include <string>
#include <tlx/cmdline_parser.hpp>

#include "format_list.hpp"
#include "sss/string_synchronizing_set.hpp"

namespace fs = std::filesystem;

template <typename sss_t>
static void write_sss(const fs::path& path, const sss_t& sss) {
  std::vector<uint40_t> positions(sss.size());
  for (size_t i = 0; i < positions.size(); ++i) positions[i] = sss[i];
  std::ofstream out(path, std::ios::binary);
  write_in_chunks(out, (const char*) positions.data(), positions.size() * sizeof(uint40_t));
}

int main(int argc, char** argv) {
  std::vector<std::string> algorithms{"all", "sss256", "sss512", "sss1024",
                                      "sss2048"};

  std::filesystem::path text_path;
  no_init_vector<uint8_t> text;
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
      "Which string synchronizing set (tau) to generate. Options: " + format_list(algorithms));
  cp.add_path('o', "output_folder", output_path, "The output folder.");
  if (!cp.process(argc, argv)) {
    std::exit(EXIT_FAILURE);
  }

  // Check parameters
  {
    if (!fs::is_regular_file(text_path) || fs::file_size(text_path) == 0) {
      std::cout << "Text file " << text_path.string() << " is empty or does not exist.\n";
      return -1;
    }
    if (std::find(algorithms.begin(), algorithms.end(), algorithm) ==
        algorithms.end()) {
      std::cout << "Algorithm " << algorithm << " is not specified.\n Use one of "
                << format_list(algorithms) << "\n";
      return -1;
    }
    if(!output_path.empty() && !fs::is_directory(output_path)) {
      std::cout << "Folder " << output_path.string() << " does not exist.\n";
      return -1;
    }
  }

  const uint64_t size = fs::file_size(text_path);
  text.reserve(size + 4096 * 4);
  resize_no_init(text, size);
  std::ifstream in(text_path, std::ios::binary);
  read_in_chunks(in, (char*) text.data(), size);

  if(output_path == "") {
    output_path = text_path;
    output_path += ".sss";
  } else {
    output_path /= text_path.filename();
    output_path += ".sss";
  }
  
  if (algorithm == "sss256" || algorithm == "all") {
    output_path.replace_extension("sss256");
    lce::sss::string_synchronizing_set<uint40_t> sss(text, uint64_t(256));
    write_sss(output_path, sss);
  }
  if (algorithm == "sss512" || algorithm == "all") {
    output_path.replace_extension("sss512");
    lce::sss::string_synchronizing_set<uint40_t> sss(text, uint64_t(512));
    write_sss(output_path, sss);
  }
  if (algorithm == "sss1024" || algorithm == "all") {
    output_path.replace_extension(".sss1024");
    lce::sss::string_synchronizing_set<uint40_t> sss(text, uint64_t(1024));
    write_sss(output_path, sss);
  }
  if (algorithm == "sss2048" || algorithm == "all") {
    output_path.replace_extension(".sss2048");
    lce::sss::string_synchronizing_set<uint40_t> sss(text, uint64_t(2048));
    write_sss(output_path, sss);
  }
  return 0;
}
