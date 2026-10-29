#pragma once
// Command Dispatcher (story #6, SEC-01, SEC-05). One central place where
// session expiry and role permission are checked for EVERY command.
// A denied command changes nothing and is written to the audit log.
#include "common/interfaces.h"

class Dispatcher {
public:
    Dispatcher(IAuthService& auth, IAuditLogger& audit);

    // session == nullptr means "not logged in" (only public Commuter commands pass).
    // Ok            -> run the command (inactivity timer is refreshed)
    // SessionExpired / PermissionDenied -> refuse, audit, change nothing
    Result<void> authorize(Session* session, Command cmd);

private:
    IAuthService& auth_;
    IAuditLogger& audit_;
};
