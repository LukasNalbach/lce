#!/usr/bin/env bash
# Compiles the benchmark tools of the repository and the helper tool.

source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

# The tools are built in Release mode ("-march=native -DNDEBUG -O3") by the
# repository's own CMakeLists.txt.
#  - LCE_USE_SDSL / LCE_BUILD_LA_VECTOR enable the competitors that depend on
#    sdsl-lite (sdsl-cst, sd-array) and on la_vector.
#  - bench-pred.cpp selects these competitors and its memory measurements with
#    preprocessor macros that the CMakeLists.txt does not define for it, so
#    they are passed as compiler flags.
log "configuring (compilers: $LCE_CC, $LCE_CXX)"
cmake -S "$LCE_ROOT" -B "$LCE_BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$LCE_CC" -DCMAKE_CXX_COMPILER="$LCE_CXX" \
    -DLCE_USE_SDSL=ON -DLCE_BUILD_LA_VECTOR=ON \
    -DCMAKE_CXX_FLAGS="-DALX_BENCHMARK_SPACE -DLCE_USE_SDSL -DLCE_BUILD_LA_VECTOR" \
    > "$LCE_BUILD_DIR.configure.log" 2>&1 \
    || die "cmake failed, see $LCE_BUILD_DIR.configure.log"

log "building the benchmark tools"
cmake --build "$LCE_BUILD_DIR" -j "$LCE_BUILD_JOBS" \
    --target bench-lce bench-pred gen-sss gen-queries gen-sa-lcp \
    > "$LCE_BUILD_DIR.build.log" 2>&1 \
    || die "build failed, see $LCE_BUILD_DIR.build.log"

# helpers/text-stats reports the alphabet sizes and the sizes of the
# non-periodic synchronizing sets for Table 1.
log "building helpers/text-stats"
{
  cmake -S "$REPRO_DIR/helpers" -B "$LCE_BUILD_DIR/helpers" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER="$LCE_CC" -DCMAKE_CXX_COMPILER="$LCE_CXX" \
      -DFETCHCONTENT_SOURCE_DIR_HURCHALLA_UTIL="$LCE_BUILD_DIR/_deps/hurchalla_util-src" \
  && cmake --build "$LCE_BUILD_DIR/helpers" --target text-stats
} > "$LCE_BUILD_DIR.helpers.log" 2>&1 \
    || die "building the helper failed, see $LCE_BUILD_DIR.helpers.log"

ln -sf ../helpers/text-stats "$BENCH_DIR/text-stats"
require_binaries
log "binaries are in $BENCH_DIR"
