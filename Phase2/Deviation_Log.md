# Deviation log – Traffic Management System (Team E16)

Differences between the submitted Phase 1 documents (SRS, Architecture Design Spec, Test Plan) and the
delivered product. This is the final list for the demo: it merges the Sprint 1 list
([`Deviations_Sprint1.md`](Deviations_Sprint1.md), entries D-01 – D-10) with the Sprint 2 deviations
(D-11 onward). Open work that is still planned is listed separately at the end.

## Deviations

| # | Where | Deviation | Reason / effect |
|---|---|---|---|
| D-01 | Architecture 2.x | Auth, user store, password and file-store modules sit in `src/` (namespace `tms`), not `src/auth/` and `src/infra/`. `app/auth_adapter` connects them to the `IAuthService` contract. | Developed in parallel by one member; the adapter keeps the dispatcher unchanged. |
| D-02 | Test Plan TC-01..04 | Test users `op1` / `op2` are 3 characters, but SRS FR-07 requires 4–20. Use `oper1` / `oper2`. | SRS is the requirement; Test Plan data corrected. |
| D-03 | Architecture 3.3 (users.dat) | `failed_count` and `lock_until` are kept in memory, not stored in `users.dat`. Restarting the program clears a lockout. | users.dat stays smaller (salt, hash, iterations). |
| D-04 | Architecture 3.2 | `IAuthService` has extra `logout`, `isExpired`, `touch`; `ISignalController` has `reset`; lanes are 0-based inside interfaces, 1-based on screen. | Needed for session handling and restarting a run. |
| D-05 | SEC-02 | Password hash is PBKDF2-HMAC-SHA256 with 10,000 iterations and a 16-byte salt. | Standard construction of "SHA-256, 10,000 iterations". |
| D-06 | SRS assumption (machine clock) | Lockout and session timers run on the machine clock; `tms::SimClock` is fed with wall-clock seconds by `AuthAdapter`. | "Wait 60 s" / "idle 10 min" behave in real time. |
| D-07 | FR-02 / FR-04 | Signals serve one lane at a time, in lane order (the SRS does not define phases). An emergency gets 3 s yellow on the open lane, then EmergencyGreen (≤ 5 s). | Design decision. |
| D-08 | FR-06 | "% High congestion" counts a simulated second as High if at least one lane is High. | Definition for the report. |
| D-09 | FR-06 / SEC-08 | The create-new-only report writer is `app/report_writer`, not the File Store. | File Store only handles records (users.dat); same rules enforced. |
| D-10 | First start | The first administrator is created interactively (no default password in the program). | Avoids a built-in credential. |
| D-11 | FR-09 | "Opposing queues" = **every other lane of the same intersection**. | Follows from D-07: only one lane is open at a time, so every waiting lane opposes the green one. |
| D-12 | FR-09 | The extension is decided when a green is about to end: +5 s at a time while the condition still holds, max +20 s and 140 s green in total, then the normal 3 s yellow. The dashboard header `[green N s]` keeps showing the configured green, not the extended one. | Re-checking every 5 s stops the extension as soon as the queue clears or another lane reaches Moderate. |
| D-13 | Architecture console command set (`adaptive on\|off`) | Adaptive timing is switched by an **Administrator** under *System parameters → 5*, saved as `adaptive=0\|1` in `config.ini` and audited as `PARAM_CHANGE`. Operators cannot switch it. | It is a system-wide parameter like the speed (FR-05); persisted and audited like the others (SEC-07). |
| D-14 | Architecture console command set / screens | Text commands (`sim start`, `config set`, `help` …) are implemented as **numbered menus**. "help" is typed as `h`, `help` or `?` at any `Choice` prompt; the prompt itself is the help line: `Choice (0-11, h = help)`. | Numbered menus are easier for first-time users (NFR-04) and are validated with one rule. |
| D-15 | FR-10 / TC-13 | The Commuter view offers only congestion and reports. "Direct commands" are the Operations-menu numbers 4–11 typed in the Commuter view: they are refused with `E-AUTH-004` and audited as `anonymous \| DENIED:<command>`. | Shows the TC-13 refusal without offering any control option. |
| D-16 | FR-10 ("completed reports") | A completed report is a `.csv` or `.txt` file in `reports/`. Other files in that folder are neither listed nor readable from the Commuter view. | The report writer only creates `.csv` / `.txt`; anything else is not a report. |
| D-17 | Architecture (Commuter Read-only View) | The view is `ui/commuter_view`. It reads `SimulationEngine::snapshot()` through a **const** reference (the engine asks the monitor), not `Monitor.snapshot()` directly. | Same data; the const reference makes "no write calls" a compile-time guarantee. |
| D-18 | NFR-04 / TC-17 | The usability test needs five first-time users and is run manually (sheet: [`Usability_Test_TC17.md`](Usability_Test_TC17.md)). An automated test (`app_tc17_*`) checks that the task can be completed with the on-screen help alone. | A program cannot replace real testers; the automated test guards the help text. |
| D-19 | FR-07 / UC-07 | *Deactivate* keeps the account record (`users.dat` gets an optional 6th field `inactive`; old files still load) and ends any open session of that user at once; *delete* removes the record after a yes/no confirmation. An administrator cannot deactivate or delete their own account, which also keeps the last active administrator safe. The menu has no *reactivate* option. | SRS FR-07 asks for create, deactivate and delete only. The self-removal rule prevents locking everybody out. |
| D-20 | NFR-03 / TC-15 | Memory growth is within 10 % (+5.2 %) at 3 arrivals/min/lane, the highest load the signals can clear. At higher rates (up to 60) lane queues have no limit, so the backlog and memory grow with it (not a leak: ASan and Valgrind are clean). Details: [`Reliability_Report.md`](Reliability_Report.md). | The SRS gives no queue limit. **Open: the team decides to accept this reading or add a queue limit.** |

## Open items (planned, not yet delivered) – update before the final demo

| Item | Requirement | Owner | Status |
|---|---|---|---|
| #12 Admin user management – deactivate and delete accounts | FR-07 | Manoj | Done. *Manage user accounts* offers create, list, deactivate and delete (see D-19). |
| #18 24-hour reliability run with ASan/Valgrind | NFR-03 | Manoj | Done. 86,400 ticks pass under plain, ASan/UBSan and Valgrind; see [`Reliability_Report.md`](Reliability_Report.md) and D-20. |
| TC-17 results | NFR-04 | Team | Run the sheet with five testers and fill in the results. |
