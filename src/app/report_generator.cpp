#include "app/report_generator.h"

#include <iomanip>
#include <sstream>

namespace {

std::string fixed2(double v) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(2) << v;
    return os.str();
}

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

}  // namespace

ReportGenerator::ReportGenerator(Writer writer, IAuditLogger& audit)
    : writer_(std::move(writer)), audit_(audit) {}

Result<void> ReportGenerator::validateFileName(const std::string& name) {
    if (name.empty() || name.size() > 64) return Result<void>::failure(ErrorCode::BadFileName);
    if (name[0] == '.' || name.find("..") != std::string::npos) {
        return Result<void>::failure(ErrorCode::BadFileName);
    }
    for (char c : name) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x20 || u == 0x7f || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|') {
            return Result<void>::failure(ErrorCode::BadFileName);
        }
    }
    return Result<void>::success();
}

std::string ReportGenerator::withExtension(const std::string& name, ReportFormat format) {
    const std::string ext = (format == ReportFormat::Csv) ? ".csv" : ".txt";
    return endsWith(name, ext) ? name : name + ext;
}

std::string ReportGenerator::render(const ReportData& d, ReportFormat format) {
    std::ostringstream os;
    if (format == ReportFormat::Csv) {
        os << "metric,value\n"
           << "simulated_seconds," << d.simulatedSeconds << "\n"
           << "seed," << d.seed << "\n"
           << "intersections," << d.intersections << "\n"
           << "throughput_vehicles," << d.throughput << "\n"
           << "throughput_per_hour," << fixed2(d.throughputPerHour) << "\n"
           << "average_wait_seconds," << fixed2(d.averageWaitSeconds) << "\n"
           << "max_queue_length," << d.maxQueue << "\n"
           << "high_congestion_percent," << fixed2(d.highCongestionPercent) << "\n";
    } else {
        os << "TRAFFIC MANAGEMENT SYSTEM - SIMULATION REPORT\n"
           << "=============================================\n"
           << "Simulated time        : " << d.simulatedSeconds << " s\n"
           << "Random seed           : " << d.seed << "\n"
           << "Intersections         : " << d.intersections << "\n"
           << "Throughput            : " << d.throughput << " vehicles (" << fixed2(d.throughputPerHour)
           << " per hour)\n"
           << "Average wait          : " << fixed2(d.averageWaitSeconds) << " s\n"
           << "Maximum queue length  : " << d.maxQueue << " vehicles\n"
           << "Time at High congestion: " << fixed2(d.highCongestionPercent) << " %\n";
    }
    return os.str();
}

ReportData ReportGenerator::collect(const SimulationEngine& engine, const CongestionMonitor& monitor,
                                    std::uint32_t seed) {
    const Statistics s = engine.statistics();
    ReportData d;
    d.simulatedSeconds = s.totalTicks;
    d.seed = seed;
    d.intersections = engine.snapshot().size();
    d.throughput = s.departed;
    d.throughputPerHour = s.totalTicks == 0 ? 0.0
                                            : static_cast<double>(s.departed) * 3600.0 / static_cast<double>(s.totalTicks);
    d.averageWaitSeconds = monitor.averageWait();
    d.maxQueue = s.maxQueue;
    d.highCongestionPercent = monitor.highCongestionPercent();
    return d;
}

Result<std::string> ReportGenerator::generate(const std::string& user, ReportFormat format,
                                              const std::string& fileName, const ReportData& data) {
    const std::string name = withExtension(fileName, format);
    const Result<void> nameOk = validateFileName(name);
    if (!nameOk.ok()) {
        (void)audit_.log(user, "REPORT_EXPORT", errorCodeId(nameOk.error));
        return Result<std::string>::failure(nameOk.error);
    }
    if (data.simulatedSeconds == 0) {                                  // nothing simulated yet
        (void)audit_.log(user, "REPORT_EXPORT", errorCodeId(ErrorCode::NotRunning));
        return Result<std::string>::failure(ErrorCode::NotRunning);
    }
    const Result<std::string> written = writer_(name, render(data, format));
    if (!written.ok()) {
        (void)audit_.log(user, "REPORT_EXPORT", errorCodeId(written.error));
        return written;
    }
    (void)audit_.log(user, "REPORT_EXPORT", "OK file=" + name);
    return written;
}
