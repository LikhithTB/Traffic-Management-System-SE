# Usability test TC-17 (NFR-04)

**Requirement (SRS NFR-04):** at least 4 of 5 first-time testers start a simulation and read a lane's
congestion status within 5 minutes using only the on-screen help; menus are at most 3 levels deep.

## Preparation
1. Build the program (`make`, Windows: `mingw32-make`) and delete `users.dat`, `config.ini` and `audit.log`.
2. Start `./tms` (Windows: `.\tms.exe`) and create the administrator account yourself, then create one
   Operator account for the testers (*Operations → 11 → 1*). Log out so the **MAIN MENU** is on screen.
3. Testers must not have used the program before. Do not explain anything; only read the task card.

## Task card (read aloud, then start the timer)
> "Log in with the username and password on this card. Start a traffic simulation and tell me the
> congestion level of **lane 2**. If you are stuck, type **h** and press Enter."

## Pass criteria per tester
- Says the congestion of lane 2 (Low / Moderate / High) correctly, as shown on the dashboard.
- Time from start of the task to the answer ≤ 5 minutes.
- Did not need help from the observer.

## Results
| Tester | Date | Time (mm:ss) | Answer correct? | Used `h`? | Observer help needed? | Pass? | Notes |
|---|---|---|---|---|---|---|---|
| 1 | | | | | | | |
| 2 | | | | | | | |
| 3 | | | | | | | |
| 4 | | | | | | | |
| 5 | | | | | | | |

**Result:** ___ of 5 passed (required: ≥ 4). Observer: __________

## Menu depth (checked, also by the automated test `app_menus_are_at_most_three_levels_deep`)
| Level 1 | Level 2 | Level 3 |
|---|---|---|
| Main menu | Operations menu (after login) | Intersections · System parameters · User accounts |
| Main menu | Commuter view | – |

The task above needs only levels 1 and 2: **1** (log in) → **1** (start) → **2** (run, e.g. 60 s).
