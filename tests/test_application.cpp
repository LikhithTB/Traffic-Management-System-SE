// Whole-program tests: scripted console sessions against the real application
// (real auth, dispatcher, engine, signals, reports, files in a temporary folder).
#include <filesystem>
#include <fstream>
#include <sstream>

#include "app/application.h"
#include "scripted_input.h"
#include "test_framework.h"

namespace fs = std::filesystem;

namespace {

struct Sandbox {
    fs::path dir;
    std::uint64_t wall{1000};
    explicit Sandbox(const std::string& name) {
        dir = fs::temp_directory_path() / ("tms_app_" + name);
        fs::remove_all(dir);
        fs::create_directories(dir);
    }
    ~Sandbox() { std::error_code ec; fs::remove_all(dir, ec); }

    AppOptions options() {
        AppOptions o;
        o.usersFile = (dir / "users.dat").string();
        o.configFile = (dir / "config.ini").string();
        o.auditFile = (dir / "audit.log").string();
        o.reportsDir = (dir / "reports").string();
        o.passwordIterations = 5;                        // fast tests
        o.authSeed = 77;
        o.wallSeconds = [this]() { return wall; };
        o.sleepMs = [](int) {};
        return o;
    }
    std::string read(const std::string& name) const {
        std::ifstream f(dir / name, std::ios::binary);
        std::ostringstream s;
        s << f.rdbuf();
        return s.str();
    }
    bool exists(const std::string& name) const { return fs::exists(dir / name); }
};

struct Result2 { int code; std::string out; };

Result2 runApp(Sandbox& sb, ScriptedInput& in) {
    std::ostringstream out;
    Application app(in, out, sb.options());
    const int code = app.run();
    return {code, out.str()};
}

int count(const std::string& text, const std::string& part) {
    int n = 0;
    for (std::size_t p = text.find(part); p != std::string::npos; p = text.find(part, p + part.size())) ++n;
    return n;
}
bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

const std::vector<std::string> kFirstRun = {"admin1", "Admin1234"};
const std::vector<std::string> kLoginAdmin = {"1", "admin1", "Admin1234"};

}  // namespace

TEST(app_first_run_creates_admin_login_logout_exit) {
    Sandbox sb("first_run");
    ScriptedInput in;
    in.lines(kFirstRun).lines(kLoginAdmin).lines({"0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK_EQ(r.code, 0);
    CHECK(has(r.out, "Administrator account created."));
    CHECK(has(r.out, "Logged in as admin1 (Admin)."));
    CHECK(has(r.out, "Logged out."));
    CHECK(has(r.out, "Goodbye."));
    CHECK(!has(sb.read("users.dat"), "Admin1234"));                 // only salt + hash stored
    const std::string audit = sb.read("audit.log");
    CHECK(has(audit, "INITIAL_ADMIN_CREATED") && has(audit, "LOGIN_OK") && has(audit, "LOGOUT"));
}

TEST(app_first_run_weak_password_is_refused_three_times_then_exit_code_1) {
    Sandbox sb("weak_first");
    ScriptedInput in;
    in.lines({"admin1", "abcdefgh", "admin1", "short1", "admin1", "12345678"});
    const Result2 r = runApp(sb, in);
    CHECK_EQ(r.code, 1);
    CHECK_EQ(count(r.out, "[E-VAL-001]"), 3);
    CHECK(!sb.exists("users.dat"));
}

TEST(app_existing_accounts_skip_first_run_setup) {
    Sandbox sb("existing");
    { ScriptedInput in; in.lines(kFirstRun).line("0"); runApp(sb, in); }
    ScriptedInput in2;
    in2.lines(kLoginAdmin).lines({"0", "0"});
    const Result2 r = runApp(sb, in2);
    CHECK(!has(r.out, "No accounts exist yet"));
    CHECK(has(r.out, "Logged in as admin1"));
}

TEST(app_tc02_lockout_after_three_failures_and_unlock_after_60_seconds) {
    Sandbox sb("tc02");
    ScriptedInput in;
    in.lines(kFirstRun)
      .lines({"1", "admin1", "wrong-pass-1", "1", "admin1", "wrong-pass-2", "1", "admin1", "wrong-pass-3"})
      .lines({"1", "admin1", "Admin1234"})                          // 4th attempt, correct password
      .act([&]() { sb.wall += 59; }).lines({"1", "admin1", "Admin1234"})
      .act([&]() { sb.wall += 1; }).lines({"1", "admin1", "Admin1234", "0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK_EQ(count(r.out, "[E-AUTH-001]"), 3);                      // attempts 1-3
    CHECK_EQ(count(r.out, "[E-AUTH-002]"), 2);                      // attempt 4, and still locked at 59 s
    CHECK(has(r.out, "Logged in as admin1"));                       // after 60 s
    const std::string audit = sb.read("audit.log");
    CHECK_EQ(count(audit, "LOGIN_FAIL"), 3);
    CHECK_EQ(count(audit, "ACCOUNT_LOCKED"), 1);
}

TEST(app_tc03_session_timeout_returns_to_login) {
    Sandbox sb("tc03");
    ScriptedInput in;
    in.lines(kFirstRun).lines(kLoginAdmin)
      .lines({"10", "3", "1", "0"})                                  // timeout := 1 minute
      .lines({"11", "1", "oper1", "Pass1234", "1", "0"})             // create an operator
      .line("0")                                                     // log out
      .lines({"1", "oper1", "Pass1234"})
      .act([&]() { sb.wall += 61; })                                 // idle for 61 s
      .lines({"5"})                                                  // any command
      .line("0");
    const Result2 r = runApp(sb, in);
    CHECK(has(r.out, "Logged in as oper1 (Operator)."));
    CHECK(has(r.out, "[E-AUTH-003]"));
    CHECK(has(r.out, "Returning to the login screen."));
    CHECK(has(sb.read("config.ini"), "timeout_min=1"));
    CHECK(has(sb.read("audit.log"), "SESSION_EXPIRED"));
}

TEST(app_tc04_operator_cannot_manage_users_or_configure) {
    Sandbox sb("tc04");
    ScriptedInput in;
    in.lines(kFirstRun).lines(kLoginAdmin).lines({"11", "1", "oper1", "Pass1234", "1", "0", "0"})
      .lines({"1", "oper1", "Pass1234", "10", "11", "0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK_EQ(count(r.out, "[E-AUTH-004]"), 2);
    const std::string audit = sb.read("audit.log");
    CHECK(has(audit, "DENIED:ConfigureParameters") && has(audit, "DENIED:ManageUsers"));
    CHECK(has(audit, "USER_CREATE | OK username=oper1"));
}

TEST(app_create_user_validation_messages) {
    Sandbox sb("create_user");
    ScriptedInput in;
    in.lines(kFirstRun).lines(kLoginAdmin)
      .lines({"11", "1", "abc", "Pass1234", "1"})                    // 3-char username
      .lines({"1", "oper1", "abcdefgh", "1"})                        // no digit
      .lines({"1", "oper1", "Pass1234", "1"})                        // ok
      .lines({"1", "oper1", "Pass1234", "1"})                        // duplicate
      .lines({"2", "0", "0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK_EQ(count(r.out, "Account not created"), 3);
    CHECK(has(r.out, "Account oper1 created."));
    CHECK(has(r.out, "  oper1\n"));                                  // list shows it
    CHECK(has(r.out, "  admin1\n"));
}

TEST(app_simulation_dashboard_and_report_export) {
    Sandbox sb("sim_report");
    ScriptedInput in;
    in.lines(kFirstRun).lines(kLoginAdmin)
      .lines({"1", "2", "30", "5"})                                  // start, run 30 s, dashboard
      .lines({"8", "1", "run1"})                                     // CSV report
      .lines({"8", "1", "run1"})                                     // same name again -> refused
      .lines({"8", "2", "../evil"})                                  // unsafe name
      .lines({"8", "2", "summary"})                                  // TXT report
      .lines({"0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK(has(r.out, "Simulation started: seed 1, 1 intersection(s)."));
    CHECK(has(r.out, "LIVE DASHBOARD  (tick 30)"));
    CHECK(has(r.out, "Report saved: ") && has(r.out, "run1.csv"));
    CHECK(has(r.out, "[E-IO-001]"));                                 // create-new only
    CHECK(has(r.out, "[E-IO-002]"));                                 // path traversal
    CHECK(sb.exists("reports/run1.csv") && sb.exists("reports/summary.txt"));
    CHECK(!sb.exists("evil.csv"));
    CHECK(has(sb.read("reports/run1.csv"), "simulated_seconds,30"));
    CHECK(has(sb.read("reports/summary.txt"), "SIMULATION REPORT"));
    CHECK_EQ(count(sb.read("audit.log"), "REPORT_EXPORT"), 4);
}

TEST(app_report_before_any_simulation_is_refused) {
    Sandbox sb("report_early");
    ScriptedInput in;
    in.lines(kFirstRun).lines(kLoginAdmin).lines({"8", "1", "early", "1", "8", "1", "early", "0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK_EQ(count(r.out, "[E-SIM-001]"), 2);                        // no simulation, then started but 0 ticks run
    CHECK(has(r.out, "Run the simulation first"));
    CHECK(!sb.exists("reports/early.csv"));
}

TEST(app_commands_need_a_running_simulation) {
    Sandbox sb("not_running");
    ScriptedInput in;
    in.lines(kFirstRun).lines(kLoginAdmin).lines({"2", "3", "4", "6", "7", "0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK(count(r.out, "[E-SIM-001]") >= 5);
}

TEST(app_pause_resume_and_stop) {
    Sandbox sb("pause");
    ScriptedInput in;
    in.lines(kFirstRun).lines(kLoginAdmin)
      .lines({"1", "3", "2", "3", "2", "5", "3", "4", "0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK(has(r.out, "Simulation paused."));
    CHECK(has(r.out, "The simulation is paused. Resume it first"));
    CHECK(has(r.out, "Simulation resumed."));
    CHECK(has(r.out, "Simulation stopped at tick 5."));
}

TEST(app_emergency_vehicle_is_served_and_audited) {
    Sandbox sb("emergency");
    ScriptedInput in;
    in.lines(kFirstRun).lines(kLoginAdmin)
      .lines({"1", "2", "3", "7", "1", "2", "2", "10", "0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK(has(r.out, "Emergency vehicle placed in lane 2"));
    CHECK(has(r.out, "[EMERGENCY]"));
    const std::string audit = sb.read("audit.log");
    CHECK(has(audit, "EMERGENCY_INJECTED") && has(audit, "EMERGENCY_CLEARED"));
}

TEST(app_emergency_unknown_target_message) {
    Sandbox sb("emergency_bad");
    ScriptedInput in;
    in.lines(kFirstRun).lines(kLoginAdmin).lines({"1", "7", "99", "1", "0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK(has(r.out, "No such intersection or lane."));
}

TEST(app_change_green_time_validates_and_audits) {
    Sandbox sb("green");
    ScriptedInput in;
    in.lines(kFirstRun).lines(kLoginAdmin)
      .lines({"1", "6", "1", "4", "121", "40"})                      // out of range twice? (4 then 121 then 40)
      .lines({"6", "99", "30"})                                      // unknown id
      .lines({"0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK(has(r.out, "Green time in seconds must be between 5 and 120."));
    CHECK(has(r.out, "Green time updated; it applies from the next signal cycle."));
    CHECK(has(r.out, "Unknown intersection id 99."));
    CHECK(has(sb.read("audit.log"), "SET_GREEN | intersection=1 green=40"));
}

TEST(app_invalid_menu_input_never_crashes_and_names_field_and_range) {
    Sandbox sb("invalid");
    ScriptedInput in;
    in.lines(kFirstRun).lines(kLoginAdmin)
      .lines({"abc", "99", "", "  ", std::string(300, '9'), "0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK_EQ(r.code, 0);
    CHECK(has(r.out, "Choice must be a number."));
    CHECK(has(r.out, "Choice must be between 0 and 11."));
    CHECK(has(r.out, "Choice must not be empty and must be at most 256 characters."));
}

TEST(app_end_of_input_exits_cleanly_at_every_level) {
    Sandbox sb("eof");
    { ScriptedInput in; in.lines(kFirstRun);                                          CHECK_EQ(runApp(sb, in).code, 0); }
    { ScriptedInput in; in.line("1");                                                 CHECK_EQ(runApp(sb, in).code, 0); }
    { ScriptedInput in; in.lines({"1", "admin1"});                                    CHECK_EQ(runApp(sb, in).code, 0); }
    { ScriptedInput in; in.lines(kLoginAdmin);                                        CHECK_EQ(runApp(sb, in).code, 0); }
    { ScriptedInput in; in.lines(kLoginAdmin).lines({"9", "2", "Name"});              CHECK_EQ(runApp(sb, in).code, 0); }
    { ScriptedInput in; in.lines(kLoginAdmin).lines({"10"});                          CHECK_EQ(runApp(sb, in).code, 0); }
    { ScriptedInput in; in.lines(kLoginAdmin).lines({"11", "1", "newuser1"});         CHECK_EQ(runApp(sb, in).code, 0); }
    { ScriptedInput in; in.lines({"2"});                                              CHECK_EQ(runApp(sb, in).code, 0); }
}

TEST(app_intersection_management_add_edit_remove_and_persist) {
    Sandbox sb("intersections");
    ScriptedInput in;
    in.lines(kFirstRun).lines(kLoginAdmin).line("9")
      .lines({"2", "Park Road", "9", "2", "Park Road", "2", "15", "35", "40"})    // 9 lanes refused, then valid
      .lines({"1"})                                                                // list
      .lines({"3", "2", "Park Rd 2", "1", "50", "60"})                            // edit
      .lines({"3", "77"})                                                          // unknown id
      .lines({"4", "1", "4", "77"})                                                // remove #1, unknown
      .lines({"1", "0", "0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK(has(r.out, "Number of lanes must be between 1 and 8."));
    CHECK(has(r.out, "Intersection #2 added."));
    CHECK(has(r.out, "Intersection #2 updated."));
    CHECK(has(r.out, "Intersection #1 removed."));
    CHECK(has(r.out, "Park Rd 2"));
    const std::string ini = sb.read("config.ini");
    CHECK(has(ini, "intersection=2|Park Rd 2|1|60|50"));
    CHECK(!has(ini, "intersection=1|"));
}

TEST(app_cannot_start_without_intersections_and_max_is_ten) {
    Sandbox sb("max_ten");
    ScriptedInput in;
    in.lines(kFirstRun).lines(kLoginAdmin).line("9").lines({"4", "1"});              // remove the default one
    for (int i = 0; i < 11; ++i) in.lines({"2", "Road" + std::to_string(i), "1", "10", "20"});
    in.lines({"0", "1", "0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK(has(r.out, "[E-SIM-002]"));                                                 // the 11th is refused
    CHECK(has(r.out, "Simulation started: seed 1, 10 intersection(s)."));
}

TEST(app_start_without_any_intersection_gives_hint) {
    Sandbox sb("no_inter");
    ScriptedInput in;
    in.lines(kFirstRun).lines(kLoginAdmin).line("9").lines({"4", "1", "0", "1", "0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK(has(r.out, "Add at least one intersection first (menu 9)."));
}

TEST(app_configuration_changes_are_validated_saved_and_audited) {
    Sandbox sb("config");
    ScriptedInput in;
    in.lines(kFirstRun).lines(kLoginAdmin).line("10")
      .lines({"1", "99"})                                                           // seed
      .lines({"2", "3", "9"})                                                       // thresholds
      .lines({"2", "9", "5"})                                                       // moderate >= high refused
      .lines({"4", "10"})                                                           // speed 10x
      .lines({"4", "5"})                                                            // 5x not allowed
      .lines({"0", "0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK(has(r.out, "Saved (seed=99)."));
    CHECK(has(r.out, "Saved (thresholds=3/9)."));
    CHECK(has(r.out, "Saved (speed=10)."));
    CHECK(count(r.out, "[E-VAL-001]") == 2);
    const std::string ini = sb.read("config.ini");
    CHECK(has(ini, "seed=99") && has(ini, "moderate=3") && has(ini, "high=9") && has(ini, "speed=10"));
    CHECK(has(sb.read("audit.log"), "PARAM_CHANGE | seed=99"));
}

TEST(app_speed_controls_how_often_the_dashboard_refreshes) {
    Sandbox sb("speed");
    ScriptedInput in;
    in.lines(kFirstRun).lines(kLoginAdmin)
      .lines({"10", "4", "10", "0"})                                                // 10x
      .lines({"1", "2", "25", "0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK(has(r.out, "(tick 10)") && has(r.out, "(tick 20)") && has(r.out, "(tick 25)"));
    CHECK(!has(r.out, "(tick 5)"));
}

TEST(app_saved_settings_are_loaded_on_next_start) {
    Sandbox sb("reload");
    { ScriptedInput in; in.lines(kFirstRun).lines(kLoginAdmin).lines({"10", "1", "123", "0", "0", "0"}); runApp(sb, in); }
    ScriptedInput in2;
    in2.lines(kLoginAdmin).lines({"1", "0", "0"});
    const Result2 r = runApp(sb, in2);
    CHECK(has(r.out, "Simulation started: seed 123"));
}

TEST(app_adaptive_timing_is_switched_on_saved_audited_and_reloaded) {          // FR-09, issue #14
    Sandbox sb("adaptive");
    {
        ScriptedInput in;
        in.lines(kFirstRun).lines(kLoginAdmin)
          .lines({"10", "5", "2", "5", "1", "0"})                                   // 2 is out of range, then on
          .lines({"1", "2", "60", "0", "0"});
        const Result2 r = runApp(sb, in);
        CHECK(has(r.out, "adaptive=off"));
        CHECK(has(r.out, "Saved (adaptive=1)."));
        CHECK(has(r.out, "adaptive=on"));
        CHECK(has(r.out, "(tick 60)"));
    }
    CHECK(has(sb.read("config.ini"), "adaptive=1"));
    CHECK(has(sb.read("audit.log"), "PARAM_CHANGE | adaptive=1"));
    ScriptedInput in2;
    in2.lines(kLoginAdmin).lines({"10", "0", "0", "0"});
    CHECK(has(runApp(sb, in2).out, "adaptive=on"));
}

TEST(app_operator_cannot_switch_adaptive_timing) {
    Sandbox sb("adaptive_op");
    ScriptedInput in;
    in.lines(kFirstRun).lines(kLoginAdmin)
      .lines({"11", "1", "oper1", "Oper12345", "1", "0", "0"})
      .lines({"1", "oper1", "Oper12345", "10", "0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK(has(r.out, "[E-AUTH-004]"));
    CHECK(!has(sb.read("audit.log"), "adaptive="));
}

TEST(app_corrupt_config_falls_back_to_defaults_with_error_code) {
    Sandbox sb("corrupt_cfg");
    { std::ofstream f(sb.dir / "config.ini"); f << "seed=oops\n"; }
    ScriptedInput in;
    in.lines(kFirstRun).line("0");
    const Result2 r = runApp(sb, in);
    CHECK_EQ(r.code, 0);
    CHECK(has(r.out, "[E-CFG-001]"));
    CHECK(has(sb.read("audit.log"), "CONFIG_CORRUPT"));
    CHECK_EQ(sb.read("config.ini"), "seed=oops\n");                                // file left untouched
}

TEST(app_damaged_user_file_stops_start_up) {
    Sandbox sb("corrupt_users");
    { std::ofstream f(sb.dir / "users.dat"); f << "this is not a user record\n"; }
    ScriptedInput in;
    const Result2 r = runApp(sb, in);
    CHECK_EQ(r.code, 1);
    CHECK(has(r.out, "The user file is damaged"));
}

TEST(app_commuter_view_is_read_only_and_needs_no_login) {
    Sandbox sb("commuter");
    ScriptedInput in;
    in.lines(kFirstRun)
      .lines({"2", "1", "2", "0"})                                                  // before any run: no data, no reports
      .lines(kLoginAdmin).lines({"1", "2", "20", "8", "1", "run1", "0"})            // an operator produces data
      .lines({"2", "1", "2", "3", "run1.csv", "3", "../x", "3", "missing.csv", "0", "0"});
    const Result2 r = runApp(sb, in);
    const std::string after = r.out.substr(r.out.find("Report saved"));
    CHECK(has(r.out, "No data available."));
    CHECK(has(r.out, "No reports yet."));
    CHECK(has(after, "congestion"));
    CHECK(has(after, "Reports:\n  run1.csv"));
    CHECK(has(after, "metric,value"));                                              // report content shown
    CHECK(has(after, "[E-IO-002]"));
    CHECK(has(after, "[E-IO-001]"));
    const std::size_t commuter = after.find("COMMUTER VIEW");
    CHECK(commuter != std::string::npos);
    CHECK(!has(after.substr(commuter), "GREEN"));                                   // no signal data or controls
}

TEST(app_tc13_commuter_cannot_reach_control_configuration_or_user_functions) {   // issue #16
    Sandbox sb("tc13");
    ScriptedInput in;
    in.lines(kFirstRun)
      .lines({"2", "4", "6", "7", "9", "10", "11", "12", "1", "0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK_EQ(r.code, 0);
    CHECK_EQ(count(r.out, "[E-AUTH-004]"), 6);                                    // every control command refused
    CHECK(has(r.out, "need a login (main menu 1)"));
    CHECK(has(r.out, "No data available."));                                        // the view still works afterwards
    CHECK(!has(r.out, "OPERATIONS MENU"));
    CHECK(!has(r.out, "SYSTEM PARAMETERS"));
    CHECK(!has(r.out, "USER ACCOUNTS"));
    CHECK(!has(r.out, "Simulation started"));
    const std::string audit = sb.read("audit.log");
    CHECK(has(audit, "anonymous | DENIED:StopSimulation | E-AUTH-004"));
    CHECK(has(audit, "anonymous | DENIED:ConfigureParameters | E-AUTH-004"));
    CHECK(has(audit, "anonymous | DENIED:ManageUsers | E-AUTH-004"));
}

TEST(app_commuter_view_hides_files_that_are_not_reports) {
    Sandbox sb("commuter_hide");
    fs::create_directories(sb.dir / "reports");
    { std::ofstream f(sb.dir / "reports" / "secret.dat"); f << "do not show"; }
    { std::ofstream f(sb.dir / "reports" / "day1.txt"); f << "Traffic report day 1"; }
    ScriptedInput in;
    in.lines(kFirstRun).lines({"2", "2", "3", "secret.dat", "3", "day1.txt", "0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK(has(r.out, "Reports:\n  day1.txt\n"));
    CHECK(!has(r.out, "do not show"));
    CHECK(has(r.out, "[E-IO-001]"));
    CHECK(has(r.out, "Traffic report day 1"));
}

TEST(app_same_seed_gives_identical_report_in_two_separate_runs) {
    auto run = [](const std::string& name) {
        Sandbox sb(name);
        ScriptedInput in;
        in.lines(kFirstRun).lines(kLoginAdmin).lines({"10", "1", "42", "0"})
          .lines({"1", "2", "120", "8", "1", "r", "0", "0"});
        runApp(sb, in);
        return sb.read("reports/r.csv");
    };
    const std::string a = run("det_a"), b = run("det_b");
    CHECK(!a.empty());
    CHECK_EQ(a, b);                                                                 // NFR-08
}

// ---------------------------------------------------------------- issue #21 (NFR-04, TC-17)

TEST(app_help_is_available_on_every_menu_screen) {
    Sandbox sb("help");
    ScriptedInput in;
    in.lines(kFirstRun)
      .lines({"h", "2", "?", "0"})                                                  // main, commuter
      .lines(kLoginAdmin).lines({"help"})                                           // operations
      .lines({"9", "h", "0", "10", "h", "0", "11", "H", "0", "0", "0"});           // the three level-3 menus
    const Result2 r = runApp(sb, in);
    CHECK_EQ(r.code, 0);
    for (const char* title : {"HELP - MAIN MENU", "HELP - COMMUTER VIEW", "HELP - OPERATIONS MENU",
                              "HELP - INTERSECTIONS", "HELP - SYSTEM PARAMETERS", "HELP - USER ACCOUNTS"}) {
        CHECK(has(r.out, title));
    }
    CHECK(has(r.out, "Choice (0-2, h = help): "));                                  // the help line on every screen
    CHECK(has(r.out, "Choice (0-11, h = help): "));
    CHECK(!has(r.out, "must be a number"));                                         // help is not an input error
}

TEST(app_tc17_first_time_user_starts_a_simulation_and_reads_lane_2_with_help_only) {
    Sandbox sb("tc17");
    ScriptedInput in;
    in.lines(kFirstRun).lines({"h"}).lines(kLoginAdmin)
      .lines({"h", "1", "2", "60", "0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK(has(r.out, "Quick start"));
    CHECK(has(r.out, "To read the congestion of lane 2"));
    CHECK(has(r.out, "Simulation started"));
    const std::size_t dash = r.out.rfind("LIVE DASHBOARD");
    CHECK(dash != std::string::npos);
    CHECK(r.out.find("\n  2     ", dash) != std::string::npos);                    // the row of lane 2
}

TEST(app_menus_are_at_most_three_levels_deep) {
    // Main (1) -> Operations (2) -> Intersections / Parameters / Users (3). Every option of a
    // level-3 menu is an action or a form, never another menu: 0 always returns one level up.
    Sandbox sb("depth");
    ScriptedInput in;
    in.lines(kFirstRun).lines(kLoginAdmin)
      .lines({"9", "1", "0", "10", "0", "11", "2", "0", "0", "0"});
    const Result2 r = runApp(sb, in);
    CHECK_EQ(r.code, 0);
    CHECK_EQ(count(r.out, "OPERATIONS MENU  ("), 4);                               // back at level 2 after each
    CHECK(has(r.out, "INTERSECTIONS") && has(r.out, "SYSTEM PARAMETERS") && has(r.out, "USER ACCOUNTS"));
}

TEST(app_invalid_input_still_names_field_and_range_with_help_prompt) {
    Sandbox sb("invalid_help");
    ScriptedInput in;
    in.lines(kFirstRun).lines({"x", "7", "0"});
    const Result2 r = runApp(sb, in);
    CHECK(has(r.out, "Choice must be a number."));
    CHECK(has(r.out, "Choice must be between 0 and 2."));
}
