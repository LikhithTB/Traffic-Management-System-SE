#include "ui/dashboard.h"

#include <algorithm>
#include <iomanip>
#include <ostream>
#include <sstream>

namespace dashboard {

void render(std::ostream& out, const CongestionSnapshot& snap, Tick now) {
    out << "=== LIVE DASHBOARD  (tick " << now << ") ===\n";
    if (snap.empty()) {
        out << "No simulation data. Start a simulation first.\n";
        return;
    }
    for (const auto& in : snap) {
        out << "Intersection " << in.id << ": " << in.name
            << "   [green " << in.greenSeconds << " s]\n";
        out << "  " << std::left << std::setw(6) << "Lane" << std::setw(11) << "Signal"
            << std::setw(7) << "Queue" << std::setw(12) << "Congestion" << "Load\n";
        for (const auto& lane : in.lanes) {
            const std::size_t bar = std::min<std::size_t>(lane.queueLength, 20);
            out << "  " << std::left << std::setw(6) << lane.lane
                << std::setw(11) << signalName(lane.signal)
                << std::setw(7) << lane.queueLength
                << std::setw(12) << congestionName(lane.congestion)
                << std::string(bar, '#');
            if (lane.hasEmergency) out << "  [EMERGENCY]";
            out << "\n";
        }
    }
}

void renderCommuter(std::ostream& out, const CongestionSnapshot& snap) {
    out << "=== TRAFFIC STATUS (read-only) ===\n";
    if (snap.empty()) {
        out << "No data available.\n";
        return;
    }
    for (const auto& in : snap) {
        out << in.name << "\n";
        for (const auto& lane : in.lanes) {
            out << "  Lane " << lane.lane << ": " << congestionName(lane.congestion) << " congestion\n";
        }
    }
}

std::string renderToString(const CongestionSnapshot& snap, Tick now) {
    std::ostringstream os;
    render(os, snap, now);
    return os.str();
}

std::string renderCommuterToString(const CongestionSnapshot& snap) {
    std::ostringstream os;
    renderCommuter(os, snap);
    return os.str();
}

}  // namespace dashboard
