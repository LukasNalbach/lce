# Shared helpers of the reproducibility scripts (sourced, not run).

set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/config.sh"

BENCH_DIR="$LCE_BUILD_DIR/bench"
TEXT_DIR="$LCE_WORK_DIR/texts"
QUERY_DIR="$LCE_WORK_DIR/queries"
SSS_DIR="$LCE_WORK_DIR/sss"
TMP_DIR="$LCE_WORK_DIR/tmp"
RAW_DIR="$LCE_RESULTS_DIR/raw"
FAILED_LOG="$LCE_RESULTS_DIR/failed-runs.txt"

# A TeX Live installed by 00-setup-tools.sh takes effect automatically.
if [[ -d "$LCE_TOOLS_DIR/texlive/bin/x86_64-linux" ]]; then
  PATH="$LCE_TOOLS_DIR/texlive/bin/x86_64-linux:$PATH"
fi

# Thread placement of every program started by the scripts (see config.sh).
export OMP_PROC_BIND="$LCE_OMP_PROC_BIND" OMP_PLACES="$LCE_OMP_PLACES"

log() { printf '[%s] %s\n' "$(date '+%F %T')" "$*" >&2; }
die() { log "ERROR: $*"; exit 1; }

# The texts selected by LCE_TEXTS or LCE_TIER, one per line.
selected_texts() {
  if [[ -n "${LCE_TEXTS:-}" ]]; then
    printf '%s\n' $LCE_TEXTS
    return
  fi
  case "$LCE_TIER" in
    pizza_and_chili) printf '%s\n' "${TEXTS_PIZZA_AND_CHILI[@]}" ;;
    all)             printf '%s\n' "${TEXTS_PIZZA_AND_CHILI[@]}" "${TEXTS_LARGE[@]}" ;;
    *)               die "unknown LCE_TIER '$LCE_TIER' (use pizza_and_chili or all)" ;;
  esac
}

is_large() {
  local t
  for t in "${TEXTS_LARGE[@]}"; do [[ $t == "$1" ]] && return 0; done
  return 1
}

contains() {  # <needle> <element>...
  local needle=$1 e; shift
  for e in "$@"; do [[ $e == "$needle" ]] && return 0; done
  return 1
}

require_binaries() {
  local b
  for b in bench-lce bench-pred gen-sss gen-queries gen-sa-lcp text-stats; do
    [[ -x "$BENCH_DIR/$b" ]] || die "$BENCH_DIR/$b not found; run scripts/01-build.sh first"
  done
}

# run_logged <output file> <threads> <command...>
#
# Runs the command with OMP_NUM_THREADS=<threads> and stores its standard
# output in <output file>. A run that already completed is skipped, so an
# interrupted script can simply be started again. A failing command does not
# abort the script: its partial output is kept as <output file>.failed and
# the failure is recorded in results/failed-runs.txt.
run_logged() {
  local out=$1 threads=$2; shift 2
  if [[ -e "$out.ok" ]]; then
    log "  skip (already done): ${out#"$LCE_RESULTS_DIR"/}"
    return 0
  fi
  mkdir -p "$(dirname "$out")" "$TMP_DIR"
  rm -f "$out.failed"
  local start=$SECONDS rc=0
  # sdsl writes temporary files into the working directory
  (cd "$TMP_DIR" && OMP_NUM_THREADS=$threads exec $LCE_RUN_PREFIX "$@") \
      > "$out.tmp" 2> "$out.err" || rc=$?
  if [[ $rc -eq 0 ]]; then
    mv "$out.tmp" "$out"
    touch "$out.ok"
    log "  done in $((SECONDS - start)) s: ${out#"$LCE_RESULTS_DIR"/}"
  else
    mv "$out.tmp" "$out.failed"
    echo "$(date '+%F %T') exit=$rc threads=$threads: $*" >> "$FAILED_LOG"
    log "  FAILED (exit code $rc) after $((SECONDS - start)) s: $*"
  fi
  return 0
}

# collect_results <output file> <raw output file>...
# Concatenates the RESULT lines of all completed runs.
collect_results() {
  local out=$1 f; shift
  : > "$out.tmp"
  for f in "$@"; do
    [[ -e "$f.ok" ]] && grep -h '^RESULT' "$f" >> "$out.tmp" || true
  done
  mv "$out.tmp" "$out"
  log "wrote $out ($(wc -l < "$out") result lines)"
}
