# Longest Common Extension (LCE)
This repository contains implementations of Longest Common Extension (LCE) data structures based on string synchronizing sets [1] and in-place fingerprinting [2] as well as practical predecessor/successor data structures.

## CLI Build Instructions
This implementation has been tested on Ubuntu 24.04 with gcc-12 and g++-12 installed.

```shell
git clone --recurse-submodules https://github.com/LukasNalbach/lce.git
cd lce
mkdir build
cd build
cmake ..
make
```

The benchmark tools are written to `build/bench/` and the test executables to `build/tests/`. The text classes `direct_text`, `packed_text` and `split_text`, which the LCE data structures accept as input, are in [compact-texts](https://github.com/LukasNalbach/compact-texts) (submodule `external/compact-texts`); the build settings, the memory and logging helpers and the libraries ips4o, libsais, malloc_count, sdsl-lite and googletest come from [string-utils](https://github.com/LukasNalbach/string-utils) (submodule `external/string-utils`).

## CLI Programs

### Benchmark Tools (`build/bench/`)
- gen-queries (generates LCE queries)
- gen-sa-lcp (generates suffix array- and LCP-array files for gen-queries)
- bench-lce (benchmarks LCE data structures using generated LCE queries)
- gen-sss (generates a string synchronizing set for predecessor queries)
- bench-pred (benchmarks predecessor data structures using a generated SSS)

### Test Executables (`build/tests/`)
- test-lce
- test-pred
- test-rmq
- test-rolling-hash
- test-string-synchronizing-set
- test-text

## Supported Compilers and Systems

lce is developed on Linux and its own code is portable C++20. A 64-bit x86 CPU is required (the library uses 128-bit integers and the `cmpxchg16b` instruction). The library and the tests also build on Windows with Clang or MinGW-w64 (see [string-utils](https://github.com/LukasNalbach/string-utils)); the benchmark tools are turned off on Windows.

## CMake Build Options

| Option | Default | Effect |
| --- | --- | --- |
| `LCE_MARCH_NATIVE` | `ON` | Tune the build for the host CPU (`-march=native`); turn **OFF** for portable binaries |
| `LCE_BUILD_BENCHMARKS` | `OFF` (not available on Windows) | Build the benchmark/generator CLI tools |
| `LCE_BUILD_TESTS` | `ON` if top-level | Build the test executables |
| `LCE_USE_MALLOC_COUNT` | `ON` | Track memory usage via malloc_count |
| `LCE_ENABLE_LTO` | `ON` | Enable link-time optimization (LTO/IPO) for the lce executables |
| `LCE_USE_SDSL` | `OFF` | Build the LCE data structures that depend on SDSL |
| `LCE_BUILD_LA_VECTOR` | `OFF` | Build the predecessor data structure that depends on SDSL |

The other options and the environment variables (`LCE_HUGE_PAGES`, `LCE_TEST_ITERATIONS`, ...) are described in [string-utils](https://github.com/LukasNalbach/string-utils).

## Usage in C++
### Cmake
```cmake
add_subdirectory(lce/)
target_link_libraries(<your_target> PRIVATE lce)
```
If the project uses string-utils or compact-texts itself, it adds them first; lce then uses these copies.

### C++
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

### References
[1] Dominik Kempa and Tomasz Kociumaka. String synchronizing sets: sublinear-time BWT construction and optimal LCE data structure. In Proceedings of the 51st Annual ACM SIGACT Symposium on Theory of Computing (STOC) 2019, pages 756-767. ([arxiv.org](https://arxiv.org/abs/1904.04228))

[2] Nicola Prezza. In-Place Sparse Suffix Sorting. In Proceedings of the 29th Annual ACM-SIAM Symposium on Discrete Algorithms (SODA) 2018, pages 1496-1508. ([arxiv.org](https://arxiv.org/abs/1608.05100))
