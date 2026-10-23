// Built by tools/render_mac_stats_native.py --check-transitions. The included
// file is generated from the current firmware's real Stats layout and fonts.
#ifndef SCRYBAR_STATS_NATIVE_SOURCE
#error "Use tools/render_mac_stats_native.py --check-transitions"
#endif
#define main stats_renderer_main
#include SCRYBAR_STATS_NATIVE_SOURCE
#undef main
#include "../src/page_transition_frame.h"
#include "../src/page_transition_motion.h"
#include <cassert>

static uint16_t logical[640 * 172] = {};
static uint16_t outgoing[640 * 172], incoming[640 * 172], composed[640 * 172];
static void capture(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels) {
  for (int y = area->y1; y <= area->y2; ++y)
    for (int x = area->x1; x <= area->x2; ++x) logical[y * 640 + x] = (pixels++)->full;
  lv_disp_flush_ready(driver);
}
static void redraw() {
  lv_obj_invalidate(lv_scr_act());
  lv_refr_now(nullptr);
}
static lv_obj_t *page(const char *state) {
  g_macStats = MacStatsState{};
  g_macStatsUi = MacStatsUi{};
  g_networkHistory = MacStatsHistory::History{};
  g_lvglMacStatsRoot = lv_obj_create(lv_scr_act());
  lv_obj_remove_style_all(g_lvglMacStatsRoot);
  lv_obj_set_size(g_lvglMacStatsRoot, 640, 172);
  lv_obj_clear_flag(g_lvglMacStatsRoot, LV_OBJ_FLAG_SCROLLABLE);
  lvglInitMacStatsUi();
  fixture(state);
  lvglUpdateMacStatsUi(true);
  return g_lvglMacStatsRoot;
}

static void recordAnimationPosition(void *value, int32_t position) {
  *static_cast<int32_t *>(value) = position;
}

static unsigned checkMotionAgainstLvgl() {
  // Drive the real LVGL animation engine once per millisecond. Comparing its
  // executed callback also covers clamping/deletion after the endpoint, rather
  // than comparing two calls to the same path function with shared inputs.
  lv_anim_del(nullptr, nullptr);
  struct Motion { int32_t from; int32_t to; uint32_t duration; };
  const Motion cases[] = {
    {0, -640, 250}, {640, 0, 250}, {0, 640, 250}, {-640, 0, 250},
    {-14, -640, 250}, {626, 0, 250}, {-279, -640, 250}, {361, 0, 250},
    {-639, -640, 250}, {1, 0, 250},
    {14, 640, 250}, {-626, 0, 250}, {279, 640, 250}, {-361, 0, 250},
    {-279, 0, 250}, {361, 640, 250}, {279, 0, 250}, {-361, -640, 250},
    {0, 0, 250}, {640, 640, 250},
    {-640, 0, 1}, {0, 640, 17}, {173, -640, 350}
  };
  unsigned comparisons = 0;
  for (const Motion &motion : cases) {
    int32_t executed = INT32_MIN;
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, &executed);
    lv_anim_set_exec_cb(&animation, recordAnimationPosition);
    lv_anim_set_values(&animation, motion.from, motion.to);
    lv_anim_set_time(&animation, motion.duration);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    assert(lv_anim_start(&animation));
    for (uint32_t elapsed = 0; elapsed <= 350; ++elapsed) {
      if (elapsed) lv_tick_inc(1);
      lv_anim_refr_now();
      const int32_t calculated = PageTransitionMotion::positionAt(
          motion.from, motion.to, elapsed, motion.duration);
      if (executed != calculated) {
        std::fprintf(stderr, "Motion mismatch from=%d to=%d duration=%u elapsed=%u lvgl=%d cache=%d\n",
            motion.from, motion.to, motion.duration, elapsed, executed, calculated);
        std::abort();
      }
      if (elapsed >= motion.duration) assert(calculated == motion.to);
      if (motion.duration == 250)
        assert(calculated == PageTransitionMotion::positionAt(motion.from, motion.to, elapsed));
      ++comparisons;
    }
    assert(!lv_anim_get(&executed, recordAnimationPosition));
  }
  assert(PageTransitionMotion::positionAt(-279, 0, 0, 0) == 0);
  assert(PageTransitionMotion::positionAt(0, -640, UINT32_MAX) == -640);

  // The engine's clock also crosses uint32_t wrap during a normal 250 ms
  // slide; unsigned elapsed input must still select identical positions.
  lv_tick_inc(UINT32_MAX - lv_tick_get() - 100);
  int32_t executed = 0;
  lv_anim_t animation;
  lv_anim_init(&animation);
  lv_anim_set_var(&animation, &executed);
  lv_anim_set_exec_cb(&animation, recordAnimationPosition);
  lv_anim_set_values(&animation, 0, -640);
  lv_anim_set_time(&animation, 250);
  lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
  assert(lv_anim_start(&animation));
  for (uint32_t elapsed = 0; elapsed <= 350; ++elapsed) {
    if (elapsed) lv_tick_inc(1);
    lv_anim_refr_now();
    assert(executed == PageTransitionMotion::positionAt(0, -640, elapsed));
    ++comparisons;
  }
  assert(!lv_anim_get(&executed, recordAnimationPosition));
  return comparisons;
}

int main(int argc, char **argv) {
  if (argc != 2) return 2;
  g_uiThemeIndex = std::atoi(argv[1]);
  lv_init();
  static lv_color_t pixels[640 * 18];
  static lv_disp_draw_buf_t buffer;
  lv_disp_draw_buf_init(&buffer, pixels, nullptr, 640 * 18);
  static lv_disp_drv_t driver;
  lv_disp_drv_init(&driver);
  driver.hor_res = 640; driver.ver_res = 172;
  driver.draw_buf = &buffer; driver.flush_cb = capture;
  lv_disp_t *display = lv_disp_drv_register(&driver);
  assert(display);
  lv_obj_set_style_pad_all(lv_scr_act(), 0, 0);
  lv_obj_set_style_border_width(lv_scr_act(), 0, 0);
  lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(activeUiTheme().lvgl.screenBg), 0);
  lv_obj_t *first = page("live");
  // The production page has one global chart backing array. This test creates
  // a second independent instance, so keep the first instance's arrays stable.
  MacStatsUi firstUi = g_macStatsUi;
  lv_chart_set_ext_y_array(firstUi.networkChart, firstUi.downSeries, firstUi.chartDown);
  lv_chart_set_ext_y_array(firstUi.networkChart, firstUi.upSeries, firstUi.chartUp);
  lv_chart_set_ext_x_array(firstUi.networkChart, firstUi.downSeries, firstUi.chartX);
  lv_chart_set_ext_x_array(firstUi.networkChart, firstUi.upSeries, firstUi.chartX);
  redraw();
  assert(PageTransitionFrame::rotateLandscapeToNative(outgoing, 640 * 172, logical, 640 * 172));
  lv_obj_add_flag(first, LV_OBJ_FLAG_HIDDEN);
  lv_obj_t *second = page("long");
  redraw();
  assert(PageTransitionFrame::rotateLandscapeToNative(incoming, 640 * 172, logical, 640 * 172));
  lv_obj_clear_flag(first, LV_OBJ_FLAG_HIDDEN);
  const unsigned offsets[] = {0, 1, 79, 319, 320, 639, 640};
  unsigned comparisons = 0;
  for (bool next : {true, false}) for (unsigned offset : offsets) {
    lv_obj_set_x(first, next ? -int(offset) : int(offset));
    lv_obj_set_x(second, next ? 640 - int(offset) : int(offset) - 640);
    redraw();
    assert(PageTransitionFrame::composeLandscapeFrame(composed, 640 * 172,
      {outgoing, 640 * 172}, {incoming, 640 * 172},
      next ? PageTransitionFrame::Direction::next : PageTransitionFrame::Direction::previous, offset));
    for (size_t i = 0; i < 640 * 172; ++i) {
      if (logical[i] != composed[i]) {
        std::fprintf(stderr, "Mismatch theme=%s next=%u progress=%u x=%zu y=%zu lvgl=%04x cache=%04x\n",
          activeUiThemeId(), next, offset, i % 640, i / 640, logical[i], composed[i]);
        return 1;
      }
    }
    ++comparisons;
  }
  std::printf("LVGL cache pixels match theme=%s comparisons=%u pixels_each=%u\n",
              activeUiThemeId(), comparisons, 640 * 172);
  const unsigned motionComparisons = checkMotionAgainstLvgl();
  std::printf("LVGL motion curve matches theme=%s comparisons=%u elapsed_ms=0..350\n",
              activeUiThemeId(), motionComparisons);
  lv_disp_remove(display);
}
