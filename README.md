# Longest Common Extension (LCE)
This repository contains implementations of Longest Common Extension (LCE) data structures based on string synchronizing sets [1] and in-place fingerprinting [2] as well as practical predecessor/successor data structures.

## CLI Build Instructions
This implementation has been tested on Ubuntu 24.04 with gcc-12, g++-12, libtbb-dev and libomp-dev installed. To use the SDSL-based data structures, install and build [sdsl-lite](https://github.com/simongog/sdsl-lite) manually.

```shell
git clone --recurse-submodules https://github.com/LukasNalbach/lce.git
cd lce
mkdir build
cd build
cmake ..
make
```

The benchmark tools are written to `build/bench/` and the test executables to `build/tests/`.

## CLI Programs

### Benchmark Tools (`build/bench/`)
- gen-queries (generates LCE queries)
- gen-sa-lcp (generates suffix array- and LCP-array files for gen-queries)
- bench-lce (benchmarks LCE data structures using generated LCE queries)
- gen-sss (generates a string synchronizing set for predecessor queries)
- bench-pred (benchmarks successor data structures using a generated SSS)

### Test Executables (`build/tests/`)
- test-lce
- test-pred
- test-rmq
- test-rolling-hash
- test-string-synchronizing-set

## Supported Compilers and Systems

lce is developed on Linux and its own code is portable C++20; the platform differences are almost entirely in third-party dependencies. A 64-bit x86 CPU is required (the library uses 128-bit integers and the `cmpxchg16b` instruction).

| System | Compiler | Status |
| --- | --- | --- |
| Linux (x86-64) | GCC ≥ 12 | ✅ Primary development / test target |
| Linux (x86-64) | Clang | ✅ Supported |
| Windows (x64) | LLVM/Clang (`clang++`, GNU driver) | ✅ **Recommended Windows toolchain** — builds and runs |
| Windows (x64) | MinGW-w64 (g++) | ✅ Supported — needs a MinGW-built oneTBB (see below) |

Native MSVC (`cl.exe`) is not supported: it lacks `__uint128_t` and the `__atomic_*` / `__builtin_*` intrinsics that lce and its dependencies use throughout. Use `clang++` for MSVC-ABI-compatible binaries.

Everything Windows-specific is handled automatically by the `if(WIN32)` block in `CMakeLists.txt` (the `MSVC_COMPILER` / `_REENTRANT` / `HURCHALLA_DISALLOW_INLINE_ASM_MODMUL` defines, `-mcx16`, static-CRT linking, the `libatomic` shim, the OpenMP/libomp wiring, a `<sys/mman.h>` / `<sys/resource.h>` shim, and copying the required runtime DLLs next to the test executables). The benchmark tools depend on POSIX I/O and are turned off on Windows automatically (`LCE_BUILD_BENCHMARKS`); the library and the tests are built.

### oneTBB on Windows

ips4o's parallel sort links Intel oneTBB. Point `-DLCE_WINDOWS_TBB_ROOT=<dir>` at an extracted oneTBB:

- **Clang** (`clang++`, MSVC ABI): use a prebuilt [`oneapi-tbb-*-win`](https://github.com/uxlfoundation/oneTBB/releases) release directly. If you build oneTBB **static** (add `-DBUILD_SHARED_LIBS=OFF`), TBB is linked *into* the executables and no `tbb`/`tbb12` DLL is needed beside them; a prebuilt/shared oneTBB instead ships its DLL next to the exe (copied automatically).
- **MinGW** (GNU ABI): the prebuilt release is MSVC-ABI and will not link. Use a MinGW-ABI oneTBB — e.g. MSYS2's `mingw-w64-x86_64-tbb` package, or build oneTBB from source with your MinGW.

### Building on Windows (Clang)

Requirements: **LLVM/Clang** for Windows (`winget install LLVM.LLVM` — provides `clang`, `clang++`, `libomp`), **Visual Studio** / Build Tools (MSVC runtime + Windows SDK + bundled CMake and Ninja), and an extracted **oneTBB** (above). From an *x64 Native Tools Command Prompt* (`vcvars64`):

```shell
cmake -S . -B build -G Ninja ^
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ ^
  -DCMAKE_BUILD_TYPE=Release -DLCE_MARCH_NATIVE=OFF ^
  -DLCE_WINDOWS_TBB_ROOT="C:/path/to/oneapi-tbb-2023.0.0"
ninja -C build
```

### Building on Windows (MinGW-w64)

Requirements: a **MinGW-w64** toolchain (e.g. WinLibs or MSYS2's `mingw-w64-x86_64-gcc`), CMake, Ninja, and a MinGW-ABI **oneTBB** (above). The executables are linked `-static`, so only the OpenMP runtime (`libgomp-1.dll`) and the oneTBB DLL need to sit next to them (both staged automatically).

```shell
cmake -S . -B build -G Ninja ^
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ^
  -DCMAKE_BUILD_TYPE=Release -DLCE_MARCH_NATIVE=OFF ^
  -DLCE_WINDOWS_TBB_ROOT="C:/path/to/mingw-onetbb"
ninja -C build
```

## CMake Build Options

| Option | Default | Effect |
| --- | --- | --- |
| `LCE_MARCH_NATIVE` | `ON` | Tune the build for the host CPU (`-march=native`); turn **OFF** for portable binaries and on Windows |
| `LCE_BUILD_BENCHMARKS` | `ON` (forced `OFF` on Windows) | Build the benchmark/generator CLI tools |
| `LCE_USE_MALLOC_COUNT` | `ON` (stubbed on Windows) | Track memory usage via malloc_count |
| `LCE_USE_SDSL` | `OFF` | Build the LCE data structures that depend on SDSL |
| `LCE_BUILD_LA_VECTOR` | `OFF` | Build the predecessor data structure that depends on SDSL |
| `LCE_WINDOWS_TBB_ROOT` | *(empty)* | Windows only: root of an extracted oneTBB release |

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
