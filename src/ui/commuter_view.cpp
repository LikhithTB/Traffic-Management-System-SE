#include "ui/commuter_view.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <sstream>
#include <utility>

#include "app/report_generator.h"
#include "common/messages.h"
#include "ui/dashboard.h"

namespace fs = std::filesystem;

namespace {

bool endsWith(const std::string& s, const std::string& tail) {
    return s.size() > tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

void showError(std::ostream& out, ErrorCode e) {
    out << "[" << errorCodeId(e) << "] " << errorMessage(e, "Report file name", 0, 0) << "\n";
}

}  // namespace

CommuterView::CommuterView(const SimulationEngine& engine, std::string reportsDir)
    : engine_(engine), reportsDir_(std::move(reportsDir)) {}

bool CommuterView::isCompletedReportName(const std::string& name) {
    return ReportGenerator::validateFileName(name).ok() && (endsWith(name, ".csv") || endsWith(name, ".txt"));
}

void CommuterView::showCongestion(std::ostream& out) const {
    dashboard::renderCommuter(out, engine_.snapshot());
}

std::vector<std::string> CommuterView::completedReports() const {
    std::vector<std::string> names;
    std::error_code ec;
    for (fs::directory_iterator it(reportsDir_, ec), end; !ec && it != end; it.increment(ec)) {
        const std::string name = it->path().filename().string();
        if (it->is_regular_file(ec) && isCompletedReportName(name)) names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

void CommuterView::listReports(std::ostream& out) const {
    const std::vector<std::string> names = completedReports();
    if (names.empty()) {
        out << "No reports yet.\n";
        return;
    }
    out << "Reports:\n";
    for (const auto& n : names) out << "  " << n << "\n";
}

Result<std::string> CommuterView::readReport(const std::string& name) const {
    const Result<void> safe = ReportGenerator::validateFileName(name);
    if (!safe.ok()) return Result<std::string>::failure(safe.error);
    if (!isCompletedReportName(name)) return Result<std::string>::failure(ErrorCode::IoFailure);
    std::ifstream file((fs::path(reportsDir_) / name).string(), std::ios::binary);
    if (!file) return Result<std::string>::failure(ErrorCode::IoFailure);
    std::ostringstream text;
    text << file.rdbuf();
    return Result<std::string>::success(text.str());
}

void CommuterView::showReport(std::ostream& out, const std::string& name) const {
    const Result<std::string> r = readReport(name);
    if (!r.ok()) {
        showError(out, r.error);
        return;
    }
    out << *r.value;
}
