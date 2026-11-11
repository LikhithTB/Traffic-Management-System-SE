#include "ui/help.h"

#include <cctype>
#include <cstddef>

namespace help {
namespace {

const char* const kMain =
    "HELP - MAIN MENU\n"
    "  1  Log in as Operator or Administrator (3 wrong passwords lock the account for 60 s).\n"
    "  2  Commuter view: congestion status and saved reports, no account needed.\n"
    "  0  Exit the program.\n"
    "  Quick start: 1 (log in) -> 1 (start simulation) -> 2 (run, e.g. 60 seconds).\n"
    "  The dashboard then shows every lane with its signal, queue and congestion (Low/Moderate/High).\n"
    "  Type the number of an option and press Enter. Type h at any 'Choice' prompt for help.\n";

const char* const kOperations =
    "HELP - OPERATIONS MENU\n"
    "  1  Start a simulation with the saved settings and intersections (always start here).\n"
    "  2  Run it for N simulated seconds; the live dashboard is shown while it runs.\n"
    "  3  Pause or resume.            4  Stop the simulation.\n"
    "  5  Show the dashboard again: one row per lane - Signal, Queue, Congestion.\n"
    "  6  Change the green time of an intersection (5-120 s, applies from the next cycle).\n"
    "  7  Put an emergency vehicle in a lane; its signal turns green within 5 s.\n"
    "  8  Save a CSV or TXT report of the current run (name max 64 characters).\n"
    "  9  Manage intersections (list, add, edit, remove; max 10, 1-8 lanes).\n"
    " 10  System parameters: seed, thresholds, timeout, speed, adaptive timing (Administrator).\n"
    " 11  Create or list user accounts (Administrator).\n"
    "  0  Log out.\n"
    "  To read the congestion of lane 2: 1, then 2 (e.g. 60); in the dashboard find 2 in the\n"
    "  Lane column and read its Congestion column (Low / Moderate / High).\n"
    "  [E-AUTH-004] means your role may not use that option; [E-SIM-001] means start a simulation first.\n";

const char* const kCommuter =
    "HELP - COMMUTER VIEW (read-only)\n"
    "  1  Congestion status of every lane (by default Low < 5, Moderate 5-14, High >= 15 waiting).\n"
    "  2  List the saved reports.\n"
    "  3  Show a report: type its name exactly as listed, e.g. run1.csv.\n"
    "  0  Back to the main menu.\n"
    "  Nothing can be changed here. Control functions need a login (main menu 1).\n";

const char* const kIntersections =
    "HELP - INTERSECTIONS\n"
    "  1  List   2  Add   3  Edit (by id)   4  Remove (by id)   0  Back\n"
    "  Limits: at most 10 intersections, 1-8 lanes, 1-60 arrivals per minute per lane,\n"
    "  green time 5-120 s, name up to 40 characters. Changes apply to the NEXT simulation start.\n";

const char* const kParameters =
    "HELP - SYSTEM PARAMETERS (Administrator)\n"
    "  1  Random seed 0-2147483647: the same seed gives the same run and the same report.\n"
    "  2  Congestion thresholds: Moderate from M vehicles, High from H vehicles (M < H).\n"
    "  3  Session timeout 1-60 minutes of inactivity.\n"
    "  4  Simulation speed 1, 10 or 60 (simulated seconds per dashboard frame).\n"
    "  5  Adaptive signal timing on/off: a High lane keeps green 5 s longer at a time\n"
    "     (max +20 s, max 140 s) while every other lane is below Moderate.\n"
    "  0  Back. Every change is saved to config.ini and written to the audit log.\n";

const char* const kUsers =
    "HELP - USER ACCOUNTS (Administrator)\n"
    "  1  Create an account: username 4-20 characters (letters, digits, _ . -), password at least\n"
    "     8 characters with a letter and a digit, role 1 = Operator or 2 = Administrator.\n"
    "  2  List the accounts (deactivated ones are marked).\n"
    "  3  Deactivate an account: it can no longer log in and any open session ends; the record is kept.\n"
    "  4  Delete an account permanently (asks for confirmation).\n"
    "     You cannot deactivate or delete your own account.   0  Back.\n";

}  // namespace

const char* text(Screen screen) {
    static const char* const texts[] = {kMain, kOperations, kCommuter, kIntersections, kParameters, kUsers};
    return texts[static_cast<std::size_t>(screen)];
}

bool isRequest(const std::string& input) {
    std::string word;
    for (char c : input) {
        if (!std::isspace(static_cast<unsigned char>(c))) word.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return word == "h" || word == "help" || word == "?";
}

}  // namespace help
