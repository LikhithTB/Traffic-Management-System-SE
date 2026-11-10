// Story #13 (FR-04): regression test for a stale emergency event.
// An emergency vehicle that leaves while ANOTHER emergency is being served must not
// stay in the handler, otherwise it is "served" later with no vehicle left to release it.
#include "fakes.h"
#include "sim/signal_controller.h"
#include "sim/simulation_engine.h"
#include "test_framework.h"

namespace {
struct StaleRig {
    SignalController signals; LcgRandom rng; FakeAudit audit; CongestionMonitor monitor;
    SimulationEngine engine{signals, rng, audit, monitor};
};
int countEmergencyGreen(const CongestionSnapshot& snap, int intersectionId) {
    int n = 0;
    for (const auto& in : snap) {
        if (in.id != intersectionId) continue;
        for (const auto& l : in.lanes) if (l.signal == SignalState::EmergencyGreen) ++n;
    }
    return n;
}
}  // namespace

TEST(emergency_that_left_on_normal_green_does_not_lock_its_intersection_later) {
    StaleRig r;
    SimConfig c; c.seed = 7;
    c.intersections = {makeDef(1, 4, 1, 30), makeDef(2, 4, 1, 30)};
    CHECK(r.engine.start(c, "op").ok());
    CHECK(r.engine.injectEmergency("op", 1, 3).ok());   // first: needs preemption, is served first
    CHECK(r.engine.injectEmergency("op", 2, 1).ok());   // second: lane 1 is already green and leaves at once
    for (int t = 0; t < 60; ++t) CHECK(r.engine.tick().ok());
    // Both vehicles are gone, so no lane may still be held in EmergencyGreen.
    CHECK_EQ(countEmergencyGreen(r.engine.snapshot(), 1), 0);
    CHECK_EQ(countEmergencyGreen(r.engine.snapshot(), 2), 0);
}
