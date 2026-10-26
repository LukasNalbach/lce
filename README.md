# Longest Common Extension (LCE)

This repository contains longest common extension (LCE) data structures and predecessor data structures, together
with benchmarks and tests.

- `lce::ds::lce_sss` answers LCE queries with string synchronizing sets [1], `lce::ds::lce_rk_prezza` with in-place
  fingerprinting [2]. Further LCE data structures in [include/ds/](include/ds/) serve as baselines.
- Predecessor and successor data structures ([include/pred/](include/pred/)), range minimum queries
  ([include/rmq/](include/rmq/)), rolling hashes and string synchronizing sets.

The LCE data structures take a text as an array or as one of the text classes of
[compact-texts](https://github.com/LukasNalbach/compact-texts) (`direct_text`, `packed_text`, `split_text`).

## Contents

- [Build](#build)
- [Command-line tools](#command-line-tools)
- [C++ usage](#c-usage)
- [Tests](#tests)
- [References](#references)
- [License](#license)

## Build

Requirements: a C++20 compiler on a 64-bit x86 CPU, CMake 3.21 or later and OpenMP. Tested on Ubuntu 24.04 with
GCC 12. The library and the tests also build on Windows with Clang or MinGW-w64.

```shell
git clone https://github.com/LukasNalbach/lce.git
cd lce
mkdir build
cd build
cmake ..
make
```

The tests are written to `build/tests/` and the benchmark tools to `build/bench/`. `cmake` downloads the submodules
[compact-texts](https://github.com/LukasNalbach/compact-texts) (the text classes),
[string-utils](https://github.com/LukasNalbach/string-utils) (build settings, logging, memory helpers, ips4o,
libsais, malloc_count, sdsl-lite and googletest), tlx, PGM-index, la_vector, rk-lce and modular_arithmetic.

| Option | Description |
| --- | --- |
| `LCE_BUILD_BENCHMARKS` | build the benchmark tools (`OFF`; not on Windows) |
| `LCE_BUILD_TESTS` | build the tests (`ON` if lce is the top-level project) |
| `LCE_USE_SDSL` | build the LCE data structures that need SDSL (`OFF`) |
| `LCE_BUILD_LA_VECTOR` | build the predecessor data structure that needs SDSL (`OFF`) |
| `LCE_ENABLE_LTO` | link-time optimization of the executables (`ON`) |

The other options (`LCE_MARCH_NATIVE`, `LCE_USE_MALLOC_COUNT`, ...) and the environment variables are described in
string-utils (with the prefix `LCE`).

## Command-line tools

The benchmark tools are built with `LCE_BUILD_BENCHMARKS`; except gen-sa-lcp, they print their parameters with
`--help`.

- **gen-sa-lcp**, **gen-queries**: write the suffix array and the LCP array of a text, and LCE queries computed from
  them. `gen-sa-lcp text.txt text.sa text.lcp`
- **bench-lce**: measures the LCE data structures on generated queries.
- **gen-sss**, **bench-pred**: write the string synchronizing set of a text and measure the predecessor data structures
  on it.

## C++ usage

Add lce to a CMake project and link the target `lce`:

```cmake
add_subdirectory(lce/ EXCLUDE_FROM_ALL)
target_link_libraries(<target> PRIVATE lce)
```

If the project uses string-utils or compact-texts itself, it adds them first; lce then uses these copies.

```c++
#include <iostream>
#include <string>
#include <ds/lce_sss.hpp>

int main() {
    // create a string
    std::string text = "This is a test string";

    // build the LCE data structure (set tau = 512)
    lce::ds::lce_sss<char> ds(text, 512);

    // perform some LCE queries
    std::cout << ds.lce(3, 6) << std::endl;
    std::cout << ds.lce(5, 11) << std::endl;
    std::cout << ds.lce(11, 17) << std::endl;
}
```

## Tests

```shell
build/tests/test-lce
```

The other tests in `build/tests/` check the predecessor data structures, the range minimum queries, the rolling
hashes, the string synchronizing sets and the text classes. They check random inputs: at most `LCE_TEST_SECONDS`
seconds (default 10) or `LCE_TEST_ITERATIONS` inputs (default 1000) per function. A failed test prints its seed;
`LCE_TEST_SEED=<seed>` repeats its inputs.

## References

[1] Dominik Kempa and Tomasz Kociumaka. String synchronizing sets: sublinear-time BWT construction and optimal LCE data
structure. In Proceedings of the 51st Annual ACM SIGACT Symposium on Theory of Computing (STOC) 2019, pages 756-767.
([arxiv.org](https://arxiv.org/abs/1904.04228))

[2] Nicola Prezza. In-Place Sparse Suffix Sorting. In Proceedings of the 29th Annual ACM-SIAM Symposium on Discrete
Algorithms (SODA) 2018, pages 1496-1508. ([arxiv.org](https://arxiv.org/abs/1608.05100))

## License

This project is licensed under the BSD 2-Clause License, see [LICENSE](LICENSE) and [AUTHORS](AUTHORS). The
submodules in [external/](external/) have their own licenses.
