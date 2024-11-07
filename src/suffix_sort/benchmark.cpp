/*******************************************************************************
 * src/suffix_sorting/benchmark.cpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include <fmt/core.h>
#include <fmt/ranges.h>
#ifdef ALX_BENCHMARK_SPACE
#include <malloc_count/malloc_count.h>
#endif

#include <omp.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <tlx/cmdline_parser.hpp>
#include <vector>

#include "lce/lce_fp.hpp"
#include "lce/lce_memcmp.hpp"
#include "lce/lce_naive.hpp"
#include "lce/lce_naive_std.hpp"
#include "lce/lce_naive_wordwise.hpp"
#include "util/io.hpp"
#include "util/timer.hpp"

namespace fs = std::filesystem;

std::vector<std::string> algorithms{
    "all",  "naive", "naive_std", "naive_wordwise", "naive_memcmp", "fp8",
    "fp16", "fp32",  "fp64",      "fp128",          "fp256",        "fp512"};

class benchmark {
 public:
  fs::path text_path;
  std::vector<uint8_t> text;

  std::vector<size_t> sa;
  size_t sample_rate{1};

  std::string algorithm = "naive";

  bool check_parameters() {
    // Check text path
    if (!fs::is_regular_file(text_path) || fs::file_size(text_path) == 0) {
      fmt::print("Text file {} is empty or does not exist.\n",
                 text_path.string());
      return false;
    }

    // Check algorithm flag
    if (std::find(algorithms.begin(), algorithms.end(), algorithm) ==
        algorithms.end()) {
      fmt::print("Algorithm {} is not specified.\n Use one of {}\n", algorithm,
                 algorithms);
      return false;
    }
    return true;
  }

  void load_text() {
    alx::util::timer t;
    text = alx::util::load_vector<uint8_t>(text_path,
      std::numeric_limits<size_t>::max(), 4096 * 4);
    text.resize(text.size() +
                ((text.size() % 8) == 0 ? 0 : 8 - (text.size() % 8)));
    assert(text.size() != 0);
    fmt::print(" text={}", text_path.filename().string());
    fmt::print(" text_size={}", text.size());
    fmt::print(" text_time={}", t.get());
  }

  template <typename lce_ds_type>
  lce_ds_type benchmark_construction() {
#ifdef ALX_BENCHMARK_SPACE
    malloc_count_reset_peak();
    size_t mem_before = malloc_count_current();
#endif
    alx::util::timer t;
    lce_ds_type lce_ds(text);
    fmt::print(" threads={}", omp_get_max_threads());
    fmt::print(" c_time={}", t.get());
#ifdef ALX_BENCHMARK_SPACE
    fmt::print(" c_mem={}", malloc_count_current() - mem_before);
    fmt::print(" c_mempeak={}", malloc_count_peak() - mem_before);
#endif
    return lce_ds;
  }

  void load_sa() {
    sa.clear();
    alx::util::timer t;
    for (size_t i{sample_rate}; i < text.size(); i += sample_rate) {
      sa.push_back(i);
    }
    assert(sa.size() == text.size() / sample_rate);
    fmt::print(" sample_rate={}", sample_rate);
    fmt::print(" sa_size={}", sa.size());
    fmt::print(" sa_load_time={}", t.get());
  }

  template <typename lce_ds_type>
  void benchmark_ss() {
    if (sa.empty()) {
      return;
    }

    alx::util::timer t;
    lce_ds_type lce_ds = benchmark_construction<lce_ds_type>();

    std::sort(sa.begin(), sa.end(), [&lce_ds](size_t i, size_t j) {
      return lce_ds.is_leq_suffix(i, j);
    });

    fmt::print(" ss_time={}", t.get());

    size_t check_sum = 0;
    for (size_t i{1}; i < sa.size(); ++i) {
      check_sum += sa[i - 1] - sa[i];
    }
    fmt::print(" check_sum={}", check_sum);
  }

  template <typename lce_ds_type>
  void run(std::string const& algo_name) {
    if (algo_name != algorithm && algorithm != "all") {
      return;
    }

    // Benchmark suffix sorting
    fmt::print("RESULT algo={}", algo_name);
    load_text();
    load_sa();
    benchmark_ss<lce_ds_type>();
    fmt::print("\n");
  }
};

int main(int argc, char** argv) {
  benchmark b;

  tlx::CmdlineParser cp;
  cp.set_description(
      "This program measures suffix sorting with several LCE data "
      "structures. ");
  cp.set_author("Alexander Herlez <alexander.herlez@tu-dortmund.de>");

  cp.add_param_path("text_path", b.text_path, "The path to the text");
  cp.add_bytes('s', "sample_rate", b.sample_rate,
               "Sort every s'th sample. (default = 1)");

  cp.add_string('a', "algorithm", b.algorithm,
                fmt::format("Algorithm for string sorting: {}", algorithms));

  if (!cp.process(argc, argv)) {
    std::exit(EXIT_FAILURE);
  }

  if (!b.check_parameters()) {
    return -1;
  }

  b.run<alx::lce::lce_naive<>>("naive");
  b.run<alx::lce::lce_naive_std<>>("naive_std");
  b.run<alx::lce::lce_naive_wordwise<>>("naive_wordwise");
  b.run<alx::lce::lce_memcmp>("naive_memcmp");

  b.run<alx::lce::lce_fp<uint8_t, 8>>("fp8");
  b.run<alx::lce::lce_fp<uint8_t, 16>>("fp16");
  b.run<alx::lce::lce_fp<uint8_t, 32>>("fp32");
  b.run<alx::lce::lce_fp<uint8_t, 64>>("fp64");
  b.run<alx::lce::lce_fp<uint8_t, 128>>("fp128");
  b.run<alx::lce::lce_fp<uint8_t, 256>>("fp256");
  b.run<alx::lce::lce_fp<uint8_t, 512>>("fp512");

  // b.run<alx::lce::lce_sss<uint8_t, 512>>("fp512");
}