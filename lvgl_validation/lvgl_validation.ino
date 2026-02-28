#include <Arduino.h>
#include <Wire.h>
#define LV_CONF_INCLUDE_SIMPLE 1
#include "lv_conf.h"
#include <lvgl.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "src/axs15231b/esp_lcd_axs15231b.h"

#if LV_COLOR_DEPTH != 16
#error "LV_COLOR_DEPTH must be 16 for this display pipeline"
#endif

// ---------------- Validation toggles ----------------
#define VAL_SCENE_SECONDS 6
#define VAL_TEST_COLOR 1
#define VAL_TEST_TEXT 1
#define VAL_TEST_WIDGET 1
#define VAL_TEST_ANIM 1
#define VAL_TEST_STRESS 1
#define VAL_TOUCH_DEBUG 1
#define VAL_TOUCH_SAMPLE_LOG_MS 120
#define VAL_SWIPE_MIN_PX_DEFAULT 28
#define VAL_SWIPE_MAX_MS 2200

// ---------------- Board pins (Waveshare ESP32-S3 1.47") ----------------
static constexpr int LCD_BL_PIN = 8;
static constexpr int LCD_QSPI_CS_PIN = 9;
static constexpr int LCD_QSPI_SCK_PIN = 10;
static constexpr int LCD_QSPI_D0_PIN = 11;
static constexpr int LCD_QSPI_D1_PIN = 12;
static constexpr int LCD_QSPI_D2_PIN = 13;
static constexpr int LCD_QSPI_D3_PIN = 14;
static constexpr int LCD_RST_PIN = 21;

static constexpr int LCD_WIDTH = 172;
static constexpr int LCD_HEIGHT = 640;
static constexpr int DB_CANVAS_W = LCD_HEIGHT;  // logical landscape 640
static constexpr int DB_CANVAS_H = LCD_WIDTH;   // logical landscape 172
static constexpr int DB_NATIVE_W = LCD_WIDTH;
static constexpr int DB_NATIVE_H = LCD_HEIGHT;
static constexpr int DB_CHUNK_ROWS = 64;
static constexpr int I2C_SDA_PIN = 47;
static constexpr int I2C_SCL_PIN = 48;
static constexpr int I2C_SDA_PIN_ALT = 17;
static constexpr int I2C_SCL_PIN_ALT = 18;
static constexpr uint8_t TOUCH_I2C_ADDR = 0x3B;

static esp_lcd_panel_io_handle_t g_panelIo = nullptr;
static esp_lcd_panel_handle_t g_panel = nullptr;
static SemaphoreHandle_t g_dispFlushSem = nullptr;
static uint16_t *g_canvasBuf = nullptr;
static uint16_t *g_rotBuf = nullptr;
static uint16_t *g_dmaBuf = nullptr;

static lv_disp_draw_buf_t g_lvglDrawBuf;
static lv_disp_drv_t g_lvglDispDrv;
static lv_color_t *g_lvglBuf1 = nullptr;

static lv_obj_t *g_sceneRoot = nullptr;
static lv_obj_t *g_fpsLabel = nullptr;
static lv_obj_t *g_animBar = nullptr;
static lv_obj_t *g_touchInfo1 = nullptr;
static lv_obj_t *g_touchInfo2 = nullptr;
static lv_obj_t *g_touchInfo3 = nullptr;
static lv_obj_t *g_touchDot = nullptr;

static uint32_t g_flushCount = 0;
static uint32_t g_lastFpsMs = 0;
static uint32_t g_lastSceneMs = 0;
static uint8_t g_sceneIdx = 0;
static uint8_t g_sceneOrder[8];
static uint8_t g_sceneCount = 0;

static TwoWire g_i2cAlt = TwoWire(1);
static bool g_touchReady = false;
static bool g_touchUseAltBus = true;
static bool g_touchDown = false;
static int16_t g_touchStartX = 0;
static int16_t g_touchStartY = 0;
static int16_t g_touchLastX = 0;
static int16_t g_touchLastY = 0;
static int16_t g_touchRawX = -1;
static int16_t g_touchRawY = -1;
static uint32_t g_touchStartMs = 0;
static uint32_t g_lastTouchLogMs = 0;
static uint32_t g_swipeCount = 0;
static int16_t g_swipeMinPx = VAL_SWIPE_MIN_PX_DEFAULT;
static char g_lastSwipe[16] = "NONE";

static bool onDisplayFlushDone(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *, void *) {
  (void)panel_io;
  BaseType_t taskWoken = pdFALSE;
  if (g_dispFlushSem) xSemaphoreGiveFromISR(g_dispFlushSem, &taskWoken);
  return false;
}

static bool dispFlush() {
  if (!g_panel || !g_canvasBuf || !g_rotBuf || !g_dmaBuf || !g_dispFlushSem) return false;

  uint32_t idx = 0;
  for (int16_t j = 0; j < DB_CANVAS_W; ++j) {
    for (int16_t i = 0; i < DB_CANVAS_H; ++i) {
      g_rotBuf[idx++] = g_canvasBuf[(DB_CANVAS_W * (DB_CANVAS_H - i - 1)) + j];
    }
  }

  const int chunks = DB_NATIVE_H / DB_CHUNK_ROWS;
  const int pixelsPerChunk = DB_NATIVE_W * DB_CHUNK_ROWS;
  const size_t bytesPerChunk = pixelsPerChunk * sizeof(uint16_t);
  uint16_t *map = g_rotBuf;

  xSemaphoreGive(g_dispFlushSem);
  for (int c = 0; c < chunks; ++c) {
    xSemaphoreTake(g_dispFlushSem, portMAX_DELAY);
    memcpy(g_dmaBuf, map, bytesPerChunk);
    esp_lcd_panel_draw_bitmap(g_panel, 0, c * DB_CHUNK_ROWS, DB_NATIVE_W, (c + 1) * DB_CHUNK_ROWS, g_dmaBuf);
    map += pixelsPerChunk;
  }
  xSemaphoreTake(g_dispFlushSem, portMAX_DELAY);
  return true;
}

static bool initDisplay() {
  if (g_panel) return true;

  pinMode(LCD_BL_PIN, OUTPUT);
  digitalWrite(LCD_BL_PIN, HIGH);  // OFF during init (board uses inverted backlight logic)

  gpio_config_t rst_cfg = {};
  rst_cfg.intr_type = GPIO_INTR_DISABLE;
  rst_cfg.mode = GPIO_MODE_OUTPUT;
  rst_cfg.pin_bit_mask = (1ULL << LCD_RST_PIN);
  rst_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  rst_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
  if (gpio_config(&rst_cfg) != ESP_OK) return false;

  spi_bus_config_t buscfg = {};
  buscfg.sclk_io_num = LCD_QSPI_SCK_PIN;
  buscfg.data0_io_num = LCD_QSPI_D0_PIN;
  buscfg.data1_io_num = LCD_QSPI_D1_PIN;
  buscfg.data2_io_num = LCD_QSPI_D2_PIN;
  buscfg.data3_io_num = LCD_QSPI_D3_PIN;
  buscfg.max_transfer_sz = DB_NATIVE_W * DB_CHUNK_ROWS * 2;
  if (spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO) != ESP_OK) return false;

  g_dispFlushSem = xSemaphoreCreateBinary();
  if (!g_dispFlushSem) return false;

  esp_lcd_panel_io_spi_config_t io_config = {};
  io_config.cs_gpio_num = LCD_QSPI_CS_PIN;
  io_config.dc_gpio_num = -1;
  io_config.spi_mode = 3;
  io_config.pclk_hz = 40 * 1000 * 1000;
  io_config.trans_queue_depth = 10;
  io_config.on_color_trans_done = onDisplayFlushDone;
  io_config.user_ctx = nullptr;
  io_config.lcd_cmd_bits = 32;
  io_config.lcd_param_bits = 8;
  io_config.flags.quad_mode = true;
  if (esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &g_panelIo) != ESP_OK) return false;

  static const axs15231b_lcd_init_cmd_t lcd_init_cmds[] = {
      {0x11, (uint8_t[]){0x00}, 0, 100},
      {0x29, (uint8_t[]){0x00}, 0, 100},
  };
  axs15231b_vendor_config_t vendor_config = {};
  vendor_config.flags.use_qspi_interface = 1;
  vendor_config.init_cmds = lcd_init_cmds;
  vendor_config.init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]);

  esp_lcd_panel_dev_config_t panel_config = {};
  panel_config.reset_gpio_num = -1;
  panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  panel_config.bits_per_pixel = 16;
  panel_config.vendor_config = &vendor_config;
  if (esp_lcd_new_panel_axs15231b(g_panelIo, &panel_config, &g_panel) != ESP_OK) return false;

  gpio_set_level((gpio_num_t)LCD_RST_PIN, 1);
  delay(30);
  gpio_set_level((gpio_num_t)LCD_RST_PIN, 0);
  delay(250);
  gpio_set_level((gpio_num_t)LCD_RST_PIN, 1);
  delay(30);
  if (esp_lcd_panel_init(g_panel) != ESP_OK) return false;

  g_canvasBuf = (uint16_t *)heap_caps_malloc(DB_CANVAS_W * DB_CANVAS_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
  g_rotBuf = (uint16_t *)heap_caps_malloc(DB_NATIVE_W * DB_NATIVE_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
  g_dmaBuf = (uint16_t *)heap_caps_malloc(DB_NATIVE_W * DB_CHUNK_ROWS * sizeof(uint16_t), MALLOC_CAP_DMA);
  if (!g_canvasBuf || !g_rotBuf || !g_dmaBuf) return false;

  memset(g_canvasBuf, 0, DB_CANVAS_W * DB_CANVAS_H * sizeof(uint16_t));
  dispFlush();
  digitalWrite(LCD_BL_PIN, LOW);   // ON after panel init

  Serial.printf("[VAL] Display init ok native=%dx%d canvas=%dx%d\n", DB_NATIVE_W, DB_NATIVE_H, DB_CANVAS_W, DB_CANVAS_H);
  return true;
}

static void lvglDisplayFlushCb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
  (void)drv;
  const int32_t w = area->x2 - area->x1 + 1;
  const lv_color_t *src = color_p;

  for (int32_t y = area->y1; y <= area->y2; ++y) {
    if (y < 0 || y >= DB_CANVAS_H) {
      src += w;
      continue;
    }

    int32_t sx = 0;
    int32_t ex = w - 1;
    int32_t dstX = area->x1;
    if (dstX < 0) {
      sx = -dstX;
      dstX = 0;
    }
    const int32_t over = (area->x1 + ex) - (DB_CANVAS_W - 1);
    if (over > 0) ex -= over;

    if (sx <= ex) {
      uint16_t *dst = &g_canvasBuf[(size_t)y * (size_t)DB_CANVAS_W + (size_t)dstX];
      for (int32_t x = sx; x <= ex; ++x) {
#if LV_COLOR_DEPTH == 16
        dst[x - sx] = src[x].full;
#else
        dst[x - sx] = lv_color_to16(src[x]);
#endif
      }
    }
    src += w;
  }

  dispFlush();
  g_flushCount++;
  lv_disp_flush_ready(drv);
}

static bool initTouchDebugInput() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);
  Wire.beginTransmission(TOUCH_I2C_ADDR);
  const uint8_t errMain = Wire.endTransmission();

  g_i2cAlt.begin(I2C_SDA_PIN_ALT, I2C_SCL_PIN_ALT);
  g_i2cAlt.setClock(400000);
  g_i2cAlt.beginTransmission(TOUCH_I2C_ADDR);
  const uint8_t errAlt = g_i2cAlt.endTransmission();

  // This board typically routes touch on ALT I2C; prefer ALT when both ACK.
  if (errAlt == 0) {
    g_touchReady = true;
    g_touchUseAltBus = true;
  } else if (errMain == 0) {
    g_touchReady = true;
    g_touchUseAltBus = false;
  } else {
    g_touchReady = false;
  }

  Serial.printf("[VAL][TOUCH] probe addr=0x%02X main=%d alt=%d -> %s (%s)\n",
                TOUCH_I2C_ADDR, errMain, errAlt,
                g_touchReady ? "OK" : "FAIL",
                g_touchReady ? (g_touchUseAltBus ? "ALT" : "MAIN") : "-");
  return g_touchReady;
}

static bool readTouchLogicalPoint(int16_t &lx, int16_t &ly, uint8_t &pointsOut) {
  if (!g_touchReady) return false;

  static const uint8_t kReadCmd[11] = {0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00};
  uint8_t data[8] = {0};

  TwoWire &tb = g_touchUseAltBus ? g_i2cAlt : Wire;
  tb.beginTransmission(TOUCH_I2C_ADDR);
  tb.write(kReadCmd, sizeof(kReadCmd));
  if (tb.endTransmission() != 0) return false;

  const int n = tb.requestFrom((int)TOUCH_I2C_ADDR, 8);
  if (n != 8) return false;
  for (int i = 0; i < 8; ++i) data[i] = (uint8_t)tb.read();

  pointsOut = data[1];
  if (pointsOut == 0 || pointsOut > 1) return false;

  const int16_t rawX = ((data[2] & 0x0F) << 8) | data[3];
  const int16_t rawY = ((data[4] & 0x0F) << 8) | data[5];
  g_touchRawX = rawX;
  g_touchRawY = rawY;
  // AXS touch returns 0xFFF coordinates when no finger is present.
  if (rawX >= 0x0FFF || rawY >= 0x0FFF) return false;
  // Some panels intermittently report a phantom (0,0) point when idle.
  if (rawX == 0 && rawY == 0) return false;
  // Canonical desk orientation: USB-C on the left side.
  // For this orientation (software rotated display), use direct XY with mirror.
  int32_t tx = (int32_t)DB_CANVAS_W - 1 - (int32_t)rawX;
  int32_t ty = (int32_t)DB_CANVAS_H - 1 - (int32_t)rawY;
  if (tx < 0) tx = 0;
  if (ty < 0) ty = 0;
  if (tx >= DB_CANVAS_W) tx = DB_CANVAS_W - 1;
  if (ty >= DB_CANVAS_H) ty = DB_CANVAS_H - 1;
  lx = (int16_t)tx;
  ly = (int16_t)ty;
  if (lx < 0) lx = 0;
  if (ly < 0) ly = 0;
  if (lx >= DB_CANVAS_W) lx = DB_CANVAS_W - 1;
  if (ly >= DB_CANVAS_H) ly = DB_CANVAS_H - 1;
  return true;
}

static void updateTouchInfoLabels(uint8_t points, uint32_t durMs, const char *phase) {
  if (!g_touchInfo1 || !g_touchInfo2 || !g_touchInfo3) return;

  char b1[96];
  snprintf(b1, sizeof(b1), "Touch %s  bus=%s  points=%u  swMin=%d",
           phase,
           g_touchUseAltBus ? "ALT" : "MAIN",
           points,
           g_swipeMinPx);
  lv_label_set_text(g_touchInfo1, b1);

  char b2[128];
  const int dx = g_touchLastX - g_touchStartX;
  const int dy = g_touchLastY - g_touchStartY;
  snprintf(b2, sizeof(b2), "start(%d,%d) now(%d,%d)  dx=%d dy=%d  dur=%lums",
           g_touchStartX, g_touchStartY,
           g_touchLastX, g_touchLastY,
           dx, dy,
           (unsigned long)durMs);
  lv_label_set_text(g_touchInfo2, b2);

  char b3[96];
  snprintf(b3, sizeof(b3), "swipes=%lu  last=%s", (unsigned long)g_swipeCount, g_lastSwipe);
  lv_label_set_text(g_touchInfo3, b3);
}

static void clearScene() {
  if (g_sceneRoot) lv_obj_del(g_sceneRoot);
  g_sceneRoot = lv_obj_create(lv_scr_act());
  lv_obj_set_size(g_sceneRoot, DB_CANVAS_W, DB_CANVAS_H);
  lv_obj_set_pos(g_sceneRoot, 0, 0);
  lv_obj_set_style_radius(g_sceneRoot, 0, 0);
  lv_obj_set_style_border_width(g_sceneRoot, 0, 0);
  lv_obj_set_style_pad_all(g_sceneRoot, 0, 0);
  lv_obj_clear_flag(g_sceneRoot, LV_OBJ_FLAG_SCROLLABLE);

  g_fpsLabel = lv_label_create(g_sceneRoot);
  lv_obj_set_style_text_font(g_fpsLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(g_fpsLabel, lv_color_hex(0xD5DAE5), 0);
  lv_obj_align(g_fpsLabel, LV_ALIGN_BOTTOM_RIGHT, -8, -4);
  lv_label_set_text(g_fpsLabel, "fps --");
}

static void buildColorScene() {
  clearScene();
  lv_obj_set_style_bg_color(g_sceneRoot, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(g_sceneRoot, LV_OPA_COVER, 0);

  const uint32_t cols[] = {0xFF0000, 0x00FF00, 0x0000FF, 0x00FFFF, 0xFF00FF, 0xFFFF00};
  const int barW = DB_CANVAS_W / 6;
  for (int i = 0; i < 6; ++i) {
    lv_obj_t *bar = lv_obj_create(g_sceneRoot);
    lv_obj_set_size(bar, barW + 1, DB_CANVAS_H);
    lv_obj_set_pos(bar, i * barW, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(cols[i]), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
  }

  lv_obj_t *title = lv_label_create(g_sceneRoot);
  lv_label_set_text(title, "LVGL COLOR SANITY");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);
}

static void buildTextScene() {
  clearScene();
  lv_obj_set_style_bg_color(g_sceneRoot, lv_color_hex(0x171B2F), 0);
  lv_obj_set_style_bg_opa(g_sceneRoot, LV_OPA_COVER, 0);

  lv_obj_t *l1 = lv_label_create(g_sceneRoot);
  lv_label_set_text(l1, "Open test: small");
  lv_obj_set_style_text_font(l1, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l1, lv_color_hex(0xB6C2E6), 0);
  lv_obj_align(l1, LV_ALIGN_TOP_LEFT, 14, 16);

  lv_obj_t *l2 = lv_label_create(g_sceneRoot);
  lv_label_set_text(l2, "Text rendering 18");
  lv_obj_set_style_text_font(l2, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(l2, lv_color_hex(0xE6ECFF), 0);
  lv_obj_align_to(l2, l1, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);

  lv_obj_t *l3 = lv_label_create(g_sceneRoot);
  lv_label_set_text(l3, "AA 24 - Crisp check");
  lv_obj_set_style_text_font(l3, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(l3, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align_to(l3, l2, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);

  lv_obj_t *line = lv_obj_create(g_sceneRoot);
  lv_obj_set_size(line, 300, 1);
  lv_obj_set_pos(line, 14, 120);
  lv_obj_set_style_bg_color(line, lv_color_hex(0x4E5D91), 0);
  lv_obj_set_style_bg_opa(line, LV_OPA_80, 0);
  lv_obj_set_style_border_width(line, 0, 0);
}

static void buildWidgetScene() {
  clearScene();
  lv_obj_set_style_bg_color(g_sceneRoot, lv_color_hex(0x1C2042), 0);
  lv_obj_set_style_bg_grad_color(g_sceneRoot, lv_color_hex(0x2D3A72), 0);
  lv_obj_set_style_bg_grad_dir(g_sceneRoot, LV_GRAD_DIR_HOR, 0);
  lv_obj_set_style_bg_opa(g_sceneRoot, LV_OPA_COVER, 0);

  lv_obj_t *card = lv_obj_create(g_sceneRoot);
  lv_obj_set_size(card, 248, 146);
  lv_obj_set_pos(card, DB_CANVAS_W - 268, 13);
  lv_obj_set_style_radius(card, 20, 0);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x2C5BCC), 0);
  lv_obj_set_style_bg_grad_color(card, lv_color_hex(0x5D8EF8), 0);
  lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_shadow_width(card, 14, 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);

  lv_obj_t *hdr = lv_obj_create(card);
  lv_obj_set_size(hdr, 248, 36);
  lv_obj_set_pos(hdr, 0, 0);
  lv_obj_set_style_radius(hdr, 20, 0);
  lv_obj_set_style_bg_color(hdr, lv_color_hex(0xF4F8FF), 0);
  lv_obj_set_style_border_width(hdr, 0, 0);

  lv_obj_t *city = lv_label_create(hdr);
  lv_label_set_text(city, "Luino");
  lv_obj_set_style_text_font(city, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(city, lv_color_hex(0x2A3464), 0);
  lv_obj_align(city, LV_ALIGN_LEFT_MID, 12, 0);

  lv_obj_t *temp = lv_label_create(card);
  lv_label_set_text(temp, "10 C");
  lv_obj_set_style_text_font(temp, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(temp, lv_color_white(), 0);
  lv_obj_align(temp, LV_ALIGN_TOP_LEFT, 12, 44);

  lv_obj_t *desc = lv_label_create(card);
  lv_label_set_text(desc, "Coperto\nUmidita 74%");
  lv_obj_set_style_text_font(desc, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(desc, lv_color_hex(0xEAF0FF), 0);
  lv_obj_align(desc, LV_ALIGN_TOP_LEFT, 12, 86);
}

static void animBarCb(void *obj, int32_t v) {
  lv_obj_set_x((lv_obj_t *)obj, v);
}

static void buildAnimScene() {
  clearScene();
  lv_obj_set_style_bg_color(g_sceneRoot, lv_color_hex(0x0F1328), 0);
  lv_obj_set_style_bg_opa(g_sceneRoot, LV_OPA_COVER, 0);

  lv_obj_t *title = lv_label_create(g_sceneRoot);
  lv_label_set_text(title, "Animation sanity");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0xDCE5FF), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 10);

  g_animBar = lv_obj_create(g_sceneRoot);
  lv_obj_set_size(g_animBar, 110, 20);
  lv_obj_set_pos(g_animBar, 10, 78);
  lv_obj_set_style_radius(g_animBar, 10, 0);
  lv_obj_set_style_bg_color(g_animBar, lv_color_hex(0x45C1FF), 0);
  lv_obj_set_style_bg_opa(g_animBar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_animBar, 0, 0);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, g_animBar);
  lv_anim_set_exec_cb(&a, animBarCb);
  lv_anim_set_values(&a, 12, DB_CANVAS_W - 130);
  lv_anim_set_time(&a, 900);
  lv_anim_set_playback_time(&a, 900);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&a);
}

static void buildStressScene() {
  clearScene();
  lv_obj_set_style_bg_color(g_sceneRoot, lv_color_hex(0x090B14), 0);
  lv_obj_set_style_bg_opa(g_sceneRoot, LV_OPA_COVER, 0);

  lv_obj_t *title = lv_label_create(g_sceneRoot);
  lv_label_set_text(title, "LVGL stress: redraw + labels");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0xE2E8FA), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 12, 12);

  for (int i = 0; i < 18; ++i) {
    lv_obj_t *t = lv_label_create(g_sceneRoot);
    char buf[32];
    snprintf(buf, sizeof(buf), "row %02d  color test", i);
    lv_label_set_text(t, buf);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(t, lv_color_hex((i % 2) ? 0xB6CCFF : 0xDDE7FF), 0);
    lv_obj_set_pos(t, 14 + ((i % 3) * 200), 42 + ((i / 3) * 20));
  }
}

static void buildTouchScene() {
  clearScene();
  lv_obj_set_style_bg_color(g_sceneRoot, lv_color_hex(0x0C1233), 0);
  lv_obj_set_style_bg_opa(g_sceneRoot, LV_OPA_COVER, 0);

  lv_obj_t *title = lv_label_create(g_sceneRoot);
  lv_label_set_text(title, "TOUCH SWIPE DEBUG");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0xEAF1FF), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 12, 10);

  lv_obj_t *hint = lv_label_create(g_sceneRoot);
  lv_label_set_text(hint, "Swipe in any direction. Serial: HELP / SWMIN <px> / SWSTAT");
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(hint, lv_color_hex(0xAFC4FF), 0);
  lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 12, 42);

  g_touchInfo1 = lv_label_create(g_sceneRoot);
  lv_obj_set_style_text_font(g_touchInfo1, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(g_touchInfo1, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(g_touchInfo1, LV_ALIGN_TOP_LEFT, 12, 72);
  lv_label_set_text(g_touchInfo1, "Touch idle");

  g_touchInfo2 = lv_label_create(g_sceneRoot);
  lv_obj_set_style_text_font(g_touchInfo2, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(g_touchInfo2, lv_color_hex(0xDCE7FF), 0);
  lv_obj_align(g_touchInfo2, LV_ALIGN_TOP_LEFT, 12, 96);
  lv_label_set_text(g_touchInfo2, "start(0,0) now(0,0) dx=0 dy=0 dur=0ms");

  g_touchInfo3 = lv_label_create(g_sceneRoot);
  lv_obj_set_style_text_font(g_touchInfo3, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(g_touchInfo3, lv_color_hex(0xBFD0FF), 0);
  lv_obj_align(g_touchInfo3, LV_ALIGN_TOP_LEFT, 12, 118);
  lv_label_set_text(g_touchInfo3, "swipes=0 last=NONE");

  g_touchDot = lv_obj_create(g_sceneRoot);
  lv_obj_set_size(g_touchDot, 10, 10);
  lv_obj_set_style_radius(g_touchDot, 10, 0);
  lv_obj_set_style_bg_color(g_touchDot, lv_color_hex(0x45C9FF), 0);
  lv_obj_set_style_bg_opa(g_touchDot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_touchDot, 0, 0);
  lv_obj_set_style_shadow_width(g_touchDot, 0, 0);
  lv_obj_set_style_pad_all(g_touchDot, 0, 0);
  lv_obj_add_flag(g_touchDot, LV_OBJ_FLAG_HIDDEN);
}

static void buildScene(uint8_t scene) {
  switch (scene) {
    case 0: buildColorScene(); break;
    case 1: buildTextScene(); break;
    case 2: buildWidgetScene(); break;
    case 3: buildAnimScene(); break;
    case 5: buildTouchScene(); break;
    default: buildStressScene(); break;
  }
}

static void initSceneOrder() {
#if VAL_TOUCH_DEBUG
  g_sceneOrder[g_sceneCount++] = 5;
  return;
#endif
#if VAL_TEST_COLOR
  g_sceneOrder[g_sceneCount++] = 0;
#endif
#if VAL_TEST_TEXT
  g_sceneOrder[g_sceneCount++] = 1;
#endif
#if VAL_TEST_WIDGET
  g_sceneOrder[g_sceneCount++] = 2;
#endif
#if VAL_TEST_ANIM
  g_sceneOrder[g_sceneCount++] = 3;
#endif
#if VAL_TEST_STRESS
  g_sceneOrder[g_sceneCount++] = 4;
#endif
  if (g_sceneCount == 0) g_sceneOrder[g_sceneCount++] = 0;
}

static void pollValidationCommands() {
  static char buf[48];
  static uint8_t len = 0;

  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '\r' || c == '\n') {
      if (len == 0) continue;
      buf[len] = '\0';
      len = 0;

      String cmd(buf);
      cmd.trim();
      if (cmd.length() == 0) continue;
      cmd.toUpperCase();

      if (cmd == "HELP") {
        Serial.println("[VAL][CMD] HELP, SWSTAT, SWMIN <px>");
        continue;
      }

      if (cmd == "SWSTAT") {
        Serial.printf("[VAL][SW] ready=%d bus=%s min=%d count=%lu last=%s\n",
                      g_touchReady ? 1 : 0,
                      g_touchUseAltBus ? "ALT" : "MAIN",
                      g_swipeMinPx,
                      (unsigned long)g_swipeCount,
                      g_lastSwipe);
        continue;
      }

      if (cmd.startsWith("SWMIN")) {
        int sp = cmd.indexOf(' ');
        if (sp > 0) {
          const int v = cmd.substring(sp + 1).toInt();
          if (v >= 8 && v <= 120) {
            g_swipeMinPx = (int16_t)v;
            Serial.printf("[VAL][SW] min set to %d px\n", g_swipeMinPx);
          } else {
            Serial.println("[VAL][SW][ERR] range 8..120");
          }
        } else {
          Serial.printf("[VAL][SW] min=%d px\n", g_swipeMinPx);
        }
        continue;
      }

      Serial.printf("[VAL][CMD][WARN] unknown: %s\n", buf);
      continue;
    }

    if (len < (sizeof(buf) - 1)) {
      buf[len++] = c;
    }
  }
}

static void runTouchDebugLoop(uint32_t now) {
#if !VAL_TOUCH_DEBUG
  (void)now;
  return;
#else
  if (!g_touchReady) return;
  if (g_sceneOrder[g_sceneIdx] != 5) return;

  int16_t x = 0;
  int16_t y = 0;
  uint8_t points = 0;
  const bool touched = readTouchLogicalPoint(x, y, points);

  if (touched) {
    if (!g_touchDown) {
      g_touchDown = true;
      g_touchStartX = x;
      g_touchStartY = y;
      g_touchStartMs = now;
      Serial.printf("[VAL][TOUCH] DOWN x=%d y=%d rawX=%d rawY=%d points=%u\n",
                    x, y, g_touchRawX, g_touchRawY, points);
    }
    g_touchLastX = x;
    g_touchLastY = y;

    if (g_touchDot) {
      lv_obj_clear_flag(g_touchDot, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(g_touchDot, x - 5, y - 5);
    }

    if ((now - g_lastTouchLogMs) >= VAL_TOUCH_SAMPLE_LOG_MS) {
      g_lastTouchLogMs = now;
      const uint32_t dur = now - g_touchStartMs;
      updateTouchInfoLabels(points, dur, "DOWN");
      Serial.printf("[VAL][TOUCH] MOVE x=%d y=%d rawX=%d rawY=%d dx=%d dy=%d dur=%lums\n",
                    x, y,
                    g_touchRawX, g_touchRawY,
                    g_touchLastX - g_touchStartX,
                    g_touchLastY - g_touchStartY,
                    (unsigned long)dur);
    }
    return;
  }

  if (!g_touchDown) return;

  g_touchDown = false;
  const int dx = g_touchLastX - g_touchStartX;
  const int dy = g_touchLastY - g_touchStartY;
  const uint32_t dur = now - g_touchStartMs;
  const bool swipe = (dur <= VAL_SWIPE_MAX_MS) && (abs(dx) >= g_swipeMinPx || abs(dy) >= g_swipeMinPx);

  if (swipe) {
    if (abs(dx) >= abs(dy)) {
      strncpy(g_lastSwipe, (dx < 0) ? "LEFT" : "RIGHT", sizeof(g_lastSwipe) - 1);
    } else {
      strncpy(g_lastSwipe, (dy < 0) ? "UP" : "DOWN", sizeof(g_lastSwipe) - 1);
    }
    g_lastSwipe[sizeof(g_lastSwipe) - 1] = '\0';
    g_swipeCount++;
  } else {
    strncpy(g_lastSwipe, "NO_SWIPE", sizeof(g_lastSwipe) - 1);
    g_lastSwipe[sizeof(g_lastSwipe) - 1] = '\0';
  }

  updateTouchInfoLabels(0, dur, "UP");
  if (g_touchDot) lv_obj_add_flag(g_touchDot, LV_OBJ_FLAG_HIDDEN);
  Serial.printf("[VAL][TOUCH] UP dx=%d dy=%d dur=%lums -> %s (count=%lu)\n",
                dx, dy, (unsigned long)dur, g_lastSwipe, (unsigned long)g_swipeCount);
#endif
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n[VAL] ScryBar LVGL Validation boot");

  if (!initDisplay()) {
    Serial.println("[VAL][ERR] display init failed");
    while (true) delay(1000);
  }

  lv_init();

  const uint32_t bufPx = (uint32_t)DB_CANVAS_W * (uint32_t)DB_CANVAS_H;
  g_lvglBuf1 = (lv_color_t*)heap_caps_malloc(bufPx * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!g_lvglBuf1) g_lvglBuf1 = (lv_color_t*)malloc(bufPx * sizeof(lv_color_t));
  if (!g_lvglBuf1) {
    Serial.println("[VAL][ERR] lvgl buffer alloc failed");
    while (true) delay(1000);
  }

  lv_disp_draw_buf_init(&g_lvglDrawBuf, g_lvglBuf1, nullptr, bufPx);
  lv_disp_drv_init(&g_lvglDispDrv);
  g_lvglDispDrv.hor_res = DB_CANVAS_W;
  g_lvglDispDrv.ver_res = DB_CANVAS_H;
  g_lvglDispDrv.flush_cb = lvglDisplayFlushCb;
  g_lvglDispDrv.draw_buf = &g_lvglDrawBuf;
  g_lvglDispDrv.full_refresh = 1;
  lv_disp_drv_register(&g_lvglDispDrv);

  initSceneOrder();
  buildScene(g_sceneOrder[g_sceneIdx]);
  g_lastSceneMs = millis();
  g_lastFpsMs = millis();

#if VAL_TOUCH_DEBUG
  initTouchDebugInput();
#endif

  Serial.printf("[VAL] LVGL ready. color_depth=%d color_size=%u scenes=%u\n", LV_COLOR_DEPTH, (unsigned)sizeof(lv_color_t), g_sceneCount);
}

void loop() {
  static uint32_t lastMs = 0;
  const uint32_t now = millis();
  uint32_t dt = now - lastMs;
  if (dt > 50) dt = 50;
  if (dt == 0) dt = 1;
  lastMs = now;

  lv_tick_inc(dt);
  lv_timer_handler();

#if VAL_TOUCH_DEBUG
  pollValidationCommands();
  runTouchDebugLoop(now);
#endif

#if !VAL_TOUCH_DEBUG
  if ((now - g_lastSceneMs) >= (VAL_SCENE_SECONDS * 1000UL)) {
    g_sceneIdx = (uint8_t)((g_sceneIdx + 1) % g_sceneCount);
    buildScene(g_sceneOrder[g_sceneIdx]);
    g_lastSceneMs = now;
    Serial.printf("[VAL] scene=%u\n", g_sceneOrder[g_sceneIdx]);
  }
#endif

  if ((now - g_lastFpsMs) >= 1000UL) {
    char fpsBuf[24];
    snprintf(fpsBuf, sizeof(fpsBuf), "fps %lu", (unsigned long)g_flushCount);
    if (g_fpsLabel) lv_label_set_text(g_fpsLabel, fpsBuf);
    Serial.printf("[VAL] fps=%lu free_heap=%u free_psram=%u\n",
                  (unsigned long)g_flushCount,
                  ESP.getFreeHeap(), ESP.getFreePsram());
    g_flushCount = 0;
    g_lastFpsMs = now;
  }

  delay(5);
}
