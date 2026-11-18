#include "../src/display_idle.h"
#include <cassert>
int main() {
  const DisplayIdle::Timeouts defaults;
  assert(defaults.valid());
  assert(!DisplayIdle::expired(899999, defaults.screensaverMinutes));
  assert(DisplayIdle::expired(900000, defaults.screensaverMinutes));
  assert(!DisplayIdle::expired(1499999, defaults.screenOffMinutes));
  assert(DisplayIdle::expired(1500000, defaults.screenOffMinutes));
  assert(!DisplayIdle::expired(UINT32_MAX, 0));

  assert((DisplayIdle::Timeouts{0, 25}.valid()));
  assert((DisplayIdle::Timeouts{15, 0}.valid()));
  assert((DisplayIdle::Timeouts{0, 0}.valid()));
  assert((DisplayIdle::Timeouts{1439, 1440}.valid()));
  assert((DisplayIdle::Timeouts{1440, 0}.valid()));
  assert(!(DisplayIdle::Timeouts{25, 15}.valid()));
  assert(!(DisplayIdle::Timeouts{15, 15}.valid()));
  assert(!(DisplayIdle::Timeouts{0, 1441}.valid()));
  assert(!(DisplayIdle::Timeouts{1441, 0}.valid()));

  assert(DisplayIdle::elapsed(20, UINT32_MAX - 20) == 41);
  DisplayIdle::TouchActivity t;
  assert(!t.update(true, 100));
  assert(!t.update(false, 150));
  assert(!t.update(false, 250));
  assert(!t.update(true, 300));
  assert(t.update(true, 340));
  // A stuck controller can delay inactivity by at most 30 seconds.
  for (uint32_t now = 360; now < 30300; now += 20) t.update(true, now);
  assert(!t.update(true, 30300));
  assert(!t.update(true, 30320));
  t.update(false, 30420);
  assert(!t.update(true, 30430));
  assert(t.update(true, 30470));
  DisplayIdle::TouchActivity wrap;
  assert(!wrap.update(true, UINT32_MAX - 20));
  assert(wrap.update(true, 20));
}
