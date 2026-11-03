# Sprint 1 - Manoj (#2, #4, #5)

Build and test: `make test` (92 checks) | memory/UB check: `make asan`
Requires g++ with C++17.

| Issue | Files |
|---|---|
| #2 File store, Result<T>, seeded RNG, sim clock | include/result.hpp, rng.hpp, sim_clock.hpp, file_store.hpp, src/file_store.cpp |
| #4 Password hashing and user store | include/password.hpp, user_store.hpp, src/password.cpp, src/user_store.cpp |
| #5 Login, lockout, session timeout | include/auth.hpp, src/auth.cpp |
| Tests for all three | tests/test_sprint1.cpp |

## Design notes
- Passwords: PBKDF2-HMAC-SHA256, per-user salt, 10,000 iterations (verified against known test vectors). Dependency-free.
- User file line: `username|role|saltHex|hashHex|iterations`. Writes are atomic (temp file + rename).
- Lockout: 3 consecutive failures -> 300 simulated seconds. Correct password is refused while locked. Success resets the counter.
- Session: idle timeout 900 simulated seconds; `validateSession` refreshes the timer.
- Defaults live in `AuthConfig` - change them there if the issue/SRS specifies other values.
- Salts and session ids come from the seeded RNG so simulation runs are repeatable (a real system would use an OS CSPRNG).

## Closing the issues
One branch + PR per issue, with `Closes #N` in the PR description:
```
git checkout -b feature/issue-2-foundation
git add include/result.hpp include/rng.hpp include/sim_clock.hpp include/file_store.hpp src/file_store.cpp
git commit -m "Add Result<T>, seeded RNG, sim clock, file store (#2)"
git push -u origin feature/issue-2-foundation
```
Merge order: #2, then #4, then #5. (Tests span all three; add each test group with its PR.)
