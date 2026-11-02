#include <string>
#include "sim/signal_controller.h"
#include "test_framework.h"

namespace {

char letter(SignalState s) {
    switch (s) {
        case SignalState::Red:            return 'R';
        case SignalState::Green:          return 'G';
        case SignalState::Yellow:         return 'Y';
        case SignalState::EmergencyGreen: return 'E';
    }
    return '?';
}

// State of one lane right now, then after each of `ticks` ticks. 'G' x5 means 5 green seconds.
std::string trace(SignalController& sc, int id, int lane, int ticks) {
    std::string s(1, letter(sc.stateOf(id, lane)));
    for (int i = 0; i < ticks; ++i) { sc.tick(); s.push_back(letter(sc.stateOf(id, lane))); }
    return s;
}

int openLanes(const SignalController& sc, int id, int lanes) {
    int open = 0;
    for (int i = 0; i < lanes; ++i) if (sc.stateOf(id, i) != SignalState::Red) ++open;
    return open;
}

}  // namespace

TEST(signal_starts_with_lane0_green_others_red) {
    SignalController sc;
    sc.addIntersection(1, 3, 30);
    CHECK(sc.stateOf(1, 0) == SignalState::Green);
    CHECK(sc.stateOf(1, 1) == SignalState::Red);
    CHECK(sc.stateOf(1, 2) == SignalState::Red);
    CHECK_EQ(sc.greenSeconds(1), 30);
}

TEST(signal_green_then_fixed_3s_yellow_then_next_lane) {
    SignalController a;
    a.addIntersection(1, 2, 5);
    // lane 0: 5 s green, 3 s yellow, then red while lane 1 is served, then green again
    CHECK_EQ(trace(a, 1, 0, 16), "GGGGGYYYRRRRRRRRG");
    SignalController b;
    b.addIntersection(1, 2, 5);
    CHECK_EQ(trace(b, 1, 1, 16), "RRRRRRRRGGGGGYYYR");
}

TEST(signal_single_lane_intersection_cycles_green_yellow) {
    SignalController sc;
    sc.addIntersection(1, 1, 5);
    CHECK_EQ(trace(sc, 1, 0, 8), "GGGGGYYYG");
}

TEST(signal_never_opens_two_lanes_at_once) {
    SignalController sc;
    sc.addIntersection(1, 4, 7);
    sc.addIntersection(2, 8, 5);
    for (int i = 0; i < 600; ++i) {
        CHECK(openLanes(sc, 1, 4) == 1);
        CHECK(openLanes(sc, 2, 8) == 1);
        sc.tick();
    }
}

TEST(signal_setgreen_validates_range) {
    SignalController sc;
    sc.addIntersection(1, 2, 30);
    CHECK_EQ(sc.setGreen(1, 4).error, ErrorCode::OutOfRange);
    CHECK_EQ(sc.setGreen(1, 121).error, ErrorCode::OutOfRange);
    CHECK_EQ(sc.setGreen(9, 30).error, ErrorCode::OutOfRange);     // unknown intersection
    CHECK(sc.setGreen(1, 5).ok());
    CHECK(sc.setGreen(1, 120).ok());
    CHECK_EQ(sc.greenSeconds(1), 120);
}

TEST(signal_new_green_time_applies_from_next_cycle_and_is_shown_at_once) {
    SignalController sc;
    sc.addIntersection(1, 2, 5);
    CHECK(sc.setGreen(1, 7).ok());
    CHECK_EQ(sc.greenSeconds(1), 7);                       // dashboard sees it immediately (FR-02)
    // current cycle keeps 5 s greens; the next cycle (lane 0 again) uses 7 s
    CHECK_EQ(trace(sc, 1, 0, 23), "GGGGGYYYRRRRRRRRGGGGGGGY");
}

TEST(signal_unknown_or_bad_arguments_are_harmless) {
    SignalController sc;
    CHECK(sc.stateOf(5, 0) == SignalState::Red);
    CHECK_EQ(sc.greenSeconds(5), 0);
    sc.tick();
    sc.preempt(5, 0);
    sc.release(5);
    sc.addIntersection(1, 0, 30);                          // zero lanes ignored
    CHECK(sc.stateOf(1, 0) == SignalState::Red);
    sc.addIntersection(2, 2, 30);
    CHECK(sc.stateOf(2, -1) == SignalState::Red);
    CHECK(sc.stateOf(2, 2) == SignalState::Red);
    sc.preempt(2, 7);                                      // lane out of range ignored
    CHECK(sc.stateOf(2, 0) == SignalState::Green);
    sc.reset();
    CHECK(sc.stateOf(2, 0) == SignalState::Red);
}

TEST(emergency_on_other_lane_gets_3s_yellow_then_green_within_5s) {
    SignalController sc;
    sc.addIntersection(1, 3, 30);                          // lane 0 is green
    sc.preempt(1, 2);
    CHECK(sc.stateOf(1, 0) == SignalState::Yellow);        // conflicting lane goes yellow at once
    sc.tick(); CHECK(sc.stateOf(1, 0) == SignalState::Yellow);
    sc.tick(); CHECK(sc.stateOf(1, 0) == SignalState::Yellow);
    sc.tick();                                             // 3 s after detection
    CHECK(sc.stateOf(1, 2) == SignalState::EmergencyGreen);
    CHECK(sc.stateOf(1, 0) == SignalState::Red);
    CHECK(sc.stateOf(1, 1) == SignalState::Red);
}

TEST(emergency_on_the_green_lane_opens_immediately) {
    SignalController sc;
    sc.addIntersection(1, 3, 30);
    sc.preempt(1, 0);
    CHECK(sc.stateOf(1, 0) == SignalState::EmergencyGreen);
}

TEST(emergency_during_yellow_waits_for_yellow_to_finish) {
    SignalController sc;
    sc.addIntersection(1, 2, 5);
    for (int i = 0; i < 5; ++i) sc.tick();                 // lane 0 now yellow (3 s)
    CHECK(sc.stateOf(1, 0) == SignalState::Yellow);
    sc.preempt(1, 1);
    CHECK(sc.stateOf(1, 0) == SignalState::Yellow);
    sc.tick(); sc.tick();
    CHECK(sc.stateOf(1, 1) == SignalState::Red);           // still finishing yellow
    sc.tick();
    CHECK(sc.stateOf(1, 1) == SignalState::EmergencyGreen);
}

TEST(emergency_holds_until_release_then_normal_cycle_resumes_with_next_lane) {
    SignalController sc;
    sc.addIntersection(1, 3, 30);
    sc.preempt(1, 1);
    for (int i = 0; i < 3; ++i) sc.tick();
    for (int i = 0; i < 200; ++i) {                        // held far longer than any green time
        sc.tick();
        CHECK(sc.stateOf(1, 1) == SignalState::EmergencyGreen);
        CHECK(openLanes(sc, 1, 3) == 1);
    }
    sc.release(1);
    CHECK(sc.stateOf(1, 2) == SignalState::Green);         // cycle resumes with the following lane
    CHECK(sc.stateOf(1, 1) == SignalState::Red);
    CHECK(openLanes(sc, 1, 3) == 1);
}

TEST(emergency_release_on_last_lane_wraps_and_applies_new_green) {
    SignalController sc;
    sc.addIntersection(1, 2, 5);
    CHECK(sc.setGreen(1, 9).ok());
    sc.preempt(1, 1);
    for (int i = 0; i < 3; ++i) sc.tick();
    sc.release(1);                                         // next lane wraps to lane 0 = new cycle
    CHECK_EQ(trace(sc, 1, 0, 9), "GGGGGGGGGY");            // 9 s green, then yellow
}

TEST(emergency_second_target_while_held_switches_via_yellow) {
    SignalController sc;
    sc.addIntersection(1, 3, 30);
    sc.preempt(1, 0);
    CHECK(sc.stateOf(1, 0) == SignalState::EmergencyGreen);
    sc.preempt(1, 2);
    CHECK(sc.stateOf(1, 0) == SignalState::Yellow);
    for (int i = 0; i < 3; ++i) sc.tick();
    CHECK(sc.stateOf(1, 2) == SignalState::EmergencyGreen);
    CHECK(openLanes(sc, 1, 3) == 1);
}

TEST(emergency_release_without_hold_is_safe_and_cancels_pending_preempt) {
    SignalController sc;
    sc.addIntersection(1, 3, 30);
    sc.release(1);
    CHECK(sc.stateOf(1, 0) == SignalState::Green);
    sc.preempt(1, 2);                                      // yellow running
    sc.release(1);                                         // cancelled before opening
    for (int i = 0; i < 3; ++i) sc.tick();
    CHECK(sc.stateOf(1, 2) != SignalState::EmergencyGreen);
    CHECK(sc.stateOf(1, 1) == SignalState::Green);         // normal cycle went on to the next lane
}

TEST(emergency_on_one_intersection_does_not_affect_another) {
    SignalController sc;
    sc.addIntersection(1, 2, 30);
    sc.addIntersection(2, 2, 30);
    sc.preempt(1, 1);
    for (int i = 0; i < 3; ++i) sc.tick();
    CHECK(sc.stateOf(1, 1) == SignalState::EmergencyGreen);
    CHECK(sc.stateOf(2, 0) == SignalState::Green);
}
