#pragma once
#include <stdint.h>
#include <string.h>

namespace DisplayPerformance {
// Bounded attribution for work that starts while a cached slide owns the panel.
// Accumulate in RAM; only the explicit/periodic PERF report prints these values.
struct MotionWorkTimings {
  enum Stage : uint8_t {
    input, serial, wifi, http, power, ntp, diagnostics, imu, feeds,
    touch, benchmark, saver, model, display, count
  };
  uint32_t calls[count] = {};
  uint32_t maxUs[count] = {};
  uint32_t over2ms[count] = {};
  void record(Stage stage, uint32_t elapsedUs) {
    if (stage >= count) return;
    ++calls[stage];
    if (elapsedUs > maxUs[stage]) maxUs[stage] = elapsedUs;
    if (elapsedUs > 2000) ++over2ms[stage];
  }
};

// Forward wall-clock deltas in the caller's wrapping uint32_t time unit. The
// first unsynchronised sample establishes the origin without inventing time.
// A long pause is returned in full; repeated timestamps return exactly zero.
struct WallClockDelta {
  bool initialized = false;
  uint32_t previous = 0;

  void reset(uint32_t now) { previous = now; initialized = true; }

  uint32_t step(uint32_t now) {
    if (!initialized) { reset(now); return 0; }
    const uint32_t elapsed = now - previous;
    previous = now;
    return elapsed;
  }
};

// Nonblocking presentation-slot pacing. Unlike a relative timer, late polling
// never moves the epoch. 16,667 us is 59.9988 Hz, within 0.002% of 60 Hz.
// due/timeUntilUs use modular comparisons: service gaps and frame work must be
// shorter than 2^31 us (about 35.8 minutes), as with other wrapping deadlines.
struct AbsoluteFramePacer {
  static constexpr uint32_t periodUs = 16667;
  bool initialized = false;
  uint32_t nextDeadlineUs = 0;
  // These count admitted slots, including a slot whose pixels did not change;
  // actual DMA presentations belong in ActiveFrameIntervals separately.
  uint32_t slots = 0;
  uint32_t lateSlots = 0;
  uint32_t lastLatenessUs = 0;
  uint32_t maxLatenessUs = 0;
  uint64_t totalLatenessUs = 0;
  uint64_t missedPeriods = 0;

  // Reset scheduling and statistics; the first slot is due at this deadline.
  void reset(uint32_t firstDeadlineUs) {
    *this = AbsoluteFramePacer();
    initialized = true;
    nextDeadlineUs = firstDeadlineUs;
  }

  bool due(uint32_t nowUs) const {
    return initialized && uint32_t(nowUs - nextDeadlineUs) < 0x80000000UL;
  }

  // No initialized deadline means no usable scheduling headroom yet.
  uint32_t timeUntilUs(uint32_t nowUs) const {
    if (!initialized || due(nowUs)) return 0;
    return nextDeadlineUs - nowUs;
  }

  // Admit one due slot, recording lateness at the START of its work. Passing
  // the completion timestamp separately prevents a normal DMA duration from
  // being mislabeled as start lateness. Admission consumes the latest due slot;
  // after work, keep the most recent due slot for one immediate admission and
  // skip only periods that elapsed completely. A slight overrun can therefore
  // recover the original epoch instead of creating an unnecessary 33 ms gap.
  // Old slots never form a queue: even a long stall admits at most one current
  // slot, and an immediate zero-work second poll cannot replay older frames.
  // Invalid/early requests do not change the deadline or any statistic.
  bool advance(uint32_t admittedAtUs, uint32_t finishedAtUs) {
    if (!due(admittedAtUs)) return false;
    const uint32_t workUs = finishedAtUs - admittedAtUs;
    if (workUs >= 0x80000000UL) return false;
    const uint32_t lateness = admittedAtUs - nextDeadlineUs;
    uint64_t periods = uint64_t(lateness) / periodUs + 1;
    const uint64_t finishFromDeadline = uint64_t(lateness) + workUs;
    const uint64_t latestDuePeriod = finishFromDeadline / periodUs;
    if (latestDuePeriod > periods) periods = latestDuePeriod;
    nextDeadlineUs += uint32_t(periods * periodUs);
    ++slots;
    if (lateness) ++lateSlots;
    lastLatenessUs = lateness;
    if (lateness > maxLatenessUs) maxLatenessUs = lateness;
    totalLatenessUs += lateness;
    missedPeriods += periods - 1;
    return true;
  }

  bool advance(uint32_t nowUs) { return advance(nowUs, nowUs); }
};

// LVGL permits nullptr to refresh the current label and permits the caller to
// edit its owned text in place before setting the same pointer. Preserve both.
inline bool textNeedsUpdate(const char *current, const char *next) {
  return !current || !next || current == next || strcmp(current, next) != 0;
}

// Measure intervals only between consecutively presented animated frames.
// Idle time must not turn an otherwise smooth transition into a low FPS claim.
struct ActiveFrameIntervals {
  static constexpr uint32_t fineBinWidthUs = 250;
  static constexpr unsigned fineFiniteBins = 128;
  static constexpr unsigned fineBinCount = fineFiniteBins + 1;
  uint32_t samples = 0;
  uint64_t totalUs = 0;
  uint32_t maxUs = 0;
  uint32_t buckets[6] = {};
  // 128 finite bins through 32 ms, then an overflow bin. Keep the original
  // coarse buckets for compatibility; this adds 516 bytes per measurement.
  uint32_t fineBuckets[fineBinCount] = {};
  uint32_t previousUs = 0;
  bool previousActive = false;

  void presented(uint32_t nowUs, bool active) {
    if (active && previousActive) {
      const uint32_t interval = nowUs - previousUs;
      ++samples;
      totalUs += interval;
      if (interval > maxUs) maxUs = interval;
      const uint32_t limits[] = {16667, 20000, 25000, 33334, 50000};
      unsigned bucket = 0;
      while (bucket < 5 && interval > limits[bucket]) ++bucket;
      ++buckets[bucket];
      unsigned fineBucket = interval ? (interval - 1) / fineBinWidthUs : 0;
      if (fineBucket >= fineFiniteBins) fineBucket = fineFiniteBins;
      ++fineBuckets[fineBucket];
    }
    previousUs = nowUs;
    previousActive = active;
  }

  void endActiveRun() { previousActive = false; }

  // Histogram percentile is an upper bound, not an invented exact latency.
  uint32_t p95UpperUs() const {
    if (!samples) return 0;
    const uint32_t threshold = samples - samples / 20;
    const uint32_t limits[] = {16667, 20000, 25000, 33334, 50000};
    uint32_t accumulated = 0;
    for (unsigned i = 0; i < 5; ++i) {
      accumulated += buckets[i];
      if (accumulated >= threshold) return limits[i];
    }
    return maxUs;
  }

  // Nearest-rank percentile, reported as a histogram upper bound. A percentile
  // in the overflow bin uses the observed maximum rather than a false 32 ms
  // cap. Invalid percentages and empty/entirely-zero runs return zero.
  uint32_t percentileUpperUs(uint8_t percent) const {
    if (!samples || !percent || percent > 100 || !maxUs) return 0;
    const uint32_t threshold = uint32_t((uint64_t(samples) * percent + 99) / 100);
    uint64_t accumulated = 0;
    for (unsigned i = 0; i < fineFiniteBins; ++i) {
      accumulated += fineBuckets[i];
      if (accumulated >= threshold) return uint32_t(i + 1) * fineBinWidthUs;
    }
    return maxUs;
  }

  double framesPerSecond() const {
    return totalUs ? double(samples) * 1000000.0 / double(totalUs) : 0.0;
  }
};
}
