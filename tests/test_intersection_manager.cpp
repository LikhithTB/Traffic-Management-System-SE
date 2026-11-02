#include "fakes.h"
#include "sim/intersection_manager.h"
#include "test_framework.h"

TEST(intersections_add_assigns_incrementing_ids) {
    IntersectionManager m;
    auto a = m.add(makeDef(0, 4, 20));
    auto b = m.add(makeDef(0, 2, 30));
    CHECK(a.ok() && b.ok());
    CHECK_EQ(*a.value, 1);
    CHECK_EQ(*b.value, 2);
    CHECK_EQ(m.size(), 2u);
    CHECK(m.find(2) != nullptr);
    CHECK(m.find(3) == nullptr);
}

TEST(intersections_add_validates_every_field_and_changes_nothing_on_error) {
    IntersectionManager m;
    CHECK_EQ(m.add(makeDef(0, 0, 20)).error, ErrorCode::OutOfRange);     // 0 lanes
    CHECK_EQ(m.add(makeDef(0, 9, 20)).error, ErrorCode::OutOfRange);     // 9 lanes
    CHECK_EQ(m.add(makeDef(0, 2, 0)).error, ErrorCode::OutOfRange);      // rate 0
    CHECK_EQ(m.add(makeDef(0, 2, 61)).error, ErrorCode::OutOfRange);     // rate 61
    CHECK_EQ(m.add(makeDef(0, 2, 20, 4)).error, ErrorCode::OutOfRange);  // green 4
    CHECK_EQ(m.add(makeDef(0, 2, 20, 121)).error, ErrorCode::OutOfRange);// green 121
    IntersectionDef mismatch = makeDef(0, 3, 20);
    mismatch.arrivalPerMin.pop_back();
    CHECK_EQ(m.add(mismatch).error, ErrorCode::OutOfRange);              // rates != lanes
    CHECK_EQ(m.size(), 0u);
    CHECK(m.add(makeDef(0, 8, 60, 120)).ok());                           // limits themselves are fine
    CHECK(m.add(makeDef(0, 1, 1, 5)).ok());
}

TEST(intersections_name_rules) {
    IntersectionManager m;
    IntersectionDef d = makeDef(0, 2, 20);
    d.name = "";                          CHECK_EQ(m.add(d).error, ErrorCode::BadLength);
    d.name = "   ";                       CHECK_EQ(m.add(d).error, ErrorCode::BadLength);
    d.name = std::string(41, 'x');        CHECK_EQ(m.add(d).error, ErrorCode::BadLength);
    d.name = "Bad|Name";                  CHECK_EQ(m.add(d).error, ErrorCode::BadLength);
    d.name = "Line\nBreak";               CHECK_EQ(m.add(d).error, ErrorCode::BadLength);
    d.name = std::string(40, 'x');        CHECK(m.add(d).ok());
}

TEST(intersections_maximum_is_ten) {
    IntersectionManager m;
    for (int i = 0; i < 10; ++i) CHECK(m.add(makeDef(0, 2, 10)).ok());
    CHECK_EQ(m.add(makeDef(0, 2, 10)).error, ErrorCode::CapacityExceeded);
    CHECK_EQ(m.size(), 10u);
    CHECK(m.remove(5).ok());
    CHECK(m.add(makeDef(0, 2, 10)).ok());                                // room again
    CHECK(m.find(11) != nullptr);                                        // ids are not reused
}

TEST(intersections_duplicate_explicit_id_rejected) {
    IntersectionManager m;
    CHECK(m.add(makeDef(7, 2, 10)).ok());
    CHECK_EQ(m.add(makeDef(7, 2, 10)).error, ErrorCode::OutOfRange);
    CHECK_EQ(m.add(makeDef(-1, 2, 10)).error, ErrorCode::OutOfRange);
    auto next = m.add(makeDef(0, 2, 10));
    CHECK_EQ(*next.value, 8);                                            // auto id continues after 7
}

TEST(intersections_edit_keeps_id_and_validates) {
    IntersectionManager m;
    CHECK(m.add(makeDef(0, 2, 10)).ok());
    IntersectionDef changed = makeDef(99, 4, 25, 45);
    changed.name = "New name";
    CHECK(m.edit(1, changed).ok());
    const IntersectionDef* d = m.find(1);
    CHECK(d != nullptr);
    CHECK_EQ(d->id, 1);                                                  // id not changed by edit
    CHECK_EQ(d->lanes, 4);
    CHECK_EQ(d->greenSeconds, 45);
    CHECK_EQ(d->name, "New name");
    CHECK_EQ(m.edit(1, makeDef(1, 9, 25)).error, ErrorCode::OutOfRange);
    CHECK_EQ(m.find(1)->lanes, 4);                                       // failed edit changed nothing
    CHECK_EQ(m.edit(42, makeDef(42, 2, 10)).error, ErrorCode::OutOfRange);
}

TEST(intersections_remove) {
    IntersectionManager m;
    CHECK(m.add(makeDef(0, 2, 10)).ok());
    CHECK(m.add(makeDef(0, 2, 10)).ok());
    CHECK(m.remove(1).ok());
    CHECK_EQ(m.size(), 1u);
    CHECK(m.find(1) == nullptr);
    CHECK_EQ(m.remove(1).error, ErrorCode::OutOfRange);
    m.clear();
    CHECK_EQ(m.size(), 0u);
    CHECK_EQ(*m.add(makeDef(0, 2, 10)).value, 1);                        // clear restarts ids
}
