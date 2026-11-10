#pragma once
// Signal Controller (story #8, FR-02, FR-04 timing). State machine per intersection.
//
// Normal cycle: lanes are served one at a time, in order. A served lane is Green
// for `green` seconds, then Yellow for exactly 3 s, then the next lane turns Green.
// All other lanes are Red, so two lanes are never open at once.
// A new green duration (setGreen) is applied from the start of the NEXT cycle,
// i.e. when the controller wraps back to lane 0 (FR-02).
//
// Emergency (FR-04): preempt(id, lane) gives the lane EmergencyGreen after the
// currently open lane has shown 3 s of yellow (<= 5 s total). The lane stays open
// until release(id), then the normal cycle resumes with the following lane.
//
// Timing policy (FR-09): when a green is about to end, the policy may extend it by
// 5 s at a time, up to +20 s and 140 s of green in total, then the normal 3 s yellow
// follows. The extension applies to that green only; greenSeconds() keeps showing the
// configured value. The default policy is FixedTiming, which never extends.
#include <map>
#include "common/interfaces.h"
#include "sim/timing_policy.h"

class SignalController : public ISignalController {
public:
    static constexpr int kYellowSeconds = 3;
    static constexpr int kMinGreen = 5;
    static constexpr int kMaxGreen = 120;
    static constexpr int kExtensionStep = 5;
    static constexpr int kMaxExtension = 20;
    static constexpr int kMaxTotalGreen = 140;

    void reset() override;
    void addIntersection(int id, int laneCount, int greenSeconds) override;
    Result<void> setGreen(int id, int seconds) override;      // 5..120, else OutOfRange
    void tick() override;                                     // advance all signals by 1 s
    SignalState stateOf(int id, int lane) const override;     // lane is 0-based; unknown -> Red
    int greenSeconds(int id) const override;                  // configured value (0 if unknown id)
    void preempt(int id, int lane) override;
    void release(int id) override;

    // FR-09. The policy is not owned and must outlive its use; nullptr selects FixedTiming.
    // It stays selected across reset(). A change applies at the next end-of-green decision.
    void setTimingPolicy(const ITimingPolicy* policy);
    int extendedSeconds(int id) const;                        // extension of the current green

private:
    enum class Phase { Green, Yellow, EmergencyHold };
    struct State {
        int lanes{1};
        int activeGreen{30};        // green used by the running cycle
        int configuredGreen{30};    // value set by the operator, used from the next cycle
        int current{0};             // lane that is open (Green/Yellow/EmergencyGreen)
        Phase phase{Phase::Green};
        int remaining{30};          // seconds left in the current Green or Yellow phase
        bool preempting{false};     // yellow is running because of an emergency
        int target{0};              // emergency lane
        int extended{0};            // seconds added to the current green (FR-09)
    };

    static void nextLane(State& s);
    bool mayExtend(int id, const State& s) const;
    std::map<int, State> states_;
    const ITimingPolicy* policy_{nullptr};                    // nullptr = FixedTiming
};
