// NFR-03 / TC-15 (story #18): 24 simulated hours (86,400 ticks) at the maximum configuration
// (10 intersections x 8 lanes) with random emergencies, pauses, green-time changes, dashboard
// renders and report exports. The real engine, signal controller, monitor and file audit logger
// are used. Build and run it with tools/reliability.sh (plain, ASan+UBSan, Valgrind).
//
//   reliability_run [--ticks N] [--rate R] [--seed S] [--adaptive 0|1] [--quiet]
//     R = arrivals per minute per lane (1..60, default 3: the load the signals can clear)
//
// Exit code 0 = all ticks completed and every command succeeded. Memory (VmRSS) is printed once
// per simulated hour together with the number of queued vehicles, so growth caused by a real
// backlog can be told apart from growth caused by a leak.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "app/report_generator.h"
#include "infra/audit_logger.h"
#include "rng.hpp"
#include "sim/signal_controller.h"
#include "sim/simulation_engine.h"
#include "sim/timing_policy.h"
#include "ui/dashboard.h"

namespace {

class RunRng : public IRandom {                     // same adapter the application uses
public:
    void seed(std::uint32_t s) override { rng_.reseed(s); }
    std::uint32_t next() override { return static_cast<std::uint32_t>(rng_.next() >> 32); }
private:
    tms::SeededRng rng_{1};
};

long rssKb() {                                       // resident set size, Linux only
    std::ifstream f("/proc/self/status");
    std::string key;
    while (f >> key) {
        if (key == "VmRSS:") { long kb = 0; f >> kb; return kb; }
        f.ignore(256, '\n');
    }
    return -1;
}

std::size_t queued(const SimulationEngine& e) {
    std::size_t n = 0;
    for (const auto& i : e.snapshot())
        for (const auto& l : i.lanes) n += l.queueLength;
    return n;
}

struct Args { long ticks = 86400; int rate = 3; unsigned seed = 42; bool adaptive = false; bool quiet = false; };

Args parse(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        const std::string k = argv[i];
        if (k == "--quiet") a.quiet = true;
        else if (i + 1 < argc && k == "--ticks") a.ticks = std::atol(argv[++i]);
        else if (i + 1 < argc && k == "--rate") a.rate = std::atoi(argv[++i]);
        else if (i + 1 < argc && k == "--seed") a.seed = static_cast<unsigned>(std::atol(argv[++i]));
        else if (i + 1 < argc && k == "--adaptive") a.adaptive = std::atoi(argv[++i]) != 0;
    }
    return a;
}

IntersectionDef makeIntersection(int id, int rate) {
    IntersectionDef d;
    d.id = id; d.name = "Intersection " + std::to_string(id); d.lanes = 8;
    d.arrivalPerMin.assign(8, rate);
    d.greenSeconds = 20 + id;
    return d;
}

}  // namespace

int main(int argc, char** argv) {
    const Args a = parse(argc, argv);
    std::setvbuf(stdout, nullptr, _IOLBF, 0);                       // progress visible while running
    const auto logPath = (std::filesystem::temp_directory_path() / "tms_reliability_audit.log").string();
    std::filesystem::remove(logPath);

    FileAuditLogger audit(logPath);
    SignalController signals;
    RunRng rng;
    CongestionMonitor monitor;
    SimulationEngine engine(signals, rng, audit, monitor);
    AdaptiveTiming adaptive([&engine](int id, int lane) { return engine.queueLength(id, lane); });
    if (a.adaptive) signals.setTimingPolicy(&adaptive);

    SimConfig cfg;
    cfg.seed = a.seed;
    for (int i = 1; i <= 10; ++i) cfg.intersections.push_back(makeIntersection(i, a.rate));
    if (!engine.start(cfg, "reliability").ok()) { std::cerr << "start failed\n"; return 2; }

    std::printf("NFR-03 run: %ld ticks, 10 x 8 lanes, %d arrivals/min/lane, seed %u, adaptive %s\n",
                a.ticks, a.rate, a.seed, a.adaptive ? "on" : "off");
    const long startKb = rssKb();
    std::printf("%-6s %-10s %-12s %-10s\n", "hour", "RSS (kB)", "queued veh.", "tick");
    std::printf("%-6d %-10ld %-12zu %-10d\n", 0, startKb, queued(engine), 0);

    long failures = 0, emergencies = 0, greens = 0, pauses = 0, renders = 0, reports = 0, peakKb = startKb;
    long hour1Kb = -1;
    const auto begin = std::chrono::steady_clock::now();
    for (long t = 1; t <= a.ticks; ++t) {
        if (t % 211 == 0) {                                          // random emergency (deterministic schedule)
            const int id = static_cast<int>(t / 211 % 10) + 1, lane = static_cast<int>(t / 211 % 8) + 1;
            if (!engine.injectEmergency("reliability", id, lane).ok()) ++failures; else ++emergencies;
        }
        if (t % 997 == 0) {                                          // operator changes a green time
            if (!signals.setGreen(static_cast<int>(t / 997 % 10) + 1, 5 + static_cast<int>(t / 997 % 116)).ok()) ++failures;
            else ++greens;
        }
        if (t % 5003 == 0) {                                         // pause / resume (tick is refused while paused)
            if (!engine.pause("reliability").ok() || engine.tick().ok() || !engine.resume("reliability").ok()) ++failures;
            else ++pauses;
        }
        if (!engine.tick().ok()) ++failures;
        if (t % 600 == 0) {
            if (dashboard::renderToString(engine.snapshot(), engine.now()).empty()) ++failures;
            ++renders;
        }
        if (t % 3600 == 0) {                                         // hourly CSV report + memory sample
            const std::string csv = ReportGenerator::render(ReportGenerator::collect(engine, monitor, a.seed), ReportFormat::Csv);
            if (csv.empty()) ++failures; else ++reports;
            const long kb = rssKb();
            if (t == 3600) hour1Kb = kb;
            if (kb > peakKb) peakKb = kb;
            if (!a.quiet || t % 21600 == 0)
                std::printf("%-6ld %-10ld %-12zu %-10ld\n", t / 3600, kb, queued(engine), t);
        }
    }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    const long endKb = rssKb();

    auto pct = [](long from, long to) { return from > 0 ? 100.0 * static_cast<double>(to - from) / static_cast<double>(from) : 0.0; };
    std::printf("\nticks run          : %ld (engine clock %llu s) in %.1f s wall\n", a.ticks,
                static_cast<unsigned long long>(engine.now()), secs);
    std::printf("commands           : %ld emergencies, %ld green changes, %ld pause/resume, %ld dashboards, %ld reports\n",
                emergencies, greens, pauses, renders, reports);
    std::printf("failed commands    : %ld\n", failures);
    std::printf("memory at start    : %ld kB\n", startKb);
    if (hour1Kb > 0) std::printf("memory after 1 h   : %ld kB  (steady-state baseline)\n", hour1Kb);
    std::printf("memory at end      : %ld kB  (peak %ld kB)\n", endKb, peakKb);
    std::printf("growth start->end  : %+.1f %%\n", pct(startKb, endKb));
    if (hour1Kb > 0) std::printf("growth 1 h->end    : %+.1f %%   (limit +10 %%)\n", pct(hour1Kb, endKb));
    std::printf("queued vehicles at end: %zu\n", queued(engine));
    std::uint64_t h = 1469598103934665603ULL;                        // FNV-1a of the final CSV report: two runs
    for (unsigned char c : ReportGenerator::render(ReportGenerator::collect(engine, monitor, a.seed), ReportFormat::Csv))
        h = (h ^ c) * 1099511628211ULL;                              // with the same arguments must print the same value
    std::printf("final report checksum : %016llx\n", static_cast<unsigned long long>(h));
    std::filesystem::remove(logPath);
    return (failures == 0 && engine.now() == static_cast<unsigned long long>(a.ticks)) ? 0 : 1;
}
