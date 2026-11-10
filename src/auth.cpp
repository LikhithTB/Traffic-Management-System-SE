#include "auth.hpp"

namespace tms {

namespace {
constexpr std::size_t kMaxInput = 256;   // NFR-06

bool badInput(const std::string& user, const std::string& password) {
  return user.empty() || password.empty() || user.size() > kMaxInput || password.size() > kMaxInput;
}
}

AuthService::AuthService(const UserStore& users, SimClock& clock, SeededRng& rng, AuthConfig cfg)
    : users_(users), clock_(clock), rng_(rng), cfg_(cfg) {}

void AuthService::audit(const std::string& user, const std::string& action, const std::string& result) const {
  if (audit_ != nullptr) (void)audit_->log(user, action, result);
}

bool AuthService::isLocked(const std::string& username) const {
  auto it = attempts_.find(username);
  return it != attempts_.end() && it->second.locked && it->second.lockedUntil > clock_.now();
}

Result<Session> AuthService::rejectUnknown(const std::string& username, const std::string& password) {
  (void)users_.authenticate(username, password);
  audit(username, "LOGIN_FAIL", "E-AUTH-001");
  return Result<Session>::Err(ErrorCode::AuthFailed, "invalid username or password");
}

// True while the account is locked (and audits the blocked attempt); clears an expired lock.
bool AuthService::lockActive(const std::string& username, Attempts& att, SimClock::Tick now) {
  if (!att.locked) return false;
  if (att.lockedUntil > now) {
    audit(username, "LOGIN_BLOCKED", "E-AUTH-002");
    return true;
  }
  att = Attempts{};
  return false;
}

void AuthService::recordFailure(const std::string& username, Attempts& att, SimClock::Tick now) {
  ++att.failed;
  audit(username, "LOGIN_FAIL", "E-AUTH-001");
  if (att.failed < cfg_.maxFailedAttempts) return;
  // The attempt that triggers the lock is still a normal failed attempt (E-AUTH-001);
  // the NEXT attempt, even with the right password, is refused with E-AUTH-002 (TC-02).
  att.locked = true;
  att.lockedUntil = now + cfg_.lockoutSeconds;
  audit(username, "ACCOUNT_LOCKED", "locked " + std::to_string(cfg_.lockoutSeconds) + "s");
}

Result<Session> AuthService::openSession(const std::string& username, Role role, SimClock::Tick now) {
  std::uint64_t id;
  do { id = rng_.next(); } while (id == 0 || sessions_.count(id) != 0);
  Session s{id, username, role, now};
  sessions_[id] = s;
  audit(username, "LOGIN_OK", "OK");
  return Result<Session>::Ok(s);
}

Result<Session> AuthService::login(const std::string& username, const std::string& password) {
  if (badInput(username, password)) {
    return Result<Session>::Err(ErrorCode::InvalidArgument, "username and password must be 1-256 characters");
  }
  const SimClock::Tick now = clock_.now();

  // Unknown user: same cost and same answer as a wrong password, and no state is kept
  // (otherwise anyone could fill memory with made-up names or "lock" names that do not exist).
  if (!users_.exists(username)) return rejectUnknown(username, password);

  Attempts& att = attempts_[username];
  if (lockActive(username, att, now)) {
    return Result<Session>::Err(ErrorCode::LockedOut,
                                "account locked; try again in " + std::to_string(att.lockedUntil - now) + "s");
  }
  const auto auth = users_.authenticate(username, password);
  if (!auth.ok()) {
    recordFailure(username, att, now);
    return Result<Session>::Err(ErrorCode::AuthFailed, "invalid username or password");
  }
  att = Attempts{};
  return openSession(username, auth.value(), now);
}

Result<Session> AuthService::validateSession(std::uint64_t sessionId) {
  auto it = sessions_.find(sessionId);
  if (it == sessions_.end())
    return Result<Session>::Err(ErrorCode::SessionExpired, "no active session");
  const SimClock::Tick now = clock_.now();
  if (now - it->second.lastActivity >= cfg_.sessionTimeoutSeconds) {
    audit(it->second.username, "SESSION_EXPIRED", "E-AUTH-003");
    sessions_.erase(it);
    return Result<Session>::Err(ErrorCode::SessionExpired, "session timed out; please log in again");
  }
  it->second.lastActivity = now;
  return Result<Session>::Ok(it->second);
}

void AuthService::logout(std::uint64_t sessionId) {
  auto it = sessions_.find(sessionId);
  if (it == sessions_.end()) return;
  audit(it->second.username, "LOGOUT", "OK");
  sessions_.erase(it);
}

}  // namespace tms
