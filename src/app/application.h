#pragma once
// Console application (integration issue): ties login, dispatcher, simulation engine, dashboard,
// reports, intersection management and configuration together (UC-01 .. UC-12).
// All input/output goes through the given streams, so the whole program can be scripted in tests.
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <memory>
#include <string>

#include "app/auth_adapter.h"
#include "app/config_manager.h"
#include "app/dispatcher.h"
#include "app/report_generator.h"
#include "infra/audit_logger.h"
#include "sim/congestion_monitor.h"
#include "sim/signal_controller.h"
#include "sim/simulation_engine.h"

struct AppOptions {
    std::string usersFile{"users.dat"};
    std::string configFile{"config.ini"};
    std::string auditFile{"audit.log"};
    std::string reportsDir{"reports"};
    unsigned passwordIterations{tms::kDefaultIterations};
    std::uint64_t authSeed{1};                          // salts/session ids; main() uses a random value
    std::function<std::uint64_t()> wallSeconds;         // machine clock (default: real time since start)
    std::function<void(int)> sleepMs;                   // pause between dashboard frames (default: real sleep)
};

class Application {
public:
    Application(std::istream& in, std::ostream& out, AppOptions options);
    int run();                                          // 0 = normal exit, 1 = start-up error

private:
    using Handler = void (Application::*)(const Session&);

    // start-up
    bool startup();
    void loadConfig();
    bool createFirstAdmin();
    // menus
    void mainMenu();
    void loginFlow();
    void sessionLoop(Session& session);
    bool handle(int choice, Session& session);
    void commuterMenu();
    // operator / admin actions
    void startSimulation(const Session& s);
    void runSimulation(const Session& s);
    void pauseOrResume(const Session& s);
    void stopSimulation(const Session& s);
    void showDashboard(const Session& s);
    void changeGreenTime(const Session& s);
    void injectEmergency(const Session& s);
    void generateReport(const Session& s);
    void manageIntersections(const Session& s);
    void configureSystem(const Session& s);
    void manageUsers(const Session& s);
    // sub-steps
    void listIntersections();
    void addIntersection();
    void editIntersection();
    void removeIntersection();
    bool promptIntersection(IntersectionDef& def);
    void listReports();
    void viewReport();
    void createUserDialog(const Session& s);
    void saveConfig();
    struct Change {                                      // one entry of the configuration menu
        bool entered{false};                             // false: input was not completed
        Result<void> result{Result<void>::success()};
        std::string what;                                // text for the audit log
    };
    Change changeSetting(int menuChoice);
    Change changeSeed();
    Change changeThresholds();
    Change changeTimeout();
    Change changeSpeed();
    // helpers
    int chooseOption(int lo, int hi);                   // -1 at end of input
    bool inputEnded() const;
    void showError(ErrorCode e, const std::string& field = "value", long long lo = 0, long long hi = 0);
    bool simulationActive();

    AppOptions opts_;
    std::istream& in_;
    std::ostream& out_;
    FileAuditLogger audit_;
    tms::SimClock clock_;
    tms::SeededRng authRng_;
    tms::UserStore users_;
    tms::AuthService auth_;
    AuthAdapter adapter_;
    Dispatcher dispatcher_;
    ConfigManager config_;
    SignalController signals_;
    class SimRng;                                       // adapts tms::SeededRng to IRandom
    std::unique_ptr<IRandom> simRng_;
    CongestionMonitor monitor_;
    SimulationEngine engine_;
    ReportGenerator reports_;
    std::uint32_t lastSeed_{0};
};
