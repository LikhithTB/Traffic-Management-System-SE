# Deviations from the submitted SRS / Architecture Design Spec / Test Plan (Sprint 1)

Merge these entries into the team's main deviation list before the final demo.

| # | Where | Deviation | Reason / effect |
|---|---|---|---|
| 1 | Architecture 2.x | Auth, user store, password and file-store modules sit in `src/` (namespace `tms`), not `src/auth/` and `src/infra/`. `app/auth_adapter` connects them to the `IAuthService` contract. | Developed in parallel by one member; adapter keeps the dispatcher unchanged. |
| 2 | Test Plan TC-01..04 | Test users `op1` / `op2` are 3 characters, but SRS FR-07 requires 4–20. Use `oper1` / `oper2`. | SRS is the requirement; Test Plan data corrected. |
| 3 | Architecture 3.3 (users.dat) | `failed_count` and `lock_until` are kept in memory, not stored in `users.dat`. Restarting the program clears a lockout. | Planned improvement; users.dat stays smaller (salt, hash, iterations). |
| 4 | Architecture 3.2 | `IAuthService` has extra `logout`, `isExpired`, `touch`; `ISignalController` has `reset`; lanes are 0-based inside interfaces, 1-based on screen. | Needed for session handling and restarting a run. |
| 5 | SEC-02 | Password hash is PBKDF2-HMAC-SHA256 with 10,000 iterations and a 16-byte salt (the standard form of "SHA-256, 10,000 iterations"). | Same security goal, standard construction. |
| 6 | SRS assumption (machine clock) | Lockout and session timers run on the machine clock; `tms::SimClock` is fed with wall-clock seconds by `AuthAdapter`. | Required so "wait 60 s" / "idle 10 min" behave in real time. |
| 7 | FR-02 / FR-04 | Signals serve one lane at a time, in lane order (the SRS does not define phases). An emergency gets 3 s yellow on the open lane, then EmergencyGreen (≤ 5 s). | Design decision documented here. |
| 8 | FR-06 | "% High congestion" counts a simulated second as High if at least one lane is High. | Definition for the report. |
| 9 | FR-06 / SEC-08 | The create-new-only report writer is `app/report_writer`, not the File Store. | File Store only handles records (users.dat); same rules enforced. |
| 10 | First start | The first administrator is created interactively (no default password in the program). | Avoids a built-in credential. |
| 11 | Not in this change | Deactivate/delete accounts (FR-07, #12) and adaptive signal timing (FR-09, #14) are Sprint 2 items. | See the project board. |
