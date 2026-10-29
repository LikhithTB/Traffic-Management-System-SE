# Team workflow (Team E16)

## Rules
1. **Never push to `main`.** One story = one branch = one Pull Request.
2. Branch name: `feature/story-<issue#>-<short-name>` (e.g. `feature/story-7-sim-engine`).
3. Commit messages: `feat: ... (#7)`, `fix: ... (#7)`, `test: ... (#7)`, `docs: ...`.
4. PR description starts with `Closes #<issue>` and states how it was tested.
5. The **Build** check must be green. The code must compile with zero warnings (`make`).
6. Reviewer approves against the issue's acceptance criteria; repo owner merges.
7. Edit only the files you own (table below). Need a change elsewhere? Ask the owner.
8. Before every new story: `git checkout main` then `git pull`.

## Build
```
make        # build ./tms  (g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror)
make run
make test   # build and run all unit tests in tests/
make clean
```
On Windows use `mingw32-make` instead of `make`.

## Layout and ownership (Sprint 1)
| Folder / file | Owner |
|---|---|
| `src/common/`, `src/main.cpp`, `Makefile`, `tests/test_framework.h`, `tests/fakes.h`, `src/app/dispatcher*`, `src/ui/*`, `src/sim/congestion_monitor*` | Likhith |
| `src/infra/file_store*`, `src/infra/rng*`, `src/infra/sim_clock*`, `src/auth/*` | Manoj |
| `src/infra/audit_logger*`, `src/sim/simulation_engine*`, `src/sim/emergency_handler*` | Devashruth |
| `src/sim/signal_controller*`, `src/sim/intersection_manager*`, `src/app/config_manager*` | Manish |

**Module contracts:** the interfaces in `src/common/interfaces.h` (IAuthService, IAuditLogger, IRandom,
ISignalController) are agreed by everyone. Implement them exactly; changing a signature needs all four
members to agree. Test doubles for them are in `tests/fakes.h`.

**Tests:** every story adds tests in `tests/test_<module>.cpp` using `TEST(name) { CHECK(...); }`.
CI runs `make test`; a failing test blocks the merge.

Each module is a `.h` / `.cpp` pair. Include shared headers as `#include "common/result.h"`.
Expected failures return `Result<T>` with an `ErrorCode` (see `src/common/error_code.h`).
