// Story #18 (NFR-03): checks that guard the 24-hour run (the run itself is tools/reliability.sh).
#include <string>
#include "fakes.h"
#include "sim/signal_controller.h"
#include "sim/simulation_engine.h"
#include "test_framework.h"

namespace {

struct RelRig {
    SignalController signals; LcgRandom rng; FakeAudit audit; CongestionMonitor monitor;
    SimulationEngine engine{signals, rng, audit, monitor};
};

SimConfig maxConfig(int rate) {
    SimConfig c; c.seed = 42;
    for (int i = 1; i <= 10; ++i) c.intersections.push_back(makeDef(i, 8, rate, 20 + i));
    return c;
}

bool laneHasEmergency(const RelRig& r, int id, int lane1) {
    for (const auto& in : r.engine.snapshot())
        if (in.id == id) return in.lanes[static_cast<std::size_t>(lane1 - 1)].hasEmergency;
    return false;
}

}  // namespace

// hasEmergency is read from the front of the queue: it must still be true with a long backlog
// behind the emergency vehicle, with two emergencies queued, and false once both have left.
TEST(rel_emergency_flag_follows_the_front_of_a_long_queue) {
    RelRig r;
    CHECK(r.engine.start(maxConfig(60), "op").ok());
    for (int t = 0; t < 300; ++t) CHECK(r.engine.tick().ok());          // build a backlog
    CHECK(!laneHasEmergency(r, 3, 5));
    CHECK(r.engine.injectEmergency("op", 3, 5).ok());
    CHECK(r.engine.injectEmergency("op", 3, 5).ok());
    CHECK(laneHasEmergency(r, 3, 5));
    CHECK(!laneHasEmergency(r, 3, 6));                                   // other lanes unaffected
    CHECK(r.engine.queueLength(3, 4) > 10);
    for (int t = 0; t < 400 && laneHasEmergency(r, 3, 5); ++t) CHECK(r.engine.tick().ok());
    CHECK(!laneHasEmergency(r, 3, 5));                                   // both served and gone
}

// 10,000 ticks at the maximum configuration and highest arrival rate leave ~800,000 queued
// vehicles. The run must finish (before the fix each tick scanned every queue) and every tick must succeed.
TEST(rel_saturated_max_configuration_run_completes_and_keeps_counting) {
    RelRig r;
    CHECK(r.engine.start(maxConfig(60), "op").ok());
    for (int t = 0; t < 10000; ++t) {
        if (t % 211 == 0) CHECK(r.engine.injectEmergency("op", t / 211 % 10 + 1, t / 211 % 8 + 1).ok());
        CHECK(r.engine.tick().ok());
    }
    CHECK_EQ(r.engine.now(), static_cast<Tick>(10000));
    CHECK(r.engine.queueLength(1, 0) > 1000);                            // really saturated
}
