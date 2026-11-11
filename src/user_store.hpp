#pragma once
// Issue #4: persistent user store. File format, one user per line:
//   username|role|saltHex|hashHex|iterations[|inactive]
// The optional 6th field is written only for deactivated accounts (issue #12), so files
// written before that change still load.
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
  bool active = true;  // false = deactivated: cannot log in, record is kept (FR-07)
};

class UserStore {
 public:
  UserStore(FileStore file, SeededRng& rng, unsigned iterations = kDefaultIterations);

  Result<void> load();  // missing file => empty store
  Result<void> addUser(const std::string& username, const std::string& password, Role role);
  Result<void> removeUser(const std::string& username);
  Result<void> setActive(const std::string& username, bool active);  // FR-07 deactivate/reactivate
  Result<Role> authenticate(const std::string& username, const std::string& password) const;

  bool exists(const std::string& username) const { return users_.count(username) != 0; }
  bool isActive(const std::string& username) const;
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
