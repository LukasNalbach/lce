#!/usr/bin/env bash
# Generates Table 1 and Fig. 2-9 from RESULT files.
#
#   scripts/05-make-figures.sh [<results directory> [<output directory>]]
#
# Without arguments the measurements in results/ are used and the output is
# written to output/.

source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

results="$(realpath "${1:-$LCE_RESULTS_DIR}")"
output="$(realpath -m "${2:-$LCE_OUTPUT_DIR}")"
[[ -d $results ]] || die "$results does not exist"
command -v pdflatex >/dev/null \
    || die "pdflatex not found; run scripts/00-setup-tools.sh first"
mkdir -p "$output"

# The alphabet sizes of Table 1 are properties of the texts; when figures are
# made from another results directory, they are taken from results/.
stats="$results/text-stats.txt"
[[ -e $stats ]] || stats="$LCE_RESULTS_DIR/text-stats.txt"

python3 "$REPRO_DIR/plot/make_plots.py" --results "$results" --stats "$stats" \
    --templates "$REPRO_DIR/plot/paper/plots" --out "$output"

cp "$REPRO_DIR/plot/reproduced.tex" "$output/figures.tex"
(
  cd "$output"
  export TEXINPUTS=".:$REPRO_DIR/plot/paper:"
  for pass in 1 2; do
    pdflatex -interaction=nonstopmode -halt-on-error figures.tex > figures.out \
        || die "pdflatex failed, see $output/figures.log"
  done
  rm -f figures.aux figures.out
)
log "table:   $output/table-datasets.txt"
log "figures: $output/figures.pdf"
