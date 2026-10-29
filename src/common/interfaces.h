#pragma once
// Module contracts (Architecture Design Spec 3.2). Everyone codes against these.
// Changing a signature here needs agreement from all four members.
#include <cstdint>
#include <string>
#include "common/permissions.h"
#include "common/result.h"
#include "common/sim_types.h"
#include "common/types.h"

struct NewUser {
    std::string username;      // 4-20 chars, unique
    std::string password;
    Role role{Role::Operator};
};

// Implemented by Manoj: src/auth/ (stories #4, #5, #12).
class IAuthService {
public:
    virtual ~IAuthService() = default;
    virtual Result<Session> login(const std::string& user, const std::string& password) = 0;
    virtual void logout(Session& session) = 0;
    virtual bool isExpired(const Session& session) const = 0;
    virtual bool hasPermission(const Session& session, Command cmd) const = 0;  // use roleAllows()
    virtual void touch(Session& session) = 0;                                     // reset inactivity timer
    virtual Result<void> createUser(const Session& session, const NewUser& user) = 0;
};

// Implemented by Devashruth: src/infra/audit_logger (story #3).
class IAuditLogger {
public:
    virtual ~IAuditLogger() = default;
    // Appends "timestamp | user | action | result". No edit/delete exists (SEC-07).
    virtual Result<void> log(const std::string& user, const std::string& action,
                             const std::string& result) = 0;
};

// Implemented by Manoj: src/infra/rng (story #2).
class IRandom {
public:
    virtual ~IRandom() = default;
    virtual void seed(std::uint32_t seed) = 0;
    virtual std::uint32_t next() = 0;          // same seed -> same sequence (NFR-08)
};

// Implemented by Manish: src/sim/signal_controller (stories #8, #14, FR-04 timing).
// Lanes are 0-based here.
class ISignalController {
public:
    virtual ~ISignalController() = default;
    virtual void reset() = 0;                                              // remove all intersections
    virtual void addIntersection(int id, int laneCount, int greenSeconds) = 0;
    virtual Result<void> setGreen(int id, int seconds) = 0;                // 5..120, applies next cycle
    virtual void tick() = 0;                                               // advance all signals 1 s
    virtual SignalState stateOf(int id, int lane) const = 0;
    virtual int greenSeconds(int id) const = 0;
    virtual void preempt(int id, int lane) = 0;                            // 3 s yellow, then EmergencyGreen (FR-04)
    virtual void release(int id) = 0;                                      // resume normal cycle
};
