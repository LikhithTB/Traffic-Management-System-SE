#pragma once
// Issue #2: simulated clock. Time only moves when the simulation advances it,
// so runs are repeatable and tests never sleep. Unit: simulated seconds.
#include <cstdint>

namespace tms {

class SimClock {
 public:
  using Tick = std::uint64_t;
  Tick now() const { return now_; }
  void advance(Tick seconds) { now_ += seconds; }
  void set(Tick t) { now_ = t; }

 private:
  Tick now_ = 0;
};

}  // namespace tms
