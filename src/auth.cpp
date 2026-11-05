#include "auth.hpp"

namespace tms {

namespace {
constexpr std::size_t kMaxInput = 256;   // NFR-06
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

Result<Session> AuthService::login(const std::string& username, const std::string& password) {
  if (username.empty() || password.empty() || username.size() > kMaxInput || password.size() > kMaxInput) {
    return Result<Session>::Err(ErrorCode::InvalidArgument,
                                "username and password must be 1-256 characters");
  }
  const SimClock::Tick now = clock_.now();

  // Unknown user: same cost and same answer as a wrong password, and no state is kept
  // (otherwise anyone could fill memory with made-up names or "lock" names that do not exist).
  if (!users_.exists(username)) {
    (void)users_.authenticate(username, password);
    audit(username, "LOGIN_FAIL", "E-AUTH-001");
    return Result<Session>::Err(ErrorCode::AuthFailed, "invalid username or password");
  }

  Attempts& att = attempts_[username];
  if (att.locked) {
    if (att.lockedUntil > now) {
      audit(username, "LOGIN_BLOCKED", "E-AUTH-002");
      return Result<Session>::Err(ErrorCode::LockedOut,
                                  "account locked; try again in " + std::to_string(att.lockedUntil - now) + "s");
    }
    att = Attempts{};  // lockout expired
  }

  auto auth = users_.authenticate(username, password);
  if (!auth.ok()) {
    ++att.failed;
    audit(username, "LOGIN_FAIL", "E-AUTH-001");
    if (att.failed >= cfg_.maxFailedAttempts) {
      // The attempt that triggers the lock is still a normal failed attempt (E-AUTH-001);
      // the NEXT attempt, even with the right password, is refused with E-AUTH-002 (TC-02).
      att.locked = true;
      att.lockedUntil = now + cfg_.lockoutSeconds;
      audit(username, "ACCOUNT_LOCKED", "locked " + std::to_string(cfg_.lockoutSeconds) + "s");
    }
    return Result<Session>::Err(ErrorCode::AuthFailed, "invalid username or password");
  }

  att = Attempts{};
  std::uint64_t id;
  do { id = rng_.next(); } while (id == 0 || sessions_.count(id) != 0);
  Session s{id, username, auth.value(), now};
  sessions_[id] = s;
  audit(username, "LOGIN_OK", "OK");
  return Result<Session>::Ok(s);
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
