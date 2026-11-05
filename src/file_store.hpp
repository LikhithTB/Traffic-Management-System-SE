#pragma once
// Issue #2: minimal line-oriented file store (one record per line).
#include <string>
#include <vector>

#include "result.hpp"

namespace tms {

class FileStore {
 public:
  // ownerOnly: files are made readable/writable by the owner only (0600 on Linux) - use for users.dat.
  explicit FileStore(std::string path, bool ownerOnly = false);
  const std::string& path() const { return path_; }
  bool exists() const;
  Result<std::vector<std::string>> readLines() const;                 // skips blank lines
  Result<void> writeLines(const std::vector<std::string>& lines) const;  // atomic: temp file + rename
  Result<void> appendLine(const std::string& line) const;
  Result<void> remove() const;

 private:
  std::string path_;
  bool ownerOnly_ = false;
};

}  // namespace tms
