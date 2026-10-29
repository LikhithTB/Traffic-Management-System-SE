#include "fakes.h"
#include "sim/simulation_engine.h"
#include "test_framework.h"

namespace {
struct Rig {
    FakeSignals signals; FixedRandom rng{0}; FakeAudit audit; CongestionMonitor monitor;
    SimulationEngine engine{signals, rng, audit, monitor};
};
SimConfig cfgOf(std::vector<IntersectionDef> defs, std::uint32_t seed = 1) {
    SimConfig c; c.seed = seed; c.intersections = std::move(defs); return c;
}
}

TEST(engine_start_validates_configuration) {
    Rig r;
    CHECK_EQ(r.engine.start(cfgOf({}), "op").error, ErrorCode::NotRunning);
    std::vector<IntersectionDef> eleven;
    for (int i = 1; i <= 11; ++i) eleven.push_back(makeDef(i, 2, 10));
    CHECK_EQ(r.engine.start(cfgOf(eleven), "op").error, ErrorCode::CapacityExceeded);
    CHECK_EQ(r.engine.start(cfgOf({makeDef(1, 9, 10)}), "op").error, ErrorCode::OutOfRange);   // 9 lanes
    CHECK_EQ(r.engine.start(cfgOf({makeDef(1, 2, 61)}), "op").error, ErrorCode::OutOfRange);   // rate 61
    CHECK_EQ(r.engine.start(cfgOf({makeDef(1, 2, 0)}), "op").error, ErrorCode::OutOfRange);    // rate 0
    CHECK_EQ(r.engine.start(cfgOf({makeDef(1, 2, 10, 4)}), "op").error, ErrorCode::OutOfRange);   // green 4
    CHECK_EQ(r.engine.start(cfgOf({makeDef(1, 2, 10), makeDef(1, 2, 10)}), "op").error, ErrorCode::OutOfRange);
    CHECK(r.engine.state() == SimulationEngine::State::Stopped);   // failed start changes nothing
    CHECK(r.engine.start(cfgOf({makeDef(1, 8, 60, 120)}), "op").ok());   // maximum values are fine
}

TEST(engine_tick_needs_running_simulation) {
    Rig r;
    CHECK_EQ(r.engine.tick().error, ErrorCode::NotRunning);
    CHECK(r.engine.start(cfgOf({makeDef(1, 2, 10)}), "op").ok());
    CHECK(r.engine.tick().ok());
    CHECK(r.engine.pause("op").ok());
    CHECK_EQ(r.engine.tick().error, ErrorCode::NotRunning);
    CHECK(r.engine.resume("op").ok());
    CHECK(r.engine.tick().ok());
    CHECK(r.engine.stop("op").ok());
    CHECK_EQ(r.engine.stop("op").error, ErrorCode::NotRunning);
}

TEST(engine_fifo_one_departure_per_two_seconds_on_green) {
    Rig r;   // rate 60 + RNG 0 -> one arrival every tick
    CHECK(r.engine.start(cfgOf({makeDef(1, 1, 60)}), "op").ok());
    for (int i = 0; i < 10; ++i) CHECK(r.engine.tick().ok());
    CHECK_EQ(r.engine.statistics().departed, 5u);     // 10 ticks -> 5 departures
    CHECK_EQ(r.engine.queueLength(1, 0), 5u);         // 10 arrived - 5 left
}

TEST(engine_no_departures_on_red_or_yellow) {
    Rig r;
    CHECK(r.engine.start(cfgOf({makeDef(1, 2, 60)}), "op").ok());
    r.signals.force(1, 0, SignalState::Red);
    r.signals.force(1, 1, SignalState::Yellow);
    for (int i = 0; i < 6; ++i) CHECK(r.engine.tick().ok());
    CHECK_EQ(r.engine.statistics().departed, 0u);
    CHECK_EQ(r.engine.queueLength(1, 0), 6u);
    CHECK_EQ(r.engine.queueLength(1, 1), 6u);
}

TEST(engine_arrival_rate_is_per_lane) {
    Rig r;   // RNG always 30: arrival when 30 < rate
    r.rng = FixedRandom(30);
    IntersectionDef d = makeDef(1, 2, 10);
    d.arrivalPerMin = {10, 40};
    CHECK(r.engine.start(cfgOf({d}), "op").ok());
    r.signals.force(1, 0, SignalState::Red);
    r.signals.force(1, 1, SignalState::Red);
    for (int i = 0; i < 5; ++i) CHECK(r.engine.tick().ok());
    CHECK_EQ(r.engine.queueLength(1, 0), 0u);    // 30 < 10 false
    CHECK_EQ(r.engine.queueLength(1, 1), 5u);    // 30 < 40 true
}

TEST(engine_same_seed_gives_identical_run) {
    auto run = [](std::uint32_t seed) {
        FakeSignals sig; LcgRandom rng; FakeAudit audit; CongestionMonitor mon;
        SimulationEngine eng(sig, rng, audit, mon);
        SimConfig c = cfgOf({makeDef(1, 4, 25), makeDef(2, 3, 40)}, seed);
        CHECK(eng.start(c, "op").ok());
        for (int i = 0; i < 300; ++i) CHECK(eng.tick().ok());
        std::vector<std::size_t> v;
        for (const auto& in : eng.snapshot()) for (const auto& l : in.lanes) v.push_back(l.queueLength);
        v.push_back(eng.statistics().departed);
        v.push_back(eng.statistics().totalWait);
        return v;
    };
    CHECK(run(42) == run(42));      // NFR-08
    CHECK(run(7) == run(7));
}

TEST(engine_snapshot_has_congestion_levels_and_signals) {
    Rig r;
    CHECK(r.engine.start(cfgOf({makeDef(1, 1, 60)}), "op").ok());
    r.signals.force(1, 0, SignalState::Red);
    for (int i = 0; i < 16; ++i) CHECK(r.engine.tick().ok());
    auto snap = r.engine.snapshot();
    CHECK_EQ(snap.size(), 1u);
    CHECK_EQ(snap[0].lanes[0].queueLength, 16u);
    CHECK(snap[0].lanes[0].congestion == CongestionLevel::High);
    CHECK(snap[0].lanes[0].signal == SignalState::Red);
    CHECK_EQ(snap[0].greenSeconds, 30);
    CHECK_EQ(r.engine.statistics().maxQueue, 16u);
    CHECK(r.engine.statistics().highTicks >= 1u);
}

TEST(engine_restart_resets_state_and_statistics) {
    Rig r;
    CHECK(r.engine.start(cfgOf({makeDef(1, 1, 60)}), "op").ok());
    for (int i = 0; i < 10; ++i) CHECK(r.engine.tick().ok());
    CHECK(r.engine.start(cfgOf({makeDef(1, 1, 60)}), "op").ok());
    CHECK_EQ(r.engine.now(), 0u);
    CHECK_EQ(r.engine.queueLength(1, 0), 0u);
    CHECK_EQ(r.engine.statistics().departed, 0u);
}

TEST(engine_audits_start_stop_and_emergency) {
    Rig r;
    CHECK(r.engine.start(cfgOf({makeDef(1, 2, 10)}, 99), "alice").ok());
    CHECK(r.engine.injectEmergency("alice", 1, 2).ok());
    CHECK(r.engine.stop("alice").ok());
    CHECK(r.audit.has("SIM_START"));
    CHECK(r.audit.has("EMERGENCY_INJECTED"));
    CHECK(r.audit.has("SIM_STOP"));
    CHECK_EQ(r.audit.entries[0].user, "alice");
    CHECK(r.audit.entries[0].result.find("seed=99") != std::string::npos);
}

TEST(emergency_requires_running_and_valid_target) {
    Rig r;
    CHECK_EQ(r.engine.injectEmergency("op", 1, 1).error, ErrorCode::NotRunning);
    CHECK(r.engine.start(cfgOf({makeDef(1, 2, 10)}), "op").ok());
    CHECK_EQ(r.engine.injectEmergency("op", 5, 1).error, ErrorCode::OutOfRange);   // no such intersection
    CHECK_EQ(r.engine.injectEmergency("op", 1, 3).error, ErrorCode::OutOfRange);   // lane > lanes
    CHECK_EQ(r.engine.injectEmergency("op", 1, 0).error, ErrorCode::OutOfRange);
}

TEST(emergency_preempts_then_releases_after_vehicle_departs) {
    Rig r;
    r.rng = FixedRandom(59);     // 59 < rate false for rate<=59 -> no normal arrivals
    CHECK(r.engine.start(cfgOf({makeDef(1, 3, 10)}), "op").ok());
    CHECK(r.engine.injectEmergency("op", 1, 2).ok());
    CHECK(r.engine.snapshot()[0].lanes[1].hasEmergency);
    CHECK(r.signals.preempts.empty());                    // preempt happens on the next tick
    CHECK(r.engine.tick().ok());
    CHECK_EQ(r.signals.preempts.size(), 1u);
    CHECK(r.signals.preempts[0] == std::make_pair(1, 1)); // lane 2 -> index 1
    CHECK_EQ(r.signals.releases.size(), 1u);              // vehicle left on the same tick (lane was green)
    CHECK(r.audit.has("EMERGENCY_CLEARED"));
    CHECK(!r.engine.snapshot()[0].lanes[1].hasEmergency);
}

TEST(emergency_vehicle_waits_for_green_and_release_comes_only_after_it_departs) {
    Rig r;
    r.rng = FixedRandom(59);
    r.signals.preemptDelay = 4;                          // the preempt tick itself advances the signal once,
                                                         // so the lane stays yellow for ticks 1-3, green in tick 4
    CHECK(r.engine.start(cfgOf({makeDef(1, 2, 10)}), "op").ok());
    CHECK(r.engine.injectEmergency("op", 1, 1).ok());
    for (int i = 0; i < 3; ++i) {                        // lane still yellow: nothing may leave
        CHECK(r.engine.tick().ok());
        CHECK_EQ(r.engine.queueLength(1, 0), 1u);
        CHECK(r.signals.releases.empty());               // signal is held
    }
    CHECK(r.engine.tick().ok());                         // 4th tick: green -> vehicle departs -> released
    CHECK_EQ(r.engine.queueLength(1, 0), 0u);
    CHECK_EQ(r.signals.releases.size(), 1u);
    CHECK_EQ(r.signals.preempts.size(), 1u);             // preempted exactly once, not every tick
}

TEST(simultaneous_emergencies_are_served_in_arrival_order) {
    Rig r;
    r.rng = FixedRandom(59);
    CHECK(r.engine.start(cfgOf({makeDef(1, 3, 10)}), "op").ok());
    CHECK(r.engine.injectEmergency("op", 1, 3).ok());     // first
    CHECK(r.engine.injectEmergency("op", 1, 1).ok());     // second, same tick
    for (int i = 0; i < 6; ++i) CHECK(r.engine.tick().ok());
    CHECK_EQ(r.signals.preempts.size(), 2u);
    CHECK(r.signals.preempts[0] == std::make_pair(1, 2)); // lane 3 first
    CHECK(r.signals.preempts[1] == std::make_pair(1, 0)); // then lane 1
    CHECK_EQ(r.signals.releases.size(), 2u);
}

TEST(two_emergencies_in_one_lane_keep_arrival_order) {
    Rig r;
    r.rng = FixedRandom(59);
    CHECK(r.engine.start(cfgOf({makeDef(1, 1, 10)}), "op").ok());
    CHECK(r.engine.injectEmergency("op", 1, 1).ok());
    CHECK(r.engine.injectEmergency("op", 1, 1).ok());
    CHECK_EQ(r.engine.queueLength(1, 0), 2u);
    for (int i = 0; i < 6; ++i) CHECK(r.engine.tick().ok());
    CHECK_EQ(r.engine.queueLength(1, 0), 0u);
    CHECK_EQ(r.signals.releases.size(), 2u);
}

TEST(emergency_handler_orders_by_arrival_then_sequence) {
    EmergencyHandler h;
    CHECK(!h.hasActive());
    h.report(5, 10, 1, 0);
    h.report(2, 11, 1, 1);
    h.report(2, 12, 1, 2);
    CHECK_EQ(h.active().vehicleId, 11u);
    h.complete();
    CHECK_EQ(h.active().vehicleId, 12u);
    h.complete();
    CHECK_EQ(h.active().vehicleId, 10u);
    h.complete();
    CHECK(!h.hasActive());
    h.report(1, 1, 1, 0);
    h.clear();
    CHECK_EQ(h.pending(), 0u);
}
