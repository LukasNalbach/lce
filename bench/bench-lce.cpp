/*******************************************************************************
 * lce/ds/benchmark.cpp
 *
 * Copyright (C) 2023 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include <fmt/core.h>
#include <fmt/ranges.h>
#ifdef LCE_BENCHMARK_SPACE
#include <malloc_count/malloc_count.h>
#endif

#include <omp.h>

#include <algorithm>
#include <filesystem>
#include <gsaca-double-sort/uint_types.hpp>
#include <iostream>
#include <string>
#include <tlx/cmdline_parser.hpp>
#include <vector>

#ifdef LCE_USE_SDSL
#include "ds/lce_sdsl_cst.hpp"
#endif

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
#include "util/io.hpp"
#include "util/timer.hpp"

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
  std::vector<size_t> queries;
  size_t num_queries = size_t{1'000'000};
  size_t lce_from = 0;
  size_t lce_to = 20;

  std::string algorithm = "naive";

  bool check_parameters() {
    // Check text path
    if (!fs::is_regular_file(text_path) || fs::file_size(text_path) == 0) {
      fmt::print("Text file {} is empty or does not exist.\n",
                 text_path.string());
      return false;
    }

    // Check queries path
    if (queries_path.empty()) {
      queries_path = text_path;
      queries_path.remove_filename();
    }
    if (!fs::is_directory(queries_path)) {
      fmt::print("Query directory {} does not exist.\n", queries_path.string());
      return false;
    }
    for (size_t i = lce_from; i < lce_to; ++i) {
      fs::path q_path = queries_path;
      q_path.append(fmt::format("lce_{}", i));
      if (!fs::is_regular_file(q_path)) {
        fmt::print("Query file {} does not exist.\n", queries_path.string());
        return false;
      }
    }

    // Check algorithm
    if (std::find(algorithms.begin(), algorithms.end(), algorithm) ==
            algorithms.end() &&
        std::find(algorithm_sets.begin(), algorithm_sets.end(), algorithm) ==
            algorithm_sets.end()) {
      fmt::print("Algorithm {} is not specified.\n Use one of {} or {}\n",
                 algorithm, algorithms, algorithm_sets);
      return false;
    }
    return true;
  }

  void load_text() {
    lce::util::timer t;
    if (text.empty()) {
      text = lce::util::load_vector<uint8_t>(
          text_path, std::numeric_limits<size_t>::max(), 4096 * 4, 8);
      assert(text.size() != 0);
      assert(text.size() % 8 == 0);
    }
    fmt::print(" text={}", text_path.filename().string());
    fmt::print(" text_size={}", text.size());
    fmt::print(" text_time={}", t.get());
  }

  template <typename ds_type>
  ds_type benchmark_construction() {
#ifdef LCE_BENCHMARK_SPACE
    malloc_count_reset_peak();
    size_t mem_before = malloc_count_current();
#endif
    lce::util::timer t;
    ds_type ds(text);
    fmt::print(" threads={}", omp_get_max_threads());
    fmt::print(" c_time={}", t.get());
#ifdef LCE_BENCHMARK_SPACE
    fmt::print(" c_mem={}", malloc_count_current() - mem_before);
    fmt::print(" c_mempeak={}", malloc_count_peak() - mem_before);
#endif
    return ds;
  }

  void load_queries(size_t lce_cur) {
    lce::util::timer t;
    // First load queries from file
    fs::path cur_query_path = queries_path;
    cur_query_path.append(fmt::format("lce_{}", lce_cur));
    queries = lce::util::load_vector<size_t>(cur_query_path);

    // Now clone queries until there are num_unique_queries many
    if (queries.size() != 0) {
      size_t num_unique_queries = queries.size();
      assert(num_unique_queries % 2 == 0);
      queries.resize(num_queries * 2);
      for (size_t i = num_unique_queries; i < queries.size(); ++i) {
        queries[i] = queries[i % num_unique_queries];
      }
    }

    assert(queries.size() == 0 || queries.size() == num_queries * 2);
    fmt::print(" q_size={}", queries.size() / 2);
    fmt::print(" q_load_time={}", t.get());
  }

  template <typename ds_type>
  void benchmark_queries(ds_type& ds) {
    if (queries.empty()) {
      return;
    }
    size_t check_sum = 0;
    lce::util::timer t;
    for (size_t i = 0; i < queries.size(); i += 2) {
      check_sum += ds.lce(queries[i], queries[i + 1]);
    }

    fmt::print(" q_time={}", t.get());
    fmt::print(" check_sum={}", check_sum);
  }

  template <typename ds_type>
  void run(std::string const& algo_name) {
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
    fmt::print("RESULT algo={}", algo_name);

    if (algo_name == "sdsl_cst") {
      std::string text_path_string = text_path.string();
      text = std::vector<uint8_t>(text_path_string.begin(), text_path_string.end());
      fmt::print(" text={}", text_path.filename().string());
      fmt::print(" text_size={}", std::filesystem::file_size(text_path));
    } else {
      load_text();
    }

    ds_type ds = benchmark_construction<ds_type>();
    fmt::print("\n");

    // Benchmark queries
    size_t lce_cur = lce_from;
    while (lce_cur < lce_to) {
      fmt::print("RESULT algo={}_queries", algo_name);
      fmt::print(" text={}", text_path.filename().string());
      fmt::print(" lce_range={}", lce_cur);
      load_queries(lce_cur);
      benchmark_queries<ds_type>(ds);
      fmt::print("\n");
      ++lce_cur;
    }
  }
};

namespace std {
template <>
struct hash<gsaca_lyndon::uint40_t> {
  auto operator()(const gsaca_lyndon::uint40_t& xyz) const -> size_t {
    return hash<uint64_t>{}(xyz.u64());
  }
};
}  // namespace std

int main(int argc, char** argv) {
  benchmark b;

  tlx::CmdlineParser cp;
  cp.set_description(
      "This program measures construction time and LCE query time for "
      "several "
      "LCE data structures. Generate LCE queries with gen_queries");
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
      "Use only lce queries which return at least 2^{from}  (default=0).");
  cp.add_bytes(
      "to", b.lce_to,
      "Use only lce queries which return up to 2^{to}-1 with (default=21)");

  cp.add_string(
      'a', "algorithm", b.algorithm,
      fmt::format("Name of data structure which is benchmarked. Options: {}",
                  algorithms));
  if (!cp.process(argc, argv)) {
    std::exit(EXIT_FAILURE);
  }
  if (!b.check_parameters()) {
    return -1;
  }

  using namespace lce::ds;
  using gsaca_lyndon::uint40_t;

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

  b.run<lce_sss_naive<uint8_t, 256, uint40_t, false>>("sss_naive256");
  b.run<lce_sss_naive<uint8_t, 512, uint40_t, false>>("sss_naive512");
  b.run<lce_sss_naive<uint8_t, 1024, uint40_t, false>>("sss_naive1024");
  b.run<lce_sss_naive<uint8_t, 2048, uint40_t, false>>("sss_naive2048");
  b.run<lce_sss_naive<uint8_t, 256, uint40_t, true>>("sss_naive256pl");
  b.run<lce_sss_naive<uint8_t, 512, uint40_t, true>>("sss_naive512pl");
  b.run<lce_sss_naive<uint8_t, 1024, uint40_t, true>>("sss_naive1024pl");
  b.run<lce_sss_naive<uint8_t, 2048, uint40_t, true>>("sss_naive2048pl");

  b.run<lce_sss_noss<uint8_t, 256, uint40_t, false>>("sss_noss256");
  b.run<lce_sss_noss<uint8_t, 512, uint40_t, false>>("sss_noss512");
  b.run<lce_sss_noss<uint8_t, 1024, uint40_t, false>>("sss_noss1024");
  b.run<lce_sss_noss<uint8_t, 2048, uint40_t, false>>("sss_noss2048");
  b.run<lce_sss_noss<uint8_t, 256, uint40_t, true>>("sss_noss256pl");
  b.run<lce_sss_noss<uint8_t, 512, uint40_t, true>>("sss_noss512pl");
  b.run<lce_sss_noss<uint8_t, 1024, uint40_t, true>>("sss_noss1024pl");
  b.run<lce_sss_noss<uint8_t, 2048, uint40_t, true>>("sss_noss2048pl");

  b.run<lce_sss<uint8_t, 256, uint40_t, false>>("sss256");
  b.run<lce_sss<uint8_t, 512, uint40_t, false>>("sss512");
  b.run<lce_sss<uint8_t, 1024, uint40_t, false>>("sss1024");
  b.run<lce_sss<uint8_t, 2048, uint40_t, false>>("sss2048");
  b.run<lce_sss<uint8_t, 256, uint40_t, true>>("sss256pl");
  b.run<lce_sss<uint8_t, 512, uint40_t, true>>("sss512pl");
  b.run<lce_sss<uint8_t, 1024, uint40_t, true>>("sss1024pl");
  b.run<lce_sss<uint8_t, 2048, uint40_t, true>>("sss2048pl");

  b.run<lce_classic<uint8_t, uint40_t>>("classic");

#ifdef LCE_USE_SDSL
  b.run<lce_sdsl_cst>("sdsl_cst");
#endif
}