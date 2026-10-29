#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>
#ifndef _WIN32
#include <sys/stat.h>
#endif
#include "infra/audit_logger.h"
#include "test_framework.h"

namespace {
std::vector<std::string> readLines(const std::string& path) {
    std::ifstream in(path);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(in, l)) lines.push_back(l);
    return lines;
}
FileAuditLogger::Timestamper fixedTime() { return [] { return std::string("2026-10-29 10:00:00"); }; }
}

TEST(audit_writes_one_line_per_event_in_agreed_format) {
    const std::string path = "test_audit_1.log";
    std::remove(path.c_str());
    FileAuditLogger log(path, fixedTime());
    CHECK(log.log("alice", "LOGIN_OK", "OK").ok());
    CHECK(log.log("bob", "LOGIN_FAIL", "E-AUTH-001").ok());
    auto lines = readLines(path);
    CHECK_EQ(lines.size(), 2u);
    CHECK_EQ(lines[0], "2026-10-29 10:00:00 | alice | LOGIN_OK | OK");
    CHECK_EQ(lines[1], "2026-10-29 10:00:00 | bob | LOGIN_FAIL | E-AUTH-001");
    std::remove(path.c_str());
}

TEST(audit_is_append_only_across_restarts) {
    const std::string path = "test_audit_2.log";
    std::remove(path.c_str());
    { FileAuditLogger a(path, fixedTime()); CHECK(a.log("u1", "A", "OK").ok()); }
    { FileAuditLogger b(path, fixedTime()); CHECK(b.log("u2", "B", "OK").ok()); }   // must not truncate
    CHECK_EQ(readLines(path).size(), 2u);
    std::remove(path.c_str());
}

TEST(audit_values_cannot_forge_lines_or_columns) {
    const std::string path = "test_audit_3.log";
    std::remove(path.c_str());
    FileAuditLogger log(path, fixedTime());
    CHECK(log.log("evil\n2026-01-01 | admin | LOGIN_OK | OK", "ACT|ION", "r\r\nx").ok());
    auto lines = readLines(path);
    CHECK_EQ(lines.size(), 1u);                         // still a single line
    std::size_t bars = 0;
    for (char c : lines[0]) if (c == '|') ++bars;
    CHECK_EQ(bars, 3u);                                 // exactly 4 columns
    std::remove(path.c_str());
}

TEST(audit_limits_field_length) {
    const std::string path = "test_audit_4.log";
    std::remove(path.c_str());
    FileAuditLogger log(path, fixedTime());
    CHECK(log.log(std::string(5000, 'a'), "A", "OK").ok());
    CHECK(readLines(path)[0].size() < 400u);
    std::remove(path.c_str());
}

TEST(audit_reports_io_failure) {
    FileAuditLogger log("no_such_folder_xyz/audit.log", fixedTime());
    CHECK_EQ(log.log("u", "A", "OK").error, ErrorCode::IoFailure);
}

#ifndef _WIN32
TEST(audit_file_is_owner_only_0600) {
    const std::string path = "test_audit_5.log";
    std::remove(path.c_str());
    FileAuditLogger log(path, fixedTime());
    CHECK(log.log("u", "A", "OK").ok());
    struct stat st{};
    CHECK(stat(path.c_str(), &st) == 0);
    CHECK_EQ(static_cast<int>(st.st_mode & 0777), 0600);
    std::remove(path.c_str());
}
#endif

TEST(audit_default_timestamp_has_expected_shape) {
    const std::string ts = FileAuditLogger::defaultTimestamp();
    CHECK_EQ(ts.size(), 19u);       // "YYYY-MM-DD HH:MM:SS"
    CHECK(ts[4] == '-' && ts[10] == ' ' && ts[13] == ':');
}
