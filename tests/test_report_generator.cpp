#include <chrono>
#include "app/report_generator.h"
#include "fakes.h"
#include "test_framework.h"

namespace {

struct Capture {
    std::vector<std::pair<std::string, std::string>> files;   // name, content
    ErrorCode failWith{ErrorCode::Ok};
    ReportGenerator::Writer writer() {
        return [this](const std::string& name, const std::string& content) -> Result<std::string> {
            if (failWith != ErrorCode::Ok) return Result<std::string>::failure(failWith);
            files.push_back({name, content});
            return Result<std::string>::success("reports/" + name);
        };
    }
};

ReportData sample() {
    ReportData d;
    d.simulatedSeconds = 3600; d.seed = 42; d.intersections = 3;
    d.throughput = 1800; d.throughputPerHour = 1800.0;
    d.averageWaitSeconds = 12.345; d.maxQueue = 18; d.highCongestionPercent = 23.5;
    return d;
}

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

}  // namespace

TEST(report_file_name_rules) {
    CHECK(ReportGenerator::validateFileName("run1.csv").ok());
    CHECK(ReportGenerator::validateFileName(std::string(64, 'a')).ok());
    CHECK_EQ(ReportGenerator::validateFileName("").error, ErrorCode::BadFileName);
    CHECK_EQ(ReportGenerator::validateFileName(std::string(65, 'a')).error, ErrorCode::BadFileName);
    CHECK_EQ(ReportGenerator::validateFileName("../x.csv").error, ErrorCode::BadFileName);
    CHECK_EQ(ReportGenerator::validateFileName("a..b").error, ErrorCode::BadFileName);
    CHECK_EQ(ReportGenerator::validateFileName("dir/file").error, ErrorCode::BadFileName);
    CHECK_EQ(ReportGenerator::validateFileName("dir\\file").error, ErrorCode::BadFileName);
    CHECK_EQ(ReportGenerator::validateFileName("C:evil").error, ErrorCode::BadFileName);
    CHECK_EQ(ReportGenerator::validateFileName(".hidden").error, ErrorCode::BadFileName);
    CHECK_EQ(ReportGenerator::validateFileName("a|b").error, ErrorCode::BadFileName);
    CHECK_EQ(ReportGenerator::validateFileName(std::string("a\nb")).error, ErrorCode::BadFileName);
}

TEST(report_extension_is_added_once) {
    CHECK_EQ(ReportGenerator::withExtension("run1", ReportFormat::Csv), "run1.csv");
    CHECK_EQ(ReportGenerator::withExtension("run1.csv", ReportFormat::Csv), "run1.csv");
    CHECK_EQ(ReportGenerator::withExtension("run1", ReportFormat::Txt), "run1.txt");
    CHECK_EQ(ReportGenerator::withExtension("run1.csv", ReportFormat::Txt), "run1.csv.txt");
}

TEST(report_csv_content) {
    const std::string csv = ReportGenerator::render(sample(), ReportFormat::Csv);
    CHECK(csv.rfind("metric,value\n", 0) == 0);
    CHECK(has(csv, "simulated_seconds,3600\n"));
    CHECK(has(csv, "seed,42\n"));
    CHECK(has(csv, "intersections,3\n"));
    CHECK(has(csv, "throughput_vehicles,1800\n"));
    CHECK(has(csv, "throughput_per_hour,1800.00\n"));
    CHECK(has(csv, "average_wait_seconds,12.35\n") || has(csv, "average_wait_seconds,12.34\n"));
    CHECK(has(csv, "max_queue_length,18\n"));
    CHECK(has(csv, "high_congestion_percent,23.50\n"));
}

TEST(report_txt_content) {
    const std::string txt = ReportGenerator::render(sample(), ReportFormat::Txt);
    CHECK(has(txt, "SIMULATION REPORT"));
    CHECK(has(txt, "1800 vehicles"));
    CHECK(has(txt, "Maximum queue length  : 18"));
    CHECK(has(txt, "23.50 %"));
}

TEST(report_generate_writes_file_and_audits) {
    Capture cap; FakeAudit audit;
    ReportGenerator gen(cap.writer(), audit);
    auto r = gen.generate("alice", ReportFormat::Csv, "run1", sample());
    CHECK(r.ok());
    CHECK_EQ(*r.value, "reports/run1.csv");
    CHECK_EQ(cap.files.size(), 1u);
    CHECK_EQ(cap.files[0].first, "run1.csv");
    CHECK(has(cap.files[0].second, "throughput_vehicles,1800"));
    CHECK_EQ(audit.entries.size(), 1u);
    CHECK_EQ(audit.entries[0].user, "alice");
    CHECK_EQ(audit.entries[0].action, "REPORT_EXPORT");
    CHECK(has(audit.entries[0].result, "run1.csv"));
}

TEST(report_unsafe_name_is_rejected_before_anything_is_written) {
    Capture cap; FakeAudit audit;
    ReportGenerator gen(cap.writer(), audit);
    CHECK_EQ(gen.generate("alice", ReportFormat::Csv, "../../etc/passwd", sample()).error, ErrorCode::BadFileName);
    CHECK_EQ(gen.generate("alice", ReportFormat::Csv, std::string(62, 'a'), sample()).error, ErrorCode::BadFileName);  // + ".csv" = 66
    CHECK(cap.files.empty());
    CHECK_EQ(audit.entries.size(), 2u);                      // refused exports are audited too
    CHECK_EQ(audit.entries[0].result, "E-IO-002");
}

TEST(report_without_simulation_data_is_refused) {
    Capture cap; FakeAudit audit;
    ReportGenerator gen(cap.writer(), audit);
    ReportData empty;                                        // simulatedSeconds == 0
    CHECK_EQ(gen.generate("alice", ReportFormat::Txt, "r", empty).error, ErrorCode::NotRunning);
    CHECK(cap.files.empty());
}

TEST(report_writer_errors_are_passed_on_and_audited) {
    Capture cap; cap.failWith = ErrorCode::IoFailure; FakeAudit audit;   // e.g. file already exists
    ReportGenerator gen(cap.writer(), audit);
    auto r = gen.generate("alice", ReportFormat::Csv, "run1", sample());
    CHECK_EQ(r.error, ErrorCode::IoFailure);
    CHECK_EQ(audit.entries.size(), 1u);
    CHECK_EQ(audit.entries[0].result, "E-IO-001");
}

TEST(report_collects_real_numbers_from_the_engine_and_exports_fast) {
    FakeSignals signals; LcgRandom rng; FakeAudit audit; CongestionMonitor monitor;
    SimulationEngine engine(signals, rng, audit, monitor);
    SimConfig cfg; cfg.seed = 42;
    cfg.intersections = {makeDef(1, 4, 30, 20), makeDef(2, 2, 45, 20), makeDef(3, 3, 20, 20)};
    CHECK(engine.start(cfg, "op").ok());
    for (int i = 0; i < 3600; ++i) CHECK(engine.tick().ok());       // one simulated hour

    const ReportData d = ReportGenerator::collect(engine, monitor, 42);
    CHECK_EQ(d.simulatedSeconds, 3600u);
    CHECK_EQ(d.seed, 42u);
    CHECK_EQ(d.intersections, 3u);
    CHECK_EQ(d.throughput, engine.statistics().departed);
    CHECK(d.throughput > 0u);
    CHECK(d.throughputPerHour == static_cast<double>(d.throughput));   // exactly 3600 s
    CHECK(d.averageWaitSeconds >= 0.0);
    CHECK(d.highCongestionPercent >= 0.0 && d.highCongestionPercent <= 100.0);

    Capture cap; ReportGenerator gen(cap.writer(), audit);
    const auto begin = std::chrono::steady_clock::now();
    CHECK(gen.generate("op", ReportFormat::Csv, "hour", d).ok());
    CHECK(gen.generate("op", ReportFormat::Txt, "hour", d).ok());
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
    CHECK(ms < 3000);                                                  // FR-06: export within 3 s
    CHECK_EQ(cap.files.size(), 2u);
}

TEST(report_same_seed_gives_identical_report_text) {
    auto run = [] {
        FakeSignals signals; LcgRandom rng; FakeAudit audit; CongestionMonitor monitor;
        SimulationEngine engine(signals, rng, audit, monitor);
        SimConfig cfg; cfg.seed = 7; cfg.intersections = {makeDef(1, 4, 30, 20)};
        CHECK(engine.start(cfg, "op").ok());
        for (int i = 0; i < 600; ++i) CHECK(engine.tick().ok());
        return ReportGenerator::render(ReportGenerator::collect(engine, monitor, 7), ReportFormat::Csv);
    };
    CHECK_EQ(run(), run());                                            // NFR-08
}
