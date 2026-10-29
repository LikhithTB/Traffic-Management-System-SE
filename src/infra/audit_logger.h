#pragma once
// Audit Logger (story #3, SEC-07, SEC-08). Append-only: this class has no
// function to edit, truncate or delete records. File is owner-only (0600 on Linux).
#include <functional>
#include <string>
#include "common/interfaces.h"

class FileAuditLogger : public IAuditLogger {
public:
    using Timestamper = std::function<std::string()>;

    // path e.g. "audit.log". The timestamper can be replaced in tests.
    explicit FileAuditLogger(std::string path, Timestamper timestamper = defaultTimestamp);

    // Writes one line:  2026-10-29 14:03:11 | user | ACTION | result
    // Fields are cleaned so a value cannot forge extra lines or columns.
    Result<void> log(const std::string& user, const std::string& action,
                     const std::string& result) override;

    const std::string& path() const { return path_; }
    static std::string defaultTimestamp();

private:
    std::string path_;
    Timestamper timestamper_;
};
