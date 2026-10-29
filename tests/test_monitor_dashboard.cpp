#include "sim/congestion_monitor.h"
#include "ui/dashboard.h"
#include "fakes.h"
#include "test_framework.h"

namespace {
CongestionSnapshot oneLane(std::size_t q, SignalState s = SignalState::Green, bool emerg = false) {
    LaneSnapshot l; l.lane = 1; l.signal = s; l.queueLength = q; l.hasEmergency = emerg;
    IntersectionSnapshot in; in.id = 1; in.name = "Main St"; in.greenSeconds = 30; in.lanes.push_back(l);
    return {in};
}
}

TEST(monitor_congestion_thresholds) {
    CongestionMonitor m;
    CHECK(m.evaluate(oneLane(0))[0].lanes[0].congestion == CongestionLevel::Low);
    CHECK(m.evaluate(oneLane(4))[0].lanes[0].congestion == CongestionLevel::Low);
    CHECK(m.evaluate(oneLane(5))[0].lanes[0].congestion == CongestionLevel::Moderate);
    CHECK(m.evaluate(oneLane(14))[0].lanes[0].congestion == CongestionLevel::Moderate);
    CHECK(m.evaluate(oneLane(15))[0].lanes[0].congestion == CongestionLevel::High);
}

TEST(monitor_thresholds_are_configurable) {
    CongestionMonitor m(3, 6);
    CHECK(m.evaluate(oneLane(3))[0].lanes[0].congestion == CongestionLevel::Moderate);
    CHECK(m.evaluate(oneLane(6))[0].lanes[0].congestion == CongestionLevel::High);
}

TEST(monitor_statistics_for_reports) {
    CongestionMonitor m;
    m.update(oneLane(2));
    m.update(oneLane(15));
    m.update(oneLane(20));
    m.update(oneLane(1));
    m.recordDeparture(4);
    m.recordDeparture(6);
    const Statistics s = m.statistics();
    CHECK_EQ(s.totalTicks, 4u);
    CHECK_EQ(s.highTicks, 2u);
    CHECK_EQ(s.maxQueue, 20u);
    CHECK_EQ(s.departed, 2u);
    CHECK_EQ(s.totalWait, 10u);
    CHECK(m.averageWait() == 5.0);
    CHECK(m.highCongestionPercent() == 50.0);
    CHECK(m.snapshot()[0].lanes[0].congestion == CongestionLevel::Low);
    m.reset();
    CHECK_EQ(m.statistics().totalTicks, 0u);
    CHECK(m.averageWait() == 0.0);
    CHECK(m.highCongestionPercent() == 0.0);
}

TEST(dashboard_shows_signal_queue_and_congestion) {
    CongestionMonitor m;
    const std::string text = dashboard::renderToString(m.evaluate(oneLane(16, SignalState::Red, true)), 125);
    CHECK(text.find("tick 125") != std::string::npos);
    CHECK(text.find("Intersection 1: Main St") != std::string::npos);
    CHECK(text.find("RED") != std::string::npos);
    CHECK(text.find("16") != std::string::npos);
    CHECK(text.find("High") != std::string::npos);
    CHECK(text.find("EMERGENCY") != std::string::npos);
}

TEST(dashboard_empty_snapshot_has_friendly_message) {
    CHECK(dashboard::renderToString({}, 0).find("Start a simulation") != std::string::npos);
}

TEST(commuter_view_is_read_only_congestion_only) {
    CongestionMonitor m;
    const std::string text = dashboard::renderCommuterToString(m.evaluate(oneLane(7, SignalState::Green)));
    CHECK(text.find("Moderate") != std::string::npos);
    CHECK(text.find("Main St") != std::string::npos);
    CHECK(text.find("GREEN") == std::string::npos);       // no signal/control data
    CHECK(text.find("green") == std::string::npos);
}
