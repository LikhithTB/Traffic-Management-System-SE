#!/usr/bin/env bash
# Line coverage of the program code (src/**/*.cpp, without main.cpp) achieved by the unit tests.
# NFR-05: must be >= 70 %. Usage:  bash tools/coverage.sh [threshold]      (default threshold 70)
# Needs g++ and gcov of the same version (GCC). Output: build_cov/ (ignored by git).
set -euo pipefail
THRESHOLD="${1:-70}"
CXX="${CXX:-g++}"
GCOV="${GCOV:-gcov}"
cd "$(dirname "$0")/.."
OUT=build_cov
rm -rf "$OUT" && mkdir "$OUT"

OBJS=()
for f in $(find src -name '*.cpp' ! -name main.cpp | sort) $(ls tests/*.cpp | sort); do
  obj="$OUT/$(echo "${f%.cpp}" | tr '/' '_').o"
  "$CXX" -std=c++17 -O0 --coverage -Isrc -Itests -c "$f" -o "$obj"
  OBJS+=("$obj")
done
"$CXX" --coverage -o "$OUT/tests_cov" "${OBJS[@]}"

( cd "$OUT" && ./tests_cov > tests_cov.log 2>&1 ) || { tail -20 "$OUT/tests_cov.log"; echo "tests failed"; exit 1; }
tail -1 "$OUT/tests_cov.log"

# gcov per program object; only .cpp files of src/ are counted (headers are counted via their .cpp users)
: > "$OUT/summary.txt"
for obj in "$OUT"/src_*.o; do
  "$GCOV" -n "$obj" 2>/dev/null | awk '
    /^File / { name=$2; gsub(/\x27/, "", name) }
    /^Lines executed:/ { if (name ~ /^src[\/\\].*\.cpp$/) { split($2, a, ":"); pct=a[2]; sub(/%/, "", pct); n=$4; printf "%s %s %d\n", name, pct, n } }
  ' >> "$OUT/summary.txt"
done
sort -u "$OUT/summary.txt" -o "$OUT/summary.txt"

awk -v thr="$THRESHOLD" '
  { printf "  %-40s %6.2f %%  (%d lines)\n", $1, $2, $3; hit += $2 * $3 / 100; total += $3 }
  END {
    pct = (total > 0) ? 100 * hit / total : 0
    printf "\nTOTAL line coverage of src/*.cpp: %.2f %% (%d of %d lines)\n", pct, hit, total
    if (pct + 0.0001 < thr) { printf "FAIL: below the required %d %%\n", thr; exit 1 }
    printf "OK: at least %d %%\n", thr
  }' "$OUT/summary.txt"
