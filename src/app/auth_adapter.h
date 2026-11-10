#pragma once
// Adapter (integration): lets the Command Dispatcher use Manoj's tms::AuthService through the
// team contract IAuthService. Translates roles, sessions and error codes (E-AUTH-001..004),
// keeps lockout/session timers on the machine clock (SRS assumption), and audits user creation.
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "auth.hpp"
#include "common/interfaces.h"

class AuthAdapter : public IAuthService {
public:
    using WallClock = std::function<tms::SimClock::Tick()>;      // seconds on the machine clock

    AuthAdapter(tms::AuthService& auth, tms::UserStore& users, tms::SimClock& clock,
                IAuditLogger& audit, WallClock wall);

    Result<Session> login(const std::string& user, const std::string& password) override;
    void logout(Session& session) override;
    bool isExpired(const Session& session) const override;     // counts as activity (called per command)
    bool hasPermission(const Session& session, Command cmd) const override;
    void touch(Session& session) override;
    Result<void> createUser(const Session& session, const NewUser& user) override;

    std::vector<std::string> listUsers() const { return users_.usernames(); }
    void setSessionTimeoutMinutes(int minutes);                 // FR-05: 1..60

private:
    void sync() const;                                          // clock := machine clock

    tms::AuthService& auth_;
    tms::UserStore& users_;
    tms::SimClock& clock_;
    IAuditLogger& audit_;
    WallClock wall_;
    std::map<std::string, std::uint64_t> ids_;                  // username -> session id
};
