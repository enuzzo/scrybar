#pragma once
#include <stdint.h>

namespace DisplayIdle {
inline uint32_t elapsed(uint32_t now, uint32_t last) { return now - last; }

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
