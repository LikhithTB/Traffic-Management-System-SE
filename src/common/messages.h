#pragma once
// Fixed user-facing messages for each ErrorCode (Architecture Design Spec 3.4).
// Messages name the field and allowed range (NFR-06) and never reveal paths,
// hashes or internals.
#include <string>
#include "common/error_code.h"

inline std::string errorMessage(ErrorCode e, const std::string& field = "value",
                                long long lo = 0, long long hi = 0) {
    switch (e) {
        case ErrorCode::Ok:               return "OK";
        case ErrorCode::AuthInvalid:      return "Wrong username or password.";
        case ErrorCode::AuthLocked:       return "Account locked. Try again in 60 seconds.";
        case ErrorCode::SessionExpired:   return "Session expired. Please log in again.";
        case ErrorCode::PermissionDenied: return "You do not have permission for this command.";
        case ErrorCode::OutOfRange:
            return field + " must be between " + std::to_string(lo) + " and " + std::to_string(hi) + ".";
        case ErrorCode::NotNumeric:       return field + " must be a number.";
        case ErrorCode::BadLength:
            return field + " must not be empty and must be at most 256 characters.";
        case ErrorCode::ConfigCorrupt:    return "Configuration file was invalid; defaults are in use.";
        case ErrorCode::IoFailure:        return "The file could not be read or written.";
        case ErrorCode::BadFileName:
            return "File name must be at most 64 characters with no '/', '\\' or '..'.";
        case ErrorCode::NotRunning:       return "Start a simulation first.";
        case ErrorCode::CapacityExceeded: return "Maximum number of intersections (10) reached.";
        case ErrorCode::Internal:         return "An unexpected error occurred.";
    }
    return "An unexpected error occurred.";
}
