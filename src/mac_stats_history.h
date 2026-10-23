#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "mac_stats_format.h"

// Receipt-time history only: a missing sample is a gap, never measured idle.
// Fixed storage keeps the API callback and LVGL update free of heap allocations.
namespace MacStatsHistory {
constexpr size_t kCapacity = 64;
constexpr size_t kPointCount = kCapacity * 2;  // real samples plus explicit line-break markers
constexpr uint32_t kWindowMs = 60000;
constexpr uint32_t kFreshnessMs = 15000;
constexpr uint32_t kLineBreakMs = kFreshnessMs;  // delivery may drop intermediate known samples
constexpr uint16_t kXRange = 6000;  // 10 ms units fit LVGL's signed 16-bit chart coordinates

struct Point {
  float down = 0, up = 0;
  bool hasDown = false, hasUp = false;
  uint16_t x = 0;
};
struct Window {
  Point points[kPointCount] = {};
  size_t count = 0;
  float peak = 0;
};
class History {
 public:
  void clear() { head_ = count_ = 0; }
  void accept(uint32_t atMs, const char *host, const char *scope,
              float down, bool hasDown, float up, bool hasUp) {
    if (count_ && (atMs - lastAtMs_ > kFreshnessMs ||
                   std::strcmp(host_, host) || std::strcmp(scope_, scope))) clear();
    std::snprintf(host_, sizeof(host_), "%s", host);
    std::snprintf(scope_, sizeof(scope_), "%s", scope);
    lastAtMs_ = atMs;
    samples_[head_] = {atMs, {down, up,
        hasDown && MacStatsFormat::validRate(down), hasUp && MacStatsFormat::validRate(up)}};
    head_ = (head_ + 1) % kCapacity;
    if (count_ < kCapacity) ++count_;
    ++revision_;
  }
  Window window(uint32_t nowMs) const {
    Window out;
    uint32_t previousAt = 0;
    // Preserve real timestamps and every accepted peak. Resampling against
    // NOW-relative bins changes old curve shapes when the clock phase moves.
    for (size_t n = count_; n > 0; --n) {
      const Sample &sample = samples_[(head_ + kCapacity - n) % kCapacity];
      const uint32_t age = nowMs - sample.atMs;
      if (age > kWindowMs) continue;
      const uint16_t x = (kWindowMs - age) / 10;
      if (out.count && sample.atMs - previousAt >= kLineBreakMs) {
        const uint16_t previousX = out.points[out.count - 1].x;
        out.points[out.count++].x = (previousX + x) / 2;
      }
      out.points[out.count] = sample.point;
      out.points[out.count++].x = x;
      if (sample.point.hasDown && sample.point.down > out.peak) out.peak = sample.point.down;
      if (sample.point.hasUp && sample.point.up > out.peak) out.peak = sample.point.up;
      previousAt = sample.atMs;
    }
    return out;
  }
  uint32_t revision() const { return revision_; }
  size_t sampleCount() const { return count_; }
  bool newestSample(size_t index, uint32_t &atMs, Point &point) const {
    if (index >= count_) return false;
    const Sample &sample = samples_[(head_ + kCapacity - index - 1) % kCapacity];
    atMs = sample.atMs; point = sample.point;
    return true;
  }

 private:
  struct Sample { uint32_t atMs; Point point; };
  Sample samples_[kCapacity] = {};
  size_t head_ = 0, count_ = 0;
  uint32_t lastAtMs_ = 0, revision_ = 0;
  char host_[81] = {}, scope_[9] = {};
};
}
