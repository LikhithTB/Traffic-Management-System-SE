#pragma once
#include <cstddef>
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
// Table lookup in the order of the enum above (kept in step by the static_assert).
inline const char* errorCodeId(ErrorCode e) {
    static constexpr const char* ids[] = {"OK",         "E-AUTH-001", "E-AUTH-002", "E-AUTH-003", "E-AUTH-004",
                                          "E-VAL-001",  "E-VAL-002",  "E-VAL-003",  "E-CFG-001",  "E-IO-001",
                                          "E-IO-002",   "E-SIM-001",  "E-SIM-002",  "E-SYS-001"};
    static_assert(sizeof(ids) / sizeof(ids[0]) == static_cast<std::size_t>(ErrorCode::Internal) + 1,
                  "errorCodeId table must have one entry per ErrorCode");
    const std::size_t i = static_cast<std::size_t>(e);
    return i < sizeof(ids) / sizeof(ids[0]) ? ids[i] : "E-SYS-001";
}
