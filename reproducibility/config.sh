# Configuration of the reproducibility scripts.
#
# Every setting below can be overridden from the environment, for example
#   LCE_TIER=all ./run-all.sh
# This file is sourced by scripts/common.sh; it is not meant to be run.

REPRO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LCE_ROOT="$(dirname "$REPRO_DIR")"

# ---------------------------------------------------------------------------
# Which texts to run on
# ---------------------------------------------------------------------------
#   pizza_and_chili  the 12 Pizza&Chili texts of Table 1          (default)
#   all              pizza_and_chili + the three large texts
#                    (wiki.txt, dna.txt, cc.txt)
# LCE_TEXTS="a b c" selects an explicit list instead.
: "${LCE_TIER:=pizza_and_chili}"

TEXTS_PIZZA_AND_CHILI=(dblp.xml dna english proteins sources
                       cere coreutils Escherichia_Coli einstein.de.txt
                       einstein.en.txt influenza kernel)
TEXTS_LARGE=(wiki.txt dna.txt cc.txt)

# ---------------------------------------------------------------------------
# Directories (everything the scripts write stays below reproducibility/)
# ---------------------------------------------------------------------------
: "${LCE_BUILD_DIR:=$REPRO_DIR/build}"       # CMake build tree
: "${LCE_WORK_DIR:=$REPRO_DIR/work}"         # texts, queries, SSS files, temp
: "${LCE_RESULTS_DIR:=$REPRO_DIR/results}"   # raw logs and RESULT files
: "${LCE_OUTPUT_DIR:=$REPRO_DIR/output}"     # generated tables and figures
: "${LCE_TOOLS_DIR:=$REPRO_DIR/tools}"       # locally installed TeX Live

# Where to look for inputs that already exist on this machine before
# downloading (texts) or generating (queries) them. Colon-separated.
: "${LCE_TEXT_SEARCH_PATH:=/scratch/data/pc:/scratch/data/pc/real:/scratch/nalbach/texts:/bigdata/nalbach:/bigdata/ellert}"
: "${LCE_QUERY_SEARCH_PATH:=/scratch/nalbach/lce_query_data}"

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
# The paper used GCC 12; fall back to the default compiler if it is missing.
if [[ -z "${LCE_CC:-}" || -z "${LCE_CXX:-}" ]]; then
  if command -v gcc-12 >/dev/null && command -v g++-12 >/dev/null; then
    LCE_CC=gcc-12 LCE_CXX=g++-12
  else
    LCE_CC=gcc LCE_CXX=g++
  fi
fi
: "${LCE_BUILD_JOBS:=16}"

# ---------------------------------------------------------------------------
# Experiments
# ---------------------------------------------------------------------------
# Thread counts of the parallel-scaling experiment (Fig. 8).
: "${LCE_SCALING_THREADS:=1 2 4 8 16 32 64}"
# Threads used to *construct* the data structures on the large texts before
# their (always single-threaded) queries are measured. Construction times of
# these runs are not used by any figure.
: "${LCE_LARGE_QUERY_THREADS:=32}"
# Threads for input preparation (suffix/LCP arrays, synchronizing sets).
: "${LCE_PREP_THREADS:=32}"
# Number of LCE queries per LCE range, and of successor queries.
: "${LCE_NUM_QUERIES:=1000000}"
: "${LCE_NUM_PRED_QUERIES:=1000000}"
# Optional prefix for every benchmark command, e.g. "numactl -i all".
: "${LCE_RUN_PREFIX:=}"

# Thread placement (OMP_PROC_BIND, OMP_PLACES). As for the measurements of
# the paper, every thread is pinned to its own physical core and the threads
# are spread over the sockets: one place per core, namely its first hardware
# thread ("{0}:64" on the machine of the paper). If the CPUs of a machine are
# not numbered that way, the equivalent "cores" is used.
default_omp_places() {
  local cores
  cores=$(lscpu -p=CORE,SOCKET 2>/dev/null | grep -v '^#' | sort -u | wc -l)
  if ((cores > 0)) && [[ $(lscpu -p=CPU,CORE,SOCKET | grep -v '^#' \
        | head -n "$cores" | cut -d, -f2,3 | sort -u | wc -l) == "$cores" ]]; then
    echo "{0}:$cores"
  else
    echo cores
  fi
}
: "${LCE_OMP_PROC_BIND:=spread}"
: "${LCE_OMP_PLACES:=$(default_omp_places)}"

# LCE data structures whose construction (1 thread) and queries are measured.
ALGOS_QUERIES=(naive naive_wordwise naive_wordwise_xor
               fp64 fp128 fp256 fp512 rk-prezza
               sss_noss256 sss_noss512 sss_noss1024 sss_noss2048
               sss256 sss512 sss1024 sss2048
               sss256pl sss512pl sss1024pl sss2048pl
               classic sdsl_cst)
# classic (isa-lcp-rmq) and sdsl_cst need more than 1 TB of RAM on the large
# texts and are skipped there, as in the paper.
ALGOS_SKIPPED_ON_LARGE=(classic sdsl_cst)
# Single-threaded construction measured for all texts (Table 1, Fig. 9) ...
ALGOS_CONSTRUCTION=(sss256 sss512 sss1024 sss2048)
# ... and additionally for the large texts (Fig. 3 and 6, bottom row).
ALGOS_CONSTRUCTION_LARGE=(sss_noss256 sss_noss512 sss_noss1024 sss_noss2048
                          fp64 rk-prezza)
# Data structure of the parallel-scaling experiment.
ALGO_SCALING=sss512

# ---------------------------------------------------------------------------
# Inputs: exact file sizes in bytes, checksums (both used to recognise the
# right file) and
# download locations (Pizza&Chili corpus).
# ---------------------------------------------------------------------------
declare -A TEXT_SIZE=(
  [dblp.xml]=296135874        [dna]=403927746
  [english]=2210395553        [proteins]=1184051855
  [sources]=210866607         [cere]=461286644
  [coreutils]=205281778       [Escherichia_Coli]=112689515
  [einstein.de.txt]=92758441  [einstein.en.txt]=467626544
  [influenza]=154808555       [kernel]=257961616
  [wiki.txt]=246327201088     [dna.txt]=224762018936
  [cc.txt]=196885192752
)
# SHA-256 checksums of the Pizza&Chili texts. A file is only used if its
# checksum matches (there are modified files of the same size around).
declare -A TEXT_SHA256=(
  [dblp.xml]=199941aea7a5f7e1be3ab05c0c606cd99052d2b6d545d6ec0b97355db673cf65
  [dna]=003b145676a5b08d67fd24588423b954d8ce7661493f35e6b170cefe6f8c7b4b
  [english]=0ab612d4f5f278eae58376ed49f5fc0c94e6a1357019dfd5a011d18651f57856
  [proteins]=9c7c3d6ce1f6d6ac7e088b69e61c5f731928b783f76aba123a25b00de46ba33e
  [sources]=ae9dcb2f97cd2da37fd384f3448b6e7fd05e412227dfa8bc537f83ac77b8a6d6
  [cere]=435c33db4c1aceff3a909192d6c18e6161988f2097fa1ae7cca7523a5b53fd84
  [coreutils]=aceb23e30aa956c099468e331437692b07c8c88e06250920cb1cefbab51fa191
  [Escherichia_Coli]=eca9987275d4ffbaa73d4dd4ebe58f307069e608d2af2d7b1ebb7c0b9c89c899
  [einstein.de.txt]=1b58542332df182f889229e2f07814a52da296d6d45f73ffb31e771e4d2d78c9
  [einstein.en.txt]=514cdf918a94d9ddf8987a5fe8972083858a09abb046896a6c55bf692e4cdd22
  [influenza]=e6f8a2e6f3071a51c29694fcab69ba4cc44f3f6079dd1d3e6b55f79077a96626
  [kernel]=74fe3cb474b4c307950ac4b92280be4c2863b77f9e59256e2fc4ae67420e30fa
)
PIZZACHILI=https://pizzachili.dcc.uchile.cl
declare -A TEXT_URL=(
  [dblp.xml]=$PIZZACHILI/texts/xml/dblp.xml.gz
  [dna]=$PIZZACHILI/texts/dna/dna.gz
  [english]=$PIZZACHILI/texts/nlang/english.gz
  [proteins]=$PIZZACHILI/texts/protein/proteins.gz
  [sources]=$PIZZACHILI/texts/code/sources.gz
  [cere]=$PIZZACHILI/repcorpus/real/cere.gz
  [coreutils]=$PIZZACHILI/repcorpus/real/coreutils.gz
  [Escherichia_Coli]=$PIZZACHILI/repcorpus/real/Escherichia_Coli.gz
  [einstein.de.txt]=$PIZZACHILI/repcorpus/real/einstein.de.txt.gz
  [einstein.en.txt]=$PIZZACHILI/repcorpus/real/einstein.en.txt.gz
  [influenza]=$PIZZACHILI/repcorpus/real/influenza.gz
  [kernel]=$PIZZACHILI/repcorpus/real/kernel.gz
)
