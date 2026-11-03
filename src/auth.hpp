#pragma once
// Issue #5: login, account lockout and idle session timeout (all driven by SimClock).
#include <cstdint>
#include <map>
#include <string>

#include "result.hpp"
#include "rng.hpp"
#include "sim_clock.hpp"
#include "user_store.hpp"

namespace tms {

struct AuthConfig {
  unsigned maxFailedAttempts = 3;         // consecutive failures before lockout
  SimClock::Tick lockoutSeconds = 300;    // lockout length (simulated seconds)
  SimClock::Tick sessionTimeoutSeconds = 900;  // idle timeout
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

  bool isLocked(const std::string& username) const;
  std::size_t activeSessions() const { return sessions_.size(); }

 private:
  struct Attempts {
    unsigned failed = 0;
    bool locked = false;
    SimClock::Tick lockedUntil = 0;
  };

  const UserStore& users_;
  SimClock& clock_;
  SeededRng& rng_;
  AuthConfig cfg_;
  std::map<std::string, Attempts> attempts_;
  std::map<std::uint64_t, Session> sessions_;
};

}  // namespace tms
