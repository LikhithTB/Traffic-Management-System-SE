// On-screen help (story #21, NFR-04).
#include <string>
#include "test_framework.h"
#include "ui/help.h"

namespace {
bool has(const char* text, const std::string& part) { return std::string(text).find(part) != std::string::npos; }
}

TEST(help_requests_are_recognised) {
    CHECK(help::isRequest("h"));
    CHECK(help::isRequest(" H "));
    CHECK(help::isRequest("help"));
    CHECK(help::isRequest("HELP\r"));
    CHECK(help::isRequest("?"));
    CHECK(!help::isRequest(""));
    CHECK(!help::isRequest("1"));
    CHECK(!help::isRequest("hello"));
}

TEST(help_every_screen_has_its_own_text) {
    const help::Screen all[] = {help::Screen::Main, help::Screen::Operations, help::Screen::Commuter,
                                help::Screen::Intersections, help::Screen::Parameters, help::Screen::Users};
    const char* const titles[] = {"MAIN MENU", "OPERATIONS MENU", "COMMUTER VIEW", "INTERSECTIONS",
                                  "SYSTEM PARAMETERS", "USER ACCOUNTS"};
    for (std::size_t i = 0; i < 6; ++i) {
        CHECK(has(help::text(all[i]), std::string("HELP - ") + titles[i]));
        CHECK(has(help::text(all[i]), "0  "));                    // every screen explains how to go back
    }
}

TEST(help_explains_the_tc17_task_start_and_read_lane_2) {
    CHECK(has(help::text(help::Screen::Main), "Quick start"));
    CHECK(has(help::text(help::Screen::Operations), "lane 2"));
    CHECK(has(help::text(help::Screen::Operations), "Congestion"));
}

TEST(help_names_the_limits_users_must_respect) {
    CHECK(has(help::text(help::Screen::Intersections), "1-8 lanes"));
    CHECK(has(help::text(help::Screen::Intersections), "5-120 s"));
    CHECK(has(help::text(help::Screen::Parameters), "1-60 minutes"));
    CHECK(has(help::text(help::Screen::Parameters), "max 140 s"));
    CHECK(has(help::text(help::Screen::Users), "4-20 characters"));
}
