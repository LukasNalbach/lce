#!/usr/bin/env bash
# Runs the successor experiment (Fig. 2) and writes results/pred.results.new.
# The successor data structures are built on the string synchronizing sets
# of the texts for tau = 256, 512, 1024 and 2048.

source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
require_binaries
mkdir -p "$SSS_DIR"

runs=()
for text in $(selected_texts); do
  text_path="$TEXT_DIR/$text"
  [[ -r $text_path ]] \
      || die "$text is missing; run scripts/02-prepare-inputs.sh first"

  log "$text: computing the synchronizing sets"
  run_logged "$RAW_DIR/gen-sss/$text.txt" "$LCE_PREP_THREADS" \
      "$BENCH_DIR/gen-sss" "$text_path" -a all -o "$SSS_DIR"

  log "$text: successor queries"
  for tau in 256 512 1024 2048; do
    sss="$SSS_DIR/$text.sss$tau"
    [[ -s $sss ]] || { log "  $sss is missing, skipped"; continue; }
    out="$RAW_DIR/pred/$text.sss$tau.txt"
    run_logged "$out" 1 "$BENCH_DIR/bench-pred" "$sss" -a all \
        -q "$LCE_NUM_PRED_QUERIES"
    runs+=("$out")
  done
done

collect_results "$LCE_RESULTS_DIR/pred.results.new" "${runs[@]}"
if [[ -s $FAILED_LOG ]]; then
  log "some runs failed, see $FAILED_LOG"
fi
