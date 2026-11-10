#pragma once
#include <cstddef>
// Role -> command permission table (SEC-01, SEC-05). The Command Dispatcher
// checks this for every command; Auth.hasPermission() must use it too.
#include "common/types.h"

enum class Command {
    StartSimulation, PauseSimulation, StopSimulation,
    SetGreenTime, InjectEmergency, ManageIntersections,
    GenerateReport, ViewDashboard,
    ViewCongestion, ViewReports,          // allowed for everyone (Commuter view, FR-10)
    ConfigureParameters, ManageUsers      // Administrator only
};

inline const char* commandName(Command c) {
    // Table in the order of the Command enum above.
    static const char* const names[] = {
        "StartSimulation", "PauseSimulation", "StopSimulation", "SetGreenTime", "InjectEmergency",
        "ManageIntersections", "GenerateReport", "ViewDashboard", "ViewCongestion", "ViewReports",
        "ConfigureParameters", "ManageUsers"};
    const std::size_t i = static_cast<std::size_t>(c);
    return i < sizeof(names) / sizeof(names[0]) ? names[i] : "Unknown";
}

inline bool isPublicCommand(Command c) {
    return c == Command::ViewCongestion || c == Command::ViewReports;
}

inline bool isAdminCommand(Command c) {
    return c == Command::ConfigureParameters || c == Command::ManageUsers;
}

inline bool isOperatorCommand(Command c) {
    return c == Command::StartSimulation || c == Command::PauseSimulation || c == Command::StopSimulation ||
           c == Command::SetGreenTime || c == Command::InjectEmergency || c == Command::ManageIntersections ||
           c == Command::GenerateReport || c == Command::ViewDashboard;
}

// Deny by default: a command that is in none of the three groups is allowed for nobody.
inline bool roleAllows(Role r, Command c) {
    if (isPublicCommand(c)) return true;
    if (isAdminCommand(c)) return r == Role::Admin;
    if (isOperatorCommand(c)) return r == Role::Operator || r == Role::Admin;
    return false;
}
