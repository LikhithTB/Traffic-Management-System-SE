#include "app/dispatcher.h"
#include "common/messages.h"
#include "fakes.h"
#include "test_framework.h"

namespace {
Session makeSession(Role r) { Session s; s.username = "user1"; s.role = r; return s; }
}

TEST(dispatcher_public_commands_need_no_login) {
    FakeAuth auth; FakeAudit audit; Dispatcher d(auth, audit);
    CHECK(d.authorize(nullptr, Command::ViewCongestion).ok());
    CHECK(d.authorize(nullptr, Command::ViewReports).ok());
}

TEST(dispatcher_denies_anonymous_control_and_audits) {
    FakeAuth auth; FakeAudit audit; Dispatcher d(auth, audit);
    auto r = d.authorize(nullptr, Command::StartSimulation);
    CHECK_EQ(r.error, ErrorCode::PermissionDenied);
    CHECK_EQ(audit.entries.size(), 1u);
    CHECK_EQ(audit.entries[0].action, "DENIED:StartSimulation");
    CHECK_EQ(audit.entries[0].result, "E-AUTH-004");
}

TEST(dispatcher_operator_can_run_simulation_but_not_admin_functions) {
    FakeAuth auth; FakeAudit audit; Dispatcher d(auth, audit);
    Session op = makeSession(Role::Operator);
    CHECK(d.authorize(&op, Command::StartSimulation).ok());
    CHECK(d.authorize(&op, Command::SetGreenTime).ok());
    CHECK(d.authorize(&op, Command::GenerateReport).ok());
    CHECK_EQ(d.authorize(&op, Command::ManageUsers).error, ErrorCode::PermissionDenied);
    CHECK_EQ(d.authorize(&op, Command::ConfigureParameters).error, ErrorCode::PermissionDenied);
    CHECK(audit.has("DENIED:ManageUsers"));
}

TEST(dispatcher_admin_can_do_everything) {
    FakeAuth auth; FakeAudit audit; Dispatcher d(auth, audit);
    Session ad = makeSession(Role::Admin);
    CHECK(d.authorize(&ad, Command::ManageUsers).ok());
    CHECK(d.authorize(&ad, Command::ConfigureParameters).ok());
    CHECK(d.authorize(&ad, Command::StartSimulation).ok());
}

TEST(dispatcher_does_not_trust_a_too_permissive_auth_service) {
    FakeAuth auth; auth.allowAll = true;            // auth says yes to everything
    FakeAudit audit; Dispatcher d(auth, audit);
    Session op = makeSession(Role::Operator);
    CHECK_EQ(d.authorize(&op, Command::ManageUsers).error, ErrorCode::PermissionDenied);
}

TEST(dispatcher_commuter_role_cannot_control) {
    FakeAuth auth; FakeAudit audit; Dispatcher d(auth, audit);
    Session c = makeSession(Role::Commuter);
    CHECK_EQ(d.authorize(&c, Command::StartSimulation).error, ErrorCode::PermissionDenied);
}

TEST(dispatcher_expired_session_is_refused_and_audited) {
    FakeAuth auth; auth.expired = true; FakeAudit audit; Dispatcher d(auth, audit);
    Session op = makeSession(Role::Operator);
    CHECK_EQ(d.authorize(&op, Command::StartSimulation).error, ErrorCode::SessionExpired);
    CHECK(audit.has("SESSION_EXPIRED"));
    CHECK_EQ(auth.touches, 0);
}

TEST(dispatcher_successful_command_refreshes_inactivity_timer) {
    FakeAuth auth; FakeAudit audit; Dispatcher d(auth, audit);
    Session op = makeSession(Role::Operator);
    CHECK(d.authorize(&op, Command::ViewDashboard).ok());
    CHECK_EQ(auth.touches, 1);
    CHECK(audit.entries.empty());
}

TEST(messages_name_field_and_range) {
    CHECK_EQ(errorMessage(ErrorCode::OutOfRange, "Lanes", 1, 8), "Lanes must be between 1 and 8.");
    CHECK_EQ(errorMessage(ErrorCode::NotNumeric, "Lanes"), "Lanes must be a number.");
    CHECK(errorMessage(ErrorCode::BadFileName).find("..") != std::string::npos);
    CHECK_EQ(std::string(errorCodeId(ErrorCode::AuthLocked)), "E-AUTH-002");
}
