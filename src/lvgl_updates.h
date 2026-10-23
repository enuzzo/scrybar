#pragma once
#include <lvgl.h>
#include "display_performance.h"

namespace LvglUpdates {
// Return true only when LVGL receives a setter call. LVGL 8 invalidates even
// equal text and equal local style assignments, so callers can count saved work.
inline bool labelText(lv_obj_t *obj, const char *text) {
  if (!DisplayPerformance::textNeedsUpdate(lv_label_get_text(obj), text)) return false;
  lv_label_set_text(obj, text);
  return true;
}

// Compare the exact local override, preserving inheritance and state selectors.
inline bool localColor(lv_obj_t *obj, lv_style_prop_t prop, lv_color_t color,
                       lv_style_selector_t selector) {
  lv_style_value_t previous;
  if (lv_obj_get_local_style_prop(obj, prop, &previous, selector) == LV_STYLE_RES_FOUND &&
      previous.color.full == color.full) return false;
  lv_style_value_t value = {};
  value.color = color;
  lv_obj_set_local_style_prop(obj, prop, value, selector);
  return true;
}

inline bool localNumber(lv_obj_t *obj, lv_style_prop_t prop, int32_t number,
                        lv_style_selector_t selector) {
  lv_style_value_t previous;
  if (lv_obj_get_local_style_prop(obj, prop, &previous, selector) == LV_STYLE_RES_FOUND &&
      previous.num == number) return false;
  lv_style_value_t value = {};
  value.num = number;
  lv_obj_set_local_style_prop(obj, prop, value, selector);
  return true;
}
}
