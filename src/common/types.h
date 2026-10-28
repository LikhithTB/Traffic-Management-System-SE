#pragma once
// Shared domain types (Architecture Design Spec section 2.4).
// Agreed by the whole team in Sprint 1. Add new shared types via a small PR.
#include <cstddef>
#include <cstdint>
#include <string>

using Tick = std::uint64_t;          // 1 tick = 1 simulated second
using TimeSec = std::int64_t;        // wall-clock seconds (lockout, session expiry)

enum class Role { Operator, Admin, Commuter };

enum class SignalState { Red, Green, Yellow, EmergencyGreen };

enum class CongestionLevel { Low, Moderate, High };

enum class VehicleType { Normal, Emergency };

struct Vehicle {
    std::uint64_t id{0};
    VehicleType type{VehicleType::Normal};
    Tick arrivalTick{0};
};

// Report figures (FR-06).
struct Statistics {
    std::uint64_t departed{0};
    std::uint64_t totalWait{0};     // sum of waiting ticks of departed vehicles
    std::size_t maxQueue{0};
    std::uint64_t highTicks{0};     // ticks spent at High congestion
    std::uint64_t totalTicks{0};
};

// One line of users.dat (FR-07, SEC-02, SEC-03).
struct UserRecord {
    std::string username;
    Role role{Role::Operator};
    std::string salt;               // hex
    std::string hash;               // hex
    int failedCount{0};
    TimeSec lockUntil{0};
    bool active{true};
};

// Result of a successful login (SEC-04).
struct Session {
    std::string username;
    Role role{Role::Commuter};
    TimeSec expiry{0};              // inactivity deadline
};

// Congestion thresholds (FR-03): Low < moderate, Moderate < high, High >= high.
inline CongestionLevel congestionFor(std::size_t queueLength,
                                     std::size_t moderate = 5,
                                     std::size_t high = 15) {
    if (queueLength >= high) return CongestionLevel::High;
    if (queueLength >= moderate) return CongestionLevel::Moderate;
    return CongestionLevel::Low;
}
