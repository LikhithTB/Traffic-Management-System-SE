// Commuter read-only view (story #16, FR-10, SEC-01, TC-13).
#include <filesystem>
#include <fstream>
#include <sstream>
#include <type_traits>

#include "fakes.h"
#include "test_framework.h"
#include "ui/commuter_view.h"

namespace fs = std::filesystem;

// Read-only by construction: the view keeps a const engine and has no non-const member function.
static_assert(std::is_same<decltype(&CommuterView::showCongestion), void (CommuterView::*)(std::ostream&) const>::value,
              "showCongestion must be const");
static_assert(std::is_same<decltype(&CommuterView::showReport),
                           void (CommuterView::*)(std::ostream&, const std::string&) const>::value,
              "showReport must be const");
static_assert(std::is_same<decltype(&CommuterView::readReport),
                           Result<std::string> (CommuterView::*)(const std::string&) const>::value,
              "readReport must be const");
static_assert(!std::is_copy_assignable<CommuterView>::value, "the view holds a const engine reference");

namespace {

struct Rig {
    FakeSignals signals;
    FixedRandom rng{0};                 // every lane gets a vehicle every tick
    FakeAudit audit;
    CongestionMonitor monitor;
    SimulationEngine engine{signals, rng, audit, monitor};
    fs::path dir;
    explicit Rig(const std::string& name) {
        dir = fs::temp_directory_path() / ("tms_commuter_" + name);
        fs::remove_all(dir);
        fs::create_directories(dir);
    }
    ~Rig() { std::error_code ec; fs::remove_all(dir, ec); }
    void file(const std::string& name, const std::string& text) const {
        std::ofstream f(dir / name, std::ios::binary);
        f << text;
    }
    CommuterView view() const { return CommuterView(engine, dir.string()); }
};

std::string congestion(const CommuterView& v) {
    std::ostringstream out;
    v.showCongestion(out);
    return out.str();
}

}  // namespace

TEST(commuter_view_without_simulation_shows_no_data) {
    Rig r("nodata");
    CHECK(congestion(r.view()).find("No data available.") != std::string::npos);
}

TEST(commuter_view_shows_congestion_but_no_signals_queues_or_controls) {
    Rig r("congestion");
    SimConfig c;
    c.intersections = {makeDef(1, 2, 60, 30)};
    c.intersections[0].name = "Ring Rd";
    CHECK(r.engine.start(c, "op").ok());
    r.signals.force(1, 0, SignalState::Red);                            // lane 1 never drains
    for (int i = 0; i < 20; ++i) CHECK(r.engine.tick().ok());
    const std::string text = congestion(r.view());
    CHECK(text.find("Ring Rd") != std::string::npos);
    CHECK(text.find("Lane 1: High congestion") != std::string::npos);
    CHECK(text.find("GREEN") == std::string::npos);
    CHECK(text.find("RED") == std::string::npos);
    CHECK(text.find("Queue") == std::string::npos);
    CHECK(text.find("tick") == std::string::npos);
}

TEST(commuter_view_does_not_change_the_simulation) {
    Rig r("readonly");
    SimConfig c;
    c.intersections = {makeDef(1, 3, 30, 20)};
    CHECK(r.engine.start(c, "op").ok());
    for (int i = 0; i < 10; ++i) CHECK(r.engine.tick().ok());
    const Tick before = r.engine.now();
    const std::size_t queue = r.engine.queueLength(1, 0);
    const std::size_t auditEntries = r.audit.entries.size();
    const CommuterView v = r.view();
    for (int i = 0; i < 5; ++i) (void)congestion(v);
    CHECK_EQ(r.engine.now(), before);
    CHECK_EQ(r.engine.queueLength(1, 0), queue);
    CHECK(r.engine.state() == SimulationEngine::State::Running);
    CHECK_EQ(r.audit.entries.size(), auditEntries);
}

TEST(commuter_view_lists_only_completed_reports_sorted) {
    Rig r("list");
    r.file("b.txt", "B");
    r.file("a.csv", "A");
    r.file("notes.md", "not a report");
    r.file("partial.csv.tmp", "unfinished");
    fs::create_directories(r.dir / "sub.csv");                          // a folder is not a report
    const std::vector<std::string> names = r.view().completedReports();
    CHECK(names == (std::vector<std::string>{"a.csv", "b.txt"}));
    std::ostringstream out;
    r.view().listReports(out);
    CHECK_EQ(out.str(), "Reports:\n  a.csv\n  b.txt\n");
}

TEST(commuter_view_no_reports_folder_means_no_reports) {
    Rig r("nofolder");
    const CommuterView v(r.engine, (r.dir / "missing").string());
    std::ostringstream out;
    v.listReports(out);
    CHECK_EQ(out.str(), "No reports yet.\n");
}

TEST(commuter_view_reads_a_completed_report) {
    Rig r("read");
    r.file("run1.csv", "metric,value\nthroughput_vehicles,7\n");
    const Result<std::string> rep = r.view().readReport("run1.csv");
    CHECK(rep.ok());
    CHECK_EQ(*rep.value, "metric,value\nthroughput_vehicles,7\n");
    std::ostringstream out;
    r.view().showReport(out, "run1.csv");
    CHECK_EQ(out.str(), *rep.value);
}

TEST(commuter_view_refuses_unsafe_names_other_files_and_missing_reports) {
    Rig r("refuse");
    r.file("notes.md", "private");
    const CommuterView v = r.view();
    CHECK_EQ(v.readReport("../users.dat").error, ErrorCode::BadFileName);
    CHECK_EQ(v.readReport("..\\audit.log").error, ErrorCode::BadFileName);
    CHECK_EQ(v.readReport("sub/x.csv").error, ErrorCode::BadFileName);
    CHECK_EQ(v.readReport("notes.md").error, ErrorCode::IoFailure);         // exists, but not a report
    CHECK_EQ(v.readReport("missing.csv").error, ErrorCode::IoFailure);
    std::ostringstream out;
    v.showReport(out, "../x");
    CHECK(out.str().find("[E-IO-002]") == 0);
    std::ostringstream out2;
    v.showReport(out2, "notes.md");
    CHECK(out2.str().find("[E-IO-001]") == 0);
    CHECK(out2.str().find("private") == std::string::npos);
}

TEST(commuter_view_report_name_rule) {
    CHECK(CommuterView::isCompletedReportName("a.csv"));
    CHECK(CommuterView::isCompletedReportName("day_2.txt"));
    CHECK(!CommuterView::isCompletedReportName(".csv"));
    CHECK(!CommuterView::isCompletedReportName("a.csv.tmp"));
    CHECK(!CommuterView::isCompletedReportName("a.CSVX"));
    CHECK(!CommuterView::isCompletedReportName("../a.csv"));
}
