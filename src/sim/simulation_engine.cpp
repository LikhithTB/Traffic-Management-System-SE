#include "sim/simulation_engine.h"

#include <set>

namespace {

bool isGreen(SignalState s) { return s == SignalState::Green || s == SignalState::EmergencyGreen; }

bool validDefinition(const IntersectionDef& d) {
    if (d.lanes < 1 || d.lanes > 8) return false;
    if (d.arrivalPerMin.size() != static_cast<std::size_t>(d.lanes)) return false;
    for (int rate : d.arrivalPerMin) {
        if (rate < 1 || rate > 60) return false;
    }
    return d.greenSeconds >= 5 && d.greenSeconds <= 120;
}

}  // namespace

SimulationEngine::SimulationEngine(ISignalController& signals, IRandom& rng, IAuditLogger& audit,
                                   CongestionMonitor& monitor)
    : signals_(signals), rng_(rng), audit_(audit), monitor_(monitor) {}

Result<void> SimulationEngine::start(const SimConfig& cfg, const std::string& user) {
    if (cfg.intersections.empty()) return Result<void>::failure(ErrorCode::NotRunning);
    if (cfg.intersections.size() > 10) return Result<void>::failure(ErrorCode::CapacityExceeded);

    std::set<int> ids;
    for (const auto& d : cfg.intersections) {
        if (!validDefinition(d) || !ids.insert(d.id).second) {
            return Result<void>::failure(ErrorCode::OutOfRange);
        }
    }

    // Everything valid: reset and build the new run (state changes only after validation).
    signals_.reset();
    inters_.clear();
    emergency_.clear();
    activePreempted_ = false;
    monitor_.setThresholds(cfg.moderateAt, cfg.highAt);
    monitor_.reset();
    rng_.seed(cfg.seed);
    now_ = 0;
    nextVehicleId_ = 1;

    for (const auto& d : cfg.intersections) {
        InterState is;
        is.def = d;
        is.lanes.resize(static_cast<std::size_t>(d.lanes));
        for (std::size_t i = 0; i < is.lanes.size(); ++i) is.lanes[i].ratePerMin = d.arrivalPerMin[i];
        inters_.push_back(std::move(is));
        signals_.addIntersection(d.id, d.lanes, d.greenSeconds);
    }
    state_ = State::Running;
    (void)audit_.log(user, "SIM_START", "OK seed=" + std::to_string(cfg.seed));
    return Result<void>::success();
}

Result<void> SimulationEngine::pause(const std::string& user) {
    if (state_ != State::Running) return Result<void>::failure(ErrorCode::NotRunning);
    state_ = State::Paused;
    (void)audit_.log(user, "SIM_PAUSE", "OK");
    return Result<void>::success();
}

Result<void> SimulationEngine::resume(const std::string& user) {
    if (state_ != State::Paused) return Result<void>::failure(ErrorCode::NotRunning);
    state_ = State::Running;
    (void)audit_.log(user, "SIM_RESUME", "OK");
    return Result<void>::success();
}

Result<void> SimulationEngine::stop(const std::string& user) {
    if (state_ == State::Stopped) return Result<void>::failure(ErrorCode::NotRunning);
    state_ = State::Stopped;
    (void)audit_.log(user, "SIM_STOP", "OK tick=" + std::to_string(now_));
    return Result<void>::success();
}

Result<void> SimulationEngine::tick() {
    if (state_ != State::Running) return Result<void>::failure(ErrorCode::NotRunning);
    generateArrivals();
    servePendingEmergency();
    signals_.tick();
    departVehicles();
    monitor_.update(raw());
    ++now_;
    return Result<void>::success();
}

Result<void> SimulationEngine::injectEmergency(const std::string& user, int intersectionId, int lane) {
    if (state_ != State::Running) return Result<void>::failure(ErrorCode::NotRunning);
    InterState* is = find(intersectionId);
    if (is == nullptr || lane < 1 || lane > is->def.lanes) {
        return Result<void>::failure(ErrorCode::OutOfRange);
    }
    const std::size_t idx = static_cast<std::size_t>(lane - 1);
    Vehicle v;
    v.id = nextVehicleId_++;
    v.type = VehicleType::Emergency;
    v.arrivalTick = now_;

    // Emergency vehicles go to the front, behind earlier emergencies (arrival order kept).
    auto& q = is->lanes[idx].queue;
    auto it = q.begin();
    while (it != q.end() && it->type == VehicleType::Emergency) ++it;
    q.insert(it, v);

    emergency_.report(now_, v.id, intersectionId, static_cast<int>(idx));   // detected in this tick
    (void)audit_.log(user, "EMERGENCY_INJECTED",
                     "intersection=" + std::to_string(intersectionId) + " lane=" + std::to_string(lane));
    return Result<void>::success();
}

CongestionSnapshot SimulationEngine::snapshot() const { return monitor_.evaluate(raw()); }

std::size_t SimulationEngine::queueLength(int intersectionId, int lane0) const {
    const InterState* is = find(intersectionId);
    if (is == nullptr || lane0 < 0 || static_cast<std::size_t>(lane0) >= is->lanes.size()) return 0;
    return is->lanes[static_cast<std::size_t>(lane0)].queue.size();
}

SimulationEngine::InterState* SimulationEngine::find(int id) {
    for (auto& is : inters_) {
        if (is.def.id == id) return &is;
    }
    return nullptr;
}

const SimulationEngine::InterState* SimulationEngine::find(int id) const {
    for (const auto& is : inters_) {
        if (is.def.id == id) return &is;
    }
    return nullptr;
}

CongestionSnapshot SimulationEngine::raw() const {
    CongestionSnapshot snap;
    for (const auto& is : inters_) {
        IntersectionSnapshot in;
        in.id = is.def.id;
        in.name = is.def.name;
        in.greenSeconds = signals_.greenSeconds(is.def.id);
        for (std::size_t i = 0; i < is.lanes.size(); ++i) {
            LaneSnapshot ls;
            ls.lane = static_cast<int>(i) + 1;
            ls.signal = signals_.stateOf(is.def.id, static_cast<int>(i));
            ls.queueLength = is.lanes[i].queue.size();
            for (const auto& v : is.lanes[i].queue) {
                if (v.type == VehicleType::Emergency) ls.hasEmergency = true;
            }
            in.lanes.push_back(ls);
        }
        snap.push_back(in);
    }
    return snap;
}

// FR-01: arrival with probability rate/60 per lane per tick. Exactly one random
// number is drawn per lane per tick, so the same seed always gives the same run.
void SimulationEngine::generateArrivals() {
    for (auto& is : inters_) {
        for (auto& lane : is.lanes) {
            if (static_cast<int>(rng_.next() % 60U) < lane.ratePerMin) {
                Vehicle v;
                v.id = nextVehicleId_++;
                v.type = VehicleType::Normal;
                v.arrivalTick = now_;
                lane.queue.push_back(v);
            }
        }
    }
}

// FR-04: serve emergencies one at a time, in arrival order.
void SimulationEngine::servePendingEmergency() {
    if (emergency_.hasActive() && !activePreempted_) {
        const EmergencyEvent& e = emergency_.active();
        signals_.preempt(e.intersectionId, e.lane);
        activePreempted_ = true;
    }
}

// FR-01: at most one departure per 2 simulated seconds per lane, only on green.
void SimulationEngine::departVehicles() {
    for (auto& is : inters_) {
        for (std::size_t i = 0; i < is.lanes.size(); ++i) {
            LaneState& lane = is.lanes[i];
            if (lane.queue.empty()) continue;
            if (!isGreen(signals_.stateOf(is.def.id, static_cast<int>(i)))) continue;
            if (lane.hasDeparted && now_ - lane.lastDeparture < 2) continue;

            const Vehicle v = lane.queue.front();
            lane.queue.pop_front();
            lane.hasDeparted = true;
            lane.lastDeparture = now_;
            monitor_.recordDeparture(now_ - v.arrivalTick);

            if (v.type == VehicleType::Emergency && emergency_.hasActive() &&
                emergency_.active().vehicleId == v.id) {
                emergency_.complete();
                signals_.release(is.def.id);          // resume the normal cycle
                activePreempted_ = false;
                (void)audit_.log("system", "EMERGENCY_CLEARED",
                                 "intersection=" + std::to_string(is.def.id) +
                                     " lane=" + std::to_string(i + 1));
            }
        }
    }
}
