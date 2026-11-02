#pragma once
// Intersection Manager (story #9, FR-08, UC-05). Add / edit / delete intersections:
// max 10, 1-8 lanes each, arrival rate 1-60 vehicles/min per lane, green 5-120 s.
// Changes apply to the next simulation start (the engine copies the definitions).
#include <cstddef>
#include <vector>
#include "common/result.h"
#include "common/sim_types.h"

class IntersectionManager {
public:
    static constexpr std::size_t kMaxIntersections = 10;
    static constexpr std::size_t kMaxNameLength = 40;

    // def.id == 0 -> a new id is assigned. Returns the id.
    //   more than 10 -> CapacityExceeded; bad lanes/rates/green/duplicate id -> OutOfRange;
    //   empty/too long/illegal name -> BadLength. A failed call changes nothing.
    Result<int> add(IntersectionDef def);
    Result<void> edit(int id, IntersectionDef def);     // id is kept; unknown id -> OutOfRange
    Result<void> remove(int id);                         // unknown id -> OutOfRange

    const IntersectionDef* find(int id) const;           // nullptr if unknown
    const std::vector<IntersectionDef>& list() const { return items_; }
    std::size_t size() const { return items_.size(); }
    void clear();

    static Result<void> validate(const IntersectionDef& def);

private:
    std::vector<IntersectionDef> items_;
    int nextId_{1};
};
