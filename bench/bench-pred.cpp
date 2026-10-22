/*******************************************************************************
 * bench/bench-pred.cpp
 *
 * Copyright (C) 2022 Alexander Herlez <alexander.herlez@tu-dortmund.de>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifdef LCE_BENCHMARK_SPACE
#include <malloc_count.h>
#endif

#include <assert.h>
#include <omp.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <misc/packed_integers.hpp>
#include <misc/files.hpp>
#include <misc/log.hpp>
#include <misc/memory.hpp>
#include <iostream>
#include <iterator>
#include <random>
#include <string>
#include <tlx/cmdline_parser.hpp>
#include <vector>

#include "format_list.hpp"
#include "pred/binsearch_std.hpp"
#include "pred/binsearch_cache.hpp"
#include "pred/rank_index.hpp"
#include "pred/j_index.hpp"
#include "pred/pgm_index.hpp"
#include "pred/pred_index.hpp"

#ifdef LCE_USE_SDSL
#include "pred/sd_array_index.hpp"
#endif

#ifdef LCE_BUILD_LA_VECTOR
#include "pred/la_vector_index.hpp"
#endif


namespace fs = std::filesystem;

std::vector<std::string> algorithms{
  "all", "binsearch_std", "binsearch_cache", "rank_index", "j_index",
  "pred_index6", "pred_index7", "pred_index8", "pred_index9",
  "pred_index10", "pred_index11", "pred_index12",
  "pgm_index8", "pgm_index16", "pgm_index32", "pgm_index64", "pgm_index128",
#ifdef LCE_USE_SDSL
  "sd_array1", "sd_array2", "sd_array4", "sd_array8", "sd_array16",
  "sd_array32", "sd_array64", "sd_array128", "sd_array256", "sd_array512",
#endif
#ifdef LCE_BUILD_LA_VECTOR
  "la_vector1", "la_vector2", "la_vector4", "la_vector8", "la_vector16",
  "la_vector32", "la_vector64", "la_vector128", "la_vector256", "la_vector512",
#endif
};

class benchmark {
 public:
  typedef uint64_t data_type;

  fs::path data_path;
  std::vector<data_type> data;

  std::vector<size_t> queries;
  size_t num_queries = 1'000'000;
  bool no_pred = false;
  bool no_succ = false;

  std::string algorithm = "binsearch_std";

  bool check_parameters() {
    // Check data path
    if (!fs::is_regular_file(data_path) || fs::file_size(data_path) == 0) {
      std::cout << "Data file " << data_path.string() << " is empty or does not exist.\n";
      return false;
    }
    // Check algorithm
    if (std::find(algorithms.begin(), algorithms.end(), algorithm) ==
        algorithms.end()) {
      std::cout << "Algorithm " << algorithm << " is not specified.\n Use one of "
                << format_list(algorithms) << "\n";
      return false;
    }
    return true;
  }

  void load_data() {
    auto t = now();
    no_init_vector<uint40_t> data_5byte;
    resize_no_init(data_5byte, fs::file_size(data_path) / sizeof(uint40_t));
    std::ifstream in(data_path, std::ios::binary);
    read_in_chunks(in, (char*) data_5byte.data(), data_5byte.size() * sizeof(uint40_t));
    data = std::vector<uint64_t>(data_5byte.begin(), data_5byte.end());
    assert(data.size() != 0);
    std::cout << " data=" << data_path.filename().string();
    std::cout << " data_size=" << data.size();
    std::cout << " data_time=" << time_diff_ns(t) / 1'000'000;
  }

  void load_queries() {
    auto t = now();
    queries.resize(num_queries);
    // std::random_device rd;
    std::mt19937 gen(1337);
    std::uniform_int_distribution<uint64_t> distrib(0, data.back());
    for (size_t i = 0; i < num_queries; ++i) {
      queries[i] = distrib(gen);
    }
    std::cout << " q_size=" << queries.size();
    std::cout << " q_gen_time=" << time_diff_ns(t) / 1'000'000;
  }

  template <typename pred_ds_type, typename... ctor_args_t>
  pred_ds_type benchmark_construction(ctor_args_t... ctor_args) {
#ifdef LCE_BENCHMARK_SPACE
    malloc_count_reset_peak();
    size_t mem_before = malloc_count_current();
#endif
    auto t = now();
    pred_ds_type pred_ds(data, ctor_args...);
    std::cout << " threads=" << omp_get_max_threads();
    std::cout << " c_time=" << time_diff_ns(t) / 1'000'000;
#ifdef LCE_BENCHMARK_SPACE
    std::cout << " c_mem=" << malloc_count_current() - mem_before;
    std::cout << " c_mempeak=" << malloc_count_peak() - mem_before;
#endif
    return pred_ds;
  }

  template <typename pred_ds_type>
  void benchmark_queries(pred_ds_type& pred_ds) {
    if (!no_pred) {
      auto t = now();
      size_t check_sum = 0;
      for (size_t i = 0; i < queries.size(); ++i) {
        check_sum += pred_ds.predecessor(queries[i]).pos;
      }
      std::cout << " pred_time=" << time_diff_ns(t) / 1'000'000;
      std::cout << " check_sum=" << check_sum;
    }

    if (!no_succ) {
      auto t = now();
      size_t check_sum = 0;
      for (size_t i = 0; i < queries.size(); ++i) {
        check_sum += pred_ds.successor(queries[i]).pos;
      }
      std::cout << " succ_time=" << time_diff_ns(t) / 1'000'000;
      std::cout << " check_sum=" << check_sum;
    }
  }

 public:
  template <typename pred_ds_type, typename... ctor_args_t>
  void run(std::string const& algo_name, ctor_args_t... ctor_args) {
    if (algo_name != algorithm && algorithm != "all") {
      return;
    }

    // Construction
    std::cout << "RESULT algo=" << algo_name;
    load_data();
    pred_ds_type pred_ds = benchmark_construction<pred_ds_type>(ctor_args...);

    // Queries
    load_queries();
    benchmark_queries<pred_ds_type>(pred_ds);
    std::cout << "\n";
  }
};

int main(int argc, char** argv) {
  benchmark b;
  tlx::CmdlineParser cp;
  cp.set_description(
      "This program measures construction time and query time for several "
      "predecessor data structures.");
  cp.set_author("Alexander Herlez <alexander.herlez@tu-dortmund.de>");
  cp.add_param_path("data_path", b.data_path,
                    "The path to the integers queried");
  cp.add_bytes('q', "num_queries", b.num_queries,
               "Number of queries that are executed (default=1,000,000).");
  cp.add_flag("no_pred", b.no_pred, "Don't benchmark predecessor queries.");
  cp.add_flag("no_succ", b.no_succ, "Don't benchmark successor queries.");

  cp.add_string(
      'a', "algorithm", b.algorithm,
      "Name of data structure which is benchmarked. Options: " + format_list(algorithms));

  if (!cp.process(argc, argv)) {
    std::exit(EXIT_FAILURE);
  }
  if (!b.check_parameters()) {
    return -1;
  }

  b.run<lce::pred::binsearch_std<uint64_t>>("binsearch_std");
  b.run<lce::pred::binsearch_cache<uint64_t>>("binsearch_cache");
  b.run<lce::pred::j_index<uint64_t>>("j_index");
  b.run<lce::pred::rank_index<uint64_t>>("rank_index");

  b.run<lce::pred::pred_index<uint64_t, uint32_t>>("pred_index6", uint8_t(6));
  b.run<lce::pred::pred_index<uint64_t, uint32_t>>("pred_index7", uint8_t(7));
  b.run<lce::pred::pred_index<uint64_t, uint32_t>>("pred_index8", uint8_t(8));
  b.run<lce::pred::pred_index<uint64_t, uint32_t>>("pred_index9", uint8_t(9));
  b.run<lce::pred::pred_index<uint64_t, uint32_t>>("pred_index10", uint8_t(10));
  b.run<lce::pred::pred_index<uint64_t, uint32_t>>("pred_index11", uint8_t(11));
  b.run<lce::pred::pred_index<uint64_t, uint32_t>>("pred_index12", uint8_t(12));

  b.run<lce::pred::pgm_index<uint64_t, 8>>("pgm_index8");
  b.run<lce::pred::pgm_index<uint64_t, 16>>("pgm_index16");
  b.run<lce::pred::pgm_index<uint64_t, 32>>("pgm_index32");
  b.run<lce::pred::pgm_index<uint64_t, 64>>("pgm_index64");
  b.run<lce::pred::pgm_index<uint64_t, 128>>("pgm_index128");
  
#ifdef LCE_USE_SDSL
  b.run<lce::pred::sd_array_index<uint64_t, 1>>("sd_array1");
  b.run<lce::pred::sd_array_index<uint64_t, 2>>("sd_array2");
  b.run<lce::pred::sd_array_index<uint64_t, 4>>("sd_array4");
  b.run<lce::pred::sd_array_index<uint64_t, 8>>("sd_array8");
  b.run<lce::pred::sd_array_index<uint64_t, 16>>("sd_array16");
  b.run<lce::pred::sd_array_index<uint64_t, 32>>("sd_array32");
  b.run<lce::pred::sd_array_index<uint64_t, 64>>("sd_array64");
  b.run<lce::pred::sd_array_index<uint64_t, 128>>("sd_array128");
  b.run<lce::pred::sd_array_index<uint64_t, 256>>("sd_array256");
  b.run<lce::pred::sd_array_index<uint64_t, 512>>("sd_array512");
#endif

#ifdef LCE_BUILD_LA_VECTOR
  b.run<lce::pred::la_vector_index<uint64_t, 1>>("la_vector1");
  b.run<lce::pred::la_vector_index<uint64_t, 2>>("la_vector2");
  b.run<lce::pred::la_vector_index<uint64_t, 4>>("la_vector4");
  b.run<lce::pred::la_vector_index<uint64_t, 8>>("la_vector8");
  b.run<lce::pred::la_vector_index<uint64_t, 16>>("la_vector16");
  b.run<lce::pred::la_vector_index<uint64_t, 32>>("la_vector32");
  b.run<lce::pred::la_vector_index<uint64_t, 64>>("la_vector64");
  b.run<lce::pred::la_vector_index<uint64_t, 128>>("la_vector128");
  b.run<lce::pred::la_vector_index<uint64_t, 256>>("la_vector256");
  b.run<lce::pred::la_vector_index<uint64_t, 512>>("la_vector512");
#endif
}