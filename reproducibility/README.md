# Reproducing the experiments

This directory contains everything needed to rerun the experiments of the paper

> P. Dinklage, J. Ellert, J. Fischer, A. Herlez, T. Kociumaka, F. Kurpicz, L. Nalbach:
> *Practical Data Structures for Longest Common Extensions* (ACM Transactions on Algorithms)

and to regenerate its table and figures (Table 1 and Fig. 2 to 9; Fig. 1 is an illustration).
The scripts work on the source code of this repository at tag `TALG-2027`.

## Quick start

```shell
cd reproducibility
./run-all.sh          # the 12 Pizza&Chili texts, about 6 hours
./run-all.sh all      # additionally the three large texts, about 5 days
```

The results are

- `output/figures.pdf`: Table 1 and Fig. 2 to 9, numbered as in the paper, one per page,
- `output/table-datasets.txt`: Table 1 as plain text.

Use a terminal multiplexer (`tmux`, `screen`) for the longer runs. If a run is interrupted, start the same command again: completed measurements are kept and skipped.

Running times are measured, so the machine has to be otherwise idle. The benchmark threads are pinned to fixed cores, starting with CPU 0 (see `LCE_OMP_PLACES` in `config.sh`); a second benchmark or any other job that is pinned the same way would share these cores and distort the measurements. Do not start two runs at the same time.

Panels of texts that were not measured stay empty in the figures. In particular, the default run leaves the panels of wiki.txt, dna.txt and cc.txt empty.

## Environment

The measurements of the paper were made on a machine with two AMD EPYC 7452 CPUs (2 x 32 cores) and 1 TB of RAM running Ubuntu 24.04, with GCC 12. On that machine all inputs are already in place and the scripts link them instead of downloading or generating them (see `LCE_TEXT_SEARCH_PATH` and `LCE_QUERY_SEARCH_PATH` in `config.sh`).

On another machine the following is needed (Ubuntu 24.04 package names):

```shell
sudo apt install build-essential gcc-12 g++-12 cmake git wget perl python3 \
                 libtbb-dev libomp-dev libsdsl-dev libdivsufsort-dev
```

LaTeX with pgfplots is needed for the figures. If it is missing, `scripts/00-setup-tools.sh` installs a minimal TeX Live into `tools/` (about 200 MB, no root access needed). The default run needs about 60 GB of RAM (suffix and LCP array of "english" for the query generation); the large texts need the 1 TB machine.

## Steps

`run-all.sh` runs the following scripts in this order. Each of them can be run on its own; all of them respect the selection of texts (see Configuration).

| Script | Purpose | Output |
| --- | --- | --- |
| `scripts/00-setup-tools.sh` | Checks the prerequisites, installs TeX Live if no LaTeX is found. | `tools/` |
| `scripts/01-build.sh` | Compiles the benchmark tools of the repository (Release mode, `-march=native -DNDEBUG -O3`) and `helpers/text-stats`. | `build/bench/` |
| `scripts/02-prepare-inputs.sh` | Provides texts and LCE queries, determines the alphabet sizes. | `work/texts/`, `work/queries/`, `results/text-stats.txt` |
| `scripts/03-run-lce.sh` | Runs `bench-lce`: construction (1 to 64 threads) and queries of all LCE data structures. | `results/lce.results.new.par`, `results/lce.results.new` |
| `scripts/04-run-pred.sh` | Runs `gen-sss` and `bench-pred`: successor data structures on the synchronizing sets. | `results/pred.results.new` |
| `scripts/05-make-figures.sh` | Generates the table and the figures from the result files. | `output/` |

The output of every single benchmark run is kept in `results/raw/`. A run that fails is listed in `results/failed-runs.txt` and does not stop the scripts; its data structure is then missing in the figures. One failure is expected: the construction of `sdsl_cst` on "english" aborts (the text contains a zero byte, which sdsl-lite rejects); the paper omits `sdsl-cst` for this text as well.

## Mapping to the paper

| Paper | Content | Page of `output/figures.pdf` | Data | Measured by |
| --- | --- | --- | --- | --- |
| Fig. 2 | Successor data structures: query throughput versus space | 2 | `pred.results.new` | `04-run-pred.sh` |
| Table 1 | Text sizes, alphabet sizes, sizes of the synchronizing sets | 3, also `output/table-datasets.txt` | `lce.results.new.par`, `text-stats.txt` | `03-run-lce.sh`, `02-prepare-inputs.sh` |
| Fig. 3 | Construction time versus memory of `sss` and `sss-noss` | 4 | `lce.results.new` (large texts: `lce.results.new.par`) | `03-run-lce.sh` |
| Fig. 4 | Query throughput of the SSS-based data structures | 5 | `lce.results.new` | `03-run-lce.sh` |
| Fig. 5 | Query throughput of in-place fingerprinting and naive algorithms | 6 | `lce.results.new` | `03-run-lce.sh` |
| Fig. 6 | Construction time versus memory of all data structures | 7 | `lce.results.new` (large texts: `lce.results.new.par`) | `03-run-lce.sh` |
| Fig. 7 | Query throughput of all data structures | 8 | `lce.results.new` | `03-run-lce.sh` |
| Fig. 8 | Construction phases of `sss-512` for 1 to 64 threads | 9 | `lce.results.new.par` | `03-run-lce.sh` |
| Fig. 9 | Sizes and construction memory peaks of the parts of `sss-512` | 10 | `lce.results.new.par` | `03-run-lce.sh` |

The names in the result files (`algo=...`) differ slightly from the names in the paper:

| Paper | Result files |
| --- | --- |
| `naive`, `naive-wordwise`, `naive-wordwise-xor` | `naive`, `naive_wordwise`, `naive_wordwise_xor` |
| `fp-64` ... `fp-512` | `fp64` ... `fp512` |
| `sss-256` ... `sss-2048`, with suffix `-pl` | `sss256` ... `sss2048`, with suffix `pl` |
| `sss-noss-256` ... `sss-noss-2048` | `sss_noss256` ... `sss_noss2048` |
| `rk-prezza`, `sdsl-cst`, `isa-lcp-rmq` | `rk-prezza`, `sdsl_cst`, `classic` |
| `bin-search`, `bin-search-scan`, `rank` | `binsearch_std`, `binsearch_cache`, `rank_index` |
| `succ-index-k`, `pgm-index-e`, `la-vector-s`, `sd-array-s` | `pred_indexk`, `pgm_indexe`, `la_vectors`, `sd_arrays` |

## How the table and the figures are generated

The figures of the paper are pgfplots pictures whose data lines were generated with [sqlplot-tools](https://github.com/bingmann/sqlplot-tools): in the plot sources, every data block is preceded by the SQL statement it stems from. `plot/paper/` contains the unmodified plot sources and preamble of the paper. `plot/make_plots.py` evaluates the SQL statements in them on the new result files (with SQLite, like sqlplot-tools) and replaces the data lines; layout, styles and captions stay as they are. Three presentation rules that were applied by hand in the paper are implemented explicitly in `plot/make_plots.py`:

- Fig. 3 and 6 draw two marks per data structure, its final size and its construction memory peak, which come from two SQL statements.
- Fig. 6 draws `fp-64` and `rk-prezza`, which overwrite the text and are regarded as in-place, at the bottom of the logarithmic memory axis (0.01 B/n).
- Fig. 2 draws successor data structures without extra memory at the bottom of the logarithmic memory axis (the tick labelled 0).

## Inputs

**Texts.** The 12 texts of the regular and the repetitive corpus of [Pizza&Chili](https://pizzachili.dcc.uchile.cl) are downloaded if they are not found on the machine. The three large texts (wiki.txt, 246 GB; dna.txt, 225 GB; cc.txt, 197 GB) were assembled as described in Section 6.1 of the paper. They cannot be downloaded as such and are provided on the machine of the paper.

**LCE queries.** For every text there are 21 files `lce_0`, ..., `lce_20`; file `lce_k` contains up to 100,000 pairs of text positions whose LCE is 0 for k = 0 and lies in [2^(k-1), 2^k) for k > 0. The benchmark answers 1,000,000 queries per file by repeating the pairs. For the 12 small texts the files are generated by `gen-sa-lcp` and `gen-queries` of this repository (in RAM, about 25 bytes per character). For the large texts this is not possible in 1 TB of RAM; their suffix and LCP arrays were computed with external-memory algorithms and the resulting query files are provided on the machine of the paper.

## Running time

Approximate running times on the machine of the paper:

| Selection | Texts | Time |
| --- | --- | --- |
| `pizza_and_chili` (default) | the 12 Pizza&Chili texts | about 6 hours |
| `all` | additionally wiki.txt, dna.txt, cc.txt | about 5 days |

Most of the time of `all` is spent on the single-threaded constructions on the large texts (30 to 90 ns per character for each of the SSS-based data structures and for `rk-prezza`).

## Configuration

All settings are in `config.sh` and can be overridden from the environment. The most useful ones:

| Variable | Default | Meaning |
| --- | --- | --- |
| `LCE_TIER` | `pizza_and_chili` | `pizza_and_chili` or `all`; also the argument of `run-all.sh` |
| `LCE_TEXTS` | | explicit list of texts, e.g. `LCE_TEXTS="dblp.xml english"` |
| `LCE_SCALING_THREADS` | `1 2 4 8 16 32 64` | thread counts of Fig. 8 |
| `LCE_LARGE_QUERY_THREADS` | `32` | threads that construct the data structures on the large texts before the single-threaded queries are measured |
| `LCE_OMP_PROC_BIND`, `LCE_OMP_PLACES` | `spread`, one place per physical core | thread placement (`OMP_PROC_BIND`, `OMP_PLACES`), as for the measurements of the paper |
| `LCE_CC`, `LCE_CXX` | `gcc-12`, `g++-12` | compilers |
| `LCE_WORK_DIR`, `LCE_RESULTS_DIR`, `LCE_OUTPUT_DIR` | `work`, `results`, `output` | where inputs, measurements and figures are written |
| `LCE_TEXT_SEARCH_PATH`, `LCE_QUERY_SEARCH_PATH` | paths on the machine of the paper | where existing texts and query files are looked up |

Examples:

```shell
LCE_TEXTS="dblp.xml english" ./run-all.sh       # two texts only
scripts/03-run-lce.sh && scripts/05-make-figures.sh   # rerun one step
```

To repeat a measurement, delete its files `results/raw/.../<name>.txt*` (or the whole `results/` directory) and run the scripts again.

## Remarks

- `bench-lce` is started once per text, data structure and thread count. Queries are always answered by a single thread.
- On the large texts the data structures are constructed with 32 threads before their queries are measured; the single-threaded construction is measured separately. `sdsl-cst` and `isa-lcp-rmq` need more than 1 TB of RAM there and are skipped, as in the paper.
- `bench-pred.cpp` selects the competitors that depend on sdsl-lite and la_vector and its memory measurements with preprocessor macros that `CMakeLists.txt` does not define for it. `scripts/01-build.sh` therefore passes `-DALX_BENCHMARK_SPACE -DLCE_USE_SDSL -DLCE_BUILD_LA_VECTOR` as compiler flags; the source code is not modified.
- The sizes of the non-periodic synchronizing sets of "cere" in Table 1 are computed by `helpers/text-stats.cpp`, which calls the scan routine of the library; `bench-lce` only reports the size of the periodic variant that the library switches to.
