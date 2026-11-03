#include "auth.hpp"

namespace tms {

AuthService::AuthService(const UserStore& users, SimClock& clock, SeededRng& rng, AuthConfig cfg)
    : users_(users), clock_(clock), rng_(rng), cfg_(cfg) {}

bool AuthService::isLocked(const std::string& username) const {
  auto it = attempts_.find(username);
  return it != attempts_.end() && it->second.locked && it->second.lockedUntil > clock_.now();
}

Result<Session> AuthService::login(const std::string& username, const std::string& password) {
  const SimClock::Tick now = clock_.now();
  Attempts& att = attempts_[username];

  if (att.locked) {
    if (att.lockedUntil > now) {
      return Result<Session>::Err(ErrorCode::LockedOut,
                                  "account locked; try again in " + std::to_string(att.lockedUntil - now) + "s");
    }
    att = Attempts{};  // lockout expired
  }

  auto auth = users_.authenticate(username, password);
  if (!auth.ok()) {
    ++att.failed;
    if (att.failed >= cfg_.maxFailedAttempts) {
      att.locked = true;
      att.lockedUntil = now + cfg_.lockoutSeconds;
      return Result<Session>::Err(ErrorCode::LockedOut,
                                  "too many failed attempts; account locked for " +
                                      std::to_string(cfg_.lockoutSeconds) + "s");
    }
    return Result<Session>::Err(ErrorCode::AuthFailed, "invalid username or password");
  }

  att = Attempts{};
  std::uint64_t id;
  do { id = rng_.next(); } while (id == 0 || sessions_.count(id) != 0);
  Session s{id, username, auth.value(), now};
  sessions_[id] = s;
  return Result<Session>::Ok(s);
}

Result<Session> AuthService::validateSession(std::uint64_t sessionId) {
  auto it = sessions_.find(sessionId);
  if (it == sessions_.end())
    return Result<Session>::Err(ErrorCode::SessionExpired, "no active session");
  const SimClock::Tick now = clock_.now();
  if (now - it->second.lastActivity >= cfg_.sessionTimeoutSeconds) {
    sessions_.erase(it);
    return Result<Session>::Err(ErrorCode::SessionExpired, "session timed out; please log in again");
  }
  it->second.lastActivity = now;
  return Result<Session>::Ok(it->second);
}

void AuthService::logout(std::uint64_t sessionId) { sessions_.erase(sessionId); }

}  // namespace tms
