#pragma once
#include <optional>
#include <utility>
#include "common/error_code.h"

// Result<T>: a value or an ErrorCode (Architecture Design Spec section 3.2).
// Expected failures are returned, not thrown.
template <class T>
struct Result {
    std::optional<T> value;
    ErrorCode error{ErrorCode::Ok};

    bool ok() const { return error == ErrorCode::Ok; }
    static Result success(T v) { return Result{std::optional<T>(std::move(v)), ErrorCode::Ok}; }
    static Result failure(ErrorCode e) { return Result{std::nullopt, e}; }
};

// Result<void>: success or an ErrorCode, no value.
template <>
struct Result<void> {
    ErrorCode error{ErrorCode::Ok};

    bool ok() const { return error == ErrorCode::Ok; }
    static Result success() { return Result{ErrorCode::Ok}; }
    static Result failure(ErrorCode e) { return Result{e}; }
};
