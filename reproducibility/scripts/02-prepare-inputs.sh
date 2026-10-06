#!/usr/bin/env bash
# Provides the inputs of the selected texts:
#   work/texts/<text>             the text
#   work/queries/<text>/lce_<k>   LCE queries whose result is in [2^(k-1), 2^k)
#   results/text-stats.txt        alphabet sizes etc. for Table 1
# Inputs that already exist on this machine are linked; otherwise the texts
# are downloaded and the queries are generated.

source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
require_binaries
mkdir -p "$TEXT_DIR" "$QUERY_DIR" "$TMP_DIR"

file_size() { stat -L -c %s "$1"; }

# is_expected_text <file> <text>: the file has the size and, where one is
# known, the SHA-256 checksum of the text used in the paper.
is_expected_text() {
  local file=$1 text=$2 sum="${TEXT_SHA256[$2]:-}"
  [[ -f $file && -r $file && $(file_size "$file") == "${TEXT_SIZE[$text]}" ]] \
      || return 1
  [[ -z $sum ]] && return 0
  if [[ $(sha256sum "$file" | cut -d' ' -f1) != "$sum" ]]; then
    log "$text: $file has the right size but a different content, not used"
    return 1
  fi
}

prepare_text() {
  local text=$1 target="$TEXT_DIR/$1" dir
  [[ -n ${TEXT_SIZE[$text]:-} ]] || die "unknown text '$text'"
  [[ -e $target ]] && is_expected_text "$target" "$text" && return
  rm -f "$target"
  local dirs
  IFS=: read -ra dirs <<< "$LCE_TEXT_SEARCH_PATH"
  for dir in "${dirs[@]}"; do
    if is_expected_text "$dir/$text" "$text"; then
      ln -s "$dir/$text" "$target"
      log "$text: using $dir/$text"
      return
    fi
  done
  local url="${TEXT_URL[$text]:-}"
  [[ -n $url ]] || die "$text (${TEXT_SIZE[$text]} bytes) was not found in \
$LCE_TEXT_SEARCH_PATH and cannot be downloaded; see README.md"
  log "$text: downloading $url"
  wget -q -O "$target.gz" "$url"
  gunzip -f "$target.gz"
  is_expected_text "$target" "$text" \
      || die "$target is not the expected text (size or checksum differs)"
}

has_queries() {
  local k
  for k in $(seq 0 20); do
    [[ -r "$1/lce_$k" ]] || return 1
  done
}

prepare_queries() {
  local text=$1 target="$QUERY_DIR/$1" dir
  has_queries "$target" && return
  rm -rf "$target"
  local dirs
  IFS=: read -ra dirs <<< "$LCE_QUERY_SEARCH_PATH"
  for dir in "${dirs[@]}"; do
    if has_queries "$dir/$text"; then
      ln -s "$dir/$text" "$target"
      log "$text: using the queries in $dir/$text"
      return
    fi
  done
  # gen-sa-lcp builds the suffix and LCP array in RAM (about 25 bytes per
  # character); this is not feasible for the large texts.
  is_large "$text" && die "no queries for $text were found in \
$LCE_QUERY_SEARCH_PATH and they cannot be generated on this machine; see README.md"
  log "$text: generating queries (suffix array and LCP array)"
  local sa="$TMP_DIR/$text.sa5" lcp="$TMP_DIR/$text.lcp5"
  OMP_NUM_THREADS=$LCE_PREP_THREADS \
      "$BENCH_DIR/gen-sa-lcp" "$TEXT_DIR/$text" "$sa" "$lcp" > /dev/null
  mkdir -p "$target.tmp"
  "$BENCH_DIR/gen-queries" "$TEXT_DIR/$text" --sa "$sa" --lcp "$lcp" \
      -o "$target.tmp" > "$target.tmp/gen-queries.log"
  rm -f "$sa" "$lcp"
  mv "$target.tmp" "$target"
}

stats=()
for text in $(selected_texts); do
  prepare_text "$text"
  prepare_queries "$text"
  run_logged "$RAW_DIR/text-stats/$text.txt" "$LCE_PREP_THREADS" \
      "$BENCH_DIR/text-stats" "$TEXT_DIR/$text"
  stats+=("$RAW_DIR/text-stats/$text.txt")
done
collect_results "$LCE_RESULTS_DIR/text-stats.txt" "${stats[@]}"
log "inputs are ready in $LCE_WORK_DIR"
