# Coverage and complexity report (issue #19, NFR-05)

Requirement: unit-test **line coverage ≥ 70 %** and **cyclomatic complexity ≤ 10 per function**.

## Result
| Measure | Required | Measured |
|---|---|---|
| Unit tests (`make test`) | all pass | **144 tests, 0 failed checks** |
| Line coverage of `src/**/*.cpp` (without `main.cpp`) | ≥ 70 % | **96.54 %** (1480 of 1533 lines) |
| Functions with cyclomatic complexity > 10 | 0 | **0** (`lizard`: average complexity 3.0, 260 functions) |
| Memory / undefined-behaviour checks | no findings | AddressSanitizer + UBSan: clean (`-O0` and `-O2`) |

Measured with g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0 on Linux. **Re-run on the team machine and replace the numbers and the screenshot before the demo.**

## How to measure
```
bash tools/coverage.sh 70        # builds the tests with gcov, prints coverage per file and in total, fails below 70 %
bash tools/complexity.sh         # needs:  pip install lizard
```
Windows (PowerShell, compiler on PATH): run the same script with Git Bash, e.g. `& "C:\Program Files\Git\bin\bash.exe" tools/coverage.sh 70`.
GitHub Actions runs both checks on every push and pull request (`.github/workflows/build.yml`), so a drop below the limits blocks a merge.

## Coverage per file
| File | Line coverage | Lines |
|---|---|---|
| src/app/application.cpp | 96.80 % | 406 |
| src/app/auth_adapter.cpp | 83.75 % | 80 |
| src/app/config_manager.cpp | 99.24 % | 131 |
| src/app/dispatcher.cpp | 100.00 % | 16 |
| src/app/report_generator.cpp | 100.00 % | 71 |
| src/app/report_writer.cpp | 94.44 % | 18 |
| src/auth.cpp | 100.00 % | 66 |
| src/file_store.cpp | 88.14 % | 59 |
| src/infra/audit_logger.cpp | 86.49 % | 37 |
| src/password.cpp | 99.01 % | 101 |
| src/sim/congestion_monitor.cpp | 100.00 % | 35 |
| src/sim/emergency_handler.cpp | 100.00 % | 15 |
| src/sim/intersection_manager.cpp | 100.00 % | 46 |
| src/sim/signal_controller.cpp | 100.00 % | 77 |
| src/sim/simulation_engine.cpp | 98.66 % | 149 |
| src/ui/dashboard.cpp | 100.00 % | 38 |
| src/ui/validator.cpp | 97.18 % | 71 |
| src/user_store.cpp | 93.16 % | 117 |

`main.cpp` only creates the application object and is not counted; everything it calls is covered by the scripted
whole-program tests in `tests/test_application.cpp` (login, lockout, timeout, roles, simulation, dashboard,
emergency, reports, configuration, commuter view, corrupt files, end of input).

## Test plan traceability
| Test Plan case | Automated tests |
|---|---|
| TC-01 accounts, password policy, hashes differ | `sprint1_tc01_*`, `app_create_user_validation_messages` |
| TC-02 lockout after 3 failures, unlock after 60 s, audit counts | `sprint1_tc02_*`, `app_tc02_*` |
| TC-03 session timeout | `sprint1_tc03_*`, `app_tc03_*` |
| TC-04 operator cannot use admin functions | `app_tc04_*`, `dispatcher_*` |
| Emergency vehicle priority (FR-04) | `emergency_*`, `integration_emergency_*`, `app_emergency_*` |
| Report export, unsafe file name, create-new only (FR-06, SEC-08) | `report_*`, `app_simulation_dashboard_and_report_export` |
| Same seed, same results (NFR-08) | `engine_same_seed_*`, `app_same_seed_*`, `report_same_seed_*` |
