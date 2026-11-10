// Story #17: performance and repeatability checks (NFR-01, NFR-02, NFR-08).
// Test Plan: TC-14 (performance) and TC-16 (repeatability). Real SignalController + engine.
#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include "app/report_generator.h"
#include "fakes.h"
#include "sim/signal_controller.h"
#include "sim/simulation_engine.h"
#include "ui/dashboard.h"
#include "test_framework.h"

namespace {

struct PerfRig {
    SignalController signals; LcgRandom rng; FakeAudit audit; CongestionMonitor monitor;
    SimulationEngine engine{signals, rng, audit, monitor};
};

// Maximum configuration from the SRS: 10 intersections x 8 lanes, highest arrival rate.
SimConfig maxConfig(std::uint32_t seed) {
    SimConfig c; c.seed = seed;
    for (int i = 1; i <= 10; ++i) c.intersections.push_back(makeDef(i, 8, 60, 20 + i));
    return c;
}

using Clock = std::chrono::steady_clock;
double msSince(Clock::time_point t) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}

// One full run with a fixed emergency schedule; returns the CSV report text.
std::string reportOfRun(std::uint32_t seed, int ticks) {
    PerfRig r;
    CHECK(r.engine.start(maxConfig(seed), "op").ok());
    for (int t = 0; t < ticks; ++t) {
        if (t % 211 == 0) CHECK(r.engine.injectEmergency("op", t / 211 % 10 + 1, t / 211 % 8 + 1).ok());
        CHECK(r.engine.tick().ok());
    }
    return ReportGenerator::render(ReportGenerator::collect(r.engine, r.monitor, seed), ReportFormat::Csv);
}

}  // namespace

// NFR-01 / TC-14: 95% of operator commands are processed AND shown within 1 s (200 commands).
TEST(perf_95_percent_of_commands_finish_within_1_second_at_max_configuration) {
    PerfRig r;
    CHECK(r.engine.start(maxConfig(1), "op").ok());
    for (int t = 0; t < 600; ++t) CHECK(r.engine.tick().ok());   // let queues build up first

    std::vector<double> ms;
    for (int i = 0; i < 200; ++i) {
        const auto begin = Clock::now();
        switch (i % 5) {
            case 0: CHECK(r.engine.injectEmergency("op", i % 10 + 1, i % 8 + 1).ok()); break;
            case 1: CHECK(r.signals.setGreen(i % 10 + 1, 5 + i % 100).ok()); break;
            case 2: CHECK(r.engine.pause("op").ok()); CHECK(r.engine.resume("op").ok()); break;
            case 3: CHECK(r.engine.tick().ok()); break;
            default: break;                                       // case 4: just read the screen
        }
        const std::string screen = dashboard::renderToString(r.engine.snapshot(), r.engine.now());
        CHECK(!screen.empty());                                   // "reflected on screen"
        ms.push_back(msSince(begin));
    }
    std::sort(ms.begin(), ms.end());
    const double p95 = ms[static_cast<std::size_t>(ms.size() * 95 / 100) - 1];
    std::cout << "    (200 commands at 10 x 8 lanes: p95 " << p95 << " ms, max " << ms.back() << " ms)\n";
#if defined(__OPTIMIZE__) && !defined(__SANITIZE_ADDRESS__)   // timing limits: optimized builds only
    CHECK(p95 <= 1000.0);
#endif
}

// NFR-02 / TC-14: fast mode >= 1000 ticks/s at max configuration (one hour in <= 4 s).
TEST(perf_fast_mode_runs_at_least_1000_ticks_per_second_at_max_configuration) {
    PerfRig r;
    CHECK(r.engine.start(maxConfig(1), "op").ok());
    const auto begin = Clock::now();
    for (int t = 0; t < 3600; ++t) CHECK(r.engine.tick().ok());
    const double ms = msSince(begin);
    const double ticksPerSecond = 3600.0 / (ms / 1000.0);
    std::cout << "    (3600 ticks: " << ms << " ms = " << static_cast<long>(ticksPerSecond) << " ticks/s)\n";
#if defined(__OPTIMIZE__) && !defined(__SANITIZE_ADDRESS__)   // timing limits: optimized builds only
    CHECK(ticksPerSecond >= 1000.0);
    CHECK(ms <= 4000.0);
#endif
}

// NFR-08 / TC-16: same seed + same configuration -> identical report (100% match).
TEST(repeat_same_seed_gives_byte_identical_report_at_max_configuration) {
    const std::string a = reportOfRun(42, 3600);
    const std::string b = reportOfRun(42, 3600);
    CHECK(!a.empty());
    CHECK_EQ(a, b);
}

// TC-16 step 3: a different seed must give a different result, or the check proves nothing.
TEST(repeat_different_seed_gives_a_different_report) {
    CHECK(reportOfRun(42, 3600) != reportOfRun(43, 3600));
}
