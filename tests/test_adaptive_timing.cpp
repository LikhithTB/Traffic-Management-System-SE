// Adaptive signal timing (story #14, FR-09, test case TC-07).
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "fakes.h"
#include "sim/signal_controller.h"
#include "sim/simulation_engine.h"
#include "sim/timing_policy.h"
#include "test_framework.h"

namespace {

// Queue lengths the test can change while the signal runs.
struct Queues {
    std::map<std::pair<int, int>, std::size_t> len;
    AdaptiveTiming::QueueReader reader() {
        return [this](int id, int lane) {
            const auto it = len.find({id, lane});
            return it == len.end() ? std::size_t{0} : it->second;
        };
    }
};

// Seconds that lane stays Green, counted from a controller that has just opened it.
int greenLength(SignalController& sc, int id, int lane) {
    int seconds = 0;
    while (sc.stateOf(id, lane) == SignalState::Green && seconds < 1000) {
        ++seconds;
        sc.tick();
    }
    return seconds;
}

struct Rig {
    Queues queues;
    AdaptiveTiming adaptive{queues.reader()};
    SignalController sc;
    explicit Rig(int lanes = 2, int green = 10) {
        sc.addIntersection(1, lanes, green);
        sc.setTimingPolicy(&adaptive);
    }
};

}  // namespace

// --- policy decision -------------------------------------------------------------

TEST(adaptive_policy_needs_high_queue_and_every_opposing_queue_below_moderate) {
    Queues q;
    const AdaptiveTiming p(q.reader());
    q.len[{1, 0}] = 15;                               // exactly High
    q.len[{1, 1}] = 4;                                // just below Moderate
    q.len[{1, 2}] = 0;
    CHECK(p.shouldExtendGreen(1, 0, 3));
    q.len[{1, 0}] = 14;                               // just below High
    CHECK(!p.shouldExtendGreen(1, 0, 3));
    q.len[{1, 0}] = 15;
    q.len[{1, 2}] = 5;                                // one opposing lane exactly Moderate
    CHECK(!p.shouldExtendGreen(1, 0, 3));
}

TEST(adaptive_policy_single_lane_has_no_opposing_queue) {
    Queues q;
    const AdaptiveTiming p(q.reader());
    q.len[{1, 0}] = 15;
    CHECK(p.shouldExtendGreen(1, 0, 1));
}

TEST(adaptive_policy_uses_configured_thresholds) {
    Queues q;
    AdaptiveTiming p(q.reader());
    p.setThresholds(2, 8);
    q.len[{1, 0}] = 8;
    q.len[{1, 1}] = 1;
    CHECK(p.shouldExtendGreen(1, 0, 2));
    q.len[{1, 1}] = 2;
    CHECK(!p.shouldExtendGreen(1, 0, 2));
}

TEST(fixed_timing_never_extends) {
    const FixedTiming p;
    CHECK(!p.shouldExtendGreen(1, 0, 2));
}

// --- signal controller -----------------------------------------------------------

TEST(adaptive_default_controller_is_fixed_timing) {
    SignalController sc;
    sc.addIntersection(1, 2, 10);
    CHECK_EQ(greenLength(sc, 1, 0), 10);
}

TEST(adaptive_extends_by_20s_at_most_then_normal_yellow) {               // TC-07 first part
    Rig r;
    r.queues.len[{1, 0}] = 30;
    r.queues.len[{1, 1}] = 1;
    CHECK_EQ(greenLength(r.sc, 1, 0), 30);                               // 10 + 20
    CHECK(r.sc.stateOf(1, 0) == SignalState::Yellow);
    CHECK_EQ(r.sc.extendedSeconds(1), 20);
    for (int i = 0; i < 3; ++i) r.sc.tick();
    CHECK(r.sc.stateOf(1, 1) == SignalState::Green);                     // yellow stayed exactly 3 s
    CHECK_EQ(r.sc.extendedSeconds(1), 0);
}

TEST(adaptive_no_extension_when_opposing_queue_is_moderate) {            // TC-07 second part
    Rig r;
    r.queues.len[{1, 0}] = 30;
    r.queues.len[{1, 1}] = 8;
    CHECK_EQ(greenLength(r.sc, 1, 0), 10);
    CHECK_EQ(r.sc.extendedSeconds(1), 0);
}

TEST(adaptive_no_extension_when_lane_is_not_high) {
    Rig r;
    r.queues.len[{1, 0}] = 14;
    CHECK_EQ(greenLength(r.sc, 1, 0), 10);
}

TEST(adaptive_grows_in_5s_steps_and_stops_when_queue_drops) {
    Rig r;
    r.queues.len[{1, 0}] = 30;
    for (int i = 0; i < 10; ++i) r.sc.tick();
    CHECK(r.sc.stateOf(1, 0) == SignalState::Green);
    CHECK_EQ(r.sc.extendedSeconds(1), 5);
    for (int i = 0; i < 5; ++i) r.sc.tick();
    CHECK_EQ(r.sc.extendedSeconds(1), 10);
    r.queues.len[{1, 0}] = 3;                                            // queue has cleared
    for (int i = 0; i < 5; ++i) {
        CHECK(r.sc.stateOf(1, 0) == SignalState::Green);
        r.sc.tick();
    }
    CHECK(r.sc.stateOf(1, 0) == SignalState::Yellow);
    CHECK_EQ(r.sc.extendedSeconds(1), 10);
}

TEST(adaptive_stops_when_opposing_queue_reaches_moderate_during_extension) {
    Rig r;
    r.queues.len[{1, 0}] = 30;
    for (int i = 0; i < 10; ++i) r.sc.tick();
    CHECK_EQ(r.sc.extendedSeconds(1), 5);
    r.queues.len[{1, 1}] = 5;
    for (int i = 0; i < 5; ++i) r.sc.tick();
    CHECK(r.sc.stateOf(1, 0) == SignalState::Yellow);
    CHECK_EQ(r.sc.extendedSeconds(1), 5);
}

TEST(adaptive_total_green_never_exceeds_140s) {
    Rig r(2, 120);
    r.queues.len[{1, 0}] = 500;
    CHECK_EQ(greenLength(r.sc, 1, 0), SignalController::kMaxTotalGreen);
}

TEST(adaptive_total_limit_is_enforced_even_if_a_policy_always_says_yes) {
    struct Always : ITimingPolicy {
        bool shouldExtendGreen(int, int, int) const override { return true; }
    } always;
    SignalController sc;
    sc.addIntersection(1, 2, 120);
    sc.setTimingPolicy(&always);
    CHECK_EQ(greenLength(sc, 1, 0), 140);
    SignalController small;
    small.addIntersection(1, 2, 5);
    small.setTimingPolicy(&always);
    CHECK_EQ(greenLength(small, 1, 0), 25);
}

TEST(adaptive_extension_does_not_change_configured_green_or_the_next_lane) {
    Rig r;
    r.queues.len[{1, 0}] = 30;
    (void)greenLength(r.sc, 1, 0);
    CHECK_EQ(r.sc.greenSeconds(1), 10);
    r.queues.len[{1, 0}] = 0;
    for (int i = 0; i < 3; ++i) r.sc.tick();                             // yellow
    CHECK_EQ(greenLength(r.sc, 1, 1), 10);
}

TEST(adaptive_each_green_gets_a_fresh_allowance) {
    Rig r(1, 10);
    r.queues.len[{1, 0}] = 30;
    CHECK_EQ(greenLength(r.sc, 1, 0), 30);
    for (int i = 0; i < 3; ++i) r.sc.tick();
    CHECK_EQ(greenLength(r.sc, 1, 0), 30);
}

TEST(adaptive_decides_for_each_intersection_separately) {
    Queues q;
    const AdaptiveTiming p(q.reader());
    SignalController sc;
    sc.addIntersection(1, 2, 10);
    sc.addIntersection(2, 2, 10);
    sc.setTimingPolicy(&p);
    q.len[{1, 0}] = 30;
    for (int i = 0; i < 10; ++i) sc.tick();
    CHECK_EQ(sc.extendedSeconds(1), 5);
    CHECK_EQ(sc.extendedSeconds(2), 0);
    CHECK(sc.stateOf(2, 0) == SignalState::Yellow);
}

TEST(adaptive_can_be_switched_off_and_survives_reset) {
    Rig r;
    r.queues.len[{1, 0}] = 30;
    r.sc.reset();
    r.sc.addIntersection(1, 2, 10);
    for (int i = 0; i < 10; ++i) r.sc.tick();
    CHECK_EQ(r.sc.extendedSeconds(1), 5);                                // still adaptive after reset()
    r.sc.setTimingPolicy(nullptr);
    for (int i = 0; i < 5; ++i) r.sc.tick();
    CHECK(r.sc.stateOf(1, 0) == SignalState::Yellow);                    // the running step finishes
    CHECK_EQ(r.sc.extendedSeconds(1), 5);
}

TEST(adaptive_unknown_intersection_reports_no_extension) {
    SignalController sc;
    CHECK_EQ(sc.extendedSeconds(99), 0);
}

TEST(adaptive_emergency_during_extension_still_opens_within_3s_of_yellow) {
    Rig r;
    r.queues.len[{1, 0}] = 30;
    for (int i = 0; i < 12; ++i) r.sc.tick();
    CHECK_EQ(r.sc.extendedSeconds(1), 5);
    r.sc.preempt(1, 1);
    CHECK(r.sc.stateOf(1, 0) == SignalState::Yellow);
    for (int i = 0; i < 3; ++i) r.sc.tick();
    CHECK(r.sc.stateOf(1, 1) == SignalState::EmergencyGreen);
    CHECK_EQ(r.sc.extendedSeconds(1), 0);
    r.sc.release(1);                                                     // resumes on lane 0, not extended yet
    CHECK(r.sc.stateOf(1, 0) == SignalState::Green);
    CHECK_EQ(r.sc.extendedSeconds(1), 0);
}

TEST(adaptive_emergency_on_the_extended_lane_opens_at_once_and_holds) {
    Rig r;
    r.queues.len[{1, 0}] = 30;
    for (int i = 0; i < 12; ++i) r.sc.tick();
    r.sc.preempt(1, 0);
    CHECK(r.sc.stateOf(1, 0) == SignalState::EmergencyGreen);
    for (int i = 0; i < 200; ++i) r.sc.tick();
    CHECK(r.sc.stateOf(1, 0) == SignalState::EmergencyGreen);            // no end-of-green while held
    CHECK_EQ(r.sc.extendedSeconds(1), 0);
}

// --- real engine + real controller -----------------------------------------------

namespace {

struct EngineRig {
    SignalController signals;
    LcgRandom rng;
    FakeAudit audit;
    CongestionMonitor monitor;
    SimulationEngine engine{signals, rng, audit, monitor};
    AdaptiveTiming adaptive{[this](int id, int lane) { return engine.queueLength(id, lane); }};
};

SimConfig busyLaneConfig(int rateLane0, int rateLane1, int green) {
    IntersectionDef d = makeDef(1, 2, rateLane0, green);
    d.arrivalPerMin[1] = rateLane1;
    SimConfig c;
    c.seed = 3;
    c.intersections = {d};
    return c;
}

// Ticks the engine until lane 0's first green ends and returns how long it lasted.
int firstGreenWithEngine(EngineRig& r) {
    int seconds = 0;
    while (r.signals.stateOf(1, 0) == SignalState::Green && seconds < 1000) {
        CHECK(r.engine.tick().ok());
        ++seconds;
    }
    return seconds;
}

int openLanes(const CongestionSnapshot& snap) {
    int open = 0;
    for (const auto& in : snap) {
        for (const auto& l : in.lanes) {
            if (l.signal != SignalState::Red) ++open;
        }
    }
    return open;
}

}  // namespace

TEST(integration_adaptive_gives_a_busy_lane_more_green_than_fixed) {
    EngineRig on;
    on.signals.setTimingPolicy(&on.adaptive);
    CHECK(on.engine.start(busyLaneConfig(60, 1, 40), "op").ok());
    CHECK_EQ(firstGreenWithEngine(on), 60);

    EngineRig off;
    CHECK(off.engine.start(busyLaneConfig(60, 1, 40), "op").ok());
    CHECK_EQ(firstGreenWithEngine(off), 40);
}

TEST(integration_adaptive_does_not_extend_when_the_opposing_lane_is_busy) {
    EngineRig r;
    r.signals.setTimingPolicy(&r.adaptive);
    CHECK(r.engine.start(busyLaneConfig(60, 60, 40), "op").ok());
    CHECK_EQ(firstGreenWithEngine(r), 40);
}

TEST(integration_adaptive_keeps_one_open_lane_and_green_limit_over_a_long_run) {
    EngineRig r;
    r.signals.setTimingPolicy(&r.adaptive);
    SimConfig c = busyLaneConfig(60, 2, 15);
    c.intersections[0].lanes = 3;
    c.intersections[0].arrivalPerMin = {60, 2, 2};
    CHECK(r.engine.start(c, "op").ok());
    bool sawExtension = false;
    for (int t = 0; t < 3000; ++t) {
        if (t % 400 == 399) CHECK(r.engine.injectEmergency("op", 1, t % 3 + 1).ok());
        CHECK(r.engine.tick().ok());
        CHECK(openLanes(r.engine.snapshot()) == 1);
        CHECK(r.signals.extendedSeconds(1) <= SignalController::kMaxExtension);
        if (r.signals.extendedSeconds(1) > 0) sawExtension = true;
    }
    CHECK(sawExtension);
}

TEST(integration_adaptive_runs_are_repeatable_with_the_same_seed) {             // NFR-08
    const auto run = [] {
        EngineRig r;
        r.signals.setTimingPolicy(&r.adaptive);
        CHECK(r.engine.start(busyLaneConfig(50, 3, 20), "op").ok());
        for (int t = 0; t < 2000; ++t) CHECK(r.engine.tick().ok());
        const Statistics s = r.engine.statistics();
        return std::vector<std::uint64_t>{s.departed, s.totalWait, s.maxQueue, s.highTicks};
    };
    CHECK(run() == run());
}
