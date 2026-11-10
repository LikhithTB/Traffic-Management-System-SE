# User guide – Traffic Management System (Simulation)

## Build and run
```
make            # build ./tms   (Windows: mingw32-make)
./tms           # Windows: .\tms.exe
make test       # run all unit tests
```
Files are created in the folder you start the program from: `users.dat` (accounts, owner-only), `config.ini`
(settings and intersections), `audit.log` (append-only audit trail, owner-only) and `reports/` (exported reports).

## First start
If no account exists, the program asks for the first **administrator** account (username 4–20 characters,
letters/digits/`_ . -`; password at least 8 characters with a letter and a digit). Passwords are stored only as
salted PBKDF2-SHA256 hashes. One example intersection is created automatically; change it under *Manage intersections*.

## Getting help (NFR-04)
Every menu ends with the prompt `Choice (0-11, h = help)`. Type **h** (or `help`, `?`) there and press Enter to see
what each option of that screen does, its limits and a quick start. Menus are at most three levels deep:

```
MAIN MENU ─┬─ 1 Log in ── OPERATIONS MENU ─┬─ 9  Intersections
           │                               ├─ 10 System parameters (Admin)
           │                               └─ 11 User accounts (Admin)
           └─ 2 Commuter view
```
**Quick start:** main menu **1** (log in) → **1** (start simulation) → **2** (run, e.g. 60 seconds). The dashboard shows
one row per lane: signal, queue length and congestion (Low / Moderate / High).

## Main menu
| Choice | Action |
|---|---|
| 1 | Log in (3 wrong passwords lock the account for 60 s) |
| 2 | Commuter view: read-only congestion status and saved reports, no login |
| 0 | Exit |

## Operations menu (after login; an idle session ends after the configured timeout, default 10 min)
| Choice | Action | Role |
|---|---|---|
| 1 | Start a simulation (uses seed, intersections and thresholds from the configuration) | Operator, Admin |
| 2 | Run the simulation for N simulated seconds with the live dashboard | Operator, Admin |
| 3 | Pause / resume | Operator, Admin |
| 4 | Stop | Operator, Admin |
| 5 | Show the dashboard (signal, queue, congestion per lane) | Operator, Admin |
| 6 | Change the green time of an intersection (5–120 s, applies from the next cycle) | Operator, Admin |
| 7 | Inject an emergency vehicle (the signal gives it priority) | Operator, Admin |
| 8 | Generate a CSV or TXT report | Operator, Admin |
| 9 | Manage intersections (list / add / edit / remove, max 10, 1–8 lanes) | Operator, Admin |
| 10 | Configure system parameters (seed, congestion thresholds, session timeout, speed 1×/10×/60×, adaptive signal timing on/off) | Admin |
| 11 | Manage user accounts (create, list) | Admin |
| 0 | Log out | |

Commands a role may not use are refused with `[E-AUTH-004]` and written to the audit log.

## Adaptive signal timing (FR-09)
Off by default; an administrator switches it on under *Configure system parameters → 5*. While it is on, a green lane
whose queue is **High** stays green 5 s longer at a time – at most +20 s and never more than 140 s of green in total – as
long as every other lane of that intersection is below **Moderate**. Then the normal 3 s yellow follows. An emergency
vehicle still interrupts an extended green at once. The setting is saved in `config.ini` (`adaptive=1`) and audited.

## Commuter view (FR-10)
No login. Shows the congestion level of every lane and the completed reports (`.csv` / `.txt` in `reports/`) – no
signals, queue numbers or controls. Typing an operations-menu number (4–11) there is refused with `[E-AUTH-004]` and
recorded in the audit log.

## Reports
Statistics: throughput, average wait, maximum queue length, share of time at High congestion.
A report name has at most 64 characters, no path separators and no `..`; an existing report is never overwritten.
The same seed always produces the same report.

## Error codes shown to the user
`E-AUTH-001` wrong username/password · `E-AUTH-002` account locked · `E-AUTH-003` session expired ·
`E-AUTH-004` no permission · `E-VAL-00x` invalid input · `E-CFG-001` damaged config.ini (defaults used) ·
`E-IO-001` file cannot be read/written/already exists · `E-IO-002` unsafe file name ·
`E-SIM-001` no simulation running · `E-SIM-002` too many intersections.

## Troubleshooting on Windows
- **The program or the tests exit at once without any output** (exit code `0xC0000139`): Windows loaded an older
  `libstdc++-6.dll` from another program on the PATH. Build with the C++ runtime linked in:
  `mingw32-make test CXX="x86_64-w64-mingw32-g++ -static"` (and the same for `mingw32-make`).
- **"An Application Control policy has blocked this file"**: Windows Smart App Control blocks programs you compiled
  yourself. Run the program and tests in WSL (Ubuntu), or rely on the GitHub Actions *Build* check, which runs
  `make test` on every push and pull request.
