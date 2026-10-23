#include "../src/mac_stats_history.h"
#include <cassert>
#include <limits>

int main() {
  MacStatsHistory::History h;
  auto add = [&](uint32_t at, float down, bool known = true, const char *host = "Mac", const char *scope = "internet") {
    h.accept(at, host, scope, down, known, down / 2, known);
  };
  add(1000, 0);
  auto w = h.window(1000);
  assert(w.count == 1 && w.points[0].hasDown && w.points[0].down == 0); // measured idle, no invented past
  assert(w.points[0].x == 6000);
  add(2000, 20); add(4000, 40);
  w = h.window(4000);
  assert(w.count == 3 && w.points[0].x == 5700 && w.points[1].x == 5800);
  assert(w.points[2].x == 6000 && w.peak == 40);           // shared scale from all raw samples
  add(4100, 999, false);
  w = h.window(4100);
  assert(w.count == 4 && !w.points[3].hasDown && w.peak == 40); // explicit unknown preserved, old peak retained

  add(5000, 50, true, "Other Mac");
  assert(h.window(5000).count == 1);                       // host boundary
  add(6000, 60, true, "Other Mac", "local");
  assert(h.window(6000).count == 1);                       // scope boundary
  add(22000, 70, true, "Other Mac", "local");
  assert(h.window(22000).count == 1);                      // stale boundary clears previous session
  assert(h.window(82000).count == 1 && h.window(82001).count == 0); // exact 60-second expiry

  h.clear();
  for (uint32_t i = 0; i < 70; ++i) add(i * 1000, float(i));
  w = h.window(69000);
  assert(h.sampleCount() == 64 && w.count == 61);
  assert(w.points[0].down == 9 && w.points[60].down == 69);// bounded ring and real-time window
  uint32_t at = 0;
  MacStatsHistory::Point point;
  assert(h.newestSample(0, at, point) && at == 69000 && point.down == 69);
  assert(h.newestSample(63, at, point) && at == 6000 && point.down == 6);
  assert(!h.newestSample(64, at, point));                  // diagnostic cannot read outside retained samples

  h.clear();
  add(UINT32_MAX - 499, 10); add(500, 20);
  w = h.window(500);
  assert(w.count == 2 && w.points[0].x == 5900 && w.points[1].x == 6000); // millis rollover
  h.clear();
  add(1000, std::numeric_limits<float>::infinity());
  w = h.window(1000);
  assert(w.count == 1 && !w.points[0].hasDown && w.peak == 0); // invalid never becomes measured zero

  h.clear();
  for (uint32_t i = 0; i < 55; ++i) add(i * 1100, float(i));
  w = h.window(59400);
  assert(w.count == 55 && w.peak == 54);                   // no manufactured resampled values
  for (size_t i = 0; i < w.count; ++i) {
    assert(w.points[i].hasDown && w.points[i].down == i);
    if (i) assert(w.points[i].x - w.points[i - 1].x == 110); // actual 1.1-second spacing
  }

  h.clear();
  add(1000, 10); add(3000, 30); add(6000, 60);
  w = h.window(6000);
  assert(w.count == 3 && w.points[0].hasDown && w.points[1].hasDown); // known 2–3s delivery drops connect at true timestamps
  h.clear();
  add(0, 10); add(15000, 30);
  w = h.window(15000);
  assert(w.count == 3 && !w.points[1].hasDown && !w.points[1].hasUp); // freshness-length outage breaks both traces
  h.clear();
  add(0, 10); add(15001, 30);
  assert(h.window(15001).count == 1);                      // longer stale gap starts a new history

  h.clear();
  add(0, 0);
  h.accept(1300, "Mac", "internet", 13, true, 0, false);
  add(2600, 26);
  w = h.window(2600);
  assert(w.count == 3 && w.points[1].hasDown && !w.points[1].hasUp); // per-channel unknown remains a break
  assert(w.points[1].down == 13 && w.points[1].x == 5870);
  h.clear();
  add(0, 100); add(1300, 1000); add(1400, 0); add(2600, 0);
  w = h.window(2600);
  assert(w.count == 4 && w.points[1].down == 1000 && w.points[2].down == 0 && w.peak == 1000); // narrow peak survives close receipts

  // Actual device cadence: advancing only the clock must translate the whole
  // shape rigidly and never change values, flags or peak before any expiry.
  h.clear();
  const uint32_t intervals[] = {1042,1265,1016,1036,1042,1004,1008,1071,946,986,1032,1141,1067,1057,934,951,1277,870,1018,1062};
  at = 100000;
  for (unsigned i = 0; i < 55; ++i) {
    if (i) at += intervals[(i - 1) % 20];
    const float up = i == 37 ? 1000.f : 10.f + float(i % 7);
    h.accept(at, "Mac", "internet", 1, true, up, true);
  }
  const auto baseline = h.window(at);
  assert(baseline.count == 55 && baseline.peak == 1000);
  for (uint32_t phase = 0; phase < 1000; phase += 10) {
    w = h.window(at + phase);
    assert(w.count == baseline.count && w.peak == baseline.peak);
    for (size_t i = 0; i < w.count; ++i) {
      assert(w.points[i].down == baseline.points[i].down && w.points[i].up == baseline.points[i].up);
      assert(w.points[i].hasDown == baseline.points[i].hasDown && w.points[i].hasUp == baseline.points[i].hasUp);
      assert(w.points[i].x + phase / 10 == baseline.points[i].x);
    }
  }
}
