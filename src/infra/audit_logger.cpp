#include "infra/audit_logger.h"

#include <cerrno>
#include <ctime>
#include <fstream>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

// Replace control characters and the column separator, and limit length, so a
// user-supplied value cannot break the one-line-per-event format.
std::string clean(const std::string& in) {
    std::string out;
    const std::size_t limit = 200;
    for (char c : in) {
        if (out.size() >= limit) break;
        const unsigned char u = static_cast<unsigned char>(c);
        out.push_back((u < 0x20 || c == '|') ? ' ' : c);
    }
    return out;
}

}  // namespace

FileAuditLogger::FileAuditLogger(std::string path, Timestamper timestamper)
    : path_(std::move(path)), timestamper_(std::move(timestamper)) {}

std::string FileAuditLogger::defaultTimestamp() {
    const std::time_t t = std::time(nullptr);
    const std::tm* local = std::localtime(&t);
    if (local == nullptr) return "0000-00-00 00:00:00";
    char buf[32];
    if (std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", local) == 0) {
        return "0000-00-00 00:00:00";
    }
    return buf;
}

Result<void> FileAuditLogger::log(const std::string& user, const std::string& action,
                                  const std::string& result) {
    const std::string line = timestamper_() + " | " + clean(user) + " | " + clean(action) +
                             " | " + clean(result) + "\n";
#ifndef _WIN32
    // Created with mode 0600 from the start (SEC-08); O_APPEND = append-only writes.
    const int fd = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (fd < 0) return Result<void>::failure(ErrorCode::IoFailure);
    (void)::fchmod(fd, 0600);   // enforce mode even if the file already existed
    const char* p = line.data();
    std::size_t left = line.size();
    while (left > 0) {
        const ssize_t n = ::write(fd, p, left);
        if (n < 0) {
            if (errno == EINTR) continue;
            ::close(fd);
            return Result<void>::failure(ErrorCode::IoFailure);
        }
        p += n;
        left -= static_cast<std::size_t>(n);
    }
    ::close(fd);
#else
    std::ofstream out(path_, std::ios::app | std::ios::binary);
    if (!out) return Result<void>::failure(ErrorCode::IoFailure);
    out << line;
    out.flush();
    if (!out) return Result<void>::failure(ErrorCode::IoFailure);
#endif
    return Result<void>::success();
}
