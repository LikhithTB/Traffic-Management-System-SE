#include "sim/timing_policy.h"

#include <utility>

bool FixedTiming::shouldExtendGreen(int, int, int) const { return false; }

AdaptiveTiming::AdaptiveTiming(QueueReader queues, std::size_t moderateAt, std::size_t highAt)
    : queues_(std::move(queues)), moderateAt_(moderateAt), highAt_(highAt) {}

void AdaptiveTiming::setThresholds(std::size_t moderateAt, std::size_t highAt) {
    moderateAt_ = moderateAt;
    highAt_ = highAt;
}

bool AdaptiveTiming::shouldExtendGreen(int id, int lane, int laneCount) const {
    if (queues_(id, lane) < highAt_) return false;
    for (int other = 0; other < laneCount; ++other) {
        if (other != lane && queues_(id, other) >= moderateAt_) return false;
    }
    return true;
}
