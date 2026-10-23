// Built against the current extracted firmware by render_mac_stats_native.py.
#ifndef SCRYBAR_STATS_NATIVE_SOURCE
#error "Use tools/render_mac_stats_native.py --check-chart-only"
#endif
#define main stats_renderer_main
#include SCRYBAR_STATS_NATIVE_SOURCE
#undef main
#include <cassert>

static uint16_t logical[640 * 172], expected[640 * 172];
static uint64_t copiedPixels = 0;
static void captureChartUpdate(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels) {
  copiedPixels += uint64_t(lv_area_get_width(area)) * lv_area_get_height(area);
  for (int y = area->y1; y <= area->y2; ++y)
    for (int x = area->x1; x <= area->x2; ++x) logical[y * 640 + x] = (pixels++)->full;
  lv_disp_flush_ready(driver);
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
  driver.draw_buf = &buffer; driver.flush_cb = captureChartUpdate;
  lv_disp_t *display = lv_disp_drv_register(&driver);
  assert(display);
  lv_obj_set_style_pad_all(lv_scr_act(), 0, 0);
  lv_obj_set_style_border_width(lv_scr_act(), 0, 0);
  g_lvglMacStatsRoot = lv_obj_create(lv_scr_act());
  lv_obj_remove_style_all(g_lvglMacStatsRoot);
  lv_obj_set_size(g_lvglMacStatsRoot, 640, 172);
  lv_obj_clear_flag(g_lvglMacStatsRoot, LV_OBJ_FLAG_SCROLLABLE);
  lvglInitMacStatsUi(); fixture("live"); lvglUpdateMacStatsUi(true);
  lv_obj_update_layout(g_lvglMacStatsRoot); lv_refr_now(display);

  uint64_t chartPixels = 0, fullPixels = 0;
  const uint32_t base = nativeNow;
  for (unsigned step = 1; step <= 5; ++step) {
    nativeNow = base + step * 200;
    copiedPixels = 0;
    lvglUpdateMacStatsNetworkChart(false); lv_refr_now(display);
    chartPixels += copiedPixels;
    std::memcpy(expected, logical, sizeof(logical));

    // Restore the same prior instant, then take the existing full update path.
    nativeNow -= 200;
    lvglUpdateMacStatsUi(true); lv_refr_now(display);
    nativeNow += 200;
    copiedPixels = 0;
    lvglUpdateMacStatsUi(false); lv_refr_now(display);
    fullPixels += copiedPixels;
    assert(!std::memcmp(expected, logical, sizeof(logical)));
  }
  // This regression must both preserve pixels and avoid unrelated redraw work.
  assert(chartPixels * 2 < fullPixels);
  std::printf("Chart-only pixels match theme=%s frames=5 chart_pixels=%llu full_pixels=%llu reduction=%.1f%%\n",
      activeUiThemeId(), (unsigned long long)chartPixels, (unsigned long long)fullPixels,
      100.0 * (1.0 - double(chartPixels) / fullPixels));
  lv_disp_remove(display);
}
