#include "sim/signal_controller.h"

#include <algorithm>

void SignalController::reset() { states_.clear(); }

void SignalController::addIntersection(int id, int laneCount, int greenSeconds) {
    if (laneCount < 1) return;
    State s;
    s.lanes = laneCount;
    s.configuredGreen = std::min(kMaxGreen, std::max(kMinGreen, greenSeconds));
    s.activeGreen = s.configuredGreen;
    s.current = 0;
    s.phase = Phase::Green;
    s.remaining = s.activeGreen;
    states_[id] = s;
}

Result<void> SignalController::setGreen(int id, int seconds) {
    auto it = states_.find(id);
    if (it == states_.end() || seconds < kMinGreen || seconds > kMaxGreen) {
        return Result<void>::failure(ErrorCode::OutOfRange);
    }
    it->second.configuredGreen = seconds;     // takes effect at the start of the next cycle
    return Result<void>::success();
}

void SignalController::nextLane(State& s) {
    s.current = (s.current + 1) % s.lanes;
    if (s.current == 0) s.activeGreen = s.configuredGreen;     // new cycle: apply new green time
    s.phase = Phase::Green;
    s.remaining = s.activeGreen;
}

void SignalController::tick() {
    for (auto& entry : states_) {
        State& s = entry.second;
        if (s.phase == Phase::EmergencyHold) continue;          // held until release()
        if (--s.remaining > 0) continue;
        if (s.phase == Phase::Green) {
            s.phase = Phase::Yellow;
            s.remaining = kYellowSeconds;
        } else if (s.preempting) {                              // yellow finished -> hand over
            s.phase = Phase::EmergencyHold;
            s.current = s.target;
            s.preempting = false;
        } else {
            nextLane(s);
        }
    }
}

SignalState SignalController::stateOf(int id, int lane) const {
    auto it = states_.find(id);
    if (it == states_.end()) return SignalState::Red;
    const State& s = it->second;
    if (lane < 0 || lane >= s.lanes) return SignalState::Red;
    if (s.phase == Phase::EmergencyHold) {
        return lane == s.current ? SignalState::EmergencyGreen : SignalState::Red;
    }
    if (lane != s.current) return SignalState::Red;
    return s.phase == Phase::Green ? SignalState::Green : SignalState::Yellow;
}

int SignalController::greenSeconds(int id) const {
    auto it = states_.find(id);
    return it == states_.end() ? 0 : it->second.configuredGreen;
}

void SignalController::preempt(int id, int lane) {
    auto it = states_.find(id);
    if (it == states_.end()) return;
    State& s = it->second;
    if (lane < 0 || lane >= s.lanes) return;

    if (s.phase == Phase::EmergencyHold) {
        if (s.current == lane) return;                          // already open for this lane
        s.phase = Phase::Yellow;                                // close the old emergency lane safely
        s.remaining = kYellowSeconds;
        s.preempting = true;
        s.target = lane;
        return;
    }
    if (s.current == lane && s.phase == Phase::Green) {         // already green: open at once
        s.phase = Phase::EmergencyHold;
        s.preempting = false;
        return;
    }
    s.preempting = true;
    s.target = lane;
    if (s.phase == Phase::Green) {                              // another lane is open: 3 s yellow first
        s.phase = Phase::Yellow;
        s.remaining = kYellowSeconds;
    }                                                           // already yellow: let it finish
}

void SignalController::release(int id) {
    auto it = states_.find(id);
    if (it == states_.end()) return;
    State& s = it->second;
    if (s.phase == Phase::EmergencyHold) {
        nextLane(s);                                            // resume with the lane after the emergency lane
    } else {
        s.preempting = false;                                   // preempt cancelled before it opened
    }
}
