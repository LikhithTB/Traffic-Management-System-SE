#include "file_store.hpp"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace tms {

FileStore::FileStore(std::string path) : path_(std::move(path)) {}

bool FileStore::exists() const {
  std::error_code ec;
  return fs::exists(path_, ec);
}

Result<std::vector<std::string>> FileStore::readLines() const {
  std::ifstream in(path_);
  if (!in) return Result<std::vector<std::string>>::Err(ErrorCode::NotFound, "cannot open " + path_);
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (!line.empty()) lines.push_back(line);
  }
  if (in.bad()) return Result<std::vector<std::string>>::Err(ErrorCode::IoError, "read failed: " + path_);
  return Result<std::vector<std::string>>::Ok(std::move(lines));
}

Result<void> FileStore::writeLines(const std::vector<std::string>& lines) const {
  std::error_code ec;
  fs::path target(path_);
  if (target.has_parent_path()) fs::create_directories(target.parent_path(), ec);

  const std::string tmp = path_ + ".tmp";
  {
    std::ofstream out(tmp, std::ios::trunc);
    if (!out) return Result<void>::Err(ErrorCode::IoError, "cannot write " + tmp);
    for (const auto& l : lines) out << l << '\n';
    out.flush();
    if (!out) {
      fs::remove(tmp, ec);
      return Result<void>::Err(ErrorCode::IoError, "write failed: " + tmp);
    }
  }
  fs::rename(tmp, path_, ec);
  if (ec) {
    fs::remove(tmp, ec);
    return Result<void>::Err(ErrorCode::IoError, "rename failed: " + path_);
  }
  return Result<void>::Ok();
}

Result<void> FileStore::appendLine(const std::string& line) const {
  std::error_code ec;
  fs::path target(path_);
  if (target.has_parent_path()) fs::create_directories(target.parent_path(), ec);
  std::ofstream out(path_, std::ios::app);
  if (!out) return Result<void>::Err(ErrorCode::IoError, "cannot append " + path_);
  out << line << '\n';
  out.flush();
  if (!out) return Result<void>::Err(ErrorCode::IoError, "append failed: " + path_);
  return Result<void>::Ok();
}

Result<void> FileStore::remove() const {
  std::error_code ec;
  fs::remove(path_, ec);
  if (ec) return Result<void>::Err(ErrorCode::IoError, "cannot remove " + path_);
  return Result<void>::Ok();
}

}  // namespace tms
