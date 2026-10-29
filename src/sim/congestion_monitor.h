#pragma once
// Congestion Monitor (story #10, FR-03). Turns queue lengths into congestion
// levels (Low < 5, Moderate 5-14, High >= 15) and accumulates the figures the
// report needs (FR-06): departures, total wait, max queue, High-congestion time.
#include "common/sim_types.h"

class CongestionMonitor {
public:
    explicit CongestionMonitor(std::size_t moderateAt = 5, std::size_t highAt = 15);

    void setThresholds(std::size_t moderateAt, std::size_t highAt);
    void reset();                                            // new run: clear statistics

    // Pure: fills in the congestion level of every lane.
    CongestionSnapshot evaluate(CongestionSnapshot raw) const;

    // Called once per tick by the engine: stores the evaluated snapshot and
    // updates statistics. A tick counts as "High" if any lane is High.
    void update(const CongestionSnapshot& raw);

    // Called by the engine each time a vehicle leaves a queue.
    void recordDeparture(Tick waitTicks);

    const CongestionSnapshot& snapshot() const { return last_; }
    Statistics statistics() const { return stats_; }
    double averageWait() const;                              // ticks (seconds) per departed vehicle
    double highCongestionPercent() const;                    // % of ticks with a High lane

private:
    std::size_t moderateAt_;
    std::size_t highAt_;
    CongestionSnapshot last_;
    Statistics stats_;
};
