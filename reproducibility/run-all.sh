#!/usr/bin/env bash
# Runs all experiments of the paper and generates its table and figures.
#
#   ./run-all.sh [pizza_and_chili|all]
#
#   pizza_and_chili  the 12 Pizza&Chili texts (default)              about 6 hours
#   all              pizza_and_chili + wiki.txt, dna.txt, cc.txt     about 5 days
#
# The steps can also be run one by one (scripts/0*.sh); see README.md.

set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
[[ $# -ge 1 ]] && export LCE_TIER=$1

scripts/00-setup-tools.sh
scripts/01-build.sh
scripts/02-prepare-inputs.sh
scripts/03-run-lce.sh
scripts/04-run-pred.sh
scripts/05-make-figures.sh
