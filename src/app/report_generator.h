#pragma once
// Report Generator (story #15, FR-06, UC-10/UC-11, SEC-08).
// Turns the statistics of a run into a CSV or TXT report and hands it to a writer
// (the File Store creates reports/<name>, create-new only). Every export is audited.
//   figures: throughput, average wait, max queue length, % of time at High congestion
#include <cstdint>
#include <functional>
#include <string>
#include "common/interfaces.h"
#include "sim/congestion_monitor.h"
#include "sim/simulation_engine.h"

enum class ReportFormat { Csv, Txt };

struct ReportData {
    std::uint64_t simulatedSeconds{0};
    std::uint32_t seed{0};
    std::size_t intersections{0};
    std::uint64_t throughput{0};            // vehicles that left a queue
    double throughputPerHour{0.0};
    double averageWaitSeconds{0.0};
    std::size_t maxQueue{0};
    double highCongestionPercent{0.0};
};

class ReportGenerator {
public:
    // The writer stores the finished text under the given file name and returns the path.
    // In the application it wraps the File Store's create-new-only report function.
    using Writer = std::function<Result<std::string>(const std::string& fileName,
                                                     const std::string& content)>;

    ReportGenerator(Writer writer, IAuditLogger& audit);

    // <= 64 chars, no '/', '\\', ':' or other path characters, no ".." (E-IO-002).
    static Result<void> validateFileName(const std::string& name);
    // Appends ".csv" / ".txt" when the name does not already end with it.
    static std::string withExtension(const std::string& name, ReportFormat format);

    // Pure text rendering (easy to test).
    static std::string render(const ReportData& data, ReportFormat format);
    // Reads the figures of the current run. totalTicks == 0 means nothing was simulated.
    static ReportData collect(const SimulationEngine& engine, const CongestionMonitor& monitor,
                              std::uint32_t seed);

    // NotRunning (E-SIM-001) if there is no simulation data; BadFileName for an unsafe name;
    // the writer's error (e.g. IoFailure, file already exists) is passed on.
    // Returns the path of the created file.
    Result<std::string> generate(const std::string& user, ReportFormat format,
                                 const std::string& fileName, const ReportData& data);

private:
    Writer writer_;
    IAuditLogger& audit_;
};
