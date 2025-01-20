# Longest Common Extension (LCE)
This repository contains implementations of Longest Common Extension (LCE) data structures based on string synchronizing sets [1] and in-place fingerprinting [2] as well as practical predecessor/successor data structures.

## CLI Build Instructions
This implementation has been tested on Ubuntu 24.04 with gcc-12, g++-12, libtbb-dev and libomp-dev installed. To use the SDSL-based data structures, install and build [sdsl-lite](https://github.com/simongog/sdsl-lite) manually.

```shell
clone https://github.com/LukasNalbach/lce
mkdir build
cd build
cmake ..
cp -rf ../patched-files/* ..
make
```

This creates the following executables in the build/ folder.

## CLI Programs
### Benchmark Tools
- gen_queries (generates LCE queries)
- gen_sa_lcp (generates suffix array- and LCP-array files for gen_queries)
- benchmark_lce (benchmarks LCE data structures using generated LCE queries)
- gen_sss (generates a string synchronizing set for predecessor queries)
- benchmark_pred (benchmarks successor data structures using a generated SSS)

### Test Executables
- test_lce
- test_pred
- test_rmq
- test_rolling_hash
- test_string_synchronizing_set

## Usage in C++
### Cmake
```cmake
add_subdirectory(lce/)
set(LCE_USE_SDSL OFF)
set(LCE_BUILD_LA_VECTOR OFF)
```

### C++
```c++
#include <iostream>
#include <ds/lce_sss.hpp>

int main() {
    // create a string
    std::string text = "This is a test string";

    // build the LCE data structure (set tau = 512)
    lce::ds::lce_sss<char, 512> ds(text);

    // perform some LCE queries
    std::cout << ds.lce(3, 6) << std::endl;
    std::cout << ds.lce(5, 11) << std::endl;
    std::cout << ds.lce(11, 17) << std::endl;
}
```

### References
[1] Dominik Kempa and Tomasz Kociumaka. String synchronizing sets: sublinear-time BWT construction and optimal LCE data structure. In Proceedings of the 51st Annual ACM SIGACT Symposium on Theory of Computing (STOC) 2019, pages 756-767. ([arxiv.org](https://arxiv.org/abs/1904.04228))

[2] Nicola Prezza. In-Place Sparse Suffix Sorting. In Proceedings of the 29th Annual ACM-SIAM Symposium on Discrete Algorithms (SODA) 2018, pages 1496-1508. ([arxiv.org](https://arxiv.org/abs/1608.05100))