#pragma once
#include <stdint.h>
#include <lvgl.h>

namespace PageTransitionMotion {
// Evaluate exactly the LVGL8 curve used by ordinary ScryBar page slides,
// without allocating an animation or running unrelated LVGL timers. Positions
// are page coordinates (within +/-640); duration fits LVGL's signed ms field.
inline int32_t positionAt(int32_t from, int32_t to, uint32_t elapsedMs,
                          uint32_t durationMs = 250) {
  if (!durationMs || elapsedMs >= durationMs) return to;
  lv_anim_t animation = {};
  animation.start_value = from;
  animation.end_value = to;
  animation.time = static_cast<int32_t>(durationMs);
  animation.act_time = static_cast<int32_t>(elapsedMs);
  return lv_anim_path_ease_out(&animation);
}
}
