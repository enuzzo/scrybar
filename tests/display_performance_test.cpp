#include "../src/display_performance.h"
#include <cassert>
#include <cmath>
#include <array>

static void checkWallClockDelta() {
  DisplayPerformance::WallClockDelta clock;
  assert(clock.step(100) == 0);  // Unsynchronised origin invents no elapsed ms.
  assert(clock.step(100) == 0);
  assert(clock.step(116) == 16);
  assert(clock.step(377) == 261); // A blocked HTTP call must not lose 211 ms.
  assert(clock.step(377) == 0);
  assert(clock.step(393) == 16);
  assert(clock.previous == 393);

  clock.reset(100);
  const uint32_t samples[] = {100, 116, 377, 377, 393};
  uint32_t total = 0;
  for (uint32_t now : samples) total += clock.step(now);
  assert(total == 293); // Partitioning calls cannot change wall-clock time.

  clock.reset(UINT32_MAX - 7);
  assert(clock.step(5) == 13);
  assert(clock.step(5) == 0);
  assert(clock.step(100005) == 100000); // No clamp on long forward gaps.
  clock.reset(100005); // Preparation synchronises the origin once.
  assert(clock.step(100005) == 0);
  assert(clock.step(100018) == 13);
}

static std::array<uint64_t, 8> pacingState(const DisplayPerformance::AbsoluteFramePacer &p) {
  return {{p.initialized, p.nextDeadlineUs, p.slots, p.lateSlots,
           p.lastLatenessUs, p.maxLatenessUs, p.totalLatenessUs, p.missedPeriods}};
}

static void checkAbsoluteFramePacer() {
  using Pacer = DisplayPerformance::AbsoluteFramePacer;
  const uint32_t period = Pacer::periodUs;
  Pacer p;
  const auto uninitialized = pacingState(p);
  assert(!p.due(0) && p.timeUntilUs(0) == 0 && !p.advance(0));
  assert(pacingState(p) == uninitialized);

  p.reset(100);
  assert(!p.due(99) && p.timeUntilUs(99) == 1);
  assert(p.due(100) && p.timeUntilUs(100) == 0);
  assert(p.due(101) && p.timeUntilUs(101) == 0);
  const auto notYetConsumed = pacingState(p);
  assert(!p.advance(99));
  assert(!p.advance(100, 99)); // A completion before admission is invalid.
  assert(pacingState(p) == notYetConsumed);
  assert(p.advance(100, 13100));
  assert(p.nextDeadlineUs == 100 + period);
  assert(p.slots == 1 && p.lateSlots == 0 && p.missedPeriods == 0);
  assert(p.lastLatenessUs == 0 && p.totalLatenessUs == 0);
  assert(p.timeUntilUs(13100) == period - 13000);
  assert(!p.due(13100));

  // Repeated 1 ms late polls stay attached to the original epoch. The normal
  // 13 ms frame workload is not falsely recorded as admission lateness.
  p.reset(0);
  for (uint32_t frame = 0; frame < 120; ++frame) {
    const uint32_t now = frame * period + 1000;
    assert(p.due(now));
    assert(p.advance(now, now + 13000));
    assert(p.nextDeadlineUs == (frame + 1) * period);
    assert(p.lastLatenessUs == 1000 && p.maxLatenessUs == 1000);
    assert(!p.due(now + 13000));
    assert(p.timeUntilUs(now + 13000) == period - 14000);
  }
  assert(p.slots == 120 && p.lateSlots == 120);
  assert(p.totalLatenessUs == 120000 && p.missedPeriods == 0);

  // Missing several deadlines admits one current frame and skips expired
  // slots. Polling again at the same time cannot trigger a catch-up burst.
  p.reset(500);
  const uint32_t late = 500 + 3 * period + 7;
  assert(p.advance(late));
  assert(p.nextDeadlineUs == 500 + 4 * period);
  assert(p.slots == 1 && p.lateSlots == 1 && p.missedPeriods == 3);
  assert(p.lastLatenessUs == 3 * period + 7);
  const auto afterLate = pacingState(p);
  assert(!p.advance(late) && !p.due(late));
  assert(pacingState(p) == afterLate);
  assert(p.timeUntilUs(late) == period - 7);

  // Exact-boundary lateness still consumes just one slot; it does not leave
  // an immediately due historical slot behind.
  p.reset(0);
  assert(p.advance(period));
  assert(p.nextDeadlineUs == 2 * period && p.missedPeriods == 1);
  assert(!p.due(period));

  // A workload finishing on or just after the next deadline keeps that slot.
  // One immediate admission recovers the epoch instead of forcing a 33 ms gap.
  p.reset(0);
  assert(p.advance(0, period));
  assert(p.nextDeadlineUs == period && p.missedPeriods == 0 && p.due(period));
  assert(p.advance(period, 2 * period + 1));
  assert(p.nextDeadlineUs == 2 * period && p.missedPeriods == 0);
  assert(p.totalLatenessUs == 0 && p.lateSlots == 0);
  assert(p.due(2 * period + 1));
  assert(p.advance(2 * period + 1, 3 * period - 100));
  assert(p.nextDeadlineUs == 3 * period && p.missedPeriods == 0);
  assert(!p.due(3 * period - 100));
  assert(p.lastLatenessUs == 1 && p.totalLatenessUs == 1);

  p.reset(0);
  assert(p.advance(0, 17000));
  assert(p.nextDeadlineUs == period && p.due(17000));
  assert(p.timeUntilUs(17000) == 0 && p.missedPeriods == 0);
  assert(p.advance(17000, 30000));
  assert(p.nextDeadlineUs == 2 * period && !p.due(30000));
  assert(p.timeUntilUs(30000) == 3334);
  assert(p.lastLatenessUs == 333 && p.missedPeriods == 0);

  // A 40 ms frame completely misses only the 16.667 ms slot. The 33.334 ms
  // slot is still current, not an accumulated queue of historical requests.
  p.reset(0);
  assert(p.advance(0, 40000));
  assert(p.nextDeadlineUs == 2 * period && p.missedPeriods == 1);
  assert(p.due(40000) && p.timeUntilUs(40000) == 0);
  assert(p.advance(40000));
  assert(p.nextDeadlineUs == 3 * period && p.missedPeriods == 1);
  const auto afterRecovery = pacingState(p);
  assert(!p.advance(40000) && !p.due(40000));
  assert(pacingState(p) == afterRecovery);

  // Late admission itself consumes the newest slot, even when work finishes
  // before the following one; it cannot preserve an already admitted slot.
  p.reset(0);
  assert(p.advance(40000, 41000));
  assert(p.nextDeadlineUs == 3 * period && p.missedPeriods == 2);
  assert(!p.due(41000));

  // Deadlines, admission times and frame completion can all cross micros()
  // wrap. The phase and headroom remain correct on either side.
  const uint32_t origin = UINT32_MAX - 8000;
  p.reset(origin);
  assert(!p.due(origin - 1) && p.timeUntilUs(origin - 1) == 1);
  assert(p.advance(origin, uint32_t(origin + 13000)));
  const uint32_t second = origin + period;
  assert(p.nextDeadlineUs == second);
  assert(!p.due(uint32_t(second - 1)) && p.timeUntilUs(uint32_t(second - 1)) == 1);
  assert(p.due(second));
  assert(p.advance(second + 1000, second + 14000));
  assert(p.nextDeadlineUs == uint32_t(origin + 2 * period));
  assert(p.slots == 2 && p.lateSlots == 1 && p.totalLatenessUs == 1000);

  // Lateness totals must survive more than a uint32_t worth of accumulated
  // microseconds; long diagnostic runs cannot silently wrap this statistic.
  p.reset(0);
  for (unsigned i = 0; i < 10; ++i) {
    const uint32_t admitted = p.nextDeadlineUs + 1000000000U;
    assert(p.advance(admitted));
    assert(!p.due(admitted));
  }
  assert(p.totalLatenessUs == uint64_t(1000000000U) * 10);
  assert(p.maxLatenessUs == 1000000000U && p.slots == 10);
  assert(p.missedPeriods == uint64_t(1000000000U / period) * 10);
  p.reset(1234);
  assert(p.nextDeadlineUs == 1234 && p.due(1234));
  assert(p.slots == 0 && p.lateSlots == 0 && p.lastLatenessUs == 0);
  assert(p.maxLatenessUs == 0 && p.totalLatenessUs == 0 && p.missedPeriods == 0);
}

static void checkFineIntervalPercentiles() {
  using Intervals = DisplayPerformance::ActiveFrameIntervals;
  static_assert(sizeof(Intervals::fineBuckets) == 516, "Fine histogram RAM budget changed");
  Intervals timing;
  for (uint8_t percentile : {uint8_t(0), uint8_t(1), uint8_t(50), uint8_t(95), uint8_t(99), uint8_t(100), uint8_t(101), uint8_t(255)})
    assert(timing.percentileUpperUs(percentile) == 0);
  uint32_t now = 1000;
  timing.presented(now, true);
  for (unsigned i = 0; i < 100; ++i) {
    const uint32_t interval = i < 50 ? 16667 : i < 95 ? 16800 : i < 99 ? 20123 : 40001;
    now += interval;
    timing.presented(now, true);
  }
  assert(timing.samples == 100);
  assert(timing.percentileUpperUs(1) == 16750);
  assert(timing.percentileUpperUs(50) == 16750);
  assert(timing.percentileUpperUs(95) == 17000);
  assert(timing.percentileUpperUs(99) == 20250);
  assert(timing.percentileUpperUs(100) == 40001);
  assert(timing.percentileUpperUs(0) == 0 && timing.percentileUpperUs(101) == 0);
  assert(timing.percentileUpperUs(255) == 0);
  assert(timing.p95UpperUs() == 20000); // Historical coarse result is unchanged.
  uint32_t coarseCount = 0, fineCount = 0;
  for (uint32_t count : timing.buckets) coarseCount += count;
  for (uint32_t count : timing.fineBuckets) fineCount += count;
  assert(coarseCount == timing.samples && fineCount == timing.samples);
  assert(timing.fineBuckets[66] == 50 && timing.fineBuckets[67] == 45);
  assert(timing.fineBuckets[80] == 4 && timing.fineBuckets[128] == 1);

  Intervals boundaries;
  now = 777;
  boundaries.presented(now, true);
  const uint32_t lengths[] = {0, 1, 249, 250, 251, 499, 500, 501, 32000, 32001, UINT32_MAX};
  for (uint32_t interval : lengths) {
    now += interval;
    boundaries.presented(now, true);
  }
  assert(boundaries.samples == 11 && boundaries.fineBuckets[0] == 4);
  assert(boundaries.fineBuckets[1] == 3 && boundaries.fineBuckets[2] == 1);
  assert(boundaries.fineBuckets[127] == 1 && boundaries.fineBuckets[128] == 2);
  assert(boundaries.percentileUpperUs(95) == UINT32_MAX);
  assert(boundaries.percentileUpperUs(100) == UINT32_MAX);
  Intervals zero;
  zero.presented(123, true);
  zero.presented(123, true);
  assert(zero.samples == 1 && zero.fineBuckets[0] == 1);
  assert(zero.percentileUpperUs(50) == 0 && zero.percentileUpperUs(100) == 0);

  Intervals wrap;
  wrap.presented(UINT32_MAX - 1000, true);
  wrap.presented(15666, true);
  assert(wrap.samples == 1 && wrap.totalUs == 16667 && wrap.fineBuckets[66] == 1);
  assert(wrap.percentileUpperUs(50) == 16750 && wrap.percentileUpperUs(99) == 16750);
  wrap.presented(100000, false);
  wrap.presented(200000, true);
  assert(wrap.samples == 1 && wrap.fineBuckets[66] == 1); // Idle creates no bins.

  // The percentile threshold must not overflow uint32_t, even though ordinary
  // PERF windows are bounded to far fewer samples than this synthetic state.
  Intervals large;
  large.samples = UINT32_MAX;
  large.maxUs = 40000;
  large.fineBuckets[66] = UINT32_MAX - 1;
  large.fineBuckets[128] = 1;
  assert(large.percentileUpperUs(99) == 16750);
  assert(large.percentileUpperUs(100) == 40000);
}

static void checkMotionWorkTimings() {
  using Work = DisplayPerformance::MotionWorkTimings;
  struct Guarded {
    uint32_t before = 0x16A75C39;
    Work value;
    uint32_t after = 0xE92840BD;
  } guarded;
  auto &work = guarded.value;
  for (unsigned stage = 0; stage < Work::count; ++stage) {
    assert(work.calls[stage] == 0);
    assert(work.maxUs[stage] == 0);
    assert(work.over2ms[stage] == 0);
  }

  // Boundary samples count as calls, but only strictly more than 2 ms count
  // as slow work. Smaller later samples must not overwrite the maximum.
  work.record(Work::input, 0);
  work.record(Work::input, 2000);
  assert(work.calls[Work::input] == 2 && work.maxUs[Work::input] == 2000);
  assert(work.over2ms[Work::input] == 0);
  work.record(Work::input, 2001);
  work.record(Work::input, 17);
  assert(work.calls[Work::input] == 4 && work.maxUs[Work::input] == 2001);
  assert(work.over2ms[Work::input] == 1);

  // Give every stage a different count and maximum. A recorded HTTP stall
  // must not alter display/input attribution or any other stage.
  uint32_t expectedCalls[Work::count] = {};
  uint32_t expectedMax[Work::count] = {};
  uint32_t expectedOver[Work::count] = {};
  expectedCalls[Work::input] = 4;
  expectedMax[Work::input] = 2001;
  expectedOver[Work::input] = 1;
  uint32_t expectedTotalCalls = 4;
  for (unsigned index = 0; index < Work::count; ++index) {
    const auto stage = static_cast<Work::Stage>(index);
    for (unsigned sample = 0; sample <= index; ++sample) {
      const uint32_t duration = 1000 + index * 137 + sample;
      work.record(stage, duration);
      ++expectedCalls[index];
      ++expectedTotalCalls;
      if (duration > expectedMax[index]) expectedMax[index] = duration;
      if (duration > 2000) ++expectedOver[index];
    }
  }
  work.record(Work::http, 260933);
  ++expectedCalls[Work::http];
  ++expectedTotalCalls;
  expectedMax[Work::http] = 260933;
  ++expectedOver[Work::http];
  work.record(Work::diagnostics, UINT32_MAX);
  ++expectedCalls[Work::diagnostics];
  ++expectedTotalCalls;
  expectedMax[Work::diagnostics] = UINT32_MAX;
  ++expectedOver[Work::diagnostics];

  uint32_t actualTotalCalls = 0;
  for (unsigned stage = 0; stage < Work::count; ++stage) {
    assert(work.calls[stage] == expectedCalls[stage]);
    assert(work.maxUs[stage] == expectedMax[stage]);
    assert(work.over2ms[stage] == expectedOver[stage]);
    actualTotalCalls += work.calls[stage];
  }
  assert(actualTotalCalls == expectedTotalCalls);

  const Work beforeInvalid = work;
  work.record(Work::count, UINT32_MAX);
  work.record(static_cast<Work::Stage>(255), UINT32_MAX);
  work.record(static_cast<Work::Stage>(-1), UINT32_MAX);
  for (unsigned stage = 0; stage < Work::count; ++stage) {
    assert(work.calls[stage] == beforeInvalid.calls[stage]);
    assert(work.maxUs[stage] == beforeInvalid.maxUs[stage]);
    assert(work.over2ms[stage] == beforeInvalid.over2ms[stage]);
  }
  assert(guarded.before == 0x16A75C39 && guarded.after == 0xE92840BD);
}

int main() {
  using namespace DisplayPerformance;
  checkWallClockDelta();
  checkAbsoluteFramePacer();
  checkFineIntervalPercentiles();
  checkMotionWorkTimings();
  char owned[] = "LIVE";
  char equal[] = "LIVE";
  assert(!textNeedsUpdate(owned, equal));
  assert(textNeedsUpdate(owned, "STALE"));
  assert(textNeedsUpdate(owned, nullptr));
  assert(textNeedsUpdate(nullptr, "LIVE"));
  assert(textNeedsUpdate(owned, owned)); // In-place edits require LVGL refresh.
  char unicodeA[] = "\xC2\xB0" "C", unicodeB[] = "\xC2\xB0" "C";
  assert(!textNeedsUpdate(unicodeA, unicodeB));

  ActiveFrameIntervals timing;
  timing.presented(1000, true);
  timing.presented(17667, true);
  timing.presented(34334, true);
  assert(timing.samples == 2 && timing.totalUs == 33334);
  assert(std::abs(timing.framesPerSecond() - 60.0) < .01);
  assert(timing.p95UpperUs() == 16667);
  timing.presented(1000000, false);
  timing.presented(2000000, true);
  assert(timing.samples == 2); // Idle gap is excluded.
  timing.presented(2060000, true);
  assert(timing.samples == 3 && timing.maxUs == 60000);
  assert(timing.p95UpperUs() == 60000);
  timing.endActiveRun();
  timing.presented(3000000, true);
  assert(timing.samples == 3);

  ActiveFrameIntervals wrap;
  wrap.presented(UINT32_MAX - 1000, true);
  wrap.presented(15666, true);
  assert(wrap.samples == 1 && wrap.totalUs == 16667);
  ActiveFrameIntervals empty;
  assert(empty.framesPerSecond() == 0 && empty.p95UpperUs() == 0);
}
