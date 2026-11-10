#pragma once
// Report file writer (issue #15, FR-06, SEC-08). Used as the Writer of ReportGenerator.
// Creates <reportsDir>/<name>: create-new only (an existing report is never overwritten),
// name <= 64 chars with no path separators or "..", and the file must lie directly
// inside the reports directory.
#include <string>
#include "common/result.h"

// BadFileName for an unsafe name; IoFailure if the file exists or cannot be written.
// Returns the path of the new file.
Result<std::string> writeReportFile(const std::string& reportsDir, const std::string& name,
                                    const std::string& content);
