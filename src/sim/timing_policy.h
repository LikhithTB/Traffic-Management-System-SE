#pragma once
// Timing policies for the Signal Controller (story #14, FR-09; Strategy pattern).
// The controller asks its policy whether to extend a green that is about to end.
// The controller owns the limits (5 s steps, +20 s, 140 s total); a policy only
// decides whether the traffic justifies another step.
//   FixedTiming    - never extends (default, FR-02 behaviour).
//   AdaptiveTiming - extends a High lane while every opposing queue is below Moderate.
// "Opposing" means every other lane of the same intersection, because the controller
// serves one lane at a time. A single-lane intersection has no opposing queue.
#include <cstddef>
#include <functional>

class ITimingPolicy {
public:
    virtual ~ITimingPolicy() = default;
    virtual bool shouldExtendGreen(int id, int lane, int laneCount) const = 0;   // lane is 0-based
};

class FixedTiming : public ITimingPolicy {
public:
    bool shouldExtendGreen(int id, int lane, int laneCount) const override;
};

class AdaptiveTiming : public ITimingPolicy {
public:
    // Returns the queue length of (intersection id, 0-based lane).
    // In the application: [&engine](int id, int lane) { return engine.queueLength(id, lane); }
    using QueueReader = std::function<std::size_t(int id, int lane)>;

    // Use the same thresholds as the Congestion Monitor (High >= highAt, Moderate >= moderateAt).
    explicit AdaptiveTiming(QueueReader queues, std::size_t moderateAt = 5, std::size_t highAt = 15);

    void setThresholds(std::size_t moderateAt, std::size_t highAt);
    bool shouldExtendGreen(int id, int lane, int laneCount) const override;

private:
    QueueReader queues_;
    std::size_t moderateAt_;
    std::size_t highAt_;
};
