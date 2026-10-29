#include "app/dispatcher.h"

#include "common/messages.h"

Dispatcher::Dispatcher(IAuthService& auth, IAuditLogger& audit) : auth_(auth), audit_(audit) {}

Result<void> Dispatcher::authorize(Session* session, Command cmd) {
    if (isPublicCommand(cmd)) return Result<void>::success();   // FR-10: no login needed

    const std::string action = std::string("DENIED:") + commandName(cmd);

    if (session == nullptr) {
        (void)audit_.log("anonymous", action, errorCodeId(ErrorCode::PermissionDenied));
        return Result<void>::failure(ErrorCode::PermissionDenied);
    }
    if (auth_.isExpired(*session)) {
        (void)audit_.log(session->username, "SESSION_EXPIRED", errorCodeId(ErrorCode::SessionExpired));
        return Result<void>::failure(ErrorCode::SessionExpired);
    }
    // Two independent checks: the shared table and the auth service.
    if (!roleAllows(session->role, cmd) || !auth_.hasPermission(*session, cmd)) {
        (void)audit_.log(session->username, action, errorCodeId(ErrorCode::PermissionDenied));
        return Result<void>::failure(ErrorCode::PermissionDenied);
    }
    auth_.touch(*session);
    return Result<void>::success();
}
