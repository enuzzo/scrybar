#include "../src/lvgl_updates.h"
#include <cassert>
#include <cstdio>

static uint32_t presentedFrames = 0;
static void flush(lv_disp_drv_t *driver, const lv_area_t *, lv_color_t *) {
  if (lv_disp_flush_is_last(driver)) ++presentedFrames;
  lv_disp_flush_ready(driver);
}
static uint32_t refresh() {
  const uint32_t before = presentedFrames;
  lv_refr_now(nullptr);
  return presentedFrames - before;
}

int main() {
  lv_init();
  static lv_color_t pixels[200 * 8];
  static lv_disp_draw_buf_t buffer;
  lv_disp_draw_buf_init(&buffer, pixels, nullptr, 200 * 8);
  static lv_disp_drv_t driver;
  lv_disp_drv_init(&driver);
  driver.hor_res = 200;
  driver.ver_res = 80;
  driver.draw_buf = &buffer;
  driver.flush_cb = flush;
  lv_disp_t *display = lv_disp_drv_register(&driver);
  assert(display);
  lv_obj_t *label = lv_label_create(lv_scr_act());
  assert(LvglUpdates::labelText(label, "LIVE"));
  assert(LvglUpdates::localColor(label, LV_STYLE_TEXT_COLOR, lv_color_hex(0x5599FF), 0));
  assert(LvglUpdates::localNumber(label, LV_STYLE_OPA, LV_OPA_COVER, 0));
  assert(refresh() == 1);

  uint32_t before = presentedFrames;
  for (unsigned i = 0; i < 1000; ++i) {
    lv_label_set_text(label, "LIVE");
    lv_obj_set_style_text_color(label, lv_color_hex(0x5599FF), 0);
    lv_obj_set_style_opa(label, LV_OPA_COVER, 0);
    refresh();
  }
  const uint32_t originalFrames = presentedFrames - before;
  assert(originalFrames == 1000);
  before = presentedFrames;
  for (unsigned i = 0; i < 1000; ++i) {
    assert(!LvglUpdates::labelText(label, "LIVE"));
    assert(!LvglUpdates::localColor(label, LV_STYLE_TEXT_COLOR, lv_color_hex(0x5599FF), 0));
    assert(!LvglUpdates::localNumber(label, LV_STYLE_OPA, LV_OPA_COVER, 0));
    assert(refresh() == 0);
  }
  assert(presentedFrames == before);

  // Real changes, explicit refreshes and in-place text edits still redraw.
  assert(LvglUpdates::labelText(label, "TEST"));
  assert(refresh() == 1);
  assert(LvglUpdates::labelText(label, nullptr));
  assert(refresh() == 1);
  char *owned = const_cast<char *>(lv_label_get_text(label));
  owned[0] = 'B';
  assert(LvglUpdates::labelText(label, owned));
  assert(refresh() == 1);
  assert(strcmp(lv_label_get_text(label), "BEST") == 0);

  // An inherited equal value must still gain the requested local override;
  // otherwise a later theme/parent update would silently change its behavior.
  assert(lv_obj_remove_local_style_prop(label, LV_STYLE_TEXT_COLOR, 0));
  const lv_color_t inherited = lv_obj_get_style_text_color(label, 0);
  assert(LvglUpdates::localColor(label, LV_STYLE_TEXT_COLOR, inherited, 0));
  lv_style_value_t local;
  assert(lv_obj_get_local_style_prop(label, LV_STYLE_TEXT_COLOR, &local, 0) == LV_STYLE_RES_FOUND);
  assert(local.color.full == inherited.full);
  assert(LvglUpdates::localColor(label, LV_STYLE_TEXT_COLOR, inherited, LV_STATE_PRESSED));
  assert(lv_obj_get_local_style_prop(label, LV_STYLE_TEXT_COLOR, &local, LV_STATE_PRESSED) == LV_STYLE_RES_FOUND);
  assert(!LvglUpdates::localColor(label, LV_STYLE_TEXT_COLOR, inherited, LV_STATE_PRESSED));

  std::printf("LVGL %d.%d.%d: identical update batches rendered %u -> 0 frames; "
              "changed text, explicit refresh, mutable text, local styles and selectors passed.\n",
              LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH, originalFrames);
  lv_disp_remove(display);
}
