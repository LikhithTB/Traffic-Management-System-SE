#include "app/report_writer.h"

#include <filesystem>
#include <fstream>
#include <system_error>

#include "app/report_generator.h"

namespace fs = std::filesystem;

Result<std::string> writeReportFile(const std::string& reportsDir, const std::string& name,
                                    const std::string& content) {
    const Result<void> nameOk = ReportGenerator::validateFileName(name);
    if (!nameOk.ok()) return Result<std::string>::failure(nameOk.error);

    std::error_code ec;
    fs::create_directories(reportsDir, ec);
    if (ec) return Result<std::string>::failure(ErrorCode::IoFailure);

    const fs::path target = fs::path(reportsDir) / name;
    const fs::path parent = fs::weakly_canonical(target, ec).parent_path();
    if (ec || parent != fs::weakly_canonical(reportsDir, ec)) {
        return Result<std::string>::failure(ErrorCode::BadFileName);
    }
    if (fs::exists(target, ec)) return Result<std::string>::failure(ErrorCode::IoFailure);   // create-new only

    std::ofstream out(target, std::ios::binary);
    if (!out) return Result<std::string>::failure(ErrorCode::IoFailure);
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    out.flush();
    if (!out) return Result<std::string>::failure(ErrorCode::IoFailure);
    return Result<std::string>::success(target.generic_string());
}
