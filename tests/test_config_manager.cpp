#include <cstdio>
#include <fstream>
#include <sstream>
#include "app/config_manager.h"
#include "fakes.h"
#include "test_framework.h"

namespace {
bool isDefault(const ConfigManager& c) {
    return c.seed() == 1u && c.moderateAt() == 5u && c.highAt() == 15u &&
           c.sessionTimeoutMin() == 10 && c.speed() == 1 && c.intersections().size() == 0u;
}
Result<void> parseText(ConfigManager& c, const std::string& text) {
    std::istringstream in(text);
    return c.parse(in);
}
}

TEST(config_defaults) {
    ConfigManager c;
    CHECK(isDefault(c));
}

TEST(config_seed_range) {
    ConfigManager c;
    CHECK(c.setSeed(0).ok());
    CHECK(c.setSeed(2147483647LL).ok());
    CHECK_EQ(c.seed(), 2147483647u);
    CHECK_EQ(c.setSeed(-1).error, ErrorCode::OutOfRange);
    CHECK_EQ(c.setSeed(2147483648LL).error, ErrorCode::OutOfRange);
    CHECK_EQ(c.seed(), 2147483647u);                       // failed set keeps old value
}

TEST(config_thresholds_must_be_ordered) {
    ConfigManager c;
    CHECK(c.setThresholds(3, 8).ok());
    CHECK_EQ(c.moderateAt(), 3u);
    CHECK_EQ(c.highAt(), 8u);
    CHECK_EQ(c.setThresholds(0, 8).error, ErrorCode::OutOfRange);
    CHECK_EQ(c.setThresholds(8, 8).error, ErrorCode::OutOfRange);
    CHECK_EQ(c.setThresholds(9, 8).error, ErrorCode::OutOfRange);
    CHECK_EQ(c.setThresholds(5, 1001).error, ErrorCode::OutOfRange);
    CHECK_EQ(c.moderateAt(), 3u);                          // unchanged
}

TEST(config_timeout_and_speed) {
    ConfigManager c;
    CHECK(c.setSessionTimeoutMin(1).ok());
    CHECK(c.setSessionTimeoutMin(60).ok());
    CHECK_EQ(c.setSessionTimeoutMin(0).error, ErrorCode::OutOfRange);
    CHECK_EQ(c.setSessionTimeoutMin(61).error, ErrorCode::OutOfRange);
    CHECK_EQ(c.sessionTimeoutMin(), 60);
    CHECK(c.setSpeed(1).ok());
    CHECK(c.setSpeed(10).ok());
    CHECK(c.setSpeed(60).ok());
    CHECK_EQ(c.setSpeed(5).error, ErrorCode::OutOfRange);
    CHECK_EQ(c.setSpeed(0).error, ErrorCode::OutOfRange);
    CHECK_EQ(c.speed(), 60);
}

TEST(config_round_trip_keeps_everything) {
    ConfigManager a;
    CHECK(a.setSeed(12345).ok());
    CHECK(a.setThresholds(4, 12).ok());
    CHECK(a.setSessionTimeoutMin(25).ok());
    CHECK(a.setSpeed(10).ok());
    IntersectionDef d = makeDef(0, 3, 10, 45);
    d.name = "MG Road & 5th Cross";
    d.arrivalPerMin = {10, 22, 60};
    CHECK(a.intersections().add(d).ok());
    CHECK(a.intersections().add(makeDef(0, 2, 7)).ok());

    std::ostringstream out;
    a.serialize(out);
    ConfigManager b;
    CHECK(parseText(b, out.str()).ok());
    CHECK_EQ(b.seed(), 12345u);
    CHECK_EQ(b.moderateAt(), 4u);
    CHECK_EQ(b.highAt(), 12u);
    CHECK_EQ(b.sessionTimeoutMin(), 25);
    CHECK_EQ(b.speed(), 10);
    CHECK_EQ(b.intersections().size(), 2u);
    const IntersectionDef* r = b.intersections().find(1);
    CHECK(r != nullptr);
    CHECK_EQ(r->name, "MG Road & 5th Cross");
    CHECK_EQ(r->lanes, 3);
    CHECK_EQ(r->greenSeconds, 45);
    CHECK(r->arrivalPerMin == (std::vector<int>{10, 22, 60}));
}

TEST(config_adaptive_is_off_by_default_and_round_trips) {
    ConfigManager a;
    CHECK(!a.adaptive());
    a.setAdaptive(true);
    std::ostringstream out;
    a.serialize(out);
    ConfigManager b;
    CHECK(parseText(b, out.str()).ok());
    CHECK(b.adaptive());
    CHECK(parseText(b, "seed=5\n").ok());       // config files without the key stay valid
    CHECK(!b.adaptive());
    CHECK_EQ(parseText(b, "adaptive=2\n").error, ErrorCode::ConfigCorrupt);
    CHECK_EQ(parseText(b, "adaptive=yes\n").error, ErrorCode::ConfigCorrupt);
}

TEST(config_comments_blank_lines_and_crlf_are_fine) {
    ConfigManager c;
    CHECK(parseText(c, "# comment\r\n\r\nseed = 77\r\n  speed=60  \r\n").ok());
    CHECK_EQ(c.seed(), 77u);
    CHECK_EQ(c.speed(), 60);
}

TEST(config_partial_file_keeps_defaults_for_missing_keys) {
    ConfigManager c;
    CHECK(parseText(c, "seed=5\n").ok());
    CHECK_EQ(c.seed(), 5u);
    CHECK_EQ(c.highAt(), 15u);
}

TEST(config_corrupt_file_falls_back_to_defaults_with_e_cfg_001) {
    const char* bad[] = {
        "seed=abc\n",
        "seed=-4\n",
        "seed=2147483648\n",
        "speed=5\n",
        "timeout_min=0\n",
        "unknown_key=1\n",
        "no equals sign\n",
        "moderate=10\nhigh=5\n",
        "intersection=1|Main|4|30|10,20,30\n",        // 3 rates for 4 lanes
        "intersection=1|Main|9|30|1,1,1,1,1,1,1,1,1\n",
        "intersection=1|Main|2|30|10,61\n",
        "intersection=1|Main|2|3|10,10\n",
        "intersection=1|Main|2\n",
        "intersection=1|A|1|30|5\nintersection=1|B|1|30|5\n"   // duplicate id
    };
    for (const char* text : bad) {
        ConfigManager c;
        CHECK(c.setSeed(99).ok());                     // prove old values are discarded
        const Result<void> r = parseText(c, text);
        CHECK_EQ(r.error, ErrorCode::ConfigCorrupt);
        CHECK(isDefault(c));
    }
}

TEST(config_too_many_intersections_is_corrupt) {
    std::string text;
    for (int i = 1; i <= 11; ++i) text += "intersection=" + std::to_string(i) + "|X|1|30|5\n";
    ConfigManager c;
    CHECK_EQ(parseText(c, text).error, ErrorCode::ConfigCorrupt);
    CHECK(isDefault(c));
}

TEST(config_overlong_line_is_corrupt) {
    ConfigManager c;
    CHECK_EQ(parseText(c, "seed=" + std::string(2000, '1') + "\n").error, ErrorCode::ConfigCorrupt);
}

TEST(config_build_sim_config_for_the_engine) {
    ConfigManager c;
    CHECK(c.setSeed(42).ok());
    CHECK(c.setThresholds(3, 9).ok());
    CHECK(c.intersections().add(makeDef(0, 2, 15)).ok());
    const SimConfig s = c.buildSimConfig();
    CHECK_EQ(s.seed, 42u);
    CHECK_EQ(s.moderateAt, 3u);
    CHECK_EQ(s.highAt, 9u);
    CHECK_EQ(s.intersections.size(), 1u);
}

TEST(config_file_round_trip_and_missing_file) {
    const std::string path = "test_config_1.ini";
    std::remove(path.c_str());
    ConfigManager a;
    CHECK(a.setSeed(321).ok());
    CHECK(a.saveFile(path).ok());
    ConfigManager b;
    CHECK(b.loadFile(path).ok());
    CHECK_EQ(b.seed(), 321u);
    std::remove(path.c_str());
    ConfigManager c;
    CHECK_EQ(c.loadFile("no_such_dir_xyz/config.ini").error, ErrorCode::IoFailure);
    CHECK_EQ(a.saveFile("no_such_dir_xyz/config.ini").error, ErrorCode::IoFailure);
}

TEST(config_corrupt_file_is_not_overwritten_by_load) {
    const std::string path = "test_config_2.ini";
    { std::ofstream f(path); f << "seed=oops\n"; }
    ConfigManager c;
    CHECK_EQ(c.loadFile(path).error, ErrorCode::ConfigCorrupt);
    std::ifstream f(path);
    std::string line;
    std::getline(f, line);
    CHECK_EQ(line, "seed=oops");                       // original kept for the user to repair
    f.close();
    std::remove(path.c_str());
}
