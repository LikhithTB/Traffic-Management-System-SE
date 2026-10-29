#pragma once
// Emergency Handler (story #13, FR-04). Keeps emergency vehicles in a priority
// queue ordered by arrival time (simultaneous emergencies are served in order of
// arrival). The engine asks it which emergency is currently being served.
#include <cstdint>
#include <queue>
#include <vector>
#include "common/types.h"

struct EmergencyEvent {
    Tick arrivalTick{0};
    std::uint64_t seq{0};          // tie-breaker for the same tick
    std::uint64_t vehicleId{0};
    int intersectionId{0};
    int lane{0};                   // 0-based
};

class EmergencyHandler {
public:
    void report(Tick arrival, std::uint64_t vehicleId, int intersectionId, int lane);
    bool hasActive() const { return !queue_.empty(); }
    const EmergencyEvent& active() const { return queue_.top(); }   // only if hasActive()
    void complete();                                                 // active vehicle departed
    void clear();
    std::size_t pending() const { return queue_.size(); }

private:
    struct Later {
        bool operator()(const EmergencyEvent& a, const EmergencyEvent& b) const {
            if (a.arrivalTick != b.arrivalTick) return a.arrivalTick > b.arrivalTick;
            return a.seq > b.seq;
        }
    };
    std::priority_queue<EmergencyEvent, std::vector<EmergencyEvent>, Later> queue_;
    std::uint64_t nextSeq_{0};
};
