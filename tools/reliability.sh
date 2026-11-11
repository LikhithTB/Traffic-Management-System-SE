#!/usr/bin/env bash
# NFR-03 / TC-15 (story #18): 24 simulated hours (86,400 ticks) at the maximum configuration
# (10 intersections x 8 lanes) with random emergencies, under plain, ASan+UBSan and Valgrind builds.
#
#   bash tools/reliability.sh [plain|asan|valgrind|all] [rate]      (default: all, rate 3)
#
# rate = arrivals per minute per lane. 3 is a load the signals can clear, so memory stays flat and the
# 10 % growth limit is checked. Any higher rate (up to 60) overloads the lanes: queues, and so memory,
# grow with the backlog. Such runs are still checked for crash, hang and sanitizer/Valgrind errors, but
# memory growth is only reported (see Phase2/Reliability_Report.md).
# Needs g++; Valgrind is optional (skipped with a notice if it is not installed). Output: build_rel/.
set -uo pipefail
MODE="${1:-all}"; RATE="${2:-3}"; TICKS="${TICKS:-86400}"; LIMIT=10
CXX="${CXX:-g++}"
cd "$(dirname "$0")/.."
OUT=build_rel; mkdir -p "$OUT"
SRCS=$(find src -name '*.cpp' ! -name main.cpp | sort)
FAIL=0

build() {  # name, flags...
  local name="$1"; shift
  "$CXX" -std=c++17 -Wall -Wextra -Wpedantic -Werror -Isrc "$@" -o "$OUT/$name" tools/reliability_run.cpp $SRCS || { echo "BUILD FAILED: $name"; exit 2; }
}

check_memory() {  # log file: enforce the 10 % limit only for a load the signals can clear
  local g
  g=$(awk '/growth start->end/ {gsub(/[+%]/,"",$4); print $4}' "$1")
  if [ "$RATE" -le 3 ]; then
    if awk -v g="$g" -v l="$LIMIT" 'BEGIN{exit !(g>l)}'; then echo "  FAIL: memory growth ${g} % > ${LIMIT} %"; FAIL=1
    else echo "  OK: memory growth ${g} % (limit ${LIMIT} %)"; fi
  else echo "  memory growth ${g} % is backlog growth at an overloaded rate: reported, not judged"; fi
}

run_plain() {
  echo "== plain -O2, $TICKS ticks, rate $RATE"
  build rel_plain -O2
  timeout 600 "$OUT/rel_plain" --ticks "$TICKS" --rate "$RATE" --quiet | tee "$OUT/plain_rate$RATE.log" | tail -12
  [ "${PIPESTATUS[0]}" -eq 0 ] && echo "  OK: no crash, hang or failed command" || { echo "  FAIL: run did not finish cleanly"; FAIL=1; }
  check_memory "$OUT/plain_rate$RATE.log"
}

run_asan() {
  echo "== AddressSanitizer + UBSan, $TICKS ticks, rate $RATE"
  build rel_asan -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer
  ASAN_OPTIONS=detect_leaks=1:abort_on_error=0 timeout 1800 "$OUT/rel_asan" --ticks "$TICKS" --rate "$RATE" --quiet > "$OUT/asan_rate$RATE.log" 2>&1
  local rc=$?; tail -6 "$OUT/asan_rate$RATE.log"
  if [ $rc -eq 0 ] && ! grep -q -E "ERROR: (Address|Leak)Sanitizer|runtime error:" "$OUT/asan_rate$RATE.log"; then echo "  OK: no ASan/UBSan/leak report"; else echo "  FAIL: exit code $rc"; FAIL=1; fi
}

run_valgrind() {
  command -v valgrind >/dev/null || { echo "== Valgrind: not installed, skipped (sudo apt install valgrind)"; return; }
  echo "== Valgrind memcheck, $TICKS ticks, rate $RATE"
  build rel_vg -O1 -g
  timeout 3000 valgrind --error-exitcode=3 --leak-check=full --errors-for-leak-kinds=definite,indirect,possible \
      --show-leak-kinds=definite,indirect,possible "$OUT/rel_vg" --ticks "$TICKS" --rate "$RATE" --quiet > "$OUT/valgrind_rate$RATE.log" 2>&1
  local rc=$?; grep -E "ERROR SUMMARY|definitely lost|indirectly lost|possibly lost|All heap blocks" "$OUT/valgrind_rate$RATE.log"
  if [ $rc -eq 0 ]; then echo "  OK: Valgrind reports 0 errors, no leaks"; else echo "  FAIL: exit code $rc (see $OUT/valgrind_rate$RATE.log)"; FAIL=1; fi
}

case "$MODE" in
  plain) run_plain ;;
  asan) run_asan ;;
  valgrind) run_valgrind ;;
  all) run_plain; run_asan; run_valgrind ;;
  *) echo "usage: $0 [plain|asan|valgrind|all] [rate]"; exit 2 ;;
esac
[ $FAIL -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $FAIL
