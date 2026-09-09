#include "../src/touch_timing.h"
#include <cassert>
int main() {
  assert(!TouchTiming::released(1069, 1000));
  assert(TouchTiming::released(1070, 1000));
  // A delayed loop must not lengthen a 500 ms gesture past the tap/swipe limits.
  assert(TouchTiming::released(6000, 1500));
  assert(TouchTiming::contactDuration(1000, 1500) == 500);
  // millis() wraparound must preserve both release debounce and held duration.
  assert(!TouchTiming::released(20, UINT32_MAX - 20));
  assert(TouchTiming::released(60, UINT32_MAX - 20));
  assert(TouchTiming::contactDuration(UINT32_MAX - 20, 20) == 41);
}
