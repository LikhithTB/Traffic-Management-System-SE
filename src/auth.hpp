#pragma once
// Issue #5: login, account lockout and idle session timeout (all driven by SimClock).
#include <cstdint>
#include <map>
#include <string>

#include "common/interfaces.h"   // IAuditLogger (SEC-07)
#include "result.hpp"
#include "rng.hpp"
#include "sim_clock.hpp"
#include "user_store.hpp"

namespace tms {

struct AuthConfig {
  unsigned maxFailedAttempts = 3;         // consecutive failures before lockout
  SimClock::Tick lockoutSeconds = 60;     // SEC-03: 60 s after 3 consecutive failures
  SimClock::Tick sessionTimeoutSeconds = 600;  // SEC-04: default 10 min (FR-05: 1-60 min)
};

struct Session {
  std::uint64_t id;
  std::string username;
  Role role;
  SimClock::Tick lastActivity;
};

class AuthService {
 public:
  AuthService(const UserStore& users, SimClock& clock, SeededRng& rng, AuthConfig cfg = {});

  Result<Session> login(const std::string& username, const std::string& password);
  Result<Session> validateSession(std::uint64_t sessionId);  // refreshes idle timer on success
  void logout(std::uint64_t sessionId);

  // Audit trail (SEC-07): LOGIN_OK, LOGIN_FAIL, ACCOUNT_LOCKED, LOGIN_BLOCKED, SESSION_EXPIRED, LOGOUT.
  // Optional; passwords are never written to it.
  void setAuditLogger(::IAuditLogger* audit) { audit_ = audit; }
  // FR-05: the administrator can change the idle timeout at run time.
  void setSessionTimeout(SimClock::Tick seconds) { cfg_.sessionTimeoutSeconds = seconds; }
  SimClock::Tick sessionTimeout() const { return cfg_.sessionTimeoutSeconds; }

  bool isLocked(const std::string& username) const;
  std::size_t activeSessions() const { return sessions_.size(); }

 private:
  struct Attempts {
    unsigned failed = 0;
    bool locked = false;
    SimClock::Tick lockedUntil = 0;
  };

  void audit(const std::string& user, const std::string& action, const std::string& result) const;

  ::IAuditLogger* audit_ = nullptr;
  const UserStore& users_;
  SimClock& clock_;
  SeededRng& rng_;
  AuthConfig cfg_;
  std::map<std::string, Attempts> attempts_;
  std::map<std::uint64_t, Session> sessions_;
};

}  // namespace tms
