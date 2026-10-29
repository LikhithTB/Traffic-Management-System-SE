#pragma once
// Simulation Engine (story #7, FR-01, NFR-08; hooks for FR-04). Owns the lane FIFO
// queues. One tick = one simulated second:
//   1 arrivals (seeded RNG)  2 emergency scan/preempt  3 signals advance
//   4 departures (1 vehicle / 2 s per green lane)      5 monitor update
#include <deque>
#include <string>
#include <vector>
#include "common/interfaces.h"
#include "sim/congestion_monitor.h"
#include "sim/emergency_handler.h"

class SimulationEngine {
public:
    enum class State { Stopped, Running, Paused };

    SimulationEngine(ISignalController& signals, IRandom& rng, IAuditLogger& audit,
                     CongestionMonitor& monitor);

    // Validates the configuration, seeds the RNG and resets everything.
    //   no intersection -> NotRunning, >10 -> CapacityExceeded,
    //   lanes not 1..8 / rate not 1..60 / green not 5..120 / duplicate id -> OutOfRange
    Result<void> start(const SimConfig& cfg, const std::string& user);
    Result<void> pause(const std::string& user);
    Result<void> resume(const std::string& user);
    Result<void> stop(const std::string& user);

    // Advances one simulated second. NotRunning unless State::Running.
    Result<void> tick();

    // Puts an emergency vehicle at the front of a lane (lane is 1-based). FR-04.
    Result<void> injectEmergency(const std::string& user, int intersectionId, int lane);

    CongestionSnapshot snapshot() const;          // current queues + signals + congestion
    Statistics statistics() const { return monitor_.statistics(); }
    State state() const { return state_; }
    Tick now() const { return now_; }
    std::size_t queueLength(int intersectionId, int lane0) const;   // 0-based lane

private:
    struct LaneState {
        std::deque<Vehicle> queue;
        int ratePerMin{0};
        bool hasDeparted{false};
        Tick lastDeparture{0};
    };
    struct InterState {
        IntersectionDef def;
        std::vector<LaneState> lanes;
    };

    InterState* find(int id);
    const InterState* find(int id) const;
    CongestionSnapshot raw() const;
    void generateArrivals();
    void servePendingEmergency();
    void departVehicles();

    ISignalController& signals_;
    IRandom& rng_;
    IAuditLogger& audit_;
    CongestionMonitor& monitor_;
    EmergencyHandler emergency_;
    std::vector<InterState> inters_;
    State state_{State::Stopped};
    Tick now_{0};
    std::uint64_t nextVehicleId_{1};
    bool activePreempted_{false};
};
