#!/usr/bin/env bash
# NFR-05: cyclomatic complexity <= 10 per function. Needs Python:  pip install lizard
# Usage: bash tools/complexity.sh        (prints the functions over the limit and fails if there are any)
set -euo pipefail
cd "$(dirname "$0")/.."
python3 -m lizard src -l cpp -C 10 -w && echo "OK: no function in src/ has a cyclomatic complexity above 10"
