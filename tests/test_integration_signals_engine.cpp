// Integration test: REAL SignalController + REAL SimulationEngine (no fakes for these two).
// Add to tests/ only after BOTH stories #7 (engine) and #8 (signal controller) are merged.
#include <chrono>
#include "fakes.h"
#include "sim/signal_controller.h"
#include "sim/simulation_engine.h"
#include "test_framework.h"

namespace {
struct RealRig {
    SignalController signals; LcgRandom rng; FakeAudit audit; CongestionMonitor monitor;
    SimulationEngine engine{signals, rng, audit, monitor};
};
SimConfig oneIntersection(int lanes, int rate, int green, std::uint32_t seed = 3) {
    SimConfig c; c.seed = seed; c.intersections = {makeDef(1, lanes, rate, green)}; return c;
}
int openLanes(const CongestionSnapshot& snap) {
    int open = 0;
    for (const auto& in : snap) for (const auto& l : in.lanes) if (l.signal != SignalState::Red) ++open;
    return open;
}
}

TEST(integration_emergency_vehicle_is_served_within_5_seconds_of_detection) {
    RealRig r;
    CHECK(r.engine.start(oneIntersection(4, 20, 30), "op").ok());
    for (int i = 0; i < 3; ++i) CHECK(r.engine.tick().ok());     // lane 0 is green, 3 s into its green
    CHECK(r.engine.injectEmergency("op", 1, 3).ok());            // detected now; lane index 2
    int ticksToServed = -1;
    for (int t = 1; t <= 10; ++t) {
        CHECK(r.engine.tick().ok());
        if (r.audit.has("EMERGENCY_CLEARED")) { ticksToServed = t; break; }
    }
    // The lane turns EmergencyGreen after the 3 s yellow and the vehicle leaves in that same
    // tick, so "served" is the evidence: green within 5 simulated seconds (FR-04).
    CHECK(ticksToServed > 0);
    CHECK(ticksToServed <= 5);
    CHECK(openLanes(r.engine.snapshot()) == 1);                  // normal cycle already resumed
}

TEST(integration_emergency_is_cleared_and_normal_cycle_resumes) {
    RealRig r;
    CHECK(r.engine.start(oneIntersection(4, 20, 30), "op").ok());
    CHECK(r.engine.injectEmergency("op", 1, 2).ok());
    for (int i = 0; i < 40 && !r.audit.has("EMERGENCY_CLEARED"); ++i) CHECK(r.engine.tick().ok());
    CHECK(r.audit.has("EMERGENCY_CLEARED"));
    for (int i = 0; i < 100; ++i) CHECK(r.engine.tick().ok());
    CHECK(openLanes(r.engine.snapshot()) == 1);
    CHECK(!r.engine.snapshot()[0].lanes[1].hasEmergency);
}

TEST(integration_only_one_lane_is_open_at_any_time_even_with_emergencies) {
    RealRig r;
    CHECK(r.engine.start(oneIntersection(6, 45, 12, 99), "op").ok());
    for (int t = 0; t < 3000; ++t) {
        if (t % 137 == 0) CHECK(r.engine.injectEmergency("op", 1, (t / 137) % 6 + 1).ok());
        CHECK(r.engine.tick().ok());
        CHECK(openLanes(r.engine.snapshot()) == 1);
    }
    CHECK(r.engine.statistics().departed > 0u);
}

TEST(integration_same_seed_gives_identical_report_numbers) {
    auto run = [](std::uint32_t seed) {
        RealRig r;
        SimConfig c; c.seed = seed; c.intersections = {makeDef(1, 4, 25, 20), makeDef(2, 3, 40, 15)};
        CHECK(r.engine.start(c, "op").ok());
        for (int i = 0; i < 1000; ++i) {
            if (i == 300) CHECK(r.engine.injectEmergency("op", 2, 2).ok());
            CHECK(r.engine.tick().ok());
        }
        const Statistics s = r.engine.statistics();
        return std::vector<std::uint64_t>{s.departed, s.totalWait, s.maxQueue, s.highTicks};
    };
    CHECK(run(5) == run(5));                                     // NFR-08
}

TEST(integration_max_configuration_runs_one_simulated_hour_in_under_4_seconds) {
    RealRig r;
    SimConfig c; c.seed = 1;
    for (int i = 1; i <= 10; ++i) c.intersections.push_back(makeDef(i, 8, 30, 20));
    CHECK(r.engine.start(c, "op").ok());
    const auto begin = std::chrono::steady_clock::now();
    for (int t = 0; t < 3600; ++t) CHECK(r.engine.tick().ok());
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - begin).count();
    std::cout << "    (3600 ticks, 10 x 8 lanes: " << ms << " ms)\n";
#if !defined(__SANITIZE_ADDRESS__)
    CHECK(ms < 4000);                                            // NFR-02 (not meaningful under ASan)
#endif
}
