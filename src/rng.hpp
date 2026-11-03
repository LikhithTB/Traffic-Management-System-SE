#pragma once
// Issue #2: seeded RNG (splitmix64). Same seed => same sequence on every platform
// (unlike std distributions, whose output is implementation-defined).
// NOTE: fine for a simulation; a real system would use an OS CSPRNG for salts/session ids.
#include <cstdint>

namespace tms {

class SeededRng {
 public:
  explicit SeededRng(std::uint64_t seed = 1) : seed_(seed), state_(seed) {}

  std::uint64_t next() {
    state_ += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = state_;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }
  // Inclusive range [lo, hi]; requires lo <= hi.
  int uniformInt(int lo, int hi) {
    std::uint64_t span = static_cast<std::uint64_t>(static_cast<long long>(hi) - lo) + 1;
    return static_cast<int>(lo + static_cast<long long>(next() % span));
  }
  double uniform01() { return static_cast<double>(next() >> 11) / 9007199254740992.0; }
  std::uint64_t seed() const { return seed_; }
  void reseed(std::uint64_t seed) { seed_ = seed; state_ = seed; }

 private:
  std::uint64_t seed_;
  std::uint64_t state_;
};

}  // namespace tms
