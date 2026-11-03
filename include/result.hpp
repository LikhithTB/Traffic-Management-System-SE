#pragma once
// Issue #2: Result<T> - explicit success/error return type (no exceptions in app code).
#include <optional>
#include <string>
#include <utility>

namespace tms {

enum class ErrorCode {
  InvalidArgument,
  NotFound,
  AlreadyExists,
  IoError,
  Corrupt,
  WeakPassword,
  AuthFailed,
  LockedOut,
  SessionExpired
};

struct Error {
  ErrorCode code;
  std::string message;
};

template <typename T>
class Result {
 public:
  static Result Ok(T value) {
    Result r;
    r.value_ = std::move(value);
    return r;
  }
  static Result Err(ErrorCode code, std::string message) {
    Result r;
    r.error_ = Error{code, std::move(message)};
    return r;
  }
  bool ok() const { return value_.has_value(); }
  explicit operator bool() const { return ok(); }
  const T& value() const { return *value_; }
  T& value() { return *value_; }
  const Error& error() const { return error_; }

 private:
  Result() = default;
  std::optional<T> value_;
  Error error_{ErrorCode::InvalidArgument, ""};
};

template <>
class Result<void> {
 public:
  static Result Ok() { return Result(true); }
  static Result Err(ErrorCode code, std::string message) {
    Result r(false);
    r.error_ = Error{code, std::move(message)};
    return r;
  }
  bool ok() const { return ok_; }
  explicit operator bool() const { return ok_; }
  const Error& error() const { return error_; }

 private:
  explicit Result(bool ok) : ok_(ok) {}
  bool ok_;
  Error error_{ErrorCode::InvalidArgument, ""};
};

// Re-wrap a failed result as a different Result type.
template <typename U, typename R>
Result<U> Propagate(const R& failed) {
  return Result<U>::Err(failed.error().code, failed.error().message);
}

}  // namespace tms
