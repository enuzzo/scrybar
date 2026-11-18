#pragma once
#include <stdint.h>

namespace DisplayIdle {
inline uint32_t elapsed(uint32_t now, uint32_t last) { return now - last; }

constexpr uint16_t kMaxMinutes = 24 * 60;

struct Timeouts {
  uint16_t screensaverMinutes = 15;
  uint16_t screenOffMinutes = 25;

  bool valid() const {
    return screensaverMinutes <= kMaxMinutes && screenOffMinutes <= kMaxMinutes &&
           (screensaverMinutes == 0 || screenOffMinutes == 0 || screensaverMinutes < screenOffMinutes);
  }

  bool operator==(const Timeouts &other) const {
    return screensaverMinutes == other.screensaverMinutes &&
           screenOffMinutes == other.screenOffMinutes;
  }
};

inline uint32_t milliseconds(uint16_t minutes) { return minutes * 60000UL; }
inline bool expired(uint32_t idleMs, uint16_t minutes) {
  return minutes != 0 && idleMs >= milliseconds(minutes);
}

// Ignore isolated ghost frames and cap a continuously asserted contact. A
// fresh contact requires a real release, including across millis() rollover.
struct TouchActivity {
  bool down = false;
  uint32_t started = 0;
  uint32_t lastSeen = 0;
  bool update(bool touched, uint32_t now) {
    if (!touched) {
      if (down && elapsed(now, lastSeen) >= 100) down = false;
      return false;
    }
    if (!down || elapsed(now, lastSeen) >= 100) {
      down = true;
      started = now;
    }
    lastSeen = now;
    const uint32_t held = elapsed(now, started);
    return held >= 40 && held < 30000;
  }
};
}
