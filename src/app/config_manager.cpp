#include "app/config_manager.h"

#include <fstream>
#include <sstream>
#include <vector>

#include "ui/validator.h"

namespace {

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const std::size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(ws) - b + 1);
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : s) {
        if (c == sep) { parts.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    parts.push_back(cur);
    return parts;
}

bool number(const std::string& text, long long lo, long long hi, long long& out) {
    const Result<long long> r = validator::parseInteger(text, lo, hi);
    if (!r.ok()) return false;
    out = *r.value;
    return true;
}

// "id|name|lanes|green|rate1,rate2,..."
bool parseIntersection(const std::string& value, IntersectionDef& def) {
    const std::vector<std::string> f = split(value, '|');
    if (f.size() != 5) return false;
    long long id = 0, lanes = 0, green = 0;
    if (!number(f[0], 1, 1000000, id) || !number(f[2], 1, 8, lanes) || !number(f[3], 5, 120, green)) {
        return false;
    }
    const std::vector<std::string> rates = split(f[4], ',');
    if (rates.size() != static_cast<std::size_t>(lanes)) return false;
    def = IntersectionDef{};
    def.id = static_cast<int>(id);
    def.name = f[1];
    def.lanes = static_cast<int>(lanes);
    def.greenSeconds = static_cast<int>(green);
    for (const auto& r : rates) {
        long long v = 0;
        if (!number(r, 1, 60, v)) return false;
        def.arrivalPerMin.push_back(static_cast<int>(v));
    }
    return true;
}

}  // namespace

Result<void> ConfigManager::setSeed(long long seed) {
    if (seed < 0 || seed > kMaxSeed) return Result<void>::failure(ErrorCode::OutOfRange);
    seed_ = static_cast<std::uint32_t>(seed);
    return Result<void>::success();
}

Result<void> ConfigManager::setThresholds(long long moderateAt, long long highAt) {
    if (moderateAt < 1 || highAt > 1000 || highAt <= moderateAt) {
        return Result<void>::failure(ErrorCode::OutOfRange);
    }
    moderateAt_ = static_cast<std::size_t>(moderateAt);
    highAt_ = static_cast<std::size_t>(highAt);
    return Result<void>::success();
}

Result<void> ConfigManager::setSessionTimeoutMin(long long minutes) {
    if (minutes < 1 || minutes > 60) return Result<void>::failure(ErrorCode::OutOfRange);
    timeoutMin_ = static_cast<int>(minutes);
    return Result<void>::success();
}

Result<void> ConfigManager::setSpeed(long long speed) {
    if (speed != 1 && speed != 10 && speed != 60) return Result<void>::failure(ErrorCode::OutOfRange);
    speed_ = static_cast<int>(speed);
    return Result<void>::success();
}

SimConfig ConfigManager::buildSimConfig() const {
    SimConfig c;
    c.seed = seed_;
    c.moderateAt = moderateAt_;
    c.highAt = highAt_;
    c.intersections = intersections_.list();
    return c;
}

void ConfigManager::resetToDefaults() { *this = ConfigManager(); }

bool ConfigManager::applyIntersection(const std::string& value) {
    IntersectionDef def;
    return parseIntersection(value, def) && intersections_.add(def).ok();
}

bool ConfigManager::applyNumber(const std::string& key, const std::string& value) {
    long long n = 0;
    if (key == "seed") return number(value, 0, kMaxSeed, n) && setSeed(n).ok();
    if (key == "timeout_min") return number(value, 1, 60, n) && setSessionTimeoutMin(n).ok();
    if (key == "speed") return number(value, 1, 60, n) && setSpeed(n).ok();
    return false;                                              // unknown key
}

bool ConfigManager::applyThreshold(const std::string& key, const std::string& value) {
    long long n = 0;
    if (!number(value, 1, 1000, n)) return false;
    (key == "moderate" ? moderateAt_ : highAt_) = static_cast<std::size_t>(n);
    return true;                                               // moderate < high is checked at the end
}

bool ConfigManager::applySetting(const std::string& key, const std::string& value) {
    if (key == "intersection") return applyIntersection(value);
    if (key == "moderate" || key == "high") return applyThreshold(key, value);
    return applyNumber(key, value);
}

bool ConfigManager::applyLine(const std::string& rawLine) {
    if (rawLine.size() > 1000) return false;
    const std::string line = trim(rawLine);
    if (line.empty() || line[0] == '#') return true;
    const std::size_t eq = line.find('=');
    if (eq == std::string::npos) return false;
    return applySetting(trim(line.substr(0, eq)), trim(line.substr(eq + 1)));
}

Result<void> ConfigManager::parse(std::istream& in) {
    ConfigManager next;                       // built aside; applied only if the whole file is valid
    bool ok = true;
    std::string raw;
    while (ok && std::getline(in, raw)) ok = next.applyLine(raw);
    ok = ok && next.highAt_ > next.moderateAt_;

    if (!ok) {
        resetToDefaults();
        return Result<void>::failure(ErrorCode::ConfigCorrupt);
    }
    *this = next;
    return Result<void>::success();
}

void ConfigManager::serialize(std::ostream& out) const {
    out << "# Traffic Management System configuration\n";
    out << "seed=" << seed_ << "\n";
    out << "moderate=" << moderateAt_ << "\n";
    out << "high=" << highAt_ << "\n";
    out << "timeout_min=" << timeoutMin_ << "\n";
    out << "speed=" << speed_ << "\n";
    for (const auto& d : intersections_.list()) {
        out << "intersection=" << d.id << "|" << d.name << "|" << d.lanes << "|" << d.greenSeconds << "|";
        for (std::size_t i = 0; i < d.arrivalPerMin.size(); ++i) {
            if (i > 0) out << ",";
            out << d.arrivalPerMin[i];
        }
        out << "\n";
    }
}

Result<void> ConfigManager::loadFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) return Result<void>::failure(ErrorCode::IoFailure);
    return parse(in);
}

Result<void> ConfigManager::saveFile(const std::string& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out) return Result<void>::failure(ErrorCode::IoFailure);
    serialize(out);
    out.flush();
    if (!out) return Result<void>::failure(ErrorCode::IoFailure);
    return Result<void>::success();
}
