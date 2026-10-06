/*******************************************************************************
 * reproducibility/helpers/text-stats.cpp
 *
 * Prints the properties of a text that Table 1 of the paper lists and that the
 * benchmark tools do not report:
 *
 *  - the alphabet size, and
 *  - the size of the non-periodic variant of the string synchronizing set for
 *    tau = 256, 512, 1024 and 2048. lce::rolling_hash::sss computes this set
 *    first and discards it if it is more than twice as large as expected
 *    (which happens on "cere"); bench-lce then only reports the size of the
 *    periodic variant.
 ******************************************************************************/

#include <omp.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

#include "rolling_hash/string_synchronizing_set.hpp"
#include "util/io.hpp"

size_t alphabet_size(std::vector<uint8_t> const& text, size_t size) {
  std::array<bool, 256> occurs{};
#pragma omp parallel
  {
    std::array<bool, 256> occurs_local{};
#pragma omp for nowait
    for (size_t i = 0; i < size; ++i) {
      occurs_local[text[i]] = true;
    }
#pragma omp critical
    for (size_t c = 0; c < 256; ++c) {
      occurs[c] = occurs[c] || occurs_local[c];
    }
  }
  size_t sigma = 0;
  for (bool const c : occurs) {
    sigma += c;
  }
  return sigma;
}

// Runs the first phase of the constructor of lce::rolling_hash::sss, with the
// same fingerprints and the same partitioning of the text among the threads.
template <uint64_t t_tau>
size_t nonperiodic_sss_size(std::vector<uint8_t> const& text) {
  using namespace lce::rolling_hash;
  size_t const size = text.size();
  if (size <= 5 * t_tau) {
    return 0;
  }
  sss<uint64_t, t_tau> const scanner;
  auto const rk_owner = std::make_unique<rk_prime<> const>(t_tau, 296819);
  auto const rk3_owner = std::make_unique<rk_prime<> const>(3 * t_tau, 296819);
  rk_prime<> const& rk = *rk_owner;
  rk_prime<> const& rk3 = *rk3_owner;
  size_t sss_size = 0;
#pragma omp parallel reduction(+ : sss_size)
  {
    size_t const sss_end = size - 2 * t_tau + 1;

    int const t = omp_get_thread_num();
    int const nt = omp_get_num_threads();
    size_t const slice_size = sss_end / nt;

    size_t const begin = t * slice_size;
    size_t const end = (t < nt - 1) ? (t + 1) * slice_size : sss_end;

    sss_size +=
        scanner.fill_synchronizing_set(text.data(), begin, end, rk, rk3)
            .first.size();
  }
  return sss_size;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: text-stats <text>" << std::endl;
    return 1;
  }
  std::filesystem::path const path(argv[1]);
  if (!std::filesystem::is_regular_file(path)) {
    std::cerr << "file not found: " << path.string() << std::endl;
    return 1;
  }

  // loaded exactly like in bench-lce
  size_t const file_size = std::filesystem::file_size(path);
  std::vector<uint8_t> const text = lce::util::load_vector<uint8_t>(
      path, std::numeric_limits<size_t>::max(), 4096 * 4, 8);

  std::cout << "RESULT text=" << path.filename().string()
            << " file_size=" << file_size
            << " text_size=" << text.size()
            << " sigma=" << alphabet_size(text, file_size)
            << " nonperiodic_sss256=" << nonperiodic_sss_size<256>(text)
            << " nonperiodic_sss512=" << nonperiodic_sss_size<512>(text)
            << " nonperiodic_sss1024=" << nonperiodic_sss_size<1024>(text)
            << " nonperiodic_sss2048=" << nonperiodic_sss_size<2048>(text)
            << std::endl;
  return 0;
}
