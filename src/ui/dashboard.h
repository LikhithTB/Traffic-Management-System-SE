#pragma once
// Live Dashboard Renderer + Commuter read-only view (story #10, FR-03, FR-10).
// Pure output functions: they only read a snapshot, so they cannot change data.
#include <iosfwd>
#include <string>
#include "common/sim_types.h"

namespace dashboard {

// Operator/Admin view: signal state, queue length, congestion level per lane.
void render(std::ostream& out, const CongestionSnapshot& snap, Tick now);

// Commuter view (no login): congestion status only, no controls, no signal data.
void renderCommuter(std::ostream& out, const CongestionSnapshot& snap);

std::string renderToString(const CongestionSnapshot& snap, Tick now);
std::string renderCommuterToString(const CongestionSnapshot& snap);

}  // namespace dashboard
