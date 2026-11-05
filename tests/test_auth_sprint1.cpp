// Unit tests for Sprint 1 (#2 foundation, #4 password/user store, #5 login/lockout/session).
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

#include "auth.hpp"
#include "file_store.hpp"
#include "password.hpp"
#include "result.hpp"
#include "rng.hpp"
#include "sim_clock.hpp"
#include "user_store.hpp"

#include "fakes.h"
#include "test_framework.h"
#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;


namespace tms {

static std::string tmpPath(const std::string& name) {
  return (fs::temp_directory_path() / ("tms_sprint1_" + name)).string();
}

// ---------------- Issue #2 ----------------
TEST(sprint1_result) {
  auto ok = Result<int>::Ok(42);
  CHECK(ok.ok() && ok.value() == 42);
  auto bad = Result<int>::Err(ErrorCode::NotFound, "missing");
  CHECK(!bad.ok() && bad.error().code == ErrorCode::NotFound && bad.error().message == "missing");
  CHECK(Result<void>::Ok().ok());
  auto badv = Result<void>::Err(ErrorCode::IoError, "disk");
  CHECK(!badv.ok() && badv.error().code == ErrorCode::IoError);
  auto p = Propagate<std::string>(bad);
  CHECK(!p.ok() && p.error().code == ErrorCode::NotFound);
}

TEST(sprint1_rng) {
  SeededRng a(123), b(123), c(124);
  bool same = true, differs = false;
  for (int i = 0; i < 100; ++i) {
    auto x = a.next(), y = b.next(), z = c.next();
    if (x != y) same = false;
    if (x != z) differs = true;
  }
  CHECK(same);
  CHECK(differs);
  SeededRng r(7);
  bool inRange = true;
  std::set<int> seen;
  for (int i = 0; i < 1000; ++i) {
    int v = r.uniformInt(3, 6);
    if (v < 3 || v > 6) inRange = false;
    seen.insert(v);
  }
  CHECK(inRange);
  CHECK(seen.size() == 4);
  bool unit = true;
  for (int i = 0; i < 1000; ++i) {
    double d = r.uniform01();
    if (d < 0.0 || d >= 1.0) unit = false;
  }
  CHECK(unit);
  r.reseed(7);
  SeededRng fresh(7);
  CHECK(r.next() == fresh.next());
}

TEST(sprint1_clock) {
  SimClock c;
  CHECK(c.now() == 0);
  c.advance(10);
  c.advance(5);
  CHECK(c.now() == 15);
  c.set(100);
  CHECK(c.now() == 100);
}

TEST(sprint1_file_store) {
  const std::string path = tmpPath("store.txt");
  FileStore fsx(path);
  fsx.remove();
  CHECK(!fsx.exists());
  CHECK(fsx.readLines().error().code == ErrorCode::NotFound);

  CHECK(fsx.writeLines({"alpha", "beta"}).ok());
  CHECK(fsx.exists());
  CHECK(!fs::exists(path + ".tmp"));  // atomic write leaves no temp file
  auto r = fsx.readLines();
  CHECK(r.ok() && r.value().size() == 2 && r.value()[0] == "alpha" && r.value()[1] == "beta");

  CHECK(fsx.appendLine("gamma").ok());
  r = fsx.readLines();
  CHECK(r.ok() && r.value().size() == 3 && r.value()[2] == "gamma");

  CHECK(fsx.writeLines({"only"}).ok());  // overwrite
  r = fsx.readLines();
  CHECK(r.ok() && r.value().size() == 1 && r.value()[0] == "only");

  CHECK(fsx.remove().ok());
  CHECK(!fsx.exists());
}

// ---------------- Issue #4 ----------------
TEST(sprint1_hash_primitives) {
  CHECK(toHex(sha256(toBytes("abc"))) ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  CHECK(toHex(sha256(toBytes(""))) ==
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  // RFC 6070-style vectors for PBKDF2-HMAC-SHA256
  CHECK(toHex(pbkdf2HmacSha256(toBytes("password"), toBytes("salt"), 1)) ==
        "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
  CHECK(toHex(pbkdf2HmacSha256(toBytes("password"), toBytes("salt"), 2)) ==
        "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43");
  CHECK(fromHex("zz").error().code == ErrorCode::InvalidArgument);
  CHECK(fromHex("abc").error().code == ErrorCode::InvalidArgument);
  CHECK(toHex(fromHex("00ff10").value()) == "00ff10");
}

TEST(sprint1_password_hashing) {
  SeededRng rng(1);
  std::string s1 = generateSaltHex(rng), s2 = generateSaltHex(rng);
  CHECK(s1.size() == 32 && s1 != s2);
  std::string h1 = hashPasswordHex("hunter2hunter2", s1, 1000);
  CHECK(h1 == hashPasswordHex("hunter2hunter2", s1, 1000));  // deterministic
  CHECK(h1 != hashPasswordHex("hunter2hunter2", s2, 1000));  // salt matters
  CHECK(h1 != hashPasswordHex("hunter2hunter3", s1, 1000));  // password matters
  CHECK(verifyPassword("hunter2hunter2", s1, h1, 1000));
  CHECK(!verifyPassword("wrong-password", s1, h1, 1000));
  CHECK(!verifyPassword("hunter2hunter2", s1, "deadbeef", 1000));
}

TEST(sprint1_user_store) {
  const std::string path = tmpPath("users.db");
  FileStore(path).remove();
  SeededRng rng(99);
  {
    UserStore us(FileStore(path), rng, 1000);
    CHECK(us.load().ok() && us.size() == 0);  // missing file = empty
    CHECK(us.addUser("admin", "Adm1nPassw0rd", Role::Admin).ok());
    CHECK(us.addUser("op.one", "OperatorPass1", Role::Operator).ok());
    CHECK(us.addUser("admin", "AnotherPass123", Role::Admin).error().code == ErrorCode::AlreadyExists);
    CHECK(us.addUser("bobby", "short", Role::Commuter).error().code == ErrorCode::WeakPassword);
    CHECK(us.addUser("a b", "LongEnoughPass1", Role::Commuter).error().code == ErrorCode::InvalidArgument);
    CHECK(us.addUser("x|y|z", "LongEnoughPass1", Role::Commuter).error().code == ErrorCode::InvalidArgument);
    CHECK(us.size() == 2);
  }
  {  // persistence across "restart"
    UserStore us(FileStore(path), rng, 1000);
    CHECK(us.load().ok() && us.size() == 2);
    auto a = us.authenticate("admin", "Adm1nPassw0rd");
    CHECK(a.ok() && a.value() == Role::Admin);
    auto o = us.authenticate("op.one", "OperatorPass1");
    CHECK(o.ok() && o.value() == Role::Operator);
    CHECK(us.authenticate("admin", "wrong").error().code == ErrorCode::AuthFailed);
    CHECK(us.authenticate("ghost", "whatever123").error().code == ErrorCode::AuthFailed);
    CHECK(us.removeUser("op.one").ok());
    CHECK(us.removeUser("op.one").error().code == ErrorCode::NotFound);
  }
  {
    UserStore us(FileStore(path), rng, 1000);
    CHECK(us.load().ok() && us.size() == 1 && us.exists("admin"));
  }
  {  // file never contains the plain password
    std::ifstream in(path);
    std::string all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(all.find("Adm1nPassw0rd") == std::string::npos);
    CHECK(all.find("admin|admin|") == 0);
  }
  {  // corrupt file is rejected, not crashed on
    FileStore(path).writeLines({"not-a-valid-record"});
    UserStore us(FileStore(path), rng, 1000);
    CHECK(us.load().error().code == ErrorCode::Corrupt);
    CHECK(us.size() == 0);
  }
  FileStore(path).remove();
}

// ---------------- Issue #5 ----------------
struct AuthFixture {
  std::string path = tmpPath("auth_users.db");
  SimClock clock;
  SeededRng rng{2024};
  UserStore users;
  AuthService auth;
  AuthFixture(AuthConfig cfg = {})
      : users((FileStore(path).remove(), FileStore(path)), rng, 500), auth(users, clock, rng, cfg) {
    users.load();
    users.addUser("alice", "CorrectHorse1", Role::Operator);
  }
  ~AuthFixture() { FileStore(path).remove(); }
};

TEST(sprint1_login) {
  AuthFixture f;
  auto s = f.auth.login("alice", "CorrectHorse1");
  CHECK(s.ok() && s.value().username == "alice" && s.value().role == Role::Operator && s.value().id != 0);
  CHECK(f.auth.activeSessions() == 1);
  auto bad = f.auth.login("alice", "nope-nope-nope");
  CHECK(bad.error().code == ErrorCode::AuthFailed);
  auto ghost = f.auth.login("ghost", "nope-nope-nope");
  CHECK(ghost.error().code == ErrorCode::AuthFailed);
  CHECK(ghost.error().message == bad.error().message);  // no user enumeration
  auto s2 = f.auth.login("alice", "CorrectHorse1");
  CHECK(s2.ok() && s2.value().id != s.value().id);  // unique session ids
}

TEST(sprint1_lockout) {
  AuthConfig cfg;
  cfg.maxFailedAttempts = 3;
  cfg.lockoutSeconds = 300;
  AuthFixture f(cfg);
  CHECK(f.auth.login("alice", "bad-password-1").error().code == ErrorCode::AuthFailed);
  CHECK(f.auth.login("alice", "bad-password-2").error().code == ErrorCode::AuthFailed);
  CHECK(f.auth.login("alice", "bad-password-3").error().code == ErrorCode::AuthFailed);  // the 3rd failure is still E-AUTH-001
  CHECK(f.auth.isLocked("alice"));
  // Correct password is still refused while locked.
  CHECK(f.auth.login("alice", "CorrectHorse1").error().code == ErrorCode::LockedOut);
  f.clock.advance(299);
  CHECK(f.auth.login("alice", "CorrectHorse1").error().code == ErrorCode::LockedOut);
  f.clock.advance(1);
  CHECK(!f.auth.isLocked("alice"));
  CHECK(f.auth.login("alice", "CorrectHorse1").ok());
  // Failure counter restarts after lockout expires.
  CHECK(f.auth.login("alice", "bad-password-1").error().code == ErrorCode::AuthFailed);
}

TEST(sprint1_success_resets_counter) {
  AuthFixture f;
  CHECK(f.auth.login("alice", "bad-password-1").error().code == ErrorCode::AuthFailed);
  CHECK(f.auth.login("alice", "bad-password-2").error().code == ErrorCode::AuthFailed);
  CHECK(f.auth.login("alice", "CorrectHorse1").ok());  // success resets
  CHECK(f.auth.login("alice", "bad-password-3").error().code == ErrorCode::AuthFailed);
  CHECK(f.auth.login("alice", "bad-password-4").error().code == ErrorCode::AuthFailed);
  CHECK(!f.auth.isLocked("alice"));
}

TEST(sprint1_session_timeout) {
  AuthConfig cfg;
  cfg.sessionTimeoutSeconds = 900;
  AuthFixture f(cfg);
  auto s = f.auth.login("alice", "CorrectHorse1");
  CHECK(s.ok());
  const auto id = s.value().id;
  f.clock.advance(899);
  CHECK(f.auth.validateSession(id).ok());   // activity refreshes the timer
  f.clock.advance(899);
  CHECK(f.auth.validateSession(id).ok());   // still alive: idle < 900
  f.clock.advance(900);
  CHECK(f.auth.validateSession(id).error().code == ErrorCode::SessionExpired);
  CHECK(f.auth.activeSessions() == 0);
  CHECK(f.auth.validateSession(id).error().code == ErrorCode::SessionExpired);  // stays gone
  CHECK(f.auth.validateSession(12345).error().code == ErrorCode::SessionExpired);
}

TEST(sprint1_logout) {
  AuthFixture f;
  auto s = f.auth.login("alice", "CorrectHorse1");
  CHECK(f.auth.validateSession(s.value().id).ok());
  f.auth.logout(s.value().id);
  CHECK(f.auth.activeSessions() == 0);
  CHECK(!f.auth.validateSession(s.value().id).ok());
}

TEST(sprint1_determinism) {
  // Same seed => same session ids (repeatable simulation runs).
  AuthFixture a, b;
  auto sa = a.auth.login("alice", "CorrectHorse1");
  auto sb = b.auth.login("alice", "CorrectHorse1");
  CHECK(sa.ok() && sb.ok() && sa.value().id == sb.value().id);
}

}  // namespace tms

namespace tms {

static int countAction(const FakeAudit& a, const std::string& action) {
  int n = 0;
  for (const auto& e : a.entries) if (e.action == action) ++n;
  return n;
}

TEST(sprint1_defaults_follow_the_srs) {
  AuthConfig cfg;
  CHECK_EQ(cfg.maxFailedAttempts, 3u);
  CHECK_EQ(cfg.lockoutSeconds, 60u);            // SEC-03
  CHECK_EQ(cfg.sessionTimeoutSeconds, 600u);    // SEC-04: 10 minutes
  CHECK_EQ(kDefaultIterations, 10000u);         // SEC-02
}

TEST(sprint1_tc01_username_and_password_policy) {
  const std::string path = tmpPath("tc01_users.db");
  FileStore(path).remove();
  SeededRng rng(5);
  UserStore us(FileStore(path), rng, 50);
  CHECK(us.load().ok());
  CHECK_EQ(us.addUser("abc", "Pass1234", Role::Operator).error().code, ErrorCode::InvalidArgument);     // 3 chars
  CHECK(us.addUser("abcd", "Pass1234", Role::Operator).ok());                                            // 4 chars
  CHECK(us.addUser(std::string(20, 'u'), "Pass1234", Role::Operator).ok());                              // 20 chars
  CHECK_EQ(us.addUser(std::string(21, 'u'), "Pass1234", Role::Operator).error().code, ErrorCode::InvalidArgument);
  CHECK_EQ(us.addUser("oper1", "Pass123", Role::Operator).error().code, ErrorCode::WeakPassword);        // 7 chars
  CHECK_EQ(us.addUser("oper1", "abcdefgh", Role::Operator).error().code, ErrorCode::WeakPassword);       // no digit
  CHECK_EQ(us.addUser("oper1", "12345678", Role::Operator).error().code, ErrorCode::WeakPassword);       // no letter
  CHECK_EQ(us.addUser("oper1", std::string(256, 'a') + "1", Role::Operator).error().code, ErrorCode::WeakPassword);  // 257
  CHECK(us.addUser("oper1", "Pass1234", Role::Operator).ok());
  CHECK(us.addUser("oper2", "Pass1234", Role::Operator).ok());       // same password for two users
  FileStore(path).remove();
}

TEST(sprint1_tc01_same_password_gives_different_stored_hashes_and_no_plaintext) {
  const std::string path = tmpPath("tc01b_users.db");
  FileStore(path).remove();
  SeededRng rng(6);
  UserStore us(FileStore(path), rng, 50);
  CHECK(us.load().ok());
  CHECK(us.addUser("oper1", "Pass1234", Role::Operator).ok());
  CHECK(us.addUser("oper2", "Pass1234", Role::Operator).ok());
  auto lines = FileStore(path).readLines();
  CHECK(lines.ok() && lines.value().size() == 2);
  const auto a = lines.value()[0], b = lines.value()[1];
  const auto hashOf = [](const std::string& l) { return l.substr(l.find('|', l.find('|') + 1) + 1); };
  CHECK(hashOf(a) != hashOf(b));                                        // salt and hash both differ
  CHECK(a.find("Pass1234") == std::string::npos && b.find("Pass1234") == std::string::npos);
  FileStore(path).remove();
}

TEST(sprint1_tc02_lockout_exactly_as_in_the_test_plan_with_audit_counts) {
  AuthFixture f;                       // default configuration
  FakeAudit audit;
  f.auth.setAuditLogger(&audit);
  CHECK_EQ(f.auth.login("alice", "wrong-pass-1").error().code, ErrorCode::AuthFailed);     // attempt 1: E-AUTH-001
  CHECK_EQ(f.auth.login("alice", "wrong-pass-2").error().code, ErrorCode::AuthFailed);     // attempt 2: E-AUTH-001
  CHECK_EQ(f.auth.login("alice", "wrong-pass-3").error().code, ErrorCode::AuthFailed);     // attempt 3: E-AUTH-001
  CHECK_EQ(f.auth.login("alice", "CorrectHorse1").error().code, ErrorCode::LockedOut);     // attempt 4: E-AUTH-002
  f.clock.advance(59);
  CHECK_EQ(f.auth.login("alice", "CorrectHorse1").error().code, ErrorCode::LockedOut);     // still locked at 59 s
  f.clock.advance(1);
  CHECK(f.auth.login("alice", "CorrectHorse1").ok());                                      // after 60 s: success
  CHECK_EQ(countAction(audit, "LOGIN_FAIL"), 3);
  CHECK_EQ(countAction(audit, "ACCOUNT_LOCKED"), 1);
  CHECK_EQ(countAction(audit, "LOGIN_BLOCKED"), 2);
  CHECK_EQ(countAction(audit, "LOGIN_OK"), 1);
}

TEST(sprint1_tc03_admin_can_set_a_1_minute_timeout) {
  AuthFixture f;
  FakeAudit audit;
  f.auth.setAuditLogger(&audit);
  f.auth.setSessionTimeout(60);
  CHECK_EQ(f.auth.sessionTimeout(), 60u);
  auto s = f.auth.login("alice", "CorrectHorse1");
  CHECK(s.ok());
  f.clock.advance(61);
  CHECK_EQ(f.auth.validateSession(s.value().id).error().code, ErrorCode::SessionExpired);   // E-AUTH-003
  CHECK(audit.has("SESSION_EXPIRED"));
}

TEST(sprint1_default_session_lasts_10_minutes) {
  AuthFixture f;
  auto s = f.auth.login("alice", "CorrectHorse1");
  f.clock.advance(599);
  CHECK(f.auth.validateSession(s.value().id).ok());
  f.clock.advance(600);
  CHECK_EQ(f.auth.validateSession(s.value().id).error().code, ErrorCode::SessionExpired);
}

TEST(sprint1_audit_has_login_ok_fail_logout_and_never_a_password) {
  AuthFixture f;
  FakeAudit audit;
  f.auth.setAuditLogger(&audit);
  CHECK(!f.auth.login("alice", "SecretWrong99").ok());
  auto s = f.auth.login("alice", "CorrectHorse1");
  CHECK(s.ok());
  f.auth.logout(s.value().id);
  CHECK(audit.has("LOGIN_FAIL") && audit.has("LOGIN_OK") && audit.has("LOGOUT"));
  for (const auto& e : audit.entries) {
    for (const std::string& secret : {std::string("SecretWrong99"), std::string("CorrectHorse1")}) {
      CHECK(e.user.find(secret) == std::string::npos);
      CHECK(e.action.find(secret) == std::string::npos);
      CHECK(e.result.find(secret) == std::string::npos);
    }
  }
}

TEST(sprint1_unknown_users_are_not_tracked_or_lockable) {
  AuthFixture f;
  FakeAudit audit;
  f.auth.setAuditLogger(&audit);
  for (int i = 0; i < 10; ++i) {
    CHECK_EQ(f.auth.login("ghost" + std::to_string(i % 2), "whatever-123").error().code, ErrorCode::AuthFailed);
  }
  CHECK(!f.auth.isLocked("ghost0"));                    // never "locked" -> answer never differs from a wrong password
  CHECK_EQ(countAction(audit, "LOGIN_FAIL"), 10);       // still audited
  CHECK_EQ(countAction(audit, "ACCOUNT_LOCKED"), 0);
}

TEST(sprint1_login_input_is_length_limited_to_256) {
  AuthFixture f;
  CHECK_EQ(f.auth.login("", "CorrectHorse1").error().code, ErrorCode::InvalidArgument);
  CHECK_EQ(f.auth.login("alice", "").error().code, ErrorCode::InvalidArgument);
  CHECK_EQ(f.auth.login(std::string(257, 'a'), "CorrectHorse1").error().code, ErrorCode::InvalidArgument);
  CHECK_EQ(f.auth.login("alice", std::string(257, 'a')).error().code, ErrorCode::InvalidArgument);
}

#ifndef _WIN32
TEST(sprint1_user_file_is_owner_only_0600) {
  const std::string path = tmpPath("perm_users.db");
  FileStore(path).remove();
  SeededRng rng(8);
  UserStore us(FileStore(path, true), rng, 50);
  CHECK(us.load().ok());
  CHECK(us.addUser("oper1", "Pass1234", Role::Operator).ok());
  struct stat st{};
  CHECK(stat(path.c_str(), &st) == 0);
  CHECK_EQ(static_cast<int>(st.st_mode & 0777), 0600);
  FileStore(path).remove();
}
#endif

}  // namespace tms
