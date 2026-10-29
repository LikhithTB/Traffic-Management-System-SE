#include "sim/congestion_monitor.h"

CongestionMonitor::CongestionMonitor(std::size_t moderateAt, std::size_t highAt)
    : moderateAt_(moderateAt), highAt_(highAt) {}

void CongestionMonitor::setThresholds(std::size_t moderateAt, std::size_t highAt) {
    moderateAt_ = moderateAt;
    highAt_ = highAt;
}

void CongestionMonitor::reset() {
    last_.clear();
    stats_ = Statistics{};
}

CongestionSnapshot CongestionMonitor::evaluate(CongestionSnapshot raw) const {
    for (auto& intersection : raw) {
        for (auto& lane : intersection.lanes) {
            lane.congestion = congestionFor(lane.queueLength, moderateAt_, highAt_);
        }
    }
    return raw;
}

void CongestionMonitor::update(const CongestionSnapshot& raw) {
    last_ = evaluate(raw);
    ++stats_.totalTicks;
    bool anyHigh = false;
    for (const auto& intersection : last_) {
        for (const auto& lane : intersection.lanes) {
            if (lane.queueLength > stats_.maxQueue) stats_.maxQueue = lane.queueLength;
            if (lane.congestion == CongestionLevel::High) anyHigh = true;
        }
    }
    if (anyHigh) ++stats_.highTicks;
}

void CongestionMonitor::recordDeparture(Tick waitTicks) {
    ++stats_.departed;
    stats_.totalWait += waitTicks;
}

double CongestionMonitor::averageWait() const {
    if (stats_.departed == 0) return 0.0;
    return static_cast<double>(stats_.totalWait) / static_cast<double>(stats_.departed);
}

double CongestionMonitor::highCongestionPercent() const {
    if (stats_.totalTicks == 0) return 0.0;
    return 100.0 * static_cast<double>(stats_.highTicks) / static_cast<double>(stats_.totalTicks);
}
