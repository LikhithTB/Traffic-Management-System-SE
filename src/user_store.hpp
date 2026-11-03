#pragma once
// Issue #4: persistent user store. File format, one user per line:
//   username|role|saltHex|hashHex|iterations
// Plain-text passwords are never stored.
#include <map>
#include <string>
#include <vector>

#include "file_store.hpp"
#include "password.hpp"
#include "result.hpp"
#include "rng.hpp"

namespace tms {

enum class Role { Admin, Operator, Commuter };

std::string roleToString(Role r);
Result<Role> roleFromString(const std::string& s);

struct User {
  std::string username;
  Role role;
  std::string saltHex;
  std::string hashHex;
  unsigned iterations;
};

class UserStore {
 public:
  UserStore(FileStore file, SeededRng& rng, unsigned iterations = kDefaultIterations);

  Result<void> load();  // missing file => empty store
  Result<void> addUser(const std::string& username, const std::string& password, Role role);
  Result<void> removeUser(const std::string& username);
  Result<Role> authenticate(const std::string& username, const std::string& password) const;

  bool exists(const std::string& username) const { return users_.count(username) != 0; }
  std::size_t size() const { return users_.size(); }
  std::vector<std::string> usernames() const;

 private:
  Result<void> save() const;

  FileStore file_;
  SeededRng& rng_;
  unsigned iterations_;
  std::map<std::string, User> users_;
};

}  // namespace tms
