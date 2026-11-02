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
#include <map>
#include "common/interfaces.h"

class SignalController : public ISignalController {
public:
    static constexpr int kYellowSeconds = 3;
    static constexpr int kMinGreen = 5;
    static constexpr int kMaxGreen = 120;

    void reset() override;
    void addIntersection(int id, int laneCount, int greenSeconds) override;
    Result<void> setGreen(int id, int seconds) override;      // 5..120, else OutOfRange
    void tick() override;                                     // advance all signals by 1 s
    SignalState stateOf(int id, int lane) const override;     // lane is 0-based; unknown -> Red
    int greenSeconds(int id) const override;                  // configured value (0 if unknown id)
    void preempt(int id, int lane) override;
    void release(int id) override;

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
    };

    static void nextLane(State& s);
    std::map<int, State> states_;
};
