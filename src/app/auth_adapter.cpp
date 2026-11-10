#include "app/auth_adapter.h"

namespace {

ErrorCode mapError(tms::ErrorCode c) {
    switch (c) {
        case tms::ErrorCode::AuthFailed:      return ErrorCode::AuthInvalid;
        case tms::ErrorCode::LockedOut:       return ErrorCode::AuthLocked;
        case tms::ErrorCode::SessionExpired:  return ErrorCode::SessionExpired;
        case tms::ErrorCode::InvalidArgument: return ErrorCode::BadLength;
        case tms::ErrorCode::IoError:
        case tms::ErrorCode::Corrupt:         return ErrorCode::IoFailure;
        default:                              return ErrorCode::Internal;
    }
}

ErrorCode mapUserError(tms::ErrorCode c) {                      // errors of "create user"
    switch (c) {
        case tms::ErrorCode::InvalidArgument:
        case tms::ErrorCode::WeakPassword:
        case tms::ErrorCode::AlreadyExists:
        case tms::ErrorCode::NotFound:        return ErrorCode::OutOfRange;     // E-VAL-001
        default:                              return mapError(c);
    }
}

Role fromTms(tms::Role r) {
    switch (r) {
        case tms::Role::Admin:    return Role::Admin;
        case tms::Role::Operator: return Role::Operator;
        case tms::Role::Commuter: return Role::Commuter;
    }
    return Role::Commuter;
}

tms::Role toTms(Role r) {
    switch (r) {
        case Role::Admin:    return tms::Role::Admin;
        case Role::Operator: return tms::Role::Operator;
        case Role::Commuter: return tms::Role::Commuter;
    }
    return tms::Role::Commuter;
}

}  // namespace

AuthAdapter::AuthAdapter(tms::AuthService& auth, tms::UserStore& users, tms::SimClock& clock,
                         IAuditLogger& audit, WallClock wall)
    : auth_(auth), users_(users), clock_(clock), audit_(audit), wall_(std::move(wall)) {}

void AuthAdapter::sync() const { clock_.set(wall_()); }

Result<Session> AuthAdapter::login(const std::string& user, const std::string& password) {
    sync();
    const auto r = auth_.login(user, password);
    if (!r.ok()) return Result<Session>::failure(mapError(r.error().code));
    ids_[r.value().username] = r.value().id;
    Session s;
    s.username = r.value().username;
    s.role = fromTms(r.value().role);
    s.expiry = static_cast<TimeSec>(clock_.now() + auth_.sessionTimeout());
    return Result<Session>::success(s);
}

void AuthAdapter::logout(Session& session) {
    sync();
    const auto it = ids_.find(session.username);
    if (it != ids_.end()) {
        auth_.logout(it->second);
        ids_.erase(it);
    }
    session.expiry = 0;
}

bool AuthAdapter::isExpired(const Session& session) const {
    sync();
    const auto it = ids_.find(session.username);
    if (it == ids_.end()) return true;
    return !auth_.validateSession(it->second).ok();
}

bool AuthAdapter::hasPermission(const Session& session, Command cmd) const {
    return ids_.count(session.username) != 0 && roleAllows(session.role, cmd);
}

void AuthAdapter::touch(Session& session) {
    sync();
    const auto it = ids_.find(session.username);
    if (it != ids_.end()) (void)auth_.validateSession(it->second);
    session.expiry = static_cast<TimeSec>(clock_.now() + auth_.sessionTimeout());
}

Result<void> AuthAdapter::createUser(const Session& session, const NewUser& user) {
    if (isExpired(session)) return Result<void>::failure(ErrorCode::SessionExpired);
    if (session.role != Role::Admin) {
        (void)audit_.log(session.username, "DENIED:ManageUsers", errorCodeId(ErrorCode::PermissionDenied));
        return Result<void>::failure(ErrorCode::PermissionDenied);
    }
    if (user.role == Role::Commuter) return Result<void>::failure(ErrorCode::OutOfRange);   // Operator or Admin only

    const auto added = users_.addUser(user.username, user.password, toTms(user.role));
    if (!added.ok()) {
        const ErrorCode e = mapUserError(added.error().code);
        (void)audit_.log(session.username, "USER_CREATE", errorCodeId(e));
        return Result<void>::failure(e);
    }
    (void)audit_.log(session.username, "USER_CREATE",
                     "OK username=" + user.username + " role=" + tms::roleToString(toTms(user.role)));
    return Result<void>::success();
}

void AuthAdapter::setSessionTimeoutMinutes(int minutes) {
    auth_.setSessionTimeout(static_cast<tms::SimClock::Tick>(minutes) * 60U);
}
