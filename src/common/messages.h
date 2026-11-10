#pragma once
// Fixed user-facing messages for each ErrorCode (Architecture Design Spec 3.4).
// Messages name the field and allowed range (NFR-06) and never reveal paths,
// hashes or internals.
#include <cstddef>
#include <string>
#include "common/error_code.h"

inline std::string errorMessage(ErrorCode e, const std::string& field = "value",
                                long long lo = 0, long long hi = 0) {
    if (e == ErrorCode::OutOfRange) {
        return field + " must be between " + std::to_string(lo) + " and " + std::to_string(hi) + ".";
    }
    if (e == ErrorCode::NotNumeric) return field + " must be a number.";
    if (e == ErrorCode::BadLength) return field + " must not be empty and must be at most 256 characters.";

    // Fixed texts, in the order of the ErrorCode enum (entries 5-7 are built above).
    static const char* const fixed[] = {
        "OK",
        "Wrong username or password.",
        "Account locked. Try again in 60 seconds.",
        "Session expired. Please log in again.",
        "You do not have permission for this command.",
        "", "", "",
        "Configuration file was invalid; defaults are in use.",
        "The file could not be read or written.",
        "File name must be at most 64 characters with no '/', '\\' or '..'.",
        "Start a simulation first.",
        "Maximum number of intersections (10) reached.",
        "An unexpected error occurred."};
    const std::size_t count = sizeof(fixed) / sizeof(fixed[0]);
    const std::size_t i = static_cast<std::size_t>(e);
    return fixed[i < count ? i : count - 1];
}
