#pragma once
// Commuter Read-only View (story #16, FR-10, SEC-01, UC-12, TC-13).
// Public view without login: congestion status (FR-03) and completed reports (FR-06) only.
// Read-only by construction: it holds a CONST engine reference and every function is const,
// so the compiler refuses any call that could start, stop or change anything.
// "Completed report" = a .csv or .txt file in the reports folder (what the Report Generator writes).
#include <iosfwd>
#include <string>
#include <vector>
#include "common/result.h"
#include "sim/simulation_engine.h"

class CommuterView {
public:
    CommuterView(const SimulationEngine& engine, std::string reportsDir);

    // Congestion level per lane; no signal states, queue numbers or controls.
    void showCongestion(std::ostream& out) const;

    // Sorted names of the completed reports (other files in the folder are not shown).
    std::vector<std::string> completedReports() const;
    void listReports(std::ostream& out) const;

    // BadFileName (E-IO-002) for an unsafe name; IoFailure (E-IO-001) if it is not a completed report.
    Result<std::string> readReport(const std::string& name) const;
    void showReport(std::ostream& out, const std::string& name) const;

    static bool isCompletedReportName(const std::string& name);

private:
    const SimulationEngine& engine_;
    std::string reportsDir_;
};
