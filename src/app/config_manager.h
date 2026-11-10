#pragma once
// Config Manager (story #11, FR-05, FR-08). System-wide parameters and the
// intersection definitions, loaded/saved as config.ini (key=value lines).
//   seed 0..2^31-1 | thresholds 1 <= moderate < high <= 1000 |
//   session timeout 1..60 min | speed 1, 10 or 60 | adaptive timing on/off (FR-09)
// A corrupt file never crashes the program: parse()/loadFile() fall back to the
// defaults and return E-CFG-001; the file on disk is left untouched.
// Who may change settings (Admin only) is enforced by the Dispatcher, not here.
#include <cstdint>
#include <iosfwd>
#include <string>
#include "common/result.h"
#include "common/sim_types.h"
#include "sim/intersection_manager.h"

class ConfigManager {
public:
    static constexpr long long kMaxSeed = 2147483647LL;     // 2^31 - 1

    // Each setter validates; on error the old value is kept (OutOfRange).
    Result<void> setSeed(long long seed);
    Result<void> setThresholds(long long moderateAt, long long highAt);
    Result<void> setSessionTimeoutMin(long long minutes);
    Result<void> setSpeed(long long speed);                 // 1, 10 or 60
    void setAdaptive(bool on) { adaptive_ = on; }           // off by default

    std::uint32_t seed() const { return seed_; }
    std::size_t moderateAt() const { return moderateAt_; }
    std::size_t highAt() const { return highAt_; }
    int sessionTimeoutMin() const { return timeoutMin_; }
    int speed() const { return speed_; }
    bool adaptive() const { return adaptive_; }

    IntersectionManager& intersections() { return intersections_; }
    const IntersectionManager& intersections() const { return intersections_; }

    SimConfig buildSimConfig() const;                       // what the engine's start() needs
    void resetToDefaults();

    // config.ini text format (see serialize()).
    Result<void> parse(std::istream& in);                   // ConfigCorrupt -> defaults restored
    void serialize(std::ostream& out) const;
    Result<void> loadFile(const std::string& path);         // IoFailure if it cannot be opened
    Result<void> saveFile(const std::string& path) const;   // IoFailure on write error

private:
    bool applyLine(const std::string& rawLine);               // one line of config.ini
    bool applySetting(const std::string& key, const std::string& value);
    bool applyNumber(const std::string& key, const std::string& value);
    bool applyThreshold(const std::string& key, const std::string& value);
    bool applyIntersection(const std::string& value);
    bool applyFlag(const std::string& value);                // adaptive=0|1

    std::uint32_t seed_{1};
    std::size_t moderateAt_{5};
    std::size_t highAt_{15};
    int timeoutMin_{10};
    int speed_{1};
    bool adaptive_{false};
    IntersectionManager intersections_;
};
