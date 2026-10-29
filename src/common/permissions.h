#pragma once
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
    switch (c) {
        case Command::StartSimulation:    return "StartSimulation";
        case Command::PauseSimulation:    return "PauseSimulation";
        case Command::StopSimulation:     return "StopSimulation";
        case Command::SetGreenTime:       return "SetGreenTime";
        case Command::InjectEmergency:    return "InjectEmergency";
        case Command::ManageIntersections:return "ManageIntersections";
        case Command::GenerateReport:     return "GenerateReport";
        case Command::ViewDashboard:      return "ViewDashboard";
        case Command::ViewCongestion:     return "ViewCongestion";
        case Command::ViewReports:        return "ViewReports";
        case Command::ConfigureParameters:return "ConfigureParameters";
        case Command::ManageUsers:        return "ManageUsers";
    }
    return "Unknown";
}

inline bool isPublicCommand(Command c) {
    return c == Command::ViewCongestion || c == Command::ViewReports;
}

inline bool roleAllows(Role r, Command c) {
    switch (c) {
        case Command::ViewCongestion:
        case Command::ViewReports:
            return true;
        case Command::StartSimulation:
        case Command::PauseSimulation:
        case Command::StopSimulation:
        case Command::SetGreenTime:
        case Command::InjectEmergency:
        case Command::ManageIntersections:
        case Command::GenerateReport:
        case Command::ViewDashboard:
            return r == Role::Operator || r == Role::Admin;
        case Command::ConfigureParameters:
        case Command::ManageUsers:
            return r == Role::Admin;
    }
    return false;
}
