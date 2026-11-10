#include "user_store.hpp"

#include <cctype>

namespace tms {
namespace {

std::vector<std::string> split(const std::string& s, char sep) {
  std::vector<std::string> parts;
  std::string cur;
  for (char c : s) {
    if (c == sep) { parts.push_back(cur); cur.clear(); }
    else cur.push_back(c);
  }
  parts.push_back(cur);
  return parts;
}

bool validUsername(const std::string& u) {
  if (u.size() < 4 || u.size() > 20) return false;   // FR-07: 4-20 characters
  for (unsigned char c : u)
    if (!(std::isalnum(c) || c == '_' || c == '.' || c == '-')) return false;
  return true;
}

// SEC-02: 8-256 characters with at least one letter and one digit.
Result<void> checkPassword(const std::string& pw) {
  if (pw.size() < 8)
    return Result<void>::Err(ErrorCode::WeakPassword, "password must be at least 8 characters with a letter and a digit");
  if (pw.size() > 256) return Result<void>::Err(ErrorCode::WeakPassword, "password must be at most 256 characters");
  bool letter = false, digit = false;
  for (unsigned char c : pw) {
    if (std::isalpha(c)) letter = true;
    if (std::isdigit(c)) digit = true;
  }
  if (!letter || !digit)
    return Result<void>::Err(ErrorCode::WeakPassword, "password must be at least 8 characters with a letter and a digit");
  return Result<void>::Ok();
}

}  // namespace

std::string roleToString(Role r) {
  switch (r) {
    case Role::Admin: return "admin";
    case Role::Operator: return "operator";
    case Role::Commuter: return "commuter";
  }
  return "commuter";
}

Result<Role> roleFromString(const std::string& s) {
  if (s == "admin") return Result<Role>::Ok(Role::Admin);
  if (s == "operator") return Result<Role>::Ok(Role::Operator);
  if (s == "commuter") return Result<Role>::Ok(Role::Commuter);
  return Result<Role>::Err(ErrorCode::InvalidArgument, "unknown role: " + s);
}

namespace {

bool parseIterations(const std::string& text, unsigned long& iterations) {
  try {
    std::size_t pos = 0;
    iterations = std::stoul(text, &pos);
    return pos == text.size() && iterations != 0;
  } catch (...) {
    return false;
  }
}

// One line of the user file: username|role|saltHex|hashHex|iterations. False if malformed.
bool parseUserLine(const std::string& line, User& out) {
  const auto f = split(line, '|');
  if (f.size() != 5 || !validUsername(f[0])) return false;
  const auto role = roleFromString(f[1]);
  if (!role.ok() || !fromHex(f[2]).ok() || !fromHex(f[3]).ok()) return false;
  unsigned long iterations = 0;
  if (!parseIterations(f[4], iterations)) return false;
  out = User{f[0], role.value(), f[2], f[3], static_cast<unsigned>(iterations)};
  return true;
}

}  // namespace

UserStore::UserStore(FileStore file, SeededRng& rng, unsigned iterations)
    : file_(std::move(file)), rng_(rng), iterations_(iterations) {}

Result<void> UserStore::load() {
  users_.clear();
  if (!file_.exists()) return Result<void>::Ok();
  auto lines = file_.readLines();
  if (!lines.ok()) return Propagate<void>(lines);

  for (const auto& line : lines.value()) {
    User u;
    if (!parseUserLine(line, u)) {
      users_.clear();
      return Result<void>::Err(ErrorCode::Corrupt, "malformed user record in " + file_.path());
    }
    users_[u.username] = u;
  }
  return Result<void>::Ok();
}

Result<void> UserStore::save() const {
  std::vector<std::string> lines;
  for (const auto& kv : users_) {
    const User& u = kv.second;
    lines.push_back(u.username + "|" + roleToString(u.role) + "|" + u.saltHex + "|" + u.hashHex + "|" +
                    std::to_string(u.iterations));
  }
  return file_.writeLines(lines);
}

Result<void> UserStore::addUser(const std::string& username, const std::string& password, Role role) {
  if (!validUsername(username))
    return Result<void>::Err(ErrorCode::InvalidArgument,
                             "username must be 4-20 chars: letters, digits, '_', '.', '-'");
  auto strong = checkPassword(password);
  if (!strong.ok()) return strong;
  if (exists(username)) return Result<void>::Err(ErrorCode::AlreadyExists, "user already exists");

  User u;
  u.username = username;
  u.role = role;
  u.iterations = iterations_;
  u.saltHex = generateSaltHex(rng_);
  u.hashHex = hashPasswordHex(password, u.saltHex, u.iterations);
  users_[username] = u;

  auto saved = save();
  if (!saved.ok()) users_.erase(username);  // roll back so memory matches disk
  return saved;
}

Result<void> UserStore::removeUser(const std::string& username) {
  auto it = users_.find(username);
  if (it == users_.end()) return Result<void>::Err(ErrorCode::NotFound, "no such user");
  User backup = it->second;
  users_.erase(it);
  auto saved = save();
  if (!saved.ok()) users_[username] = backup;
  return saved;
}

Result<Role> UserStore::authenticate(const std::string& username, const std::string& password) const {
  auto it = users_.find(username);
  if (it == users_.end()) {
    // Hash anyway so unknown users cost the same time as known ones.
    hashPasswordHex(password, "00", iterations_);
    return Result<Role>::Err(ErrorCode::AuthFailed, "invalid username or password");
  }
  const User& u = it->second;
  if (!verifyPassword(password, u.saltHex, u.hashHex, u.iterations))
    return Result<Role>::Err(ErrorCode::AuthFailed, "invalid username or password");
  return Result<Role>::Ok(u.role);
}

std::vector<std::string> UserStore::usernames() const {
  std::vector<std::string> out;
  for (const auto& kv : users_) out.push_back(kv.first);
  return out;
}

}  // namespace tms
