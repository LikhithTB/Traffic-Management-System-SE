# Reliability run – NFR-03 / TC-15 (issue #18)

**Requirement:** 24 simulated hours (86,400 ticks) at the maximum configuration without crash, hang or
memory growth above 10 %, verified with AddressSanitizer/Valgrind.
**How to repeat:** `bash tools/reliability.sh all 3` (plain, ASan+UBSan, Valgrind) and
`bash tools/reliability.sh all 60` (overload stress). Program: `tools/reliability_run.cpp`.

**Setup:** 10 intersections x 8 lanes, seed 42, real engine / signal controller / monitor / file audit
logger. During the run: 409 random emergencies, 86 green-time changes, 17 pause/resume, 144 dashboard
renders, 24 CSV reports. Machine: Ubuntu 24.04, g++ 13.3, Valgrind 3.22.

## Results

| Run | Load (arrivals/min/lane) | Result |
|---|---|---|
| Plain -O2 | 3 | 86,400 ticks in 0.4 s, 0 failed commands. Memory 4,112 kB -> 4,324 kB: **+5.2 %** (+0.8 % from hour 1 to the end). **PASS** |
| ASan + UBSan | 3 | No ASan, UBSan or leak report. **PASS** |
| Valgrind memcheck | 3 | 0 errors, all heap blocks freed. **PASS** |
| Plain / ASan / Valgrind | 60 (overload) | No crash, hang, error or leak. Memory grows with the backlog (see Finding 2). |

The final report checksum is identical in the plain and ASan builds (same seed -> same run, NFR-08).

## Finding 1 – the run used to hang (fixed)
`SimulationEngine::raw()` scanned every queued vehicle on every tick to set `hasEmergency`. With a
backlog the run time grew quadratically (rate 10: 3,600 ticks 0.2 s, 7,200 ticks 0.7 s, 14,400 ticks
4.3 s), so a full 24 h run at a high load did not finish in practice. Emergency vehicles are always
inserted in front of normal ones, so checking the first vehicle gives the same answer in O(1).
After the fix every 86,400-tick run takes under 1 s. Old and new code give identical reports (compared
at 4 load settings before the change was committed).

## Finding 2 – memory growth at overload (needs a team decision, see D-20)
Lane queues have no upper limit. One lane is served at a time (1 vehicle / 2 s), so a lane can only keep
up with about 3 arrivals per minute at 8 lanes. At a higher rate the backlog grows and memory grows
with it: at rate 60 there are 6,511,887 queued vehicles at the end. This is real data, not a leak
(Valgrind: all blocks freed, ASan: no leak). But a reader who counts rate 60 as part of "maximum
configuration" would see growth of several thousand percent.

Options: (a) accept NFR-03 as verified for a load the signals can clear and record this in D-20, or
(b) add a per-lane queue limit (design change, needs the whole team).

## Notes
- ASan RSS (about 400 MB) is inflated by ASan's own shadow memory and is not used for the 10 % check;
  memory is judged on the plain build.
