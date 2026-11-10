#include "app/application.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>

#include "app/report_writer.h"
#include "common/messages.h"
#include "ui/dashboard.h"
#include "ui/validator.h"

namespace fs = std::filesystem;

class Application::SimRng : public IRandom {
public:
    void seed(std::uint32_t s) override { rng_.reseed(s); }
    std::uint32_t next() override { return static_cast<std::uint32_t>(rng_.next() >> 32); }

private:
    tms::SeededRng rng_{1};
};

namespace {

constexpr long long kMaxId = 1000000;

AuthAdapter::WallClock makeWallClock(const AppOptions& o) {
    if (o.wallSeconds) return [f = o.wallSeconds]() { return static_cast<tms::SimClock::Tick>(f()); };
    const auto start = std::chrono::steady_clock::now();
    return [start]() {
        const auto d = std::chrono::steady_clock::now() - start;
        return static_cast<tms::SimClock::Tick>(std::chrono::duration_cast<std::chrono::seconds>(d).count());
    };
}

const char* roleName(Role r) {
    switch (r) {
        case Role::Admin:    return "Admin";
        case Role::Operator: return "Operator";
        case Role::Commuter: return "Commuter";
    }
    return "?";
}

Command commandFor(int choice) {
    static const Command table[] = {
        Command::ViewCongestion,       // 0 (unused)
        Command::StartSimulation,      // 1 start
        Command::StartSimulation,      // 2 run
        Command::PauseSimulation,      // 3 pause / resume
        Command::StopSimulation,       // 4 stop
        Command::ViewDashboard,        // 5 dashboard
        Command::SetGreenTime,         // 6 green time
        Command::InjectEmergency,      // 7 emergency
        Command::GenerateReport,       // 8 report
        Command::ManageIntersections,  // 9 intersections
        Command::ConfigureParameters,  // 10 configuration
        Command::ManageUsers           // 11 users
    };
    return table[choice];
}

std::vector<std::string> reportNames(const std::string& dir) {
    std::vector<std::string> names;
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec)) names.push_back(it->path().filename().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

}  // namespace

Application::Application(std::istream& in, std::ostream& out, AppOptions options)
    : opts_(std::move(options)),
      in_(in),
      out_(out),
      audit_(opts_.auditFile),
      authRng_(opts_.authSeed),
      users_(tms::FileStore(opts_.usersFile, true), authRng_, opts_.passwordIterations),
      auth_(users_, clock_, authRng_),
      adapter_(auth_, users_, clock_, audit_, makeWallClock(opts_)),
      dispatcher_(adapter_, audit_),
      simRng_(new SimRng()),
      engine_(signals_, *simRng_, audit_, monitor_),
      reports_([this](const std::string& name, const std::string& content) {
                   return writeReportFile(opts_.reportsDir, name, content);
               },
               audit_),
      adaptive_([this](int id, int lane) { return engine_.queueLength(id, lane); }) {
    if (!opts_.sleepMs) {
        opts_.sleepMs = [](int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); };
    }
}

// ------------------------------------------------------------------ helpers

bool Application::inputEnded() const { return in_.fail(); }

void Application::showError(ErrorCode e, const std::string& field, long long lo, long long hi) {
    out_ << "[" << errorCodeId(e) << "] " << errorMessage(e, field, lo, hi) << "\n";
}

int Application::chooseOption(int lo, int hi) {
    while (true) {
        const Result<long long> r = validator::readInteger(in_, out_, "Choice", lo, hi);
        if (r.ok()) return static_cast<int>(*r.value);
        if (inputEnded()) return -1;
    }
}

bool Application::simulationActive() {
    if (engine_.state() == SimulationEngine::State::Stopped) {
        showError(ErrorCode::NotRunning);
        return false;
    }
    return true;
}

void Application::saveConfig() {
    const Result<void> r = config_.saveFile(opts_.configFile);
    if (!r.ok()) showError(r.error);
}

// ------------------------------------------------------------------ start-up

int Application::run() {
    if (!startup()) return 1;
    mainMenu();
    out_ << "Goodbye.\n";
    return 0;
}

bool Application::startup() {
    out_ << "=== Traffic Management System (Simulation) - Team E16 ===\n";
    loadConfig();
    auth_.setAuditLogger(&audit_);
    adapter_.setSessionTimeoutMinutes(config_.sessionTimeoutMin());

    const auto loaded = users_.load();
    if (!loaded.ok()) {
        out_ << "[E-IO-001] The user file is damaged (" << loaded.error().message
             << "). Repair or remove it, then start again.\n";
        return false;
    }
    return users_.size() != 0 || createFirstAdmin();
}

void Application::loadConfig() {
    const Result<void> r = config_.loadFile(opts_.configFile);
    if (r.error == ErrorCode::ConfigCorrupt) {
        showError(ErrorCode::ConfigCorrupt);
        (void)audit_.log("system", "CONFIG_CORRUPT", errorCodeId(ErrorCode::ConfigCorrupt));
    }
    if (config_.intersections().size() == 0) {                  // first run: one example intersection
        IntersectionDef def;
        def.name = "Main St & 1st Ave";
        def.lanes = 4;
        def.arrivalPerMin = {20, 15, 25, 10};
        def.greenSeconds = 20;
        (void)config_.intersections().add(def);
    }
}

bool Application::createFirstAdmin() {
    out_ << "No accounts exist yet. Create the first administrator account.\n";
    for (int attempt = 0; attempt < 3; ++attempt) {
        const Result<std::string> name = validator::readText(in_, out_, "Username (4-20 characters)", 1, 256);
        if (!name.ok()) return false;
        out_ << "Password (min 8 characters, letter + digit): ";
        std::string pw;
        if (!validator::readLine(in_, pw)) return false;
        const auto r = users_.addUser(*name.value, pw, tms::Role::Admin);
        if (r.ok()) {
            (void)audit_.log("system", "INITIAL_ADMIN_CREATED", "OK username=" + *name.value);
            out_ << "Administrator account created.\n";
            return true;
        }
        out_ << "[E-VAL-001] " << r.error().message << "\n";
    }
    return false;
}

// ------------------------------------------------------------------ menus

void Application::mainMenu() {
    while (true) {
        out_ << "\nMAIN MENU\n  1) Log in\n  2) Commuter view (read-only)\n  0) Exit\n";
        const int c = chooseOption(0, 2);
        if (c <= 0) return;
        if (c == 1) loginFlow();
        else commuterMenu();
    }
}

void Application::loginFlow() {
    const Result<std::string> user = validator::readText(in_, out_, "Username", 1, 256);
    if (!user.ok()) return;
    out_ << "Password: ";
    std::string password;
    if (!validator::readLine(in_, password)) return;

    const Result<Session> s = adapter_.login(*user.value, password);
    if (!s.ok()) {
        showError(s.error, "Username and password");
        return;
    }
    Session session = *s.value;
    out_ << "Logged in as " << session.username << " (" << roleName(session.role) << ").\n";
    sessionLoop(session);
}

void Application::sessionLoop(Session& session) {
    while (true) {
        out_ << "\nOPERATIONS MENU  (" << session.username << ")\n"
             << "   1) Start simulation          7) Inject emergency vehicle\n"
             << "   2) Run simulation            8) Generate report (CSV/TXT)\n"
             << "   3) Pause / resume            9) Manage intersections\n"
             << "   4) Stop simulation          10) Configure system parameters (Admin)\n"
             << "   5) Show dashboard           11) Manage user accounts (Admin)\n"
             << "   6) Change green time         0) Log out\n";
        const int c = chooseOption(0, 11);
        if (c < 0) return;
        if (c == 0) {
            adapter_.logout(session);
            out_ << "Logged out.\n";
            return;
        }
        if (!handle(c, session)) return;
    }
}

bool Application::handle(int choice, Session& session) {
    const Result<void> allowed = dispatcher_.authorize(&session, commandFor(choice));
    if (!allowed.ok()) {
        showError(allowed.error);
        if (allowed.error != ErrorCode::SessionExpired) return true;
        out_ << "Returning to the login screen.\n";
        return false;
    }
    static const std::array<Handler, 12> handlers = {{
        nullptr,
        &Application::startSimulation, &Application::runSimulation, &Application::pauseOrResume,
        &Application::stopSimulation,  &Application::showDashboard, &Application::changeGreenTime,
        &Application::injectEmergency, &Application::generateReport, &Application::manageIntersections,
        &Application::configureSystem, &Application::manageUsers}};
    const Handler action = handlers.at(static_cast<std::size_t>(choice));   // choice is 1..11 here
    (this->*action)(session);
    return true;
}

void Application::commuterMenu() {
    while (true) {
        out_ << "\nCOMMUTER VIEW (read-only)\n  1) Congestion status\n  2) List reports\n  3) Show a report\n  0) Back\n";
        const int c = chooseOption(0, 3);
        if (c <= 0) return;
        const Command cmd = (c == 1) ? Command::ViewCongestion : Command::ViewReports;
        const Result<void> allowed = dispatcher_.authorize(nullptr, cmd);
        if (!allowed.ok()) { showError(allowed.error); continue; }
        if (c == 1) dashboard::renderCommuter(out_, engine_.snapshot());
        else if (c == 2) listReports();
        else viewReport();
    }
}

// ------------------------------------------------------------------ simulation control

void Application::startSimulation(const Session& s) {
    const SimConfig cfg = config_.buildSimConfig();
    if (cfg.intersections.empty()) {
        out_ << "Add at least one intersection first (menu 9).\n";
        return;
    }
    applyTimingPolicy();
    const Result<void> r = engine_.start(cfg, s.username);
    if (!r.ok()) { showError(r.error); return; }
    lastSeed_ = cfg.seed;
    out_ << "Simulation started: seed " << cfg.seed << ", " << cfg.intersections.size() << " intersection(s).\n";
}

void Application::runSimulation(const Session&) {
    if (engine_.state() == SimulationEngine::State::Paused) {
        out_ << "The simulation is paused. Resume it first (menu 3).\n";
        return;
    }
    if (!simulationActive()) return;
    const Result<long long> secs = validator::readInteger(in_, out_, "Simulated seconds to run", 1, 3600);
    if (!secs.ok()) return;

    const long long every = config_.speed();                    // frame interval: 1 frame per real second
    for (long long t = 1; t <= *secs.value; ++t) {
        if (!engine_.tick().ok()) break;
        const bool frame = (t % every == 0);
        if (!frame && t != *secs.value) continue;
        dashboard::render(out_, engine_.snapshot(), engine_.now());
        if (frame) opts_.sleepMs(1000);
    }
}

void Application::pauseOrResume(const Session& s) {
    if (!simulationActive()) return;
    if (engine_.state() == SimulationEngine::State::Running) {
        if (engine_.pause(s.username).ok()) out_ << "Simulation paused.\n";
    } else if (engine_.resume(s.username).ok()) {
        out_ << "Simulation resumed.\n";
    }
}

void Application::stopSimulation(const Session& s) {
    const Result<void> r = engine_.stop(s.username);
    if (!r.ok()) showError(r.error);
    else out_ << "Simulation stopped at tick " << engine_.now() << ".\n";
}

void Application::showDashboard(const Session&) {
    dashboard::render(out_, engine_.snapshot(), engine_.now());
}

void Application::changeGreenTime(const Session& s) {
    if (!simulationActive()) return;
    const Result<long long> id = validator::readInteger(in_, out_, "Intersection id", 1, kMaxId);
    if (!id.ok()) return;
    const Result<long long> sec = validator::readInteger(in_, out_, "Green time in seconds", 5, 120);
    if (!sec.ok()) return;
    const Result<void> r = signals_.setGreen(static_cast<int>(*id.value), static_cast<int>(*sec.value));
    if (!r.ok()) {
        out_ << "[E-VAL-001] Unknown intersection id " << *id.value << ".\n";
        return;
    }
    (void)audit_.log(s.username, "SET_GREEN",
                     "intersection=" + std::to_string(*id.value) + " green=" + std::to_string(*sec.value));
    out_ << "Green time updated; it applies from the next signal cycle.\n";
}

void Application::injectEmergency(const Session& s) {
    if (!simulationActive()) return;
    const Result<long long> id = validator::readInteger(in_, out_, "Intersection id", 1, kMaxId);
    if (!id.ok()) return;
    const Result<long long> lane = validator::readInteger(in_, out_, "Lane", 1, 8);
    if (!lane.ok()) return;
    const Result<void> r = engine_.injectEmergency(s.username, static_cast<int>(*id.value),
                                                   static_cast<int>(*lane.value));
    if (!r.ok()) {
        out_ << "[" << errorCodeId(r.error) << "] No such intersection or lane.\n";
        return;
    }
    out_ << "Emergency vehicle placed in lane " << *lane.value << "; the signal will give it priority.\n";
}

// ------------------------------------------------------------------ reports

void Application::generateReport(const Session& s) {
    const Result<long long> format = validator::readInteger(in_, out_, "Format (1 = CSV, 2 = TXT)", 1, 2);
    if (!format.ok()) return;
    const Result<std::string> name = validator::readText(in_, out_, "Report file name (max 64 chars)", 1, 64);
    if (!name.ok()) return;

    const ReportData data = ReportGenerator::collect(engine_, monitor_, lastSeed_);
    const Result<std::string> r = reports_.generate(
        s.username, *format.value == 1 ? ReportFormat::Csv : ReportFormat::Txt, *name.value, data);
    if (!r.ok()) {
        showError(r.error);
        if (r.error == ErrorCode::NotRunning) out_ << "Run the simulation first (menus 1 and 2).\n";
        return;
    }
    out_ << "Report saved: " << *r.value << "\n";
}

void Application::listReports() {
    const std::vector<std::string> names = reportNames(opts_.reportsDir);
    if (names.empty()) {
        out_ << "No reports yet.\n";
        return;
    }
    out_ << "Reports:\n";
    for (const auto& n : names) out_ << "  " << n << "\n";
}

void Application::viewReport() {
    const Result<std::string> name = validator::readText(in_, out_, "Report file name", 1, 64);
    if (!name.ok()) return;
    const Result<void> ok = ReportGenerator::validateFileName(*name.value);
    if (!ok.ok()) { showError(ok.error); return; }
    std::ifstream file((fs::path(opts_.reportsDir) / *name.value).string(), std::ios::binary);
    if (!file) { showError(ErrorCode::IoFailure); return; }
    std::ostringstream text;
    text << file.rdbuf();
    out_ << text.str();
}

// ------------------------------------------------------------------ intersections (FR-08)

void Application::manageIntersections(const Session&) {
    while (true) {
        out_ << "\nINTERSECTIONS (changes apply to the next simulation start)\n"
             << "  1) List  2) Add  3) Edit  4) Remove  0) Back\n";
        const int c = chooseOption(0, 4);
        if (c <= 0) return;
        if (c == 1) listIntersections();
        else if (c == 2) addIntersection();
        else if (c == 3) editIntersection();
        else removeIntersection();
    }
}

void Application::listIntersections() {
    const auto& list = config_.intersections().list();
    if (list.empty()) out_ << "No intersections defined.\n";
    for (const auto& d : list) {
        out_ << "  #" << d.id << "  " << d.name << "  lanes=" << d.lanes << "  green=" << d.greenSeconds
             << " s  arrivals/min=";
        for (std::size_t i = 0; i < d.arrivalPerMin.size(); ++i) out_ << (i ? "," : "") << d.arrivalPerMin[i];
        out_ << "\n";
    }
}

bool Application::promptIntersection(IntersectionDef& def) {
    const Result<std::string> name = validator::readText(in_, out_, "Name (max 40 chars)", 1, 40);
    if (!name.ok()) return false;
    const Result<long long> lanes = validator::readInteger(in_, out_, "Number of lanes", 1, 8);
    if (!lanes.ok()) return false;
    def.name = *name.value;
    def.lanes = static_cast<int>(*lanes.value);
    def.arrivalPerMin.clear();
    for (int lane = 1; lane <= def.lanes; ++lane) {
        const Result<long long> rate = validator::readInteger(
            in_, out_, "Arrivals per minute, lane " + std::to_string(lane), 1, 60);
        if (!rate.ok()) return false;
        def.arrivalPerMin.push_back(static_cast<int>(*rate.value));
    }
    const Result<long long> green = validator::readInteger(in_, out_, "Green time in seconds", 5, 120);
    if (!green.ok()) return false;
    def.greenSeconds = static_cast<int>(*green.value);
    return true;
}

void Application::addIntersection() {
    IntersectionDef def;
    if (!promptIntersection(def)) return;
    const Result<int> r = config_.intersections().add(def);
    if (!r.ok()) { showError(r.error); return; }
    saveConfig();
    out_ << "Intersection #" << *r.value << " added.\n";
}

void Application::editIntersection() {
    const Result<long long> id = validator::readInteger(in_, out_, "Intersection id", 1, kMaxId);
    if (!id.ok()) return;
    IntersectionDef def;
    if (config_.intersections().find(static_cast<int>(*id.value)) == nullptr) {
        showError(ErrorCode::OutOfRange, "Intersection id", 1, kMaxId);
        return;
    }
    if (!promptIntersection(def)) return;
    const Result<void> r = config_.intersections().edit(static_cast<int>(*id.value), def);
    if (!r.ok()) { showError(r.error); return; }
    saveConfig();
    out_ << "Intersection #" << *id.value << " updated.\n";
}

void Application::removeIntersection() {
    const Result<long long> id = validator::readInteger(in_, out_, "Intersection id", 1, kMaxId);
    if (!id.ok()) return;
    const Result<void> r = config_.intersections().remove(static_cast<int>(*id.value));
    if (!r.ok()) { showError(r.error, "Intersection id", 1, kMaxId); return; }
    saveConfig();
    out_ << "Intersection #" << *id.value << " removed.\n";
}

// ------------------------------------------------------------------ administrator functions

Application::Change Application::changeSeed() {
    Change c;
    const auto v = validator::readInteger(in_, out_, "Random seed", 0, ConfigManager::kMaxSeed);
    if (!v.ok()) return c;
    c.entered = true;
    c.result = config_.setSeed(*v.value);
    c.what = "seed=" + std::to_string(*v.value);
    return c;
}

Application::Change Application::changeThresholds() {
    Change c;
    const auto m = validator::readInteger(in_, out_, "Moderate threshold (vehicles)", 1, 999);
    if (!m.ok()) return c;
    const auto h = validator::readInteger(in_, out_, "High threshold (vehicles)", 2, 1000);
    if (!h.ok()) return c;
    c.entered = true;
    c.result = config_.setThresholds(*m.value, *h.value);
    c.what = "thresholds=" + std::to_string(*m.value) + "/" + std::to_string(*h.value);
    return c;
}

Application::Change Application::changeTimeout() {
    Change c;
    const auto v = validator::readInteger(in_, out_, "Session timeout in minutes", 1, 60);
    if (!v.ok()) return c;
    c.entered = true;
    c.result = config_.setSessionTimeoutMin(*v.value);
    if (c.result.ok()) adapter_.setSessionTimeoutMinutes(config_.sessionTimeoutMin());
    c.what = "timeout_min=" + std::to_string(*v.value);
    return c;
}

Application::Change Application::changeSpeed() {
    Change c;
    const auto v = validator::readInteger(in_, out_, "Simulation speed (1, 10 or 60)", 1, 60);
    if (!v.ok()) return c;
    c.entered = true;
    c.result = config_.setSpeed(*v.value);
    c.what = "speed=" + std::to_string(*v.value);
    return c;
}

Application::Change Application::changeAdaptive() {
    Change c;
    const auto v = validator::readInteger(in_, out_, "Adaptive signal timing (1 = on, 0 = off)", 0, 1);
    if (!v.ok()) return c;
    c.entered = true;
    config_.setAdaptive(*v.value == 1);
    applyTimingPolicy();                                        // also affects a running simulation
    c.what = "adaptive=" + std::to_string(*v.value);
    return c;
}

void Application::applyTimingPolicy() {
    adaptive_.setThresholds(config_.moderateAt(), config_.highAt());
    signals_.setTimingPolicy(config_.adaptive() ? &adaptive_ : nullptr);
}

Application::Change Application::changeSetting(int menuChoice) {
    if (menuChoice == 1) return changeSeed();
    if (menuChoice == 2) return changeThresholds();
    if (menuChoice == 3) return changeTimeout();
    if (menuChoice == 4) return changeSpeed();
    return changeAdaptive();
}

void Application::configureSystem(const Session& s) {
    while (true) {
        out_ << "\nSYSTEM PARAMETERS  seed=" << config_.seed() << "  thresholds=" << config_.moderateAt() << "/"
             << config_.highAt() << "  timeout=" << config_.sessionTimeoutMin() << " min  speed=" << config_.speed()
             << "x  adaptive=" << (config_.adaptive() ? "on" : "off")
             << "\n  1) Random seed  2) Congestion thresholds  3) Session timeout  4) Simulation speed"
             << "  5) Adaptive signal timing  0) Back\n";
        const int c = chooseOption(0, 5);
        if (c <= 0) return;
        const Change change = changeSetting(c);
        if (!change.entered) continue;
        if (!change.result.ok()) { showError(change.result.error, "Value", 1, 1000); continue; }
        (void)audit_.log(s.username, "PARAM_CHANGE", change.what);
        saveConfig();
        out_ << "Saved (" << change.what << ").\n";
    }
}

void Application::manageUsers(const Session& s) {
    while (true) {
        out_ << "\nUSER ACCOUNTS\n  1) Create account  2) List accounts  0) Back\n";
        const int c = chooseOption(0, 2);
        if (c <= 0) return;
        if (c == 1) {
            createUserDialog(s);
        } else {
            for (const auto& u : adapter_.listUsers()) out_ << "  " << u << "\n";
        }
    }
}

void Application::createUserDialog(const Session& s) {
    const Result<std::string> name = validator::readText(in_, out_, "New username (4-20 characters)", 1, 256);
    if (!name.ok()) return;
    out_ << "Password (min 8 characters, letter + digit): ";
    std::string pw;
    if (!validator::readLine(in_, pw)) return;
    const Result<long long> role = validator::readInteger(in_, out_, "Role (1 = Operator, 2 = Admin)", 1, 2);
    if (!role.ok()) return;

    NewUser nu;
    nu.username = *name.value;
    nu.password = pw;
    nu.role = (*role.value == 1) ? Role::Operator : Role::Admin;
    const Result<void> r = adapter_.createUser(s, nu);
    if (!r.ok()) {
        out_ << "[" << errorCodeId(r.error) << "] Account not created: username must be 4-20 characters "
             << "(letters, digits, _ . -) and unused; password at least 8 characters with a letter and a digit.\n";
        return;
    }
    out_ << "Account " << nu.username << " created.\n";
}
