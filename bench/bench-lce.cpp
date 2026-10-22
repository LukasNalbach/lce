/*******************************************************************************
 * bench/bench-lce.cpp
 *
 * Copyright (C) 2023 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifdef LCE_BENCHMARK_SPACE
#include <malloc_count.h>
#endif

#include <omp.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <misc/packed_integers.hpp>
#include <misc/bits.hpp>
#include <misc/files.hpp>
#include <misc/log.hpp>
#include <misc/memory.hpp>
#include <iostream>
#include <string>
#include <tlx/cmdline_parser.hpp>
#include <vector>

#ifdef LCE_USE_SDSL
#include "ds/lce_sdsl_cst.hpp"
#endif

#include "format_list.hpp"
#include "ds/lce_classic.hpp"
#include "ds/lce_fp.hpp"
#include "ds/lce_naive.hpp"
#include "ds/lce_naive_std.hpp"
#include "ds/lce_naive_wordwise.hpp"
#include "ds/lce_naive_wordwise_xor.hpp"
#include "ds/lce_rk_prezza.hpp"
#include "ds/lce_sss.hpp"
#include "ds/lce_sss_naive.hpp"
#include "ds/lce_sss_noss.hpp"

namespace fs = std::filesystem;

std::vector<std::string> algorithms{"naive",
                                    "naive_std",
                                    "naive_wordwise",
                                    "naive_wordwise_xor",
                                    "fp64",
                                    "fp128",
                                    "fp256",
                                    "fp512",
                                    "fp_unlimited",
                                    "rk-prezza",
                                    "sss_naive256",
                                    "sss_naive512",
                                    "sss_naive1024",
                                    "sss_naive2048",
                                    "sss_naive256pl",
                                    "sss_naive512pl",
                                    "sss_naive1024pl",
                                    "sss_naive2048pl",
                                    "sss_noss256",
                                    "sss_noss512",
                                    "sss_noss1024",
                                    "sss_noss2048",
                                    "sss_noss256pl",
                                    "sss_noss512pl",
                                    "sss_noss1024pl",
                                    "sss_noss2048pl",
                                    "sss256",
                                    "sss512",
                                    "sss1024",
                                    "sss2048",
                                    "sss256pl",
                                    "sss512pl",
                                    "sss1024pl",
                                    "sss2048pl",
                                    "classic",
                                    "sdsl_cst"};

std::vector<std::string> algorithm_sets{"all", "naive", "par", "main"};

std::vector<std::string> algorithms_naive{"naive", "naive_std", "naive_wordwise",
                                          "naive_wordwise_xor"};

std::vector<std::string> algorithms_par{
    "fp64",           "fp128",          "fp256",           "fp512",
    "sss_naive256",   "sss_naive512",   "sss_naive1024",   "sss_naive2048",
    "sss_naive256pl", "sss_naive512pl", "sss_naive1024pl", "sss_naive2048pl",
    "sss_noss256",    "sss_noss512",    "sss_noss1024",    "sss_noss2048",
    "sss_noss256pl",  "sss_noss512pl",  "sss_noss1024pl",  "sss_noss2048pl",
    "sss256",         "sss512",         "sss1024",         "sss2048",
    "sss256pl",       "sss512pl",       "sss1024pl",       "sss2048pl",
};

std::vector<std::string> algorithms_main{
    "naive_wordwise_xor", "fp64",          "sss_naive512", "sss_naive512pl",
    "sss_noss512",        "sss_noss512pl", "sss512",       "sss512pl"};

class benchmark {
 public:
  fs::path text_path;
  std::vector<uint8_t> text;

  fs::path queries_path;
  no_init_vector<size_t> queries;
  size_t num_queries = size_t{1'000'000};
  size_t lce_from = 0;
  size_t lce_to = 20;

  std::string algorithm = "naive";

  bool check_parameters() {
    // Check text path
    if (!fs::is_regular_file(text_path) || fs::file_size(text_path) == 0) {
      std::cout << "Text file " << text_path.string() << " is empty or does not exist.\n";
      return false;
    }

    // Check queries path
    if (queries_path.empty()) {
      queries_path = text_path;
      queries_path.remove_filename();
    }
    if (!fs::is_directory(queries_path)) {
      std::cout << "Query directory " << queries_path.string() << " does not exist.\n";
      return false;
    }
    for (size_t i = lce_from; i < lce_to; ++i) {
      fs::path q_path = queries_path;
      q_path.append("lce_" + std::to_string(i));
      if (!fs::is_regular_file(q_path)) {
        std::cout << "Query file " << q_path.string() << " does not exist.\n";
        return false;
      }
    }

    // Check algorithm
    if (std::find(algorithms.begin(), algorithms.end(), algorithm) ==
            algorithms.end() &&
        std::find(algorithm_sets.begin(), algorithm_sets.end(), algorithm) ==
            algorithm_sets.end()) {
      std::cout << "Algorithm " << algorithm << " is not specified.\n Use one of "
                << format_list(algorithms) << " or " << format_list(algorithm_sets) << "\n";
      return false;
    }
    return true;
  }

  void load_text() {
    auto t = now();
    if (text.empty()) {
      // the text, padded with zeros to a multiple of 8 bytes
      const uint64_t size = fs::file_size(text_path);
      text.reserve(div_ceil<uint64_t>(size, 8) * 8 + 4096 * 4);
      text.resize(div_ceil<uint64_t>(size, 8) * 8);
      std::ifstream in(text_path, std::ios::binary);
      read_in_chunks(in, (char*) text.data(), size);
      assert(text.size() != 0);
      assert(text.size() % 8 == 0);
    }
    std::cout << " text=" << text_path.filename().string();
    std::cout << " text_size=" << text.size();
    std::cout << " text_time=" << time_diff_ns(t) / 1'000'000;
  }

  template <typename ds_type, typename... ctor_args_t>
  ds_type benchmark_construction(ctor_args_t... ctor_args) {
#ifdef LCE_BENCHMARK_SPACE
    malloc_count_reset_peak();
    size_t mem_before = malloc_count_current();
#endif
    auto t = now();
    ds_type ds(text, ctor_args...);
    std::cout << " threads=" << omp_get_max_threads();
    std::cout << " c_time=" << time_diff_ns(t) / 1'000'000;
#ifdef LCE_BENCHMARK_SPACE
    std::cout << " c_mem=" << malloc_count_current() - mem_before;
    std::cout << " c_mempeak=" << malloc_count_peak() - mem_before;
#endif
    return ds;
  }

  void load_queries(size_t lce_cur) {
    auto t = now();
    // First load queries from file
    fs::path cur_query_path = queries_path;
    cur_query_path.append("lce_" + std::to_string(lce_cur));
    resize_no_init(queries, fs::file_size(cur_query_path) / sizeof(size_t));
    std::ifstream in(cur_query_path, std::ios::binary);
    read_in_chunks(in, (char*) queries.data(), queries.size() * sizeof(size_t));

    // Now clone queries until there are num_queries many
    if (queries.size() != 0) {
      size_t num_unique_queries = queries.size();
      assert(num_unique_queries % 2 == 0);
      queries.resize(num_queries * 2);
      for (size_t i = num_unique_queries; i < queries.size(); ++i) {
        queries[i] = queries[i % num_unique_queries];
      }
    }

    assert(queries.size() == 0 || queries.size() == num_queries * 2);
    std::cout << " q_size=" << queries.size() / 2;
    std::cout << " q_load_time=" << time_diff_ns(t) / 1'000'000;
  }

  template <typename ds_type>
  void benchmark_queries(ds_type& ds) {
    if (queries.empty()) {
      return;
    }
    size_t check_sum = 0;
    auto t = now();
    for (size_t i = 0; i < queries.size(); i += 2) {
      check_sum += ds.lce(queries[i], queries[i + 1]);
    }

    std::cout << " q_time=" << time_diff_ns(t) / 1'000'000;
    std::cout << " check_sum=" << check_sum;
  }

  template <typename ds_type, typename... ctor_args_t>
  void run(std::string const& algo_name, ctor_args_t... ctor_args) {
    if (algorithm == "main") {
      if (std::find(algorithms_main.begin(), algorithms_main.end(),
                    algo_name) == algorithms_main.end()) {
        return;
      }
    } else if (algorithm == "naive") {
      if (std::find(algorithms_naive.begin(), algorithms_naive.end(), algo_name) ==
          algorithms_naive.end()) {
        return;
      }
    } else if (algorithm == "par") {
      if (std::find(algorithms_par.begin(), algorithms_par.end(), algo_name) ==
          algorithms_par.end()) {
        return;
      }
    } else if (algorithm == "all") {
      // OK
    } else {
      if (algo_name != algorithm) {
        return;
      }
    }

    // Benchmark construction
    std::cout << "RESULT algo=" << algo_name;

    if (algo_name == "sdsl_cst") {
      std::string text_path_string = text_path.string();
      text = std::vector<uint8_t>(text_path_string.begin(), text_path_string.end());
      std::cout << " text=" << text_path.filename().string();
      std::cout << " text_size=" << std::filesystem::file_size(text_path);
    } else {
      load_text();
    }

    ds_type ds = benchmark_construction<ds_type>(ctor_args...);
    std::cout << "\n";

    // Benchmark queries
    size_t lce_cur = lce_from;
    while (lce_cur < lce_to) {
      std::cout << "RESULT algo=" << algo_name << "_queries";
      std::cout << " text=" << text_path.filename().string();
      std::cout << " lce_range=" << lce_cur;
      load_queries(lce_cur);
      benchmark_queries<ds_type>(ds);
      std::cout << "\n";
      ++lce_cur;
    }
  }
};


int main(int argc, char** argv) {
  benchmark b;

  tlx::CmdlineParser cp;
  cp.set_description(
      "This program measures construction time and LCE query time for "
      "several "
      "LCE data structures. Generate LCE queries with gen-queries");
  cp.set_author(
      "Alexander Herlez <alexander.herlez@tu-dortmund.de>\n"
      "        Florian Kurpicz  <florian.kurpicz@tu-dortmund.de>\n"
      "        Patrick Dinklage <patrick.dinklage@tu-dortmund.de>");

  cp.add_param_path("text_path", b.text_path,
                    "The path to the text which is queried");
  cp.add_path("queries_path", b.queries_path,
              "The path to the generated queries (default="
              "text_path.remove_filename()).");
  cp.add_bytes('q', "num_queries", b.num_queries,
               "Number of LCE queries that are executed (default=1,000,000).");
  cp.add_bytes(
      "from", b.lce_from,
      "Use only lce queries whose result has bit width >= from, i.e. is >= 2^{from-1} (default=0).");
  cp.add_bytes(
      "to", b.lce_to,
      "Use only lce queries whose result has bit width < to, i.e. is < 2^{to-1} (default=20).");

  cp.add_string(
      'a', "algorithm", b.algorithm,
      "Name of data structure which is benchmarked. Options: " + format_list(algorithms));
  if (!cp.process(argc, argv)) {
    std::exit(EXIT_FAILURE);
  }
  if (!b.check_parameters()) {
    return -1;
  }

  using namespace lce::ds;

  b.run<lce_naive<>>("naive");
  b.run<lce_naive_std<>>("naive_std");
  b.run<lce_naive_wordwise<>>("naive_wordwise");
  b.run<lce_naive_wordwise_xor<>>("naive_wordwise_xor");

  b.run<lce_fp<uint8_t, 64>>("fp64");
  b.run<lce_fp<uint8_t, 128>>("fp128");
  b.run<lce_fp<uint8_t, 256>>("fp256");
  b.run<lce_fp<uint8_t, 512>>("fp512");
  b.run<lce_fp<uint8_t, (size_t{1} << 40)>>("fp_unlimited");
  b.run<rklce::lce_rk_prezza>("rk-prezza");

  b.run<lce_sss_naive<uint8_t, uint40_t, false>>("sss_naive256", uint64_t(256));
  b.run<lce_sss_naive<uint8_t, uint40_t, false>>("sss_naive512", uint64_t(512));
  b.run<lce_sss_naive<uint8_t, uint40_t, false>>("sss_naive1024", uint64_t(1024));
  b.run<lce_sss_naive<uint8_t, uint40_t, false>>("sss_naive2048", uint64_t(2048));
  b.run<lce_sss_naive<uint8_t, uint40_t, true>>("sss_naive256pl", uint64_t(256));
  b.run<lce_sss_naive<uint8_t, uint40_t, true>>("sss_naive512pl", uint64_t(512));
  b.run<lce_sss_naive<uint8_t, uint40_t, true>>("sss_naive1024pl", uint64_t(1024));
  b.run<lce_sss_naive<uint8_t, uint40_t, true>>("sss_naive2048pl", uint64_t(2048));

  b.run<lce_sss_noss<uint8_t, uint40_t, false>>("sss_noss256", uint64_t(256));
  b.run<lce_sss_noss<uint8_t, uint40_t, false>>("sss_noss512", uint64_t(512));
  b.run<lce_sss_noss<uint8_t, uint40_t, false>>("sss_noss1024", uint64_t(1024));
  b.run<lce_sss_noss<uint8_t, uint40_t, false>>("sss_noss2048", uint64_t(2048));
  b.run<lce_sss_noss<uint8_t, uint40_t, true>>("sss_noss256pl", uint64_t(256));
  b.run<lce_sss_noss<uint8_t, uint40_t, true>>("sss_noss512pl", uint64_t(512));
  b.run<lce_sss_noss<uint8_t, uint40_t, true>>("sss_noss1024pl", uint64_t(1024));
  b.run<lce_sss_noss<uint8_t, uint40_t, true>>("sss_noss2048pl", uint64_t(2048));

  b.run<lce_sss<uint8_t, uint40_t, false>>("sss256", uint64_t(256));
  b.run<lce_sss<uint8_t, uint40_t, false>>("sss512", uint64_t(512));
  b.run<lce_sss<uint8_t, uint40_t, false>>("sss1024", uint64_t(1024));
  b.run<lce_sss<uint8_t, uint40_t, false>>("sss2048", uint64_t(2048));
  b.run<lce_sss<uint8_t, uint40_t, true>>("sss256pl", uint64_t(256));
  b.run<lce_sss<uint8_t, uint40_t, true>>("sss512pl", uint64_t(512));
  b.run<lce_sss<uint8_t, uint40_t, true>>("sss1024pl", uint64_t(1024));
  b.run<lce_sss<uint8_t, uint40_t, true>>("sss2048pl", uint64_t(2048));

  b.run<lce_classic<uint8_t, uint40_t>>("classic");

#ifdef LCE_USE_SDSL
  b.run<lce_sdsl_cst>("sdsl_cst");
#endif
}