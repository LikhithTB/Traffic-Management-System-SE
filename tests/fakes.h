#pragma once
// Test doubles for modules owned by other team members, so each part can be
// tested alone. They follow the contracts in src/common/interfaces.h.
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "common/interfaces.h"

struct AuditEntry { std::string user, action, result; };

class FakeAudit : public IAuditLogger {
public:
    std::vector<AuditEntry> entries;
    Result<void> log(const std::string& u, const std::string& a, const std::string& r) override {
        entries.push_back({u, a, r});
        return Result<void>::success();
    }
    bool has(const std::string& action) const {
        for (const auto& e : entries) if (e.action == action) return true;
        return false;
    }
};

class FakeAuth : public IAuthService {
public:
    bool allowAll{true};      // simulate an auth service that wrongly allows everything
    bool expired{false};
    int touches{0};
    Result<Session> login(const std::string& u, const std::string&) override {
        Session s; s.username = u; s.role = Role::Operator;
        return Result<Session>::success(s);
    }
    void logout(Session&) override {}
    bool isExpired(const Session&) const override { return expired; }
    bool hasPermission(const Session& s, Command c) const override { return allowAll || roleAllows(s.role, c); }
    void touch(Session&) override { ++touches; }
    Result<void> createUser(const Session&, const NewUser&) override { return Result<void>::success(); }
};

class FixedRandom : public IRandom {     // always returns the same number
public:
    explicit FixedRandom(std::uint32_t v = 0) : v_(v) {}
    void seed(std::uint32_t) override {}
    std::uint32_t next() override { return v_; }
private:
    std::uint32_t v_;
};

class LcgRandom : public IRandom {       // deterministic, seed dependent
public:
    void seed(std::uint32_t s) override { state_ = s; }
    std::uint32_t next() override { state_ = state_ * 1664525U + 1013904223U; return state_ >> 8; }
private:
    std::uint32_t state_{1};
};

class FakeSignals : public ISignalController {
public:
    std::vector<std::pair<int, int>> preempts;
    std::vector<int> releases;
    int ticks{0};
    int preemptDelay{0};      // ticks of yellow before the emergency lane turns green (real: 3 s)
    void reset() override { lanes_.clear(); state_.clear(); green_.clear(); pendingLane_.clear(); }
    void addIntersection(int id, int laneCount, int green) override {
        lanes_[id] = laneCount; green_[id] = green;
        for (int i = 0; i < laneCount; ++i) state_[{id, i}] = SignalState::Green;
    }
    Result<void> setGreen(int id, int s) override { green_[id] = s; return Result<void>::success(); }
    void tick() override {
        ++ticks;
        for (auto& p : pendingLane_) {
            if (p.second.second > 0 && --p.second.second == 0) {
                state_[{p.first, p.second.first}] = SignalState::EmergencyGreen;
            }
        }
    }
    SignalState stateOf(int id, int lane) const override {
        auto it = state_.find({id, lane});
        return it == state_.end() ? SignalState::Red : it->second;
    }
    int greenSeconds(int id) const override { auto it = green_.find(id); return it == green_.end() ? 0 : it->second; }
    void preempt(int id, int lane) override {
        preempts.push_back({id, lane});
        for (int i = 0; i < lanes_[id]; ++i) state_[{id, i}] = SignalState::Red;
        if (preemptDelay == 0) {
            state_[{id, lane}] = SignalState::EmergencyGreen;
        } else {
            state_[{id, lane}] = SignalState::Yellow;
            pendingLane_[id] = {lane, preemptDelay};
        }
    }
    void release(int id) override {
        releases.push_back(id);
        pendingLane_.erase(id);
        for (int i = 0; i < lanes_[id]; ++i) state_[{id, i}] = SignalState::Green;
    }
    void force(int id, int lane, SignalState s) { state_[{id, lane}] = s; }
private:
    std::map<int, int> lanes_, green_;
    std::map<int, std::pair<int, int>> pendingLane_;   // id -> (lane, ticks left)
    std::map<std::pair<int, int>, SignalState> state_;
};

inline IntersectionDef makeDef(int id, int lanes, int rate, int green = 30) {
    IntersectionDef d;
    d.id = id; d.name = "Test " + std::to_string(id); d.lanes = lanes;
    d.arrivalPerMin.assign(static_cast<std::size_t>(lanes), rate);
    d.greenSeconds = green;
    return d;
}
