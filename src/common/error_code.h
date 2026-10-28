#pragma once
// Error codes from Architecture Design Spec section 3.4 (E-xxx-nnn catalogue).
// Every operation that can fail returns Result<T> carrying one of these.

enum class ErrorCode {
    Ok,
    AuthInvalid,       // E-AUTH-001 wrong username or password
    AuthLocked,        // E-AUTH-002 account locked (3 failures, 60 s)
    SessionExpired,    // E-AUTH-003 inactivity timeout
    PermissionDenied,  // E-AUTH-004 role lacks permission
    OutOfRange,        // E-VAL-001  value out of range
    NotNumeric,        // E-VAL-002  not numeric where number required
    BadLength,         // E-VAL-003  empty or longer than 256 chars
    ConfigCorrupt,     // E-CFG-001  config.ini corrupt -> defaults
    IoFailure,         // E-IO-001   file cannot be opened/written
    BadFileName,       // E-IO-002   path separator or ".." in file name
    NotRunning,        // E-SIM-001  command needs a running simulation
    CapacityExceeded,  // E-SIM-002  more than 10 intersections
    Internal           // E-SYS-001  unexpected exception
};

// Short code used in audit log and user messages, e.g. "E-AUTH-002".
inline const char* errorCodeId(ErrorCode e) {
    switch (e) {
        case ErrorCode::Ok:               return "OK";
        case ErrorCode::AuthInvalid:      return "E-AUTH-001";
        case ErrorCode::AuthLocked:       return "E-AUTH-002";
        case ErrorCode::SessionExpired:   return "E-AUTH-003";
        case ErrorCode::PermissionDenied: return "E-AUTH-004";
        case ErrorCode::OutOfRange:       return "E-VAL-001";
        case ErrorCode::NotNumeric:       return "E-VAL-002";
        case ErrorCode::BadLength:        return "E-VAL-003";
        case ErrorCode::ConfigCorrupt:    return "E-CFG-001";
        case ErrorCode::IoFailure:        return "E-IO-001";
        case ErrorCode::BadFileName:      return "E-IO-002";
        case ErrorCode::NotRunning:       return "E-SIM-001";
        case ErrorCode::CapacityExceeded: return "E-SIM-002";
        case ErrorCode::Internal:         return "E-SYS-001";
    }
    return "E-SYS-001";
}
