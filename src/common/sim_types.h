#pragma once
// Simulation data shared by engine, signal controller, monitor, dashboard and reports.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "common/types.h"

// One intersection definition (FR-08). Lanes are numbered 1..lanes for users,
// 0..lanes-1 inside module interfaces.
struct IntersectionDef {
    int id{0};
    std::string name;
    int lanes{4};                     // 1..8
    std::vector<int> arrivalPerMin;   // one entry per lane, 1..60 vehicles/min
    int greenSeconds{30};             // 5..120
};

// Everything the engine needs to start a run (FR-05, FR-08, NFR-08).
struct SimConfig {
    std::uint32_t seed{1};
    std::vector<IntersectionDef> intersections;   // 1..10
    std::size_t moderateAt{5};                    // Low < 5
    std::size_t highAt{15};                       // High >= 15
};

struct LaneSnapshot {
    int lane{0};                                  // 1-based, for display
    SignalState signal{SignalState::Red};
    std::size_t queueLength{0};
    CongestionLevel congestion{CongestionLevel::Low};
    bool hasEmergency{false};
};

struct IntersectionSnapshot {
    int id{0};
    std::string name;
    int greenSeconds{0};
    std::vector<LaneSnapshot> lanes;
};

using CongestionSnapshot = std::vector<IntersectionSnapshot>;

inline const char* signalName(SignalState s) {
    switch (s) {
        case SignalState::Red:           return "RED";
        case SignalState::Green:         return "GREEN";
        case SignalState::Yellow:        return "YELLOW";
        case SignalState::EmergencyGreen:return "EMERG-GRN";
    }
    return "?";
}

inline const char* congestionName(CongestionLevel c) {
    switch (c) {
        case CongestionLevel::Low:      return "Low";
        case CongestionLevel::Moderate: return "Moderate";
        case CongestionLevel::High:     return "High";
    }
    return "?";
}
