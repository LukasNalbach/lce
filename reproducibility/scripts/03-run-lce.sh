#!/usr/bin/env bash
# Runs the LCE experiments (Table 1, Fig. 3-9) and writes
#   results/lce.results.new.par   construction only, 1 to 64 threads
#   results/lce.results.new       construction and queries
# Every (text, data structure, thread count) is a separate run of bench-lce
# whose output is kept in results/raw/. Completed runs are skipped when the
# script is started again.

source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
require_binaries

construction=()
queries=()
for text in $(selected_texts); do
  text_path="$TEXT_DIR/$text"
  query_path="$QUERY_DIR/$text"
  [[ -r $text_path && -d $query_path ]] \
      || die "inputs of $text are missing; run scripts/02-prepare-inputs.sh first"

  # ------------------------------------------------------------------------
  # Construction without queries: single-threaded construction time and
  # memory, sizes of the synchronizing sets, and parallel scaling of sss512.
  # ------------------------------------------------------------------------
  log "$text: construction"
  algos=("${ALGOS_CONSTRUCTION[@]}")
  is_large "$text" && algos+=("${ALGOS_CONSTRUCTION_LARGE[@]}")
  for algo in "${algos[@]}"; do
    out="$RAW_DIR/lce-construction/$text/$algo.t1.txt"
    run_logged "$out" 1 "$BENCH_DIR/bench-lce" "$text_path" \
        --queries_path "$query_path" --from 0 --to 0 -a "$algo"
    construction+=("$out")
  done
  for threads in $LCE_SCALING_THREADS; do
    [[ $threads == 1 ]] && continue  # measured above
    out="$RAW_DIR/lce-construction/$text/$ALGO_SCALING.t$threads.txt"
    run_logged "$out" "$threads" "$BENCH_DIR/bench-lce" "$text_path" \
        --queries_path "$query_path" --from 0 --to 0 -a "$ALGO_SCALING"
    construction+=("$out")
  done

  # ------------------------------------------------------------------------
  # Construction followed by 20 batches of queries (LCE values in
  # [2^(k-1), 2^k) for k = 0..19). Queries are always answered by one thread.
  # ------------------------------------------------------------------------
  log "$text: queries"
  threads=1
  is_large "$text" && threads=$LCE_LARGE_QUERY_THREADS
  for algo in "${ALGOS_QUERIES[@]}"; do
    if is_large "$text" && contains "$algo" "${ALGOS_SKIPPED_ON_LARGE[@]}"; then
      continue
    fi
    out="$RAW_DIR/lce-queries/$text/$algo.txt"
    run_logged "$out" "$threads" "$BENCH_DIR/bench-lce" "$text_path" \
        --queries_path "$query_path" -q "$LCE_NUM_QUERIES" -a "$algo"
    queries+=("$out")
  done
done

collect_results "$LCE_RESULTS_DIR/lce.results.new.par" "${construction[@]}"
collect_results "$LCE_RESULTS_DIR/lce.results.new" "${queries[@]}"
if [[ -s $FAILED_LOG ]]; then
  log "some runs failed, see $FAILED_LOG"
fi
