// Story #12 (FR-07): deactivate and delete accounts. Store, auth service and adapter level.
// Whole-program (menu) tests for the same story are in test_application.cpp.
#include <filesystem>

#include "app/auth_adapter.h"
#include "auth.hpp"
#include "fakes.h"
#include "file_store.hpp"
#include "rng.hpp"
#include "sim_clock.hpp"
#include "test_framework.h"
#include "user_store.hpp"

namespace fs = std::filesystem;

namespace {

struct UmRig {
    std::string path = (fs::temp_directory_path() / "tms_um_users.db").string();
    tms::SimClock clock;
    tms::SeededRng rng{31};
    tms::UserStore users;
    tms::AuthService auth;
    FakeAudit audit;
    std::uint64_t wall{100};
    AuthAdapter adapter;

    UmRig()
        : users((tms::FileStore(path).remove(), tms::FileStore(path)), rng, 5),
          auth(users, clock, rng),
          adapter(auth, users, clock, audit, [this]() { return wall; }) {
        users.load();
        users.addUser("admin1", "Admin1234", tms::Role::Admin);
        users.addUser("oper1", "Oper1234", tms::Role::Operator);
    }
    ~UmRig() { tms::FileStore(path).remove(); }

    Session login(const std::string& u, const std::string& p) {
        const auto r = adapter.login(u, p);
        CHECK(r.ok());
        return r.ok() ? *r.value : Session{};
    }
};

}  // namespace

TEST(um_store_deactivated_user_cannot_authenticate_but_record_is_kept) {
    UmRig r;
    CHECK(r.users.isActive("oper1"));
    CHECK(r.users.setActive("oper1", false).ok());
    CHECK(r.users.exists("oper1") && !r.users.isActive("oper1"));
    CHECK(r.users.authenticate("oper1", "Oper1234").error().code == tms::ErrorCode::AuthFailed);
    CHECK(r.users.setActive("oper1", true).ok());                       // reversible at store level
    CHECK(r.users.authenticate("oper1", "Oper1234").ok());
    CHECK(r.users.setActive("ghost", false).error().code == tms::ErrorCode::NotFound);
    CHECK(!r.users.isActive("ghost"));
}

TEST(um_store_inactive_flag_survives_restart_and_old_files_still_load) {
    UmRig r;
    CHECK(r.users.setActive("oper1", false).ok());
    {
        tms::UserStore again(tms::FileStore(r.path), r.rng, 5);
        CHECK(again.load().ok());
        CHECK(again.isActive("admin1") && !again.isActive("oper1"));
    }
    auto lines = tms::FileStore(r.path).readLines();               // 5 fields = active (pre-#12 format)
    CHECK(lines.ok());
    bool activeHasFiveFields = false, inactiveHasSix = false;
    for (const auto& l : lines.value()) {
        int bars = 0;
        for (char c : l) bars += (c == '|');
        if (l.rfind("admin1|", 0) == 0) activeHasFiveFields = (bars == 4);
        if (l.rfind("oper1|", 0) == 0) inactiveHasSix = (bars == 5 && l.size() > 9 && l.substr(l.size() - 9) == "|inactive");
    }
    CHECK(activeHasFiveFields && inactiveHasSix);
    tms::FileStore(r.path).writeLines({"oper1|operator|00|00|5|bogus"});   // unknown 6th field is corrupt
    tms::UserStore bad(tms::FileStore(r.path), r.rng, 5);
    CHECK(bad.load().error().code == tms::ErrorCode::Corrupt);
}

TEST(um_adapter_deactivate_blocks_login_ends_session_and_is_audited) {
    UmRig r;
    Session admin = r.login("admin1", "Admin1234");
    Session oper = r.login("oper1", "Oper1234");
    CHECK(!r.adapter.isExpired(oper));
    CHECK(r.adapter.deactivateUser(admin, "oper1").ok());
    CHECK(r.adapter.isExpired(oper));                                   // open session ended at once
    CHECK(!r.adapter.isActive("oper1"));
    CHECK(r.adapter.login("oper1", "Oper1234").error == ErrorCode::AuthInvalid);   // E-AUTH-001, nothing revealed
    CHECK(r.audit.has("USER_DEACTIVATE"));
    CHECK(r.audit.entries.back().result.find("OK username=oper1") != std::string::npos);
    CHECK(r.adapter.deactivateUser(admin, "oper1").ok());               // already inactive: harmless
}

TEST(um_adapter_delete_removes_account_and_allows_same_name_again) {
    UmRig r;
    Session admin = r.login("admin1", "Admin1234");
    CHECK(r.adapter.deleteUser(admin, "oper1").ok());
    CHECK(!r.users.exists("oper1"));
    CHECK(r.adapter.login("oper1", "Oper1234").error == ErrorCode::AuthInvalid);
    CHECK(r.audit.has("USER_DELETE"));
    NewUser nu; nu.username = "oper1"; nu.password = "Other1234"; nu.role = Role::Operator;
    CHECK(r.adapter.createUser(admin, nu).ok());                        // name is free again
    CHECK(r.adapter.login("oper1", "Other1234").ok());
}

TEST(um_adapter_rules_unknown_user_self_and_permission_and_expired_session) {
    UmRig r;
    Session admin = r.login("admin1", "Admin1234");
    Session oper = r.login("oper1", "Oper1234");
    CHECK(r.adapter.deactivateUser(admin, "nobody").error == ErrorCode::OutOfRange);
    CHECK(r.adapter.deleteUser(admin, "nobody").error == ErrorCode::OutOfRange);
    CHECK(r.adapter.deactivateUser(admin, "admin1").error == ErrorCode::OutOfRange);   // not yourself
    CHECK(r.adapter.deleteUser(admin, "admin1").error == ErrorCode::OutOfRange);
    CHECK(r.users.exists("admin1") && r.users.isActive("admin1"));       // the last admin is still there
    CHECK(r.adapter.deactivateUser(oper, "admin1").error == ErrorCode::PermissionDenied);
    CHECK(r.adapter.deleteUser(oper, "admin1").error == ErrorCode::PermissionDenied);
    CHECK(r.audit.has("DENIED:ManageUsers"));
    r.wall += 3600;                                                      // both sessions time out (10 min)
    CHECK(r.adapter.deactivateUser(admin, "oper1").error == ErrorCode::SessionExpired);
    CHECK(r.adapter.deleteUser(admin, "oper1").error == ErrorCode::SessionExpired);
    CHECK(r.users.isActive("oper1"));
}

TEST(um_auth_revoke_clears_failed_login_state_of_a_deleted_user) {
    UmRig r;
    for (int i = 0; i < 3; ++i) CHECK(!r.auth.login("oper1", "wrongPass1").ok());
    CHECK(r.auth.isLocked("oper1"));
    r.auth.revokeUser("oper1");
    CHECK(!r.auth.isLocked("oper1"));                                    // a re-created account starts clean
}

TEST(um_auth_session_of_a_user_removed_behind_its_back_is_refused) {
    UmRig r;
    const auto s = r.auth.login("oper1", "Oper1234");
    CHECK(s.ok());
    CHECK(r.users.setActive("oper1", false).ok());                       // no revokeUser call
    CHECK(r.auth.validateSession(s.value().id).error().code == tms::ErrorCode::SessionExpired);
    CHECK_EQ(r.auth.activeSessions(), static_cast<std::size_t>(0));
}
