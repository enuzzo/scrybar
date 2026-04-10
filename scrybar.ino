#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include "src/ui_strings.h"
#include "src/lang_types.h"
#include "src/sketch_fwd.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "config.h"
#if __has_include(<HWCDC.h>)
#include <HWCDC.h>
#endif
#if TEST_BATTERY
#include "esp_adc/adc_oneshot.h"
#endif

#if TEST_DISPLAY
#include "assets/netmilk_logo/netmilk_logo_283x152_rgb565.h"
#include "assets/img_test/cover-test-150-rgb565.h"
#if !TEST_LVGL_UI
#include "assets/weather_demo/weather_icons_mini_rgb565.h"
#endif
// ANSI art viewer removed (archived in archive/ansi/)
#include "src/fonts/font_ibmvga8x16.h" // CP437 font kept for DOOM HUD text
#if __has_include("src/doom/scrybar_prboom_runtime.h") && __has_include("src/doom/prboom/doomtype.h")
#include "src/doom/scrybar_prboom_runtime.h"
#define DB_HAS_PRBOOM_DONOR 1
#else
#define DB_HAS_PRBOOM_DONOR 0
#endif
#if __has_include("src/doom/doom_titlepic.h")
#include "src/doom/doom_titlepic.h"
#define DB_HAS_DOOM_SPIKE_ASSETS 1
#else
#define DB_HAS_DOOM_SPIKE_ASSETS 0
#endif
#endif

#if TEST_WIFI || TEST_NTP
#include <WiFi.h>
#include <time.h>
#endif

#if TEST_WIFI
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <pngle.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#if __has_include(<mbedtls/base64.h>)
#include <mbedtls/base64.h>
#include <mbedtls/platform.h>
#define DB_HAS_MBEDTLS_BASE64 1
#else
#define DB_HAS_MBEDTLS_BASE64 0
#endif
#define DB_HAS_MDNS 1
#include <Preferences.h>
#include <FFat.h>
#if __has_include(<qrcodegen.h>)
#include <qrcodegen.h>
#define DB_HAS_QRCODEGEN 1
#elif __has_include("qrcodegen.h")
#include "qrcodegen.h"
#define DB_HAS_QRCODEGEN 1
#elif __has_include(<extra/libs/qrcode/qrcodegen.h>)
#include <extra/libs/qrcode/qrcodegen.h>
#define DB_HAS_QRCODEGEN 1
#else
#define DB_HAS_QRCODEGEN 0
#endif
#endif

#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI
#include <lvgl.h>
// Funnel Display — unified typeface across all themes
LV_FONT_DECLARE(scry_font_funnel_display_12);
LV_FONT_DECLARE(scry_font_funnel_display_14);
LV_FONT_DECLARE(scry_font_funnel_display_16);
LV_FONT_DECLARE(scry_font_funnel_display_18);
LV_FONT_DECLARE(scry_font_funnel_display_20);
LV_FONT_DECLARE(scry_font_funnel_display_22);
LV_FONT_DECLARE(scry_font_funnel_display_23);
LV_FONT_DECLARE(scry_font_funnel_display_24);
LV_FONT_DECLARE(scry_font_funnel_display_25);
LV_FONT_DECLARE(scry_font_funnel_display_30);
LV_FONT_DECLARE(scry_font_funnel_display_32);
LV_FONT_DECLARE(scry_font_funnel_display_38);
LV_FONT_DECLARE(scry_font_funnel_display_countdown_60);
#if __has_include("assets/weather_icons_min/generated/weather_icons_lvgl_min.h")
#include "assets/weather_icons_min/generated/weather_icons_lvgl_min.h"
#define DB_HAS_LVGL_WEATHER_MIN_IMAGES 1
#else
#define DB_HAS_LVGL_WEATHER_MIN_IMAGES 0
#endif
#if __has_include("assets/weather_icons/generated/weather_icons_lvgl_local.h")
#include "assets/weather_icons/generated/weather_icons_lvgl_local.h"
#define DB_HAS_LVGL_WEATHER_IMAGES 1
#define DB_LVGL_WEATHER_ICON_SET "local"
#elif __has_include("assets/weather_demo/weather_images.h") && defined(LVGL_VERSION_MAJOR) && LVGL_VERSION_MAJOR >= 9
// weather_images.h uses LVGL v9 API (lv_image_dsc_t, LV_IMAGE_HEADER_MAGIC) — skip on v8
#include "assets/weather_demo/weather_images.h"
#define DB_HAS_LVGL_WEATHER_IMAGES 1
#define DB_LVGL_WEATHER_ICON_SET "demo"
#else
#define DB_HAS_LVGL_WEATHER_IMAGES 0
#define DB_LVGL_WEATHER_ICON_SET "none"
#endif
#endif

#if __has_include("esp_arduino_version.h")
#include "esp_arduino_version.h"
#endif

#if TEST_DISPLAY && !DISPLAY_BACKEND_ESP_LCD
#include <Arduino_GFX_Library.h>
#define HAS_ARDUINO_GFX 1
#else
#define HAS_ARDUINO_GFX 0
#endif

#if TEST_DISPLAY && DISPLAY_BACKEND_ESP_LCD
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "src/axs15231b/esp_lcd_axs15231b.h"
#endif

#if TEST_IMU
#include <SensorQMI8658.hpp>
#endif

#if __has_include("secrets.h")
#include "secrets.h"
#endif

// (dead RSS_FAVICON_CACHE_SIZE / MAX_BYTES / RETRY_MS removed in r216 — actual cache uses kFaviconCacheSlots)
#ifndef RSS_THUMB_CACHE_SIZE
#define RSS_THUMB_CACHE_SIZE 3
#endif
#if RSS_THUMB_CACHE_SIZE < 1
#undef RSS_THUMB_CACHE_SIZE
#define RSS_THUMB_CACHE_SIZE 1
#endif
#ifndef RSS_THUMB_MAX_BYTES
#define RSS_THUMB_MAX_BYTES 24576
#endif
#ifndef RSS_THUMB_RETRY_MS
#define RSS_THUMB_RETRY_MS 300000UL
#endif
static TwoWire I2C_MAIN(0);
static TwoWire I2C_ALT(1);
static bool g_backlightReady = false;
struct PwrButtonState {
  bool down = false;
  uint32_t downMs = 0;
  bool holdReported = false;
  bool ignoreUntilRelease = false;
  int lastRawLevel = -1;
  uint32_t pressCandidateMs = 0;
  uint32_t releaseCandidateMs = 0;
  // Power-hold progress overlay (shown after kPwrFeedbackDelayMs)
  lv_obj_t *ovBar = nullptr;   // progress bar on lv_layer_top()
  lv_obj_t *ovBg  = nullptr;   // dark strip behind bar
};
static PwrButtonState g_pwrBtn;
static constexpr uint32_t kPwrPressDebounceMs  = 45UL;
static constexpr uint32_t kPwrShortPressMinMs  = 70UL;
static constexpr uint32_t kPwrFeedbackDelayMs  = 300UL;  // hold before overlay appears
static constexpr int16_t  kPwrBarH             = 6;      // progress bar height (px)
struct NavButtonState {
  bool down = false;
  uint32_t downMs = 0;
  int lastRawLevel = -1;
  uint32_t releaseCandidateMs = 0;
};
static NavButtonState g_navBtn;
static bool g_softPowerOff = false;
#if TEST_BATTERY
struct BatteryState {
  adc_oneshot_unit_handle_t adcHandle = nullptr;
  bool ready = false;
  bool hasSample = false;
  int raw = 0;
  float voltage = 0.0f;
  int percent = -1;
  uint32_t lastSampleMs = 0;
  bool chargingLikely = false;
  uint32_t trendMs = 0;
  float trendVoltage = 0.0f;
  bool externalPowerLikely = false;
  uint32_t externalPowerHoldUntilMs = 0;
  bool energySaverActive = false;
  uint32_t energyLastEvalMs = 0;
};
static BatteryState g_batt;
#endif

// Energy saver fields folded into BatteryState (g_batt)
#if TEST_WIFI || TEST_NTP
static constexpr uint8_t WIFI_STATIC_CREDENTIALS_MAX = 5;
static constexpr uint8_t WIFI_TOTAL_CREDENTIALS_MAX = WIFI_STATIC_CREDENTIALS_MAX + WIFI_RUNTIME_CREDENTIALS_MAX;
static constexpr size_t WIFI_MAX_SSID_LEN = 32;
static constexpr size_t WIFI_MAX_PASSWORD_LEN = 64;
struct RuntimeWiFiCredential {
  char ssid[WIFI_MAX_SSID_LEN + 1] = {0};
  char password[WIFI_MAX_PASSWORD_LEN + 1] = {0};
};
struct WifiState {
  bool connected = false;
  int lastDiscReason = -1;
  bool eventRegistered = false;
  bool everConnected = false;
  uint32_t lastConnectMs = 0;
  uint32_t lastDisconnectMs = 0;
  const char *staticSsids[WIFI_STATIC_CREDENTIALS_MAX] = {nullptr};
  const char *staticPasswords[WIFI_STATIC_CREDENTIALS_MAX] = {nullptr};
  size_t staticCredCount = 0;
  RuntimeWiFiCredential runtimeCreds[WIFI_RUNTIME_CREDENTIALS_MAX];
  uint8_t runtimeCredCount = 0;
  const char *credSsids[WIFI_TOTAL_CREDENTIALS_MAX] = {nullptr};
  const char *credPasswords[WIFI_TOTAL_CREDENTIALS_MAX] = {nullptr};
  size_t credCount = 0;
  char preferredSsid[33] = "";
  char setupMode[8] = WIFI_SETUP_MODE_DEFAULT;
  bool setupApActive = false;
  bool setupApAutoStarted = false;
  uint32_t noLinkSinceMs = 0;
  char setupApSsid[33] = "";
  bool reconnectAttemptActive = false;
  uint8_t reconnectIdx = 0;
  uint32_t reconnectAttemptStartMs = 0;
  uint32_t reconnectNextAttemptMs = 0;
  uint16_t consecutiveFailCount = 0;
  uint32_t lastRadioResetMs = 0;
  bool internalDisconnect = false;
  uint32_t lastNtpAttemptMs = 0;
};
static WifiState g_wifiSt;
#endif

static constexpr size_t UI_THEME_ID_LEN = 24;

struct UiThemeWebTokens {
  const char *fontMain;
  const char *fontMono;
  const char *bgDeepest;
  const char *bgDeep;
  const char *bgSurface;
  const char *line;
  const char *lineSoft;
  const char *txt;
  const char *txt2;
  const char *txt3;
  const char *acc1;
  const char *acc2;
  const char *okbg;
  const char *secBg;
  const char *border;
  const char *heroBorder;
  const char *heroCopyBorder;
  const char *heroCopyBg;
  const char *releaseBorder;
  const char *releaseBg;
  const char *releaseKey;
  const char *releaseValue;
  const char *gridLineA;
  const char *gridLineB;
  const char *gridGlowA;
  const char *gridGlowB;
  const char *gridHorizonA;
  const char *gridHorizonB;
  const char *scanline;
  const char *vlineA;
  const char *vlineB;
  const char *btnGhostBg;
  const char *btnGhostText;
};

struct UiThemeLvglTokens {
  uint32_t screenBg;
  uint32_t panelBg;
  uint32_t headerBg;
  uint32_t headerText;
  uint32_t headerMeta;
  uint32_t weatherCardBg;
  uint32_t weatherTextPrimary;
  uint32_t weatherTextSecondary;
  uint32_t weatherGlyphOnline;
  uint32_t weatherGlyphOffline;
  uint32_t divider;
  uint32_t forecastText;
  uint32_t infoBg;
  uint32_t infoHeaderBg;
  uint32_t infoHeaderBorder;
  uint32_t infoText;
  uint32_t infoQrDark;
  uint32_t infoQrLight;
  uint32_t auxText;
  uint32_t auxMeta;
  uint32_t auxSourceText;
  uint32_t auxWhenText;
  uint32_t auxBadgeBg;
  uint32_t auxBadgeText;
  uint32_t auxQrBtnBg;
  uint32_t auxQrBtnPressedBg;
  uint32_t auxQrBtnText;
  uint32_t auxQrBtnPressedText;
  uint32_t auxRefreshBtnBg;
  uint32_t auxRefreshBtnPressedBg;
  uint32_t auxRefreshBtnText;
  uint32_t auxNextBtnBg;
  uint32_t auxNextBtnPressedBg;
  uint32_t auxNextBtnText;
  uint32_t auxQrDark;
  uint32_t auxQrLight;
  uint32_t auxQrHint;
  uint32_t wifiBarOff;
  uint32_t wifiBarOn;
  uint32_t wifiBarWave;
  uint32_t saverSky;
  uint32_t saverField;
  uint32_t saverCow;
  uint32_t saverBalloon;
  uint32_t saverTail;
  uint32_t saverFooter;
  uint32_t saverStarLow;
  uint32_t saverStarMid;
  uint32_t saverStarHigh;
};

struct UiThemeDefinition {
  const char *id;
  const char *label;
  UiThemeWebTokens web;
  UiThemeLvglTokens lvgl;
};

static const UiThemeDefinition kUiThemes[] = {
  {
    "scrybar-default",
    "ScryBar Default",
    {
      "'Montserrat',-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif",
      "ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,'Liberation Mono',monospace",
      "#070D2D",
      "#0B1437",
      "#111C44",
      "rgba(255,255,255,.11)",
      "rgba(255,255,255,.07)",
      "#FFFFFF",
      "#A3AED0",
      "#707EAE",
      "#7551FF",
      "#39B8FF",
      "rgba(1,181,116,.14)",
      "rgba(11,20,55,0.72)",
      "rgba(57,184,255,0.18)",
      "rgba(117,81,255,.34)",
      "rgba(57,184,255,.22)",
      "rgba(9,16,44,.36)",
      "rgba(117,81,255,.42)",
      "rgba(7,13,38,.72)",
      "#79d8ff",
      "#e9f0ff",
      "rgba(117,81,255,0.20)",
      "rgba(57,184,255,0.18)",
      "rgba(117,81,255,0.18)",
      "rgba(57,184,255,0.08)",
      "rgba(117,81,255,0.40)",
      "rgba(57,184,255,0.34)",
      "rgba(57,184,255,0.32)",
      "rgba(117,81,255,0.20)",
      "rgba(57,184,255,0.18)",
      "#1A2558",
      "#d9e4ff",
    },
    {
      0x000000, 0x101B44, 0x2D3F82, 0xFFFFFF, 0xAFC2F5, 0xC6DBFF, 0x1B2D63, 0x2C4784,
      0x1B2D63, 0xAAAAAA, 0x9FB5EE, 0x1B2D63, 0x000000, 0x232833, 0x3A4150, 0xEAF0FF,
      0xF6FBFF, 0x000000, 0xEAF0FF, 0xAFC2F5, 0x6FD8FF, 0x9FB5EE, 0x2B468E, 0xFFFFFF, 0xFFD34D, 0xFFF19A,
      0x1E2F63, 0x0B1E4B, 0x6FD8FF, 0xB9ECFF, 0x113063, 0x7B63FF, 0x9E8EFF, 0xF7F2FF,
      0x162B63, 0xF7FAFF, 0xEAF2FF, 0x1B2D63, 0xFFFFFF, 0xC8D6FF, 0xBFD3FF, 0x66FFB3,
      0xF3F7FF, 0xF4F7FF, 0xFFFFFF, 0x97B0E4, 0xB9B27A, 0xDDD58A, 0xFFF3B0,
    },
  },
  {
    "cyberpunk-2077",
    "Cyberpunk 2077",
    {
      "'Space Mono','Monaco','Menlo',Consolas,monospace",
      "'Space Mono','Monaco','Menlo',Consolas,monospace",
      "#04070F",
      "#091523",
      "#0E2133",
      "rgba(255,237,77,.16)",
      "rgba(73,232,255,.11)",
      "#F5F9EE",
      "#9ED6D3",
      "#5FA1A8",
      "#FFE600",
      "#27E1FF",
      "rgba(39,225,255,.14)",
      "rgba(7,18,29,0.78)",
      "rgba(39,225,255,0.28)",
      "rgba(255,230,0,.35)",
      "rgba(39,225,255,.34)",
      "rgba(7,18,29,.52)",
      "rgba(255,230,0,.48)",
      "rgba(4,12,21,.86)",
      "#4ee7ff",
      "#f7f3a0",
      "rgba(255,230,0,0.20)",
      "rgba(39,225,255,0.20)",
      "rgba(255,230,0,0.18)",
      "rgba(39,225,255,0.10)",
      "rgba(255,230,0,0.45)",
      "rgba(39,225,255,0.36)",
      "rgba(39,225,255,0.32)",
      "rgba(255,230,0,0.20)",
      "rgba(39,225,255,0.20)",
      "#10253A",
      "#D6FFF6",
    },
    {
      0x04070F, 0x0B1A2A, 0x103048, 0xFFF59A, 0x7FE7FF, 0x122A3F, 0xFFF19A, 0x82EFFF,
      0xFFE85A, 0x6C8696, 0x38DFFF, 0xFFF6B0, 0x04070F, 0x132539, 0xF2DB4A, 0xE6FCFF,
      0xF1E94A, 0x04070F, 0xE4FCFF, 0x93EFFF, 0x33E1FF, 0x1A8296, 0x1B3D57, 0xE9FFFE, 0xFFE600, 0xFFF6A8,
      0x122C42, 0x102338, 0x33E1FF, 0x99F3FF, 0x0C2740, 0x28B0D5, 0x7CDBF0, 0x042134,
      0x1C415A, 0xF4FFFE, 0xD6FFF8, 0x2A3A4C, 0xA4A139, 0x0F6272, 0x1A8296, 0x7CFFBE,
      0xFFF8C5, 0xE8FEFF, 0xFFF8C5, 0x7EB7E8, 0x9CA660, 0xD8DD8A, 0xFFF7B0,
    },
  },
  {
    "toxic-candy",
    "Toxic Candy",
    {
      "'Delius Unicase','Chakra Petch','Montserrat','Segoe UI',sans-serif",
      "'Space Mono','Monaco','Menlo',Consolas,monospace",
      "#130816",
      "#1D0A21",
      "#301238",
      "rgba(211,0,255,.18)",
      "rgba(168,255,77,.10)",
      "#FFF5FF",
      "#E3C6F1",
      "#AB85BD",
      "#FF37D5",
      "#9BFF2F",
      "rgba(155,255,47,.18)",
      "rgba(32,11,42,0.76)",
      "rgba(155,255,47,0.26)",
      "rgba(255,55,213,.40)",
      "rgba(155,255,47,.35)",
      "rgba(32,11,42,.58)",
      "rgba(155,255,47,.50)",
      "rgba(24,7,31,.84)",
      "#9BFF2F",
      "#FFD7FB",
      "rgba(255,55,213,0.22)",
      "rgba(155,255,47,0.17)",
      "rgba(255,55,213,0.20)",
      "rgba(155,255,47,0.11)",
      "rgba(255,55,213,0.50)",
      "rgba(155,255,47,0.42)",
      "rgba(155,255,47,0.35)",
      "rgba(255,55,213,0.22)",
      "rgba(155,255,47,0.22)",
      "#3A1446",
      "#F7E6FF",
    },
    {
      0x130816, 0x2A1034, 0x4A1558, 0xFFD9FB, 0xC5FF63, 0x3A1846, 0xF8F1FF, 0xB8FF65,
      0xFF6BDE, 0xA994B1, 0x9BFF2F, 0xD9FF8A, 0x130816, 0x32113D, 0x9BFF2F, 0xFCEBFF,
      0x9BFF2F, 0x130816, 0xFCEBFF, 0xD3B8E8, 0x9BFF2F, 0xC8FF88, 0x5A2172, 0xFCEBFF, 0xFF37D5, 0xFF95E8,
      0x2A0732, 0x25052B, 0x9BFF2F, 0xC8FF88, 0x1A2E00, 0xD300FF, 0xF07CFF, 0xFFF5FF,
      0x3B0E47, 0xEAFFD2, 0xFBEAFF, 0x573264, 0x9BFF2F, 0xFF4BDE, 0xE8C9FF, 0x9BFF2F,
      0xFFF0FF, 0xFFCBF7, 0xFFF0FF, 0xC5A3D9, 0x8B5AA0, 0xC37BDD, 0xFFBBF2,
    },
  },
  {
    "tokyo-transit",
    "Tokyo Transit",
    {
      "'Chakra Petch','Space Mono','Montserrat','Segoe UI',sans-serif",
      "'Space Mono','Monaco','Menlo',Consolas,monospace",
      "#060D1A",
      "#0B1730",
      "#132741",
      "rgba(0,209,255,.18)",
      "rgba(255,63,129,.11)",
      "#EAF3FF",
      "#A9C3E6",
      "#7598C4",
      "#00D1FF",
      "#FF3F81",
      "rgba(52,227,154,.14)",
      "rgba(12,24,43,0.78)",
      "rgba(0,209,255,.28)",
      "rgba(255,63,129,.36)",
      "rgba(0,209,255,.32)",
      "rgba(10,21,36,.54)",
      "rgba(0,209,255,.42)",
      "rgba(7,14,24,.82)",
      "#7BE8FF",
      "#F4FAFF",
      "rgba(0,209,255,0.20)",
      "rgba(255,63,129,0.16)",
      "rgba(0,209,255,0.20)",
      "rgba(255,63,129,0.10)",
      "rgba(0,209,255,0.45)",
      "rgba(255,63,129,0.36)",
      "rgba(0,209,255,0.32)",
      "rgba(0,209,255,0.22)",
      "rgba(255,63,129,0.20)",
      "#17355A",
      "#EAF3FF",
    },
    {
      0x060D1A, 0x132741, 0x00D1FF, 0x081624, 0x1A4B66, 0xF3F8FF, 0x162A3F, 0x355572,
      0x1C3A57, 0x71879D, 0x3D5E82, 0x1C3A57, 0x060D1A, 0x1A3760, 0x00D1FF, 0xEAF3FF,
      0xF6FBFF, 0x0B1730, 0xEAF3FF, 0xA9C3E6, 0x00D1FF, 0xFF76A9, 0x1B3A62, 0xEAF3FF, 0x17355A, 0x102A47,
      0x00D1FF, 0x4EE2FF, 0x3A1E42, 0x2B1631, 0xFF8BB6, 0x17355A, 0x102A47, 0xEAF3FF,
      0x081624, 0xF6FBFF, 0x95B9D6, 0x374A62, 0x00D1FF, 0xFF3F81, 0x132741, 0x1B3A62,
      0x00D1FF, 0xFF3F81, 0xEAF3FF, 0xA9C3E6, 0x6A8BB6, 0x8CC7EF, 0xF4FAFF,
    },
  },
  {
    "minimal-brutalist-mono",
    "Minimal Brutalist Mono",
    {
      "'IBM Plex Mono','Space Mono','Montserrat','Segoe UI',monospace",
      "'IBM Plex Mono','Space Mono','Monaco','Menlo',monospace",
      "#0A0A0A",
      "#111111",
      "#1A1A1A",
      "rgba(255,255,255,.16)",
      "rgba(255,255,255,.08)",
      "#F5F5F5",
      "#CFCFCF",
      "#8F8F8F",
      "#F1F1F1",
      "#FF3B30",
      "rgba(22,22,22,.90)",
      "rgba(20,20,20,0.82)",
      "rgba(255,255,255,.22)",
      "rgba(255,255,255,.28)",
      "rgba(255,255,255,.18)",
      "rgba(15,15,15,.66)",
      "rgba(255,255,255,.24)",
      "rgba(10,10,10,.88)",
      "#BDBDBD",
      "#F5F5F5",
      "rgba(255,255,255,0.18)",
      "rgba(255,59,48,0.18)",
      "rgba(255,255,255,0.10)",
      "rgba(255,59,48,0.10)",
      "rgba(255,255,255,0.36)",
      "rgba(255,59,48,0.30)",
      "rgba(255,255,255,0.28)",
      "rgba(255,255,255,0.14)",
      "rgba(255,59,48,0.14)",
      "#1E1E1E",
      "#F1F1F1",
    },
    {
      0x0A0A0A, 0x171717, 0xEFEFEF, 0x111111, 0x5C5C5C, 0xF4F4F4, 0x111111, 0x444444,
      0x111111, 0x666666, 0xB0B0B0, 0x111111, 0x0F0F0F, 0xE8E8E8, 0xB8B8B8, 0xF3F3F3,
      0x111111, 0xF4F4F4, 0xF2F2F2, 0xBDBDBD, 0xFF3B30, 0xF06B63, 0x2A2A2A, 0xF3F3F3, 0x1E1E1E, 0x2A2A2A,
      0xF3F3F3, 0xFFFFFF, 0x171717, 0x2B2B2B, 0xF3F3F3, 0x2C2C2C, 0x3A3A3A, 0xF3F3F3,
      0x111111, 0xF5F5F5, 0xB5B5B5, 0x3A3A3A, 0xF3F3F3, 0xFF3B30, 0x0C0C0C, 0x181818,
      0xF2F2F2, 0xFF3B30, 0xF2F2F2, 0x2A2A2A, 0x8A8A8A, 0xCFCFCF, 0xFFFFFF,
    },
  },
  // ── Mint Protocol (LIGHT — first light theme, Vibemilk DS v3) ──
  {
    "mint-protocol",
    "Mint Protocol",
    {
      "'Encode Sans Semi Expanded','Montserrat',-apple-system,BlinkMacSystemFont,sans-serif",
      "'IBM Plex Mono','Space Mono','SF Mono','Fira Code',monospace",
      "#DBE8DB", "#E7F0E7", "#F5F8F3",
      "rgba(88,133,92,.12)", "rgba(88,133,92,.06)",
      "#15311B", "#28432D", "#58855C", "#58855C", "#ADEBB3",
      "rgba(93,190,106,.16)", "rgba(219,232,219,0.72)",
      "rgba(88,133,92,0.16)", "rgba(88,133,92,.22)",
      "rgba(169,162,217,.28)", "rgba(245,248,243,.68)",
      "rgba(88,133,92,.30)", "rgba(219,232,219,.82)",
      "#58855C", "#15311B",
      "rgba(88,133,92,0.10)", "rgba(169,162,217,0.10)",
      "rgba(173,235,179,0.18)", "rgba(215,163,186,0.08)",
      "rgba(88,133,92,0.20)", "rgba(169,162,217,0.18)",
      "rgba(215,163,186,0.16)",
      "rgba(88,133,92,0.12)", "rgba(169,162,217,0.12)",
      "#EEF4ED", "#28432D",
    },
    {
      // LIGHT: screenBg darker mint → white cards float on top
      0xDBE8DB, 0xFFFFFF, 0x58855C, 0xF7FBF7, 0xDBE8DB, 0xEEF7EF, 0x15311B, 0x28432D,
      0x28432D, 0x718A75, 0x58855C, 0x15311B, 0xFFFFFF, 0x58855C, 0x456A49, 0x15311B,
      0x15311B, 0xF5F8F3, 0x15311B, 0x58855C, 0x456A49, 0x718A75, 0x58855C, 0xF7FBF7, 0xDBE8DB, 0xCFDECF,
      0x15311B, 0x15311B, 0xADEBB3, 0xCDF5D0, 0x15311B, 0x58855C, 0x456A49, 0xF7FBF7,
      0x15311B, 0xF5F8F3, 0x718A75, 0xDBE8DB, 0x58855C, 0x5DBE6A, 0xE7F0E7, 0xDBE8DB,
      0x58855C, 0xADEBB3, 0x15311B, 0x58855C, 0x718A75, 0xA9A2D9, 0xD7A3BA,
    },
  },
  // ── Cathode Ray (DARK — phosphor green CRT, Vibemilk DS v3) ──
  {
    "cathode-ray",
    "Cathode Ray",
    {
      "'Space Mono','Monaco','Menlo','Consolas','Liberation Mono',monospace",
      "'Space Mono','Monaco','Menlo','Consolas','Liberation Mono',monospace",
      "#0A0A08", "#0D120A", "#141C10",
      "rgba(51,255,51,.18)", "rgba(255,170,0,.10)",
      "#33FF33", "#22AA22", "#447744", "#33FF33", "#FFAA00",
      "rgba(51,255,51,.14)", "rgba(10,10,8,0.92)",
      "rgba(51,255,51,0.22)", "rgba(51,255,51,.30)",
      "rgba(255,170,0,.22)", "rgba(10,10,8,.52)",
      "rgba(51,255,51,.38)", "rgba(10,10,8,.82)",
      "#FFAA00", "#33FF33",
      "rgba(51,255,51,0.22)", "rgba(255,170,0,0.14)",
      "rgba(51,255,51,0.20)", "rgba(255,170,0,0.10)",
      "rgba(255,170,0,0.45)", "rgba(51,255,51,0.35)",
      "rgba(51,255,51,0.40)",
      "rgba(51,255,51,0.20)", "rgba(255,170,0,0.14)",
      "#141C10", "#33FF33",
    },
    {
      // DARK CRT: phosphor green #33FF33 + amber #FFAA00 on near-black
      0x0A0A08, 0x141C10, 0x1E2A1A, 0x33FF33, 0x22AA22, 0xE0F5D0, 0x0A0A08, 0x2D5A2D,
      0x141C10, 0x447744, 0x33FF33, 0x0A0A08, 0x0A0A08, 0x1E2A1A, 0x33FF33, 0x33FF33,
      0x33FF33, 0x0A0A08, 0x33FF33, 0x22AA22, 0xFFAA00, 0x447744, 0x1E2A1A, 0x33FF33, 0x33FF33, 0x66FF66,
      0x0A0A08, 0x0A0A08, 0xFFAA00, 0xFFCC44, 0x0A0A08, 0x1E2A1A, 0x243220, 0x33FF33,
      0x0A0A08, 0x33FF33, 0x447744, 0x141C10, 0x33FF33, 0xFFAA00, 0x0A0A08, 0x0D120A,
      0x33FF33, 0xFFAA00, 0x33FF33, 0x22AA22, 0x2D5A2D, 0x447744, 0xFFAA00,
    },
  },
};
static constexpr size_t UI_THEME_COUNT = sizeof(kUiThemes) / sizeof(kUiThemes[0]);
static uint8_t g_uiThemeIndex = 0;

static int8_t findUiThemeIndexById(const char *id) {
  if (!id || !id[0]) return -1;
  for (size_t i = 0; i < UI_THEME_COUNT; ++i) {
    if (strcmp(kUiThemes[i].id, id) == 0) return (int8_t)i;
  }
  return -1;
}

static const UiThemeDefinition &uiThemeByIndex(uint8_t idx) {
  if (idx >= UI_THEME_COUNT) idx = 0;
  return kUiThemes[idx];
}

static const UiThemeDefinition &activeUiTheme() {
  return uiThemeByIndex(g_uiThemeIndex);
}

static const char *activeUiThemeId() {
  return activeUiTheme().id;
}

static const char *activeUiThemeLabel() {
  return activeUiTheme().label;
}

static void setActiveUiThemeById(const char *id) {
  const int8_t idx = findUiThemeIndexById(id);
  g_uiThemeIndex = (idx >= 0) ? (uint8_t)idx : 0;
}

#if TEST_WIFI
static constexpr uint8_t RSS_MAX_ITEMS = 9;
static constexpr uint8_t RSS_FEED_SLOT_COUNT = 5;
static constexpr uint8_t RSS_FEED_NAME_LEN = 24;
static constexpr uint16_t RSS_FEED_URL_LEN = 280;
static constexpr uint8_t RSS_DEFAULT_FEED_ITEMS = 3;
static constexpr uint8_t WIKI_FEED_SLOT_COUNT = 3;
static const char *kWikiFeedName[WIKI_FEED_SLOT_COUNT] = {
    "Wiki Featured",
    "Wiki OnThisDay",
    "Wiki Random"
};
// URLs built dynamically from g_wikiLang in updateWikiFromFeed()

struct RuntimeRssFeedConfig {
  char name[RSS_FEED_NAME_LEN];
  char url[RSS_FEED_URL_LEN];
  uint8_t maxItems = RSS_DEFAULT_FEED_ITEMS;
};

static constexpr uint8_t UI_VIEW_FLAG_INFO = 0x01;
static constexpr uint8_t UI_VIEW_FLAG_AUX  = 0x02;
static constexpr uint8_t UI_VIEW_FLAG_WIKI = 0x04;
// 0x08 was UI_VIEW_FLAG_ANSI (archived)
static constexpr uint8_t UI_VIEW_FLAG_DOOM        = 0x10;
static constexpr uint8_t UI_VIEW_FLAG_NOW_PLAYING  = 0x20;
static constexpr uint8_t UI_VIEW_FLAG_TRANSIT = 0x40;  // auto-enables via g_transitConfig.configured
static constexpr uint8_t UI_VIEW_MASK_DEFAULT =
    UI_VIEW_FLAG_INFO |
    UI_VIEW_FLAG_AUX |
    UI_VIEW_FLAG_WIKI |
    UI_VIEW_FLAG_DOOM |
    UI_VIEW_FLAG_NOW_PLAYING;

struct RuntimeNetConfig {
  char weatherCity[32];
  float weatherLat = WEATHER_LAT;
  float weatherLon = WEATHER_LON;
  RuntimeRssFeedConfig rssFeeds[RSS_FEED_SLOT_COUNT];
  char logoUrl[220];
  char uiTheme[UI_THEME_ID_LEN];
  uint8_t enabledViewsMask = UI_VIEW_MASK_DEFAULT;
  bool ready = false;
};
static RuntimeNetConfig g_runtimeNetConfig = {};
static bool g_runtimeNetConfigNvsLoaded = false;
static char g_wordClockLang[16] = WORD_CLOCK_LANG_DEFAULT;
static char g_wikiLang[8] = "en";
// ── Canonical language whitelist (single source of truth) ────────────────────
static const char* const kAllowedLangs[] = {"it", "tlh", "en", "fr", "de", "es", "pt", "la", "eo", "l33t", "sha", "val", "bellazio", "pir", nullptr};
static bool isValidLangCode(const String &code) {
  for (int i = 0; kAllowedLangs[i]; ++i) { if (code == kAllowedLangs[i]) return true; }
  return false;
}
enum UiPageMode : uint8_t;
static bool uiPageEnabledNoEnsure(UiPageMode mode);
static UiPageMode uiLastEnabledMainViewNoEnsure();
static void setUiPage(UiPageMode mode);
#if TEST_IMU
static void syncImuActiveForUi();
#endif
#if WEB_CONFIG_ENABLED
struct WebConfigState {
  WebServer server{WEB_CONFIG_PORT};
  bool serverStarted = false;
  bool routesRegistered = false;
  DNSServer dnsServer;
  bool dnsStarted = false;
#if DB_HAS_MDNS
  bool mdnsStarted = false;
  char mdnsHost[32] = {0};
  char mdnsInstanceName[40] = {0};
#endif
#if DB_HAS_QRCODEGEN
  uint8_t *qrTempBuf = nullptr;
  uint8_t *qrDataBuf = nullptr;
#endif
};
static WebConfigState g_webCfg;
#endif

static bool updateRssFromFeed(bool force);
static bool updateWikiFromFeed(bool force);
static bool updateWeatherFromApi(bool force);
static void handleWebConfigServerLoop();
static void formatCityLabel(const char *src, char *out, size_t outLen);
static const char *runtimeUiThemeId();
static const char *runtimeUiThemeLabel();
static bool runWiFiConnectTest();
#if TEST_WIFI && WEB_CONFIG_ENABLED
static void handleWebNowPlayingGetApi();
static void handleWebNowPlayingPostApi();
static bool applyNowPlayingPayloadJson(const String &body, String &err);
static void ensureScryBarMdnsStarted();
static void stopScryBarMdns();
#endif
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI
static void lvglApplyThemeStyles(bool forceInvalidate);
static void updateLvglUi(bool force);
static void runLvglLoop();
#endif
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI && SCREENSAVER_ENABLED
static void markUserInteraction(uint32_t nowMs);
static void lvglSetScreenSaverActive(bool on);
static void handleScreenSaverLoop(uint32_t nowMs);
#endif
#if TEST_DISPLAY && TEST_NTP
static void updateDisplayClock(bool force);
#endif

static inline void pumpWebUiDuringIo() {
#if TEST_WIFI
  handleWebConfigServerLoop();
#endif
}

struct WeatherState {
  bool valid = false;
  float tempC = 0.0f;
  int humidity = 0;
  int weatherCode = -1;
  bool isDay = true;
  float windKmh = 0.0f;
  char sunrise[6] = "--:--";
  char sunset[6] = "--:--";
  int nextTemp[3] = {0, 0, 0};
  int nextCode[3] = {-1, -1, -1};
  bool nextValid[3] = {false, false, false};
  int tomorrowTemp = 0;
  int tomorrowCode = -1;
  bool tomorrowValid = false;
  uint32_t lastFetchMs = 0;
  bool dirty = false;  // set by netTask, cleared by UI update
};
static WeatherState g_weather;

struct RssItem {
  char title[220];
  char link[280];
  char pubDate[32];
  char summary[420];
  uint8_t feedSlot = 0xFF;
  bool wikiMetaReady = false;
  bool wikiMetaTried = false;
};
struct RssState {
  bool valid = false;
  RssItem items[RSS_MAX_ITEMS];
  uint8_t itemCount = 0;
  uint8_t currentIndex = 0;
  char fetchedAt[20];
  uint32_t lastFetchMs = 0;
  uint32_t lastAttemptMs = 0;
  uint32_t lastRotateMs = 0;
  int lastHttpCode = 0;
  bool dirty = false;  // set by netTask, cleared by UI update
};
static RssState g_rss = {};
static RssState g_wiki = {};

struct TransitState {
  TransitDeparture departures[TRANSIT_MAX_DEPARTURES];
  uint8_t  count = 0;
  bool     valid = false;
  bool     dirty = false;
  uint32_t lastFetchMs = 0;
  uint32_t lastAttemptMs = 0;
  int      lastHttpCode = 0;
  char     fetchedAt[12] = "--:--";
  char     stationName[TRANSIT_STATION_LEN] = {};  // official name from API
};
static TransitState  g_transitState = {};
static TransitConfig g_transitConfig = {};
// r242: tap transit view to toggle origin/terminus display (auto-reverts after 8 s)
static bool     g_transitOrgMode   = false;
static uint32_t g_transitOrgModeMs = 0;

static RssItem *g_rssParseBuf = nullptr;
static uint32_t g_wikiMetaPreloadLastMs = 0;
static uint32_t g_wikiVisiblePreloadLastMs = 0;

// --- Network background task (Core 1) ---
enum NetRequestType : uint8_t {
  NET_REQ_WEATHER = 0,
  NET_REQ_RSS,
  NET_REQ_WIKI,
  NET_REQ_FAVICON,
  NET_REQ_WIKI_META,
  NET_REQ_TRANSIT_POLL,
  NET_REQ_LAUNCH_POLL,
};
struct NetRequest {
  NetRequestType type;
  uint8_t        param;
};

static QueueHandle_t    g_netQueue  = nullptr;
static SemaphoreHandle_t g_netMutex = nullptr;
static TaskHandle_t     g_netTaskHandle = nullptr;
static bool             g_netTaskReady = false;

// Forward declarations — must appear after WeatherState/NetRequestType to
// prevent the Arduino auto-prototyper from generating broken prototypes.
static bool netFetchWeather(WeatherState &out);
static void netFetchRss();
static void netFetchWiki();
static void netFetchFavicons();
static void netFetchWikiMeta();
static void netFetchTransitDepartures();
static bool updateTransitFromApi(bool force);
static void netFetchLaunchData();
static bool updateLaunchFromApi(bool force);
static bool netEnqueue(NetRequestType type, uint8_t param);
static void netTaskMain(void *param);
#endif

#if TEST_NTP
enum UiClockMode : uint8_t {
  UI_CLOCK_MODE_CLOCKCLOCK = 0,
  UI_CLOCK_MODE_WORDCLOCK = 1,
};
struct ClockState {
  bool ntpSynced = false;
  int lastSecond = -1;
  int lastDateKey = -1;
  bool staticDrawn = false;
  uint32_t bellazioLastMinuteKey = 0xFFFFFFFFu;
  uint8_t bellazioLastLeadIdx = 0;
  uint8_t bellazioLastCloserIdx = 0;
  UiClockMode mode = UI_CLOCK_MODE_WORDCLOCK;
};
static ClockState g_clock;
enum UiPageMode : uint8_t {
  UI_PAGE_INFO = 0,
  UI_PAGE_HOME = 1,
  UI_PAGE_AUX = 2,
  UI_PAGE_WIKI = 3,
  UI_PAGE_NOW_PLAYING = 4,
  UI_PAGE_DOOM = 5,
  UI_PAGE_TRANSIT = 6,
  UI_PAGE_LAUNCH  = 7,
};
static UiPageMode g_uiPageMode = UI_PAGE_HOME;
static bool g_uiNeedsRedraw = true;
#endif

#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI
static bool g_lvglReady = false;
static uint32_t g_lvglLastTickMs = 0;
static lv_disp_draw_buf_t g_lvglDrawBuf;
static lv_disp_drv_t g_lvglDispDrv;
static lv_color_t *g_lvglBuf1 = nullptr;
static lv_color_t *g_lvglBuf2 = nullptr;  // Phase 2: double-buffering
struct LvglClockUi {
  lv_obj_t *l1 = nullptr;
  lv_obj_t *l2 = nullptr;
  lv_obj_t *l3 = nullptr;
  lv_obj_t *date = nullptr;
  lv_obj_t *header = nullptr;
  lv_obj_t *headerFill = nullptr;
  lv_obj_t *wifiBars[4] = {nullptr, nullptr, nullptr, nullptr};
  uint16_t wifiMask = 0xFFFF;
  lv_obj_t *divider = nullptr;
  lv_obj_t *block = nullptr;
};
static LvglClockUi g_clockUi;
struct LvglInfoUi {
  lv_obj_t *root = nullptr;
  lv_obj_t *card = nullptr;
  lv_obj_t *header = nullptr;
  lv_obj_t *headerFill = nullptr;
  lv_obj_t *title = nullptr;
  lv_obj_t *endpoint = nullptr;
  lv_obj_t *bodyLeft = nullptr;
  lv_obj_t *bodyRight = nullptr;
#if defined(LV_USE_QRCODE) && LV_USE_QRCODE
  lv_obj_t *webQr = nullptr;
#endif
  char lastQrPayload[96] = {0};
};
static LvglInfoUi g_infoUi;
static lv_obj_t *g_lvglHomeRoot = nullptr;
struct LvglWeatherUi {
  lv_obj_t *card = nullptr;
  lv_obj_t *header = nullptr;
  lv_obj_t *headerFill = nullptr;
  lv_obj_t *body = nullptr;
  lv_obj_t *city = nullptr;
  bool cityTickerScroll = false;
  uint32_t cityTickerNextMs = 0;
  uint32_t cityTickerEndMs = 0;
  char cityRawLast[48] = {0};
  lv_obj_t *temp = nullptr;
  lv_obj_t *icon = nullptr;
  lv_obj_t *glyph = nullptr;
  lv_obj_t *desc = nullptr;
  lv_obj_t *humidity = nullptr;
  lv_obj_t *sun = nullptr;
  lv_obj_t *wind = nullptr;
  lv_obj_t *sep = nullptr;
  lv_obj_t *forecastBar = nullptr;
  lv_obj_t *forecastBarFill = nullptr;
  lv_obj_t *forecastIcon = nullptr;
  lv_obj_t *forecastNow = nullptr;
  lv_obj_t *forecastTomorrow = nullptr;
};
static LvglWeatherUi g_weatherUi;
// ── FeedDeckUi — shared widget/state bundle for AUX and WIKI decks ──────────
struct FeedDeckUi {
  lv_obj_t *card         = nullptr;
  lv_obj_t *header       = nullptr;
  lv_obj_t *headerFill   = nullptr;
  lv_obj_t *title        = nullptr;
  lv_obj_t *status       = nullptr;
  lv_obj_t *feedIcon     = nullptr;  // AUX only; nullptr for Wiki
  lv_obj_t *qrBtn        = nullptr;
  lv_obj_t *qrBtnText    = nullptr;
  lv_obj_t *refreshBtn   = nullptr;
  lv_obj_t *refreshBtnText = nullptr;
  lv_obj_t *nextFeedBtn  = nullptr;
  lv_obj_t *nextFeedBtnText = nullptr;
  lv_obj_t *sourceBadge  = nullptr;
  lv_obj_t *sourceBadgeText = nullptr;
  lv_obj_t *sourceBadgeImg = nullptr;  // favicon lv_img (shown over text when available)
  lv_obj_t *sourceSite   = nullptr;
  lv_obj_t *news         = nullptr;
  lv_obj_t *meta         = nullptr;
  lv_obj_t *qrOverlay    = nullptr;
  lv_obj_t *qrHint       = nullptr;
#if defined(LV_USE_QRCODE) && LV_USE_QRCODE
  lv_obj_t *qr           = nullptr;
#endif
  int16_t   lastItemShown  = -1;
  char      lastQrPayload[280] = {0};
  bool      qrModalOpen    = false;
};
static FeedDeckUi g_auxDeck;
static FeedDeckUi g_wikiDeck;
struct FakeNowPlayingTrack;
struct NowPlayingUi {
  lv_obj_t *card = nullptr;
  lv_obj_t *header = nullptr;
  lv_obj_t *headerFill = nullptr;
  lv_obj_t *title = nullptr;
  lv_obj_t *statusDot = nullptr;
  lv_obj_t *status = nullptr;
  lv_obj_t *headerTime = nullptr;
  lv_obj_t *coverShell = nullptr;
  lv_obj_t *cover = nullptr;
  lv_obj_t *coverImage = nullptr;
  lv_obj_t *coverStripe = nullptr;
  lv_obj_t *coverOrb = nullptr;
  lv_obj_t *coverTop = nullptr;
  lv_obj_t *coverBottom = nullptr;
  lv_obj_t *track = nullptr;
  lv_obj_t *artist = nullptr;
  lv_obj_t *album = nullptr;
  lv_obj_t *source = nullptr;
  lv_obj_t *progressRail = nullptr;
  lv_obj_t *progressFill = nullptr;
  lv_obj_t *progressElapsed = nullptr;
  lv_obj_t *progressRemaining = nullptr;
  int8_t lastTrackIndex = -1;
  uint32_t lastLiveToken = 0;
  uint16_t lastDisplayedElapsed = 0;
  bool lastUsingLive = false;
  bool lastInSync = false;
};
struct LiveNowPlayingState {
  bool valid = false;
  bool isPlaying = false;
  bool inSync = false;
  uint32_t receivedAtMs = 0;
  uint32_t contentToken = 0;
  uint16_t durationSec = 0;
  uint16_t elapsedSec = 0;
  char title[220] = {0};
  char artist[220] = {0};
  char album[160] = {0};
  char source[48] = {0};
  char appName[48] = {0};
  char artworkUrl[220] = {0};
  char artworkId[256] = {0};
};
struct LiveNowPlayingArtwork {
  bool valid = false;
  uint16_t width = 0;
  uint16_t height = 0;
  size_t dataSize = 0;
  uint8_t *data = nullptr;
  uint32_t bgColor = 0x101418;
  char artworkId[256] = {0};
};
struct FakeNowPlayingTrack {
  const char *title;
  const char *artist;
  const char *album;
  const char *source;
  const char *coverTop;
  const char *coverBottom;
  uint16_t durationSec;
  uint16_t baseElapsedSec;
  uint32_t pageBgA;
  uint32_t pageBgB;
  uint32_t coverBgA;
  uint32_t coverBgB;
  uint32_t coverStripe;
  uint32_t coverOrb;
  uint32_t coverText;
  uint32_t progressFill;
};
static NowPlayingUi g_nowPlayingUi;
static LiveNowPlayingState g_liveNowPlaying = {};
static LiveNowPlayingArtwork g_liveNowPlayingArtwork = {};
static uint32_t g_liveNowPlayingTokenSeq = 0;
static lv_obj_t *g_lvglAuxRoot = nullptr;
static lv_obj_t *g_lvglWikiRoot = nullptr;
static lv_obj_t *g_lvglNowPlayingRoot = nullptr;
static lv_obj_t *g_lvglDoomRoot = nullptr;
struct LvglTransitUi {
  lv_obj_t *root = nullptr;
  lv_obj_t *header = nullptr;
  lv_obj_t *headerFill = nullptr;
  lv_obj_t *title = nullptr;
  lv_obj_t *station = nullptr;
  lv_obj_t *status = nullptr;
  lv_obj_t *rowBg[TRANSIT_MAX_DEPARTURES] = {};
  lv_obj_t *rowSep[TRANSIT_MAX_DEPARTURES] = {};
  lv_obj_t *lineBg[TRANSIT_MAX_DEPARTURES] = {};
  lv_obj_t *line_[TRANSIT_MAX_DEPARTURES] = {};
  lv_obj_t *dest[TRANSIT_MAX_DEPARTURES] = {};
  lv_obj_t *time_[TRANSIT_MAX_DEPARTURES] = {};
  lv_obj_t *arr[TRANSIT_MAX_DEPARTURES] = {};
  lv_obj_t *delay[TRANSIT_MAX_DEPARTURES] = {};
  lv_obj_t *platform[TRANSIT_MAX_DEPARTURES] = {};
  lv_obj_t *noData = nullptr;
};
static LvglTransitUi g_transitUi;
static lv_obj_t *g_lvglTransitRoot = nullptr;

// --- Launch Page State ---
struct LaunchState {
  LaunchItem items[LAUNCH_MAX_ITEMS];
  uint8_t  count = 0;
  bool     valid = false;
  bool     dirty = false;
  uint32_t lastFetchMs = 0;
  uint32_t lastAttemptMs = 0;
  int      lastHttpCode = 0;
  char     fetchedAt[12] = "--:--";
};
static LaunchState g_launchState = {};

struct LvglLaunchUi {
  // Shared
  lv_obj_t *header = nullptr;
  lv_obj_t *title = nullptr;
  lv_obj_t *headerCenter = nullptr;
  lv_obj_t *fetchTime = nullptr;
  lv_obj_t *noData = nullptr;
  // View 0: Hero (two-column, full detail for next launch)
  lv_obj_t *heroBg = nullptr;
  lv_obj_t *heroBadge = nullptr;
  lv_obj_t *heroBadgeLabel = nullptr;
  lv_obj_t *heroName = nullptr;
  lv_obj_t *heroVehiclePad = nullptr;
  lv_obj_t *heroCountdown = nullptr;
  lv_obj_t *heroLocation = nullptr;
  lv_obj_t *heroCountry = nullptr;
  lv_obj_t *heroWeather = nullptr;
  lv_obj_t *heroWindow = nullptr;
  // View 1: Compact (2 rows for missions 2-3)
  lv_obj_t *compactBg[2] = {};
  lv_obj_t *compactBadge[2] = {};
  lv_obj_t *compactBadgeLabel[2] = {};
  lv_obj_t *compactName[2] = {};
  lv_obj_t *compactVehicle[2] = {};
  lv_obj_t *compactLocation[2] = {};
  lv_obj_t *compactDate[2] = {};
  lv_obj_t *compactSep = nullptr;
  // QR overlay (RSS-style)
  lv_obj_t *qrOverlay = nullptr;
  lv_obj_t *qr = nullptr;       // lv_canvas
  lv_obj_t *qrHint = nullptr;
  bool      qrModalOpen = false;
  int8_t    qrItemIndex = -1;
  char      lastQrPayload[128] = {};
  // View state
  uint8_t   viewIndex = 0;
  uint32_t  lastViewRotateMs = 0;
};
static LvglLaunchUi g_launchUi;
static lv_obj_t *g_lvglLaunchRoot = nullptr;
static constexpr uint32_t NOW_PLAYING_FAKE_ROTATE_MS = 18000UL;
static constexpr uint32_t NOW_PLAYING_SYNC_TTL_MS = 5000UL;
static const lv_img_dsc_t kNowPlayingRealCover150 = {
    .header = {
        .cf = LV_IMG_CF_TRUE_COLOR,
        .always_zero = 0,
        .reserved = 0,
        .w = 150,
        .h = 150,
    },
    .data_size = sizeof(assets_img_test_cover_test_150_rgb565),
    .data = assets_img_test_cover_test_150_rgb565,
};
static lv_img_dsc_t g_nowPlayingLiveCoverImage = {
    .header = {
        .cf = LV_IMG_CF_TRUE_COLOR,
        .always_zero = 0,
        .reserved = 0,
        .w = 150,
        .h = 150,
    },
    .data_size = 0,
    .data = nullptr,
};
static constexpr FakeNowPlayingTrack kFakeNowPlayingTracks[] = {
    {
        "The City Was Electric and the Night Smelled Like Rain (Extended Skyline Rebuild Mix)",
        "Marta Bellavita and the Extremely Overprepared Weather Satellites",
        "Paper Maps for Neon Highways and Other Late Decisions",
        "TIDAL",
        "CITY",
        "RAIN",
        567,
        221,
        0x3B0A52,
        0xD43A6C,
        0x121C67,
        0x2E9BFF,
        0xF8C145,
        0xFFE26A,
        0xF7F2FF,
        0xFFD24D,
    },
    {
        "Dancing Through Seven Overlapping Calendars in a Borrowed Neon Suit",
        "The Committee for Loud, Unreasonable, and Surprisingly Elegant Synthesizers",
        "This Floor Is Lava But Make It Tasteful",
        "Spotify",
        "SEVEN",
        "SUIT",
        734,
        418,
        0x153A5B,
        0x6E1FE8,
        0x051B32,
        0x16B6C9,
        0xFF5F7A,
        0x7CF4FF,
        0xEFFFFF,
        0x7CF4FF,
    },
    {
        "Archive of Warm Machines, Side B: Notes Left Inside the Last Working Cassette Library",
        "Her Future Ghost Orchestra Featuring Alessandro From Accounting On Portable Percussion",
        "Friendly Failures, Volume IV",
        "Podcasts",
        "TAPE",
        "GLOW",
        401,
        143,
        0x2B102D,
        0xD86A29,
        0x311339,
        0xF28C38,
        0x6EF2C4,
        0xFFD08A,
        0xFFF5E7,
        0x6EF2C4,
    },
};

static bool liveNowPlayingAvailable() {
  return g_liveNowPlaying.valid && g_liveNowPlaying.title[0] != '\0';
}

static bool liveNowPlayingDisplayInSync(uint32_t nowMs) {
  if (!liveNowPlayingAvailable()) return false;
  if (!g_liveNowPlaying.inSync) return false;
  return (uint32_t)(nowMs - g_liveNowPlaying.receivedAtMs) <= NOW_PLAYING_SYNC_TTL_MS;
}

static const char *liveNowPlayingSourceLabel() {
  if (g_liveNowPlaying.source[0]) return g_liveNowPlaying.source;
  if (g_liveNowPlaying.appName[0]) return g_liveNowPlaying.appName;
  return "Companion";
}
#if TEST_DISPLAY && DOOM_SPIKE_ENABLED
static constexpr uint8_t DOOM_TOUCH_NONE = 0;
static constexpr uint8_t DOOM_TOUCH_LEFT = 1;
static constexpr uint8_t DOOM_TOUCH_CENTER = 2;
static constexpr uint8_t DOOM_TOUCH_RIGHT = 3;
struct DoomState {
  bool paletteReady = false;
  bool frameDirty = true;
  bool launchRequested = false;
  uint16_t palette565[256] = {0};
  uint32_t lastRenderLogMs = 0;
  uint8_t touchZone = DOOM_TOUCH_NONE;
  bool tiltFilterReady = false;
  bool neutralPending = false;
  bool neutralReady = false;
  uint32_t neutralArmAtMs = 0;
  uint32_t neutralStableSinceMs = 0;
  uint32_t lastTiltSampleMs = 0;
  float moveTiltDeg = 0.0f;
  float turnTiltDeg = 0.0f;
  float neutralMoveTiltDeg = 0.0f;
  float neutralTurnTiltDeg = 0.0f;
  float neutralAccumMoveDeg = 0.0f;
  float neutralAccumTurnDeg = 0.0f;
  uint16_t neutralStableSamples = 0;
  bool axisFilterReady = false;
  float moveDeltaFilteredDeg = 0.0f;
  float turnDeltaFilteredDeg = 0.0f;
  int8_t moveBin = 0;
  int8_t turnBin = 0;
};
static DoomState g_doom;
static constexpr int16_t kDoomFrameW = ((LCD_WIDTH * 4) + 2) / 3;  // 4:3 content in 172px height
static constexpr int16_t kDoomFrameH = LCD_WIDTH;
static constexpr int16_t kDoomFrameX = (LCD_HEIGHT - kDoomFrameW) / 2;
static constexpr int16_t kDoomFrameY = 0;
static constexpr int16_t kDoomLeftBandW = kDoomFrameX;
static constexpr int16_t kDoomRightBandX = kDoomFrameX + kDoomFrameW;
static constexpr float   kDoomRadToDeg = 57.2957795f;
static constexpr float   kDoomTiltComplementaryAlpha = 0.94f;
static constexpr float   kDoomMoveEngageDeg = 8.0f;
static constexpr float   kDoomMoveReleaseDeg = 5.5f;
static constexpr float   kDoomTurnEngageDeg = 7.0f;
static constexpr float   kDoomTurnReleaseDeg = 4.5f;
static constexpr float   kDoomMoveBinDeg = 5.5f;
static constexpr float   kDoomTurnBinDeg = 5.0f;
static constexpr uint16_t kDoomNeutralCaptureDelayMs = 180;
static constexpr uint16_t kDoomNeutralStableWindowMs = 320;
static constexpr uint16_t kDoomNeutralStableMinSamples = 8;
static constexpr float   kDoomNeutralCaptureGyroMaxDps = 12.0f;
static constexpr float   kDoomAxisResponseAlpha = 0.16f;
static constexpr int8_t  kDoomMoveBinMin = -6;
static constexpr int8_t  kDoomMoveBinMax = 6;
static constexpr int8_t  kDoomTurnBinMin = -6;
static constexpr int8_t  kDoomTurnBinMax = 6;
static constexpr int8_t  kDoomMoveTiltSign = -1;
static constexpr int8_t  kDoomTurnTiltSign = 1;
// (tilt/neutral fields folded into DoomState above)
#endif
struct LvglPageAnimState {
  uint32_t untilMs = 0;
  uint32_t lastRunMs = 0;
  bool dragActive = false;
};
static LvglPageAnimState g_pageAnim;
#if SCREENSAVER_ENABLED
static constexpr uint8_t kSaverSkyRowsMax = 10;
static constexpr uint8_t kSaverSkyColsMax = 80;
static constexpr uint8_t kSaverStarsPerRow = 2;

enum CowState : uint8_t {
  COW_GRAZE    = 0,
  COW_IDLE     = 1,
  COW_SLEEP    = 2,
  COW_RUN      = 3,
  COW_STARE_UP = 4,
};

enum SaverEvent : uint8_t {
  SAVER_EVENT_NONE          = 0,
  SAVER_EVENT_SHOOTING_STAR = 1,
  SAVER_EVENT_UFO           = 2,
  SAVER_EVENT_SATELLITE     = 3,
  SAVER_EVENT_RAIN          = 4,
  SAVER_EVENT_MATRIX_GLITCH = 5,
};

enum SkyPhase : uint8_t {
  SKY_NIGHT = 0,
  SKY_DAWN  = 1,
  SKY_DAY   = 2,
  SKY_DUSK  = 3,
};

enum ThoughtCategory : uint8_t {
  THOUGHT_PHILOSOPHY = 0,
  THOUGHT_HACKER     = 1,
  THOUGHT_META       = 2,
  THOUGHT_WEATHER    = 3,
  THOUGHT_EASTER_EGG = 4,
};

struct ScreensaverState {
  lv_obj_t *root = nullptr;
  lv_obj_t *sky = nullptr;
  lv_obj_t *starObj[kSaverSkyRowsMax][kSaverStarsPerRow] = {};
  lv_obj_t *field = nullptr;
  lv_obj_t *cow = nullptr;
  lv_obj_t *balloon = nullptr;
  lv_obj_t *balloonTail = nullptr;
  lv_obj_t *footer = nullptr;
  bool active = false;
  uint32_t lastUserInteractionMs = 0;
  uint32_t lastStepMs = 0;
  uint32_t rand = 0x1A2B3C4Du;
  int16_t x = -80;
  int16_t y = 0;
  int8_t colorIdx = 0;
  uint32_t wakeGuardUntilMs = 0;
  uint32_t cowNextMoveMs = 0;
  uint8_t cowStepsLeft = 0;
  int8_t cowDir = 1;
  uint8_t cols = 60;
  uint8_t rows = 6;
  uint8_t starX[kSaverSkyRowsMax][kSaverStarsPerRow] = {};
  uint8_t starLevel[kSaverSkyRowsMax][kSaverStarsPerRow] = {};
  int8_t starDir[kSaverSkyRowsMax][kSaverStarsPerRow] = {};
  uint32_t starNextMs[kSaverSkyRowsMax][kSaverStarsPerRow] = {};
  uint8_t balloonIdx = 0;
  uint32_t balloonNextMs = 0;
  bool balloonVisible = false;
  uint32_t footerNextMs = 0;
  uint32_t footerJitterNextMs = 0;
  uint8_t footerJitterIdx = 0;
  uint32_t fieldNextMs = 0;
  uint8_t fieldScroll = 0;
  char fieldBuf[256] = {0};
  // --- Pasture Simulator additions ---
  uint32_t skyNextMs = 0;
  uint32_t cowChewNextMs = 0;
  uint32_t cowStateNextMs = 0;
  uint32_t eventEndMs = 0;
  uint32_t eventCooldownMs = 0;
  uint32_t starBorrowedMask = 0;
  uint32_t cloudNextMs = 0;
  int16_t  eventX = 0;
  uint8_t  skyPhase = 0;
  uint8_t  cowState = 0;
  uint8_t  cowChewFrame = 0;
  uint8_t  eventActive = 0;
  int8_t   eventDir = 1;
  uint8_t  thoughtCategory = 0;
  uint8_t  cloudOffset = 0;
  uint8_t  cowPrevState = 0;
};
static ScreensaverState g_saver;
#endif
#endif

#if TEST_TOUCH
enum TouchAuxButton : uint8_t {
  TOUCH_AUX_BTN_NONE = 0,
  TOUCH_AUX_BTN_QR = 1,
  TOUCH_AUX_BTN_REFRESH = 2,
  TOUCH_AUX_BTN_NEXT = 3,
};
struct TouchState {
  bool ready = false;
  bool useAltBus = true;
  bool down = false;
  uint8_t missCount = 0;
  uint8_t rawPresenceCount = 0;
  bool pageDragging = false;
  TouchAuxButton auxBtnDown = TOUCH_AUX_BTN_NONE;
  uint32_t lastSwipeToggleMs = 0;
  bool awaitRelease = false;
  uint32_t releaseStartMs = 0;
  int16_t startX = 0;
  int16_t startY = 0;
  int16_t lastX = 0;
  int16_t lastY = 0;
  uint32_t startMs = 0;
};
static TouchState g_touch;
#endif

#if TEST_IMU
struct ImuState {
  bool ready = false;
  bool sensorsActive = false;
  uint8_t addr = 0;
  uint32_t lastPrintMs = 0;
  uint32_t lastShakeMs = 0;
  float lastAccelMag = 1.0f;
};
static ImuState g_imu;
#endif

#if TEST_IMU
static SensorQMI8658 g_qmi;
#endif

#if TEST_DISPLAY
static constexpr uint16_t DB_COLOR_BLACK = 0x0000;
static constexpr uint16_t DB_COLOR_WHITE = 0xFFFF;
static constexpr uint16_t DB_COLOR_RED   = 0xF800;
static constexpr uint16_t DB_COLOR_GREEN = 0x07E0;
static constexpr uint16_t DB_COLOR_BLUE  = 0x001F;
static constexpr uint16_t DB_COLOR_GRAY  = 0x7BEF;
static constexpr uint16_t DB_COLOR_YELLOW = 0xFFE0;
#endif

#if HAS_ARDUINO_GFX
static Arduino_DataBus *g_qspiBus = nullptr;
static Arduino_GFX *g_gfx = nullptr;
#endif

#if TEST_DISPLAY && DISPLAY_BACKEND_ESP_LCD
struct DisplayHwState {
  esp_lcd_panel_io_handle_t panelIo = nullptr;
  esp_lcd_panel_handle_t panel = nullptr;
  SemaphoreHandle_t flushSem = nullptr;
  uint16_t *canvasBuf = nullptr;
  bool canvasDirty = false;
  uint16_t *dmaBuf = nullptr;
  uint16_t *dmaBuf2 = nullptr;
};
static DisplayHwState g_dispHw;
static constexpr int16_t DB_CANVAS_W = LCD_HEIGHT;  // 640
static constexpr int16_t DB_CANVAS_H = LCD_WIDTH;   // 172
static constexpr int16_t DB_NATIVE_W = LCD_WIDTH;   // 172
static constexpr int16_t DB_NATIVE_H = LCD_HEIGHT;  // 640
// Keep DMA chunks small enough to leave internal heap available for TLS handshakes.
static constexpr int16_t DB_CHUNK_ROWS = 64;  // was 32 — halves semaphore overhead per frame

// --- Frame performance counters (lightweight, no per-frame logging) ---
struct PerfCounters {
  uint32_t flushCount = 0;
  uint32_t flushTotalUs = 0;
  uint32_t flushMaxUs = 0;
  uint32_t lvglFrameCount = 0;
  uint32_t lvglTotalUs = 0;
  uint32_t lvglMaxUs = 0;
  uint32_t lastResetMs = 0;
};
static PerfCounters g_perf;
// g_dispHw.canvasDirty folded into DisplayHwState (g_dispHw)
#endif

static int detectTca9554Addr();
static void setUiPage(UiPageMode mode);
static bool isPwrButtonPressed();
static void setBacklightPercent(uint8_t percent);
static bool initDisplay();
static bool dispFlush();

#if TEST_BATTERY
static int batteryPercentFromVoltage(float vbat) {
  const float span = (BATTERY_FULL_V - BATTERY_EMPTY_V);
  if (span <= 0.01f) return -1;
  const float pct = ((vbat - BATTERY_EMPTY_V) * 100.0f) / span;
  if (pct <= 0.0f) return 0;
  if (pct >= 100.0f) return 100;
  return (int)(pct + 0.5f);
}

static void initBatteryMonitor() {
  adc_oneshot_unit_init_cfg_t initCfg = {};
  initCfg.unit_id = ADC_UNIT_1;
  if (adc_oneshot_new_unit(&initCfg, &g_batt.adcHandle) != ESP_OK) {
    Serial.println("[BATT][ERR] adc_oneshot_new_unit failed");
    g_batt.ready = false;
    return;
  }

  adc_oneshot_chan_cfg_t chanCfg = {};
  chanCfg.atten = ADC_ATTEN_DB_12;
  chanCfg.bitwidth = ADC_BITWIDTH_12;
  if (adc_oneshot_config_channel(g_batt.adcHandle, (adc_channel_t)BATTERY_ADC_CHANNEL, &chanCfg) != ESP_OK) {
    Serial.println("[BATT][ERR] adc_oneshot_config_channel failed");
    g_batt.ready = false;
    return;
  }

  g_batt.ready = true;
  Serial.printf("[BATT] monitor ready (ADC1_CH%d)\n", BATTERY_ADC_CHANNEL);
}

static bool sampleBatteryNow(uint32_t nowMs, bool force) {
  if (!g_batt.ready || !g_batt.adcHandle) return false;
  if (!force && (nowMs - g_batt.lastSampleMs) < BATTERY_SAMPLE_INTERVAL_MS) return false;
  const bool hadPrev = g_batt.hasSample;
  const uint32_t prevTs = g_batt.lastSampleMs;
  const float prevV = g_batt.voltage;
  g_batt.lastSampleMs = nowMs;

  int raw = 0;
  if (adc_oneshot_read(g_batt.adcHandle, (adc_channel_t)BATTERY_ADC_CHANNEL, &raw) != ESP_OK) {
    Serial.println("[BATT][ERR] adc_oneshot_read failed");
    return false;
  }

  // Vendor demo applies x3 divider factor to recover battery voltage.
  const float adcVolts = ((float)raw * 3.3f) / 4095.0f;
  const float vbat = adcVolts * BATTERY_DIVIDER_RATIO;
  const int pct = batteryPercentFromVoltage(vbat);

  g_batt.raw = raw;
  g_batt.voltage = vbat;
  g_batt.percent = pct;
  g_batt.hasSample = true;

  if (!hadPrev) {
    g_batt.trendMs = nowMs;
    g_batt.trendVoltage = vbat;
  } else if (prevTs > 0 && nowMs > prevTs) {
    const uint32_t dtMs = nowMs - prevTs;
    const float dvNow = vbat - prevV;
    const float mvPerMinNow = (dvNow * 1000.0f) * (60000.0f / (float)dtMs);
    // Fast hint for cable plug/unplug responsiveness between two consecutive samples.
    if (mvPerMinNow >= 18.0f) {
      g_batt.externalPowerLikely = true;
      g_batt.externalPowerHoldUntilMs = nowMs + 180000UL;
    } else if (mvPerMinNow <= -18.0f) {
      g_batt.externalPowerLikely = false;
      g_batt.externalPowerHoldUntilMs = 0;
    }
    if (g_batt.trendMs == 0) {
      g_batt.trendMs = prevTs;
      g_batt.trendVoltage = prevV;
    }
    const uint32_t trendDtMs = nowMs - g_batt.trendMs;
    // Evaluate slope over a longer window to avoid ADC jitter flips.
    if (trendDtMs >= 45000UL) {
      const float dv = vbat - g_batt.trendVoltage;
      const float mvPerMin = (dv * 1000.0f) * (60000.0f / (float)trendDtMs);
      if (mvPerMin >= 6.0f) g_batt.chargingLikely = true;
      else if (mvPerMin <= -6.0f) g_batt.chargingLikely = false;
      g_batt.trendMs = nowMs;
      g_batt.trendVoltage = vbat;
    }
  }

  if (g_batt.chargingLikely) {
    g_batt.externalPowerLikely = true;
    g_batt.externalPowerHoldUntilMs = nowMs + 180000UL;
  } else if (g_batt.externalPowerLikely && nowMs >= g_batt.externalPowerHoldUntilMs) {
    g_batt.externalPowerLikely = false;
  }

  Serial.printf("[BATT] raw=%d vbat=%.3fV soc=%d%%\n", raw, vbat, pct);
  return true;
}

static const char *batteryPowerModeText() {
  if (!g_batt.hasSample) return "UNKNOWN";
  return g_batt.chargingLikely ? "CHARGING" : "BATTERY";
}

static bool batteryExternalPowerLikelyNow(uint32_t nowMs) {
  if (!g_batt.hasSample) return false;
  if (g_batt.chargingLikely) return true;
  if (g_batt.externalPowerLikely && nowMs < g_batt.externalPowerHoldUntilMs) return true;
  return false;
}

static const char *batteryPowerSourceText(uint32_t nowMs) {
  if (!g_batt.hasSample) return "UNKNOWN";
  if (batteryExternalPowerLikelyNow(nowMs)) return "USB-C";
  return "BATTERY";
}

static const char *batteryLevelColorHex(int pct) {
  if (pct >= 80) return "66FFB3";
  if (pct >= 55) return "8BEAFF";
  if (pct >= 30) return "FFE16A";
  if (pct >= 15) return "FF9E57";
  return "FF5A6A";
}

static void batteryBarsForPercent(int pct, char *out, size_t outLen) {
  if (!out || outLen == 0) return;
  int bars = 0;
  if (pct >= 80) bars = 5;
  else if (pct >= 60) bars = 4;
  else if (pct >= 40) bars = 3;
  else if (pct >= 20) bars = 2;
  else if (pct >= 8) bars = 1;
  const char *table[] = {"[.....]", "[=....]", "[==...]", "[===..]", "[====.]", "[=====]"};
  snprintf(out, outLen, "%s", table[bars]);
}
#endif

#if TEST_BATTERY && ENERGY_SAVER_ENABLED
static uint32_t weatherRefreshIntervalByEnergy() { return WEATHER_REFRESH_MS; }
static uint32_t weatherRetryIntervalByEnergy() { return WEATHER_RETRY_MS; }
static uint32_t rssRefreshIntervalByEnergy() { return RSS_REFRESH_MS; }
static uint32_t rssRetryIntervalByEnergy() { return RSS_RETRY_MS; }

static void applyEnergyPolicy(uint32_t nowMs, bool force) {
  if (!force && (nowMs - g_batt.energyLastEvalMs) < 2000UL) return;
  g_batt.energyLastEvalMs = nowMs;
  const bool nextBatteryMode = g_batt.hasSample && !batteryExternalPowerLikelyNow(nowMs);
  if (!force && nextBatteryMode == g_batt.energySaverActive) return;
  g_batt.energySaverActive = nextBatteryMode;
  const uint8_t targetBacklight = g_batt.energySaverActive ? ENERGY_BACKLIGHT_ON_BATTERY : 100;
  setBacklightPercent(targetBacklight);
  Serial.printf("[ENERGY] mode=%s backlight=%u%% batt=%d%% src=%s\n",
                g_batt.energySaverActive ? "BATTERY" : "USB-C",
                (unsigned)targetBacklight,
                g_batt.percent,
                batteryPowerSourceText(nowMs));
}
#else
static uint32_t weatherRefreshIntervalByEnergy() { return WEATHER_REFRESH_MS; }
static uint32_t weatherRetryIntervalByEnergy() { return WEATHER_RETRY_MS; }
static uint32_t rssRefreshIntervalByEnergy() { return RSS_REFRESH_MS; }
static uint32_t rssRetryIntervalByEnergy() { return RSS_RETRY_MS; }
static void applyEnergyPolicy(uint32_t nowMs, bool force) {
  (void)nowMs;
  (void)force;
}
#endif

static const char* resetReasonToStr(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "Power-on";
    case ESP_RST_EXT: return "External pin";
    case ESP_RST_SW: return "Software reset";
    case ESP_RST_PANIC: return "Exception/Panic";
    case ESP_RST_INT_WDT: return "Interrupt WDT";
    case ESP_RST_TASK_WDT: return "Task WDT";
    case ESP_RST_WDT: return "Other WDT";
    case ESP_RST_DEEPSLEEP: return "Wake from deep sleep";
    case ESP_RST_BROWNOUT: return "Brownout";
    case ESP_RST_SDIO: return "SDIO";
    default: return "Unknown";
  }
}

static void runSerialInfoTest() {
  Serial.println();
  Serial.println("=================================================");
  Serial.println("ScryBar | M0.1 Serial Hello");
  Serial.println("=================================================");

  Serial.printf("[OK] Chip model     : %s\n", ESP.getChipModel());
  Serial.printf("[OK] Chip revision  : v%d\n", ESP.getChipRevision());
  Serial.printf("[OK] CPU cores      : %d\n", ESP.getChipCores());
  Serial.printf("[OK] CPU freq (MHz) : %d\n", ESP.getCpuFreqMHz());
  Serial.printf("[OK] SDK version    : %s\n", ESP.getSdkVersion());
  Serial.printf("[OK] Flash size     : %u MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  Serial.printf("[OK] Heap total     : %u bytes\n", ESP.getHeapSize());
  Serial.printf("[OK] Heap free      : %u bytes\n", ESP.getFreeHeap());
  Serial.printf("[OK] PSRAM total    : %u bytes\n", ESP.getPsramSize());
  Serial.printf("[OK] PSRAM free     : %u bytes\n", ESP.getFreePsram());

  esp_reset_reason_t rr = esp_reset_reason();
  Serial.printf("[OK] Reset reason   : %d (%s)\n", (int)rr, resetReasonToStr(rr));
  Serial.println("[NEXT] Se tutto e' ok, passiamo a M0.2 Backlight test.");
}

static void printRuntimeSummary(uint32_t nowMs) {
  char timeBuf[24] = "--";
#if TEST_NTP
  if (g_clock.ntpSynced) {
    struct tm ti;
    if (getLocalTime(&ti, 20)) {
      strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &ti);
    }
  }
#endif

#if TEST_WIFI
  const bool wifiOk = (WiFi.status() == WL_CONNECTED) && g_wifiSt.connected;
  const char *wifiState = wifiOk ? "CONNECTED" : "DISCONNECTED";
  const char *themeState = runtimeUiThemeId();
#else
  const char *wifiState = "OFF";
  const char *themeState = activeUiThemeId();
#endif

#if TEST_NTP
  const char *ntpState = g_clock.ntpSynced ? "SYNCED" : "UNSYNCED";
#else
  const char *ntpState = "OFF";
#endif

#if TEST_WIFI
  char meteoBuf[96];
  if (g_weather.valid) {
    snprintf(meteoBuf, sizeof(meteoBuf),
             "ok %.1fC rh=%d%% code=%d wind=%.1f sun=%s/%s",
             g_weather.tempC, g_weather.humidity, g_weather.weatherCode,
             g_weather.windKmh, g_weather.sunrise, g_weather.sunset);
  } else {
    snprintf(meteoBuf, sizeof(meteoBuf), "not-ready");
  }
#else
  const char *meteoBuf = "OFF";
#endif

#if TEST_LVGL_UI && TEST_DISPLAY && DISPLAY_BACKEND_ESP_LCD
  const char *uiState = g_lvglReady ? "LVGL_READY" : "LVGL_OFF";
#else
  const char *uiState = "GFX";
#endif

#if TEST_BATTERY
  char battBuf[48];
  if (g_batt.hasSample) {
    snprintf(battBuf, sizeof(battBuf), "%s %.3fV %d%%", batteryPowerModeText(), g_batt.voltage, g_batt.percent);
  } else {
    snprintf(battBuf, sizeof(battBuf), "not-ready");
  }
#else
  const char *battBuf = "OFF";
#endif

  const int pwrRaw = gpio_get_level((gpio_num_t)PWR_BUTTON_PIN);
  const int pwrPressed = isPwrButtonPressed() ? 1 : 0;

  Serial.printf("[SUMMARY] build=%s uptime=%lu wifi=%s ntp=%s time=%s ui=%s theme=%s batt=%s pwr_mode=%s pwr_raw=%d pwr_pressed=%d meteo=%s\n",
                FW_BUILD_TAG,
                nowMs,
                wifiState,
                ntpState,
                timeBuf,
                uiState,
                themeState,
                battBuf,
                g_softPowerOff ? "SOFT_OFF" : "RUN",
                pwrRaw,
                pwrPressed,
                meteoBuf);

#if TEST_LVGL_UI && TEST_DISPLAY && DISPLAY_BACKEND_ESP_LCD
  {
    const uint32_t window = nowMs - g_perf.lastResetMs;
    const uint32_t flushAvg = g_perf.flushCount ? (g_perf.flushTotalUs / g_perf.flushCount) : 0;
    const uint32_t lvglAvg = g_perf.lvglFrameCount ? (g_perf.lvglTotalUs / g_perf.lvglFrameCount) : 0;
    const uint32_t fps = (window > 0 && g_perf.flushCount > 0) ? (g_perf.flushCount * 1000UL / window) : 0;
    Serial.printf("[PERF] window=%lums flush=%lu frames avg=%luus max=%luus lvgl_handler=%lu calls avg=%luus max=%luus fps=%lu\n",
                  window, g_perf.flushCount, flushAvg, g_perf.flushMaxUs,
                  g_perf.lvglFrameCount, lvglAvg, g_perf.lvglMaxUs, fps);
    g_perf.flushCount = 0; g_perf.flushTotalUs = 0; g_perf.flushMaxUs = 0;
    g_perf.lvglFrameCount = 0; g_perf.lvglTotalUs = 0; g_perf.lvglMaxUs = 0;
    g_perf.lastResetMs = nowMs;
  }
#endif
}

static bool i2cReadReg(TwoWire &bus, uint8_t addr, uint8_t reg, uint8_t &value) {
  bus.beginTransmission(addr);
  bus.write(reg);
  if (bus.endTransmission(false) != 0) return false;
  if (bus.requestFrom((int)addr, 1) != 1) return false;
  value = bus.read();
  return true;
}

static bool i2cWriteReg(TwoWire &bus, uint8_t addr, uint8_t reg, uint8_t value) {
  bus.beginTransmission(addr);
  bus.write(reg);
  bus.write(value);
  return (bus.endTransmission() == 0);
}

static bool tcaSetBitOutputAndLevel(TwoWire &bus, uint8_t tcaAddr, uint8_t bit, bool high) {
  uint8_t cfg = 0xFF;
  uint8_t out = 0x00;
  if (!i2cReadReg(bus, tcaAddr, 0x03, cfg)) return false;  // config
  if (!i2cReadReg(bus, tcaAddr, 0x01, out)) return false;  // output

  cfg &= (uint8_t)~(1U << bit);  // output mode
  if (high) {
    out |= (uint8_t)(1U << bit);
  } else {
    out &= (uint8_t)~(1U << bit);
  }
  return i2cWriteReg(bus, tcaAddr, 0x03, cfg) && i2cWriteReg(bus, tcaAddr, 0x01, out);
}

static bool tcaSetTwoBits(TwoWire &bus, uint8_t tcaAddr, uint8_t bitA, bool highA, uint8_t bitB, bool highB) {
  uint8_t cfg = 0xFF;
  uint8_t out = 0x00;
  if (!i2cReadReg(bus, tcaAddr, 0x03, cfg)) return false;
  if (!i2cReadReg(bus, tcaAddr, 0x01, out)) return false;

  cfg &= (uint8_t)~(1U << bitA);
  cfg &= (uint8_t)~(1U << bitB);
  if (highA) out |= (uint8_t)(1U << bitA);
  else out &= (uint8_t)~(1U << bitA);
  if (highB) out |= (uint8_t)(1U << bitB);
  else out &= (uint8_t)~(1U << bitB);

  return i2cWriteReg(bus, tcaAddr, 0x03, cfg) && i2cWriteReg(bus, tcaAddr, 0x01, out);
}

static bool tcaWriteRaw(TwoWire &bus, uint8_t tcaAddr, uint8_t cfg, uint8_t out) {
  return i2cWriteReg(bus, tcaAddr, 0x03, cfg) && i2cWriteReg(bus, tcaAddr, 0x01, out);
}

static void initBacklightPwmWaveshare() {
  if (g_backlightReady) return;

  // Matches official Waveshare Arduino demo approach:
  // 8-bit PWM on LCD_BL (GPIO8), active-low duty mapping.
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcAttach(LCD_BL_PIN, 50000 /* Hz */, 8 /* bits */);
#else
  ledcSetup(1 /* channel */, 50000 /* Hz */, 8 /* bits */);
  ledcAttachPin(LCD_BL_PIN, 1 /* channel */);
#endif
  g_backlightReady = true;
}

static void setBacklightPercent(uint8_t percent) {
  initBacklightPwmWaveshare();
  if (percent > 100) percent = 100;

  uint8_t raw = (uint8_t)((percent * 255) / 100);
  uint8_t duty = (uint8_t)(255 - raw);  // Waveshare uses inverted duty macros.
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcWrite(LCD_BL_PIN, duty);
#else
  ledcWrite(1 /* channel */, duty);
#endif
}

static void setBacklightPwm(bool on) {
  setBacklightPercent(on ? 100 : 0);
}

static bool isPwrButtonPressed() {
  const int level = gpio_get_level((gpio_num_t)PWR_BUTTON_PIN);
#if PWR_BUTTON_ACTIVE_LOW
  return (level == 0);
#else
  return (level != 0);
#endif
}

static bool isNavFirstButtonPressed() {
  const int level = gpio_get_level((gpio_num_t)NAV_FIRST_BUTTON_PIN);
#if NAV_FIRST_BUTTON_ACTIVE_LOW
  return (level == 0);
#else
  return (level != 0);
#endif
}

static void preparePowerButtonPin() {
  pinMode(PWR_BUTTON_PIN, PWR_BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
  g_pwrBtn.lastRawLevel = gpio_get_level((gpio_num_t)PWR_BUTTON_PIN);
  g_pwrBtn.ignoreUntilRelease = isPwrButtonPressed();
  Serial.printf("[PWR] init pin=%d raw=%d active_low=%d\n",
                PWR_BUTTON_PIN,
                g_pwrBtn.lastRawLevel,
                PWR_BUTTON_ACTIVE_LOW ? 1 : 0);
  if (g_pwrBtn.ignoreUntilRelease) {
    Serial.println("[PWR] Ignoring held key until release after boot.");
  }
}

static void prepareNavFirstButtonPin() {
  pinMode(NAV_FIRST_BUTTON_PIN, NAV_FIRST_BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
  g_navBtn.lastRawLevel = gpio_get_level((gpio_num_t)NAV_FIRST_BUTTON_PIN);
  Serial.printf("[NAV] first-btn init pin=%d raw=%d active_low=%d\n",
                NAV_FIRST_BUTTON_PIN,
                g_navBtn.lastRawLevel,
                NAV_FIRST_BUTTON_ACTIVE_LOW ? 1 : 0);
}

static bool setSystemEnableThroughTca9554(bool enableOn) {
#if PWR_USE_TCA9554_SYS_EN
  I2C_MAIN.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  I2C_MAIN.setClock(100000);
  const int tcaAddr = detectTca9554Addr();
  if (tcaAddr < 0) {
    Serial.println("[PWR] TCA9554 non trovato, salto power-cut HW.");
    return false;
  }
  const bool sysLevel = enableOn ? (TCA9554_SYS_EN_ACTIVE_HIGH != 0) : (TCA9554_SYS_EN_ACTIVE_HIGH == 0);
  const bool ok = tcaSetBitOutputAndLevel(I2C_MAIN, (uint8_t)tcaAddr, TCA9554_SYS_EN_BIT, sysLevel);
  Serial.printf("[PWR] SYS_EN EXIO%d=%s -> %s\n",
                TCA9554_SYS_EN_BIT,
                sysLevel ? "HIGH" : "LOW",
                ok ? "OK" : "ERR");
  return ok;
#else
  (void)enableOn;
  return false;
#endif
}

static void ensureSystemPowerLatchOnBoot() {
#if PWR_USE_TCA9554_SYS_EN
  const bool ok = setSystemEnableThroughTca9554(true);
  if (ok) {
    Serial.println("[PWR] SYS_EN asserted HIGH at boot.");
  } else {
    Serial.println("[PWR][WARN] Cannot assert SYS_EN at boot.");
  }
#endif
}

static void enterDeepSleepFromPowerButton() {
  Serial.println("[PWR] Entering deep sleep fallback.");
  setBacklightPercent(0);
#if TEST_WIFI
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
  g_wifiSt.connected = false;
#endif
#if TEST_NTP
  g_clock.ntpSynced = false;
#endif

  // Avoid immediate wake loops if key is still held when we enter deep sleep.
  const uint32_t releaseStart = millis();
  while (isPwrButtonPressed() && (millis() - releaseStart) < 8000UL) {
    delay(10);
  }

  const int wakeLevel = PWR_BUTTON_ACTIVE_LOW ? 0 : 1;
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PWR_BUTTON_PIN, wakeLevel);
  delay(20);
  esp_deep_sleep_start();
}

static void resumeFromSoftPowerOff() {
  const uint32_t nowMs = millis();
  g_softPowerOff = false;
  g_pwrBtn.ignoreUntilRelease = true;
  g_pwrBtn.down = false;
  g_pwrBtn.holdReported = false;
  g_pwrBtn.pressCandidateMs = 0;
  g_pwrBtn.releaseCandidateMs = 0;

  setBacklightPercent(100);
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI && SCREENSAVER_ENABLED
  if (g_lvglReady && g_saver.active) {
    lvglSetScreenSaverActive(false);
  }
  markUserInteraction(nowMs);
#endif

  setUiPage(UI_PAGE_HOME);
  g_uiNeedsRedraw = true;
  applyEnergyPolicy(nowMs, true);

#if TEST_WIFI
  runWiFiConnectTest();
#endif
#if TEST_NTP
  g_wifiSt.lastNtpAttemptMs = 0;
#endif
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI
  if (g_lvglReady) {
    updateLvglUi(true);
    runLvglLoop();
  }
#elif TEST_DISPLAY && TEST_NTP
  updateDisplayClock(true);
#endif
  Serial.println("[PWR] Soft-off wake confirmed, resuming HOME.");
}

static void enterSoftPowerOffFromPowerButton() {
  g_softPowerOff = true;
  Serial.println("[PWR] Entering soft-off fallback (USB-safe).");
  setBacklightPercent(0);
#if TEST_WIFI
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
  g_wifiSt.connected = false;
#endif
#if TEST_NTP
  g_clock.ntpSynced = false;
#endif

  while (isPwrButtonPressed()) {
    delay(10);
  }

  uint32_t holdStartMs = 0;
  uint32_t lastLogMs = 0;
  for (;;) {
    const bool pressed = isPwrButtonPressed();
    const uint32_t now = millis();
    if (pressed) {
      if (holdStartMs == 0) holdStartMs = now;
      const uint32_t heldMs = now - holdStartMs;
      if ((now - lastLogMs) >= 1000UL) {
        lastLogMs = now;
        Serial.printf("[PWR] Soft-off wake hold (%lu/%d ms)\n", (unsigned long)heldMs, PWR_HOLD_WAKE_MS);
      }
      if (heldMs >= (uint32_t)PWR_HOLD_WAKE_MS) {
        resumeFromSoftPowerOff();
        return;
      }
    } else {
      holdStartMs = 0;
    }
    delay(20);
  }
}

static void shutdownFromPowerButton(bool hardOffRequested) {
  Serial.printf("[PWR] Shutdown request (%s).\n", hardOffRequested ? "hard-off" : "soft-off");
  setBacklightPercent(0);
  if (!hardOffRequested) {
    enterSoftPowerOffFromPowerButton();
    return;
  }

  const bool hwCut = setSystemEnableThroughTca9554(false);
  if (hwCut) {
    // If hardware cut succeeds, MCU power should disappear shortly after this.
    delay(1200);
  }
  Serial.println("[PWR][WARN] Hard-off did not latch. Falling back to soft-off.");
  enterSoftPowerOffFromPowerButton();
}

static void onPowerButtonShortPress(uint32_t nowMs) {
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI && SCREENSAVER_ENABLED
  if (!g_lvglReady) {
    Serial.println("[PWR] Short press: screensaver unavailable (LVGL not ready).");
    return;
  }
  markUserInteraction(nowMs);
  const bool nextSaverState = !g_saver.active;
  lvglSetScreenSaverActive(nextSaverState);
  Serial.printf("[PWR] Short press: screensaver %s.\n", nextSaverState ? "ON" : "OFF");
#else
  (void)nowMs;
  Serial.println("[PWR] Short press ignored (screensaver disabled).");
#endif
}

// --- Power-hold progress overlay -------------------------------------------
#if TEST_LVGL_UI && DISPLAY_BACKEND_ESP_LCD
static void pwrOverlayShow(uint32_t nowMs) {
  if (!g_lvglReady || g_pwrBtn.ovBg) return;  // already visible
  const int16_t cW = canvasWidth();
  const int16_t cH = canvasHeight();
  lv_obj_t *layer = lv_layer_top();

  // Dark backing strip across the bottom
  g_pwrBtn.ovBg = lv_obj_create(layer);
  lv_obj_set_size(g_pwrBtn.ovBg, cW, kPwrBarH + 4);
  lv_obj_set_pos(g_pwrBtn.ovBg, 0, cH - kPwrBarH - 4);
  lv_obj_set_style_bg_color(g_pwrBtn.ovBg, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_pwrBtn.ovBg, LV_OPA_70, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_pwrBtn.ovBg, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_pwrBtn.ovBg, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_pwrBtn.ovBg, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_pwrBtn.ovBg, LV_OBJ_FLAG_SCROLLABLE);

  // Progress bar (width updated every loop tick)
  const uint32_t accentColor = activeUiTheme().lvgl.headerBg;
  g_pwrBtn.ovBar = lv_obj_create(layer);
  lv_obj_set_size(g_pwrBtn.ovBar, 1, kPwrBarH);
  lv_obj_set_pos(g_pwrBtn.ovBar, 0, cH - kPwrBarH - 2);
  lv_obj_set_style_bg_color(g_pwrBtn.ovBar, lv_color_hex(accentColor), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_pwrBtn.ovBar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_pwrBtn.ovBar, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_pwrBtn.ovBar, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_pwrBtn.ovBar, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_pwrBtn.ovBar, LV_OBJ_FLAG_SCROLLABLE);
  (void)nowMs;
}

static void pwrOverlayUpdate(uint32_t heldMs) {
  if (!g_pwrBtn.ovBar) return;
  // fraction of hold completed (0..1), clamped, starting from feedback delay
  const uint32_t elapsed = (heldMs > kPwrFeedbackDelayMs) ? (heldMs - kPwrFeedbackDelayMs) : 0;
  const uint32_t total   = PWR_HOLD_SHUTDOWN_MS - kPwrFeedbackDelayMs;
  const int16_t  cW      = canvasWidth();
  const int16_t  barW    = (int16_t)((elapsed * (uint32_t)cW) / total);
  lv_obj_set_width(g_pwrBtn.ovBar, (lv_coord_t)(barW > 0 ? barW : 1));
}

static void pwrOverlayHide() {
  if (g_pwrBtn.ovBar) { lv_obj_del(g_pwrBtn.ovBar); g_pwrBtn.ovBar = nullptr; }
  if (g_pwrBtn.ovBg)  { lv_obj_del(g_pwrBtn.ovBg);  g_pwrBtn.ovBg  = nullptr; }
}
#else
static void pwrOverlayShow(uint32_t) {}
static void pwrOverlayUpdate(uint32_t) {}
static void pwrOverlayHide() {}
#endif
// ---------------------------------------------------------------------------

static void handlePowerButtonLoop(uint32_t nowMs) {
  const int rawLevel = gpio_get_level((gpio_num_t)PWR_BUTTON_PIN);
  if (rawLevel != g_pwrBtn.lastRawLevel) {
    Serial.printf("[PWR] raw level change: %d -> %d\n", g_pwrBtn.lastRawLevel, rawLevel);
    g_pwrBtn.lastRawLevel = rawLevel;
  }

  const bool pressed = isPwrButtonPressed();
  if (g_pwrBtn.ignoreUntilRelease) {
    if (!pressed) {
      pwrOverlayHide();
      g_pwrBtn.ignoreUntilRelease = false;
      g_pwrBtn.down = false;
      g_pwrBtn.holdReported = false;
      g_pwrBtn.pressCandidateMs = 0;
      g_pwrBtn.releaseCandidateMs = 0;
      Serial.println("[PWR] Release gate cleared.");
    }
    return;
  }

  if (pressed) {
    g_pwrBtn.releaseCandidateMs = 0;
    if (!g_pwrBtn.down) {
      if (g_pwrBtn.pressCandidateMs == 0) {
        g_pwrBtn.pressCandidateMs = nowMs;
        return;
      }
      if ((nowMs - g_pwrBtn.pressCandidateMs) < kPwrPressDebounceMs) {
        return;
      }
      g_pwrBtn.down = true;
      g_pwrBtn.downMs = g_pwrBtn.pressCandidateMs;
      g_pwrBtn.holdReported = false;
      Serial.println("[PWR] Button down.");
    } else {
      const uint32_t heldMs = nowMs - g_pwrBtn.downMs;
      // Show / update progress overlay after feedback delay
      if (heldMs >= kPwrFeedbackDelayMs) {
        pwrOverlayShow(nowMs);
        pwrOverlayUpdate(heldMs);
      }
      if (!g_pwrBtn.holdReported && heldMs >= 1000UL) {
        g_pwrBtn.holdReported = true;
        Serial.printf("[PWR] Keep holding (%lu/%d ms)\n", (unsigned long)heldMs, PWR_HOLD_SHUTDOWN_MS);
      }
      if (heldMs >= (uint32_t)PWR_HOLD_SHUTDOWN_MS) {
        Serial.printf("[PWR] Long press confirmed (%lu ms).\n", (unsigned long)heldMs);
        pwrOverlayHide();
        g_pwrBtn.down = false;
        g_pwrBtn.holdReported = false;
        g_pwrBtn.pressCandidateMs = 0;
        g_pwrBtn.releaseCandidateMs = 0;
        shutdownFromPowerButton(true);
      }
    }
    return;
  }

  g_pwrBtn.pressCandidateMs = 0;
  // Button released — hide overlay (only meaningful if was down)
  if (!g_pwrBtn.down) { pwrOverlayHide(); return; }

  if (g_pwrBtn.releaseCandidateMs == 0) {
    g_pwrBtn.releaseCandidateMs = nowMs;
    return;
  }
  if ((nowMs - g_pwrBtn.releaseCandidateMs) < (uint32_t)PWR_RELEASE_DEBOUNCE_MS) {
    return;
  }

  pwrOverlayHide();
  const uint32_t heldMs = g_pwrBtn.releaseCandidateMs - g_pwrBtn.downMs;
  if (heldMs >= (uint32_t)PWR_HOLD_SHUTDOWN_MS) {
    shutdownFromPowerButton(false);
  } else if (heldMs >= kPwrShortPressMinMs) {
    Serial.printf("[PWR] Short press (%lu ms).\n", (unsigned long)heldMs);
    onPowerButtonShortPress(nowMs);
  } else {
    Serial.printf("[PWR] Ignored bounce/glitch (%lu ms).\n", (unsigned long)heldMs);
  }
  g_pwrBtn.down = false;
  g_pwrBtn.holdReported = false;
  g_pwrBtn.releaseCandidateMs = 0;
}

static void onNavFirstButtonShortPress(uint32_t nowMs) {
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI && SCREENSAVER_ENABLED
  markUserInteraction(nowMs);
  if (g_lvglReady && g_saver.active) {
    lvglSetScreenSaverActive(false);
  }
#else
  (void)nowMs;
#endif
  setUiPage(UI_PAGE_HOME);
  Serial.println("[NAV] BOOT short press -> first main view (HOME).");
}

static void handleNavFirstButtonLoop(uint32_t nowMs) {
  const int rawLevel = gpio_get_level((gpio_num_t)NAV_FIRST_BUTTON_PIN);
  if (rawLevel != g_navBtn.lastRawLevel) {
    Serial.printf("[NAV] first-btn raw level change: %d -> %d\n", g_navBtn.lastRawLevel, rawLevel);
    g_navBtn.lastRawLevel = rawLevel;
  }

  const bool pressed = isNavFirstButtonPressed();
  if (pressed) {
    g_navBtn.releaseCandidateMs = 0;
    if (!g_navBtn.down) {
      g_navBtn.down = true;
      g_navBtn.downMs = nowMs;
    }
    return;
  }

  if (!g_navBtn.down) return;
  if (g_navBtn.releaseCandidateMs == 0) {
    g_navBtn.releaseCandidateMs = nowMs;
    return;
  }
  if ((nowMs - g_navBtn.releaseCandidateMs) < (uint32_t)NAV_BUTTON_RELEASE_DEBOUNCE_MS) {
    return;
  }

  const uint32_t heldMs = g_navBtn.releaseCandidateMs - g_navBtn.downMs;
  if (heldMs <= (uint32_t)NAV_BUTTON_TAP_MAX_MS) {
    onNavFirstButtonShortPress(nowMs);
  } else {
    Serial.printf("[NAV] first-btn long press ignored (%lu ms)\n", (unsigned long)heldMs);
  }

  g_navBtn.down = false;
  g_navBtn.releaseCandidateMs = 0;
}

static void handleWakeHoldGate() {
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) return;
  Serial.printf("[PWR] Wake from PWR key, waiting %d ms hold to continue boot.\n", PWR_HOLD_WAKE_MS);
  const uint32_t start = millis();
  while (isPwrButtonPressed()) {
    const uint32_t held = millis() - start;
    if (held >= (uint32_t)PWR_HOLD_WAKE_MS) {
      Serial.println("[PWR] Wake hold confirmed.");
      return;
    }
    delay(20);
  }
  Serial.println("[PWR] Wake hold too short, back to deep sleep.");
  enterDeepSleepFromPowerButton();
}

#if TEST_DISPLAY && DISPLAY_BACKEND_ESP_LCD
static bool onDisplayFlushDone(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
  (void)panel_io;
  (void)edata;
  (void)user_ctx;
  BaseType_t taskWoken = pdFALSE;
  if (g_dispHw.flushSem) xSemaphoreGiveFromISR(g_dispHw.flushSem, &taskWoken);
  return false;
}

static inline int16_t dispWidth() { return DB_CANVAS_W; }
static inline int16_t dispHeight() { return DB_CANVAS_H; }

static void dispFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (!g_dispHw.canvasBuf || w <= 0 || h <= 0) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x >= DB_CANVAS_W || y >= DB_CANVAS_H) return;
  if ((x + w) > DB_CANVAS_W) w = DB_CANVAS_W - x;
  if ((y + h) > DB_CANVAS_H) h = DB_CANVAS_H - y;
  for (int16_t yy = y; yy < (y + h); ++yy) {
    uint16_t *row = g_dispHw.canvasBuf + (yy * DB_CANVAS_W) + x;
    for (int16_t xx = 0; xx < w; ++xx) row[xx] = color;
  }
}

static void dispDrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (w <= 1 || h <= 1) return;
  dispFillRect(x, y, w, 1, color);
  dispFillRect(x, y + h - 1, w, 1, color);
  dispFillRect(x, y, 1, h, color);
  dispFillRect(x + w - 1, y, 1, h, color);
}

static void dispFillScreen(uint16_t color) {
  if (!g_dispHw.canvasBuf) return;
  for (int i = 0; i < (DB_CANVAS_W * DB_CANVAS_H); ++i) g_dispHw.canvasBuf[i] = color;
}

// ── CGA palette + CP437 char draw (used by DOOM HUD) ─────────────────────────
#if TEST_DISPLAY
static const uint16_t kCgaPalette16[16] = {
  lv_color_make(0,   0,   0  ).full, // 0  Black
  lv_color_make(0,   0,   170).full, // 1  Dark Blue
  lv_color_make(0,   170, 0  ).full, // 2  Dark Green
  lv_color_make(0,   170, 170).full, // 3  Dark Cyan
  lv_color_make(170, 0,   0  ).full, // 4  Dark Red
  lv_color_make(170, 0,   170).full, // 5  Dark Magenta
  lv_color_make(170, 85,  0  ).full, // 6  Brown
  lv_color_make(170, 170, 170).full, // 7  Light Gray
  lv_color_make(85,  85,  85 ).full, // 8  Dark Gray
  lv_color_make(85,  85,  255).full, // 9  Bright Blue
  lv_color_make(85,  255, 85 ).full, // 10 Bright Green
  lv_color_make(85,  255, 255).full, // 11 Bright Cyan
  lv_color_make(255, 85,  85 ).full, // 12 Bright Red
  lv_color_make(255, 85,  255).full, // 13 Bright Magenta
  lv_color_make(255, 255, 85 ).full, // 14 Bright Yellow
  lv_color_make(255, 255, 255).full, // 15 White
};

// Draw one CP437 character into a pixel buffer of width bufW.
// fontW/fontH default 8×16 (IBM VGA original); supports integer downscaling.
// Uses area-average sampling for sub-pixel accuracy on block chars.
static void drawCgaChar(uint16_t *buf, int bufW, int x, int y,
                        uint8_t ch, uint8_t fgIdx, uint8_t bgIdx,
                        int fontW = 8, int fontH = 16) {
  const uint16_t fg = kCgaPalette16[fgIdx & 0x0F];
  const uint16_t bg = kCgaPalette16[bgIdx & 0x0F];
  for (int row = 0; row < fontH; ++row) {
    const int srcRowStart = (row * 16) / fontH;
    const int srcRowEnd   = ((row + 1) * 16) / fontH;
    uint16_t *line = buf + (y + row) * bufW + x;
    for (int col = 0; col < fontW; ++col) {
      const int srcColStart = (col * 8) / fontW;
      const int srcColEnd   = ((col + 1) * 8) / fontW;
      int litCount = 0, totalCount = 0;
      for (int sr = srcRowStart; sr < srcRowEnd; ++sr) {
        const uint8_t bits = kIbmVga8x16[(int)ch * 16 + sr];
        for (int sc = srcColStart; sc < srcColEnd; ++sc) {
          if (bits & (0x80 >> sc)) ++litCount;
          ++totalCount;
        }
      }
      line[col] = (totalCount > 0 && litCount * 2 >= totalCount) ? fg : bg;
    }
  }
}
#endif // TEST_DISPLAY

// ── DOOM TITLEPIC spike ───────────────────────────────────────────────────────
#if TEST_DISPLAY && DOOM_SPIKE_ENABLED && DB_HAS_DOOM_SPIKE_ASSETS
static inline uint16_t doomRgb888To565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((uint16_t)(r & 0xF8u) << 8) |
                    ((uint16_t)(g & 0xFCu) << 3) |
                    ((uint16_t)(b & 0xF8u) >> 3));
}

static inline uint8_t doomTouchZoneFromX(int16_t x) {
  if (x < kDoomFrameX) return DOOM_TOUCH_LEFT;
  if (x >= kDoomRightBandX) return DOOM_TOUCH_RIGHT;
  return DOOM_TOUCH_CENTER;
}

// Use a wider engage threshold and a smaller release threshold so DOOM does
// not chatter between 0 and +/-1 when the device is near neutral.
static inline int8_t doomQuantizeAxis(float deltaDeg, float engageDeg, float releaseDeg, float binDeg,
                                      int8_t previousBin, int8_t minBin, int8_t maxBin) {
  const float absDelta = fabsf(deltaDeg);
  const bool reversing = (previousBin != 0) && ((deltaDeg > 0.0f) != (previousBin > 0));
  const float threshold = (previousBin == 0 || reversing) ? engageDeg : releaseDeg;
  if (absDelta < threshold) return 0;

  float effectiveDeg = absDelta - engageDeg;
  if (effectiveDeg < 0.0f) effectiveDeg = 0.0f;

  int bin = 1 + (int)floorf(effectiveDeg / binDeg);
  if (deltaDeg < 0.0f) bin = -bin;
  return (int8_t)constrain(bin, (int)minBin, (int)maxBin);
}

static const char *doomTouchZoneName(uint8_t zone) {
  switch (zone) {
    case DOOM_TOUCH_LEFT: return "LEFT";
    case DOOM_TOUCH_CENTER: return "CENTER";
    case DOOM_TOUCH_RIGHT: return "RIGHT";
    default: return "NONE";
  }
}

static void doomRequestNeutralCalibrate() {
  g_doom.neutralPending = true;
  g_doom.neutralReady = false;
  g_doom.neutralArmAtMs = millis() + kDoomNeutralCaptureDelayMs;
  g_doom.neutralStableSinceMs = 0;
  g_doom.neutralStableSamples = 0;
  g_doom.neutralAccumMoveDeg = 0.0f;
  g_doom.neutralAccumTurnDeg = 0.0f;
  g_doom.tiltFilterReady = false;
  g_doom.lastTiltSampleMs = 0;
  g_doom.moveTiltDeg = 0.0f;
  g_doom.turnTiltDeg = 0.0f;
  g_doom.axisFilterReady = false;
  g_doom.moveDeltaFilteredDeg = 0.0f;
  g_doom.turnDeltaFilteredDeg = 0.0f;
  g_doom.moveBin = 0;
  g_doom.turnBin = 0;
  g_doom.frameDirty = true;
  Serial.println("[DOOM][IMU] neutral calibration requested");
}

static inline void doomFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (!g_dispHw.canvasBuf || w <= 0 || h <= 0) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if ((x + w) > DB_CANVAS_W) w = DB_CANVAS_W - x;
  if ((y + h) > DB_CANVAS_H) h = DB_CANVAS_H - y;
  if (w <= 0 || h <= 0) return;
  for (int16_t yy = y; yy < (y + h); ++yy) {
    uint16_t *row = g_dispHw.canvasBuf + ((size_t)yy * DB_CANVAS_W) + x;
    for (int16_t xx = 0; xx < w; ++xx) row[xx] = color;
  }
}

static void doomDrawText(int16_t x, int16_t y, const char *text, uint8_t fgIdx, uint8_t bgIdx,
                         int16_t fontW = 6, int16_t fontH = 12) {
  if (!g_dispHw.canvasBuf || !text || !*text) return;
  int16_t cursorX = x;
  while (*text) {
    if ((cursorX + fontW) > 0 && cursorX < DB_CANVAS_W &&
        (y + fontH) > 0 && y < DB_CANVAS_H) {
      drawCgaChar(g_dispHw.canvasBuf, DB_CANVAS_W, cursorX, y, (uint8_t)*text, fgIdx, bgIdx, fontW, fontH);
    }
    cursorX += fontW;
    ++text;
  }
}

static inline void doomSetPixel(int16_t x, int16_t y, uint16_t color) {
  if (!g_dispHw.canvasBuf) return;
  if (x < 0 || x >= DB_CANVAS_W || y < 0 || y >= DB_CANVAS_H) return;
  g_dispHw.canvasBuf[((size_t)y * DB_CANVAS_W) + (size_t)x] = color;
}

static inline uint8_t doomFontBitmapAlpha(const uint8_t *bitmap, uint32_t pixelIndex, uint8_t bpp) {
  if (!bitmap || bpp == 0) return 0;
  switch (bpp) {
    case 1: {
      const uint8_t byte = bitmap[pixelIndex >> 3];
      return (byte & (0x80u >> (pixelIndex & 0x7u))) ? 255u : 0u;
    }
    case 2: {
      const uint8_t byte = bitmap[pixelIndex >> 2];
      const uint8_t shift = (uint8_t)(6u - ((pixelIndex & 0x3u) << 1));
      return (uint8_t)(((byte >> shift) & 0x03u) * 85u);
    }
    case 4: {
      const uint8_t byte = bitmap[pixelIndex >> 1];
      const uint8_t nibble = (pixelIndex & 0x1u) ? (byte & 0x0Fu) : ((byte >> 4) & 0x0Fu);
      return (uint8_t)(nibble * 17u);
    }
    default:
      return bitmap[pixelIndex];
  }
}

static int16_t doomMeasureFontText(const lv_font_t *font, const char *text) {
  if (!font || !text) return 0;
  int16_t width = 0;
  while (*text) {
    const uint32_t letter = (uint8_t)*text;
    const uint32_t next = (uint8_t)*(text + 1);
    width += (int16_t)lv_font_get_glyph_width(font, letter, next);
    ++text;
  }
  return width;
}

static void doomDrawFontText(int16_t x, int16_t y, const char *text,
                             const lv_font_t *font, uint16_t fg,
                             bool opaqueBg = false, uint16_t bg = 0) {
  if (!g_dispHw.canvasBuf || !text || !*text || !font) return;
  const int16_t lineH = (int16_t)lv_font_get_line_height(font);
  const int16_t textW = doomMeasureFontText(font, text);
  if (opaqueBg && textW > 0 && lineH > 0) doomFillRect(x, y, textW, lineH, bg);

  int16_t cursorX = x;
  while (*text) {
    const uint32_t letter = (uint8_t)*text;
    const uint32_t next = (uint8_t)*(text + 1);
    lv_font_glyph_dsc_t glyph = {};
    if (lv_font_get_glyph_dsc(font, &glyph, letter, next)) {
      const uint8_t *bitmap = lv_font_get_glyph_bitmap(font, letter);
      if (bitmap && glyph.box_w > 0 && glyph.box_h > 0) {
        const int16_t glyphX = cursorX + glyph.ofs_x;
        const int16_t glyphY = y + lineH - font->base_line - glyph.box_h - glyph.ofs_y;
        uint32_t pixelIndex = 0;
        for (uint16_t row = 0; row < glyph.box_h; ++row) {
          for (uint16_t col = 0; col < glyph.box_w; ++col, ++pixelIndex) {
            if (doomFontBitmapAlpha(bitmap, pixelIndex, glyph.bpp) >= 48u) {
              doomSetPixel(glyphX + (int16_t)col, glyphY + (int16_t)row, fg);
            }
          }
        }
      }
    }
    cursorX += (int16_t)lv_font_get_glyph_width(font, letter, next);
    ++text;
  }
}

static void doomDrawFontTextCentered(int16_t centerX, int16_t y, const char *text,
                                     const lv_font_t *font, uint16_t fg,
                                     bool opaqueBg = false, uint16_t bg = 0) {
  const int16_t textW = doomMeasureFontText(font, text);
  doomDrawFontText(centerX - (textW / 2), y, text, font, fg, opaqueBg, bg);
}

static inline void doomDrawRectOutline(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (w < 2 || h < 2) return;
  doomFillRect(x, y, w, 1, color);
  doomFillRect(x, y + h - 1, w, 1, color);
  doomFillRect(x, y, 1, h, color);
  doomFillRect(x + w - 1, y, 1, h, color);
}

// ── DOOM HUD — "Bunker Console" ─────────────────────────────────────────────
// Dark olive-black CRT aesthetic, DOOM-authentic palette, oversized tilt
// meters, industrial touch buttons. Minimal text — the meters speak.

static void doomDrawBandOverlay() {
  const bool leftActive  = (g_doom.touchZone == DOOM_TOUCH_LEFT);
  const bool rightActive = (g_doom.touchZone == DOOM_TOUCH_RIGHT);

  // ── Palette ───────────────────────────────────────────────────────────
  const uint16_t bgDark      = lv_color_make(6, 8, 4).full;
  const uint16_t scan        = lv_color_make(10, 13, 7).full;
  const uint16_t frameDim    = lv_color_make(44, 38, 28).full;
  const uint16_t textMuted   = lv_color_make(68, 60, 44).full;
  const uint16_t textAmber   = lv_color_make(255, 176, 0).full;
  const uint16_t textWhite   = kCgaPalette16[15];
  const uint16_t greenHi     = lv_color_make(32, 210, 32).full;
  const uint16_t greenMid    = lv_color_make(16, 110, 16).full;
  const uint16_t greenDim    = lv_color_make(8, 44, 8).full;
  const uint16_t redHi       = lv_color_make(210, 36, 16).full;
  const uint16_t redMid      = lv_color_make(110, 18, 8).full;
  const uint16_t redDim      = lv_color_make(44, 10, 4).full;
  const uint16_t meterBg     = lv_color_make(3, 4, 2).full;
  const uint16_t tickCol     = lv_color_make(26, 22, 16).full;
  const uint16_t centerMark  = lv_color_make(84, 74, 54).full;
  const uint16_t shadowCol   = lv_color_make(2, 2, 1).full;

  const int16_t lw  = kDoomLeftBandW;                 // 205
  const int16_t rw  = DB_CANVAS_W - kDoomRightBandX;  // 206
  const int16_t rx  = kDoomRightBandX;                 // 434
  const int16_t bh  = DB_CANVAS_H;                     // 172
  const int16_t lcx = lw / 2;
  const int16_t rcx = rx + (rw / 2);
  const uint16_t readoutCol = g_imu.ready ? textAmber : redHi;

  // ── Backgrounds: dark olive + CRT scanlines ───────────────────────────
  doomFillRect(0, 0, lw, bh, bgDark);
  doomFillRect(rx, 0, rw, bh, bgDark);
  for (int16_t y = 1; y < bh; y += 3) {
    doomFillRect(0,  y, lw, 1, scan);
    doomFillRect(rx, y, rw, 1, scan);
  }

  // ── Separator bars at game-frame edges ────────────────────────────────
  doomFillRect(kDoomFrameX - 3, 0, 3, bh, leftActive  ? greenHi : frameDim);
  doomFillRect(rx,               0, 3, bh, rightActive ? redHi   : frameDim);
  if (leftActive)  doomFillRect(kDoomFrameX - 5, 0, 2, bh, greenDim);
  if (rightActive) doomFillRect(rx + 3,          0, 2, bh, redDim);

  // ── IMU readout strings ───────────────────────────────────────────────
  char moveBuf[16], turnBuf[16];
#if TEST_IMU
  if      (!g_imu.ready)         { snprintf(moveBuf, sizeof(moveBuf), "NO IMU");
                                  snprintf(turnBuf, sizeof(turnBuf), "NO IMU"); }
  else if (!g_doom.neutralReady) { snprintf(moveBuf, sizeof(moveBuf), "CAL");
                                  snprintf(turnBuf, sizeof(turnBuf), "CAL"); }
  else                          { snprintf(moveBuf, sizeof(moveBuf), "%+d", (int)g_doom.moveBin);
                                  snprintf(turnBuf, sizeof(turnBuf), "%+d", (int)g_doom.turnBin); }
#else
  snprintf(moveBuf, sizeof(moveBuf), "--");
  snprintf(turnBuf, sizeof(turnBuf), "--");
#endif

  // ═══════════════════════════════════════════════════════════════════════
  //  LEFT BAND — MOVEMENT
  // ═══════════════════════════════════════════════════════════════════════

  const uint16_t leftFrame = leftActive ? greenMid : frameDim;
  doomDrawRectOutline(2, 2, lw - 6, bh - 4, leftFrame);
  doomDrawFontText(8, 6, "MOVE", &scry_font_funnel_display_12, textMuted);

  // ── Vertical tilt meter (large) ───────────────────────────────────────
  const int16_t vmW = 36, vmH = 100;
  const int16_t vmX = lcx - (vmW / 2), vmY = 22;
  const int16_t vmCY = vmY + (vmH / 2);

  doomFillRect(vmX, vmY, vmW, vmH, meterBg);
  doomDrawRectOutline(vmX, vmY, vmW, vmH, leftFrame);

  for (int8_t t = 1; t < kDoomMoveBinMax; ++t) {
    const int16_t off = (int16_t)((t * ((vmH / 2) - 6)) / kDoomMoveBinMax);
    doomFillRect(vmX + 1,       vmCY - off, 4, 1, tickCol);
    doomFillRect(vmX + vmW - 5, vmCY - off, 4, 1, tickCol);
    doomFillRect(vmX + 1,       vmCY + off, 4, 1, tickCol);
    doomFillRect(vmX + vmW - 5, vmCY + off, 4, 1, tickCol);
  }

  doomFillRect(vmX - 5, vmCY - 1, vmW + 10, 2, centerMark);

  if (g_doom.moveBin != 0) {
    const int16_t halfH = (vmH / 2) - 6;
    const int16_t span  = max<int16_t>(2, (int16_t)((abs(g_doom.moveBin) * halfH) / kDoomMoveBinMax));
    const uint16_t fillC = leftActive ? greenHi : greenMid;
    if (g_doom.moveBin < 0) {
      doomFillRect(vmX + 3, vmCY - span,     vmW - 6, span, fillC);
      doomFillRect(vmX + 3, vmCY - span - 1, vmW - 6, 1,    greenDim);
    } else {
      doomFillRect(vmX + 3, vmCY + 2,        vmW - 6, span, fillC);
      doomFillRect(vmX + 3, vmCY + span + 2, vmW - 6, 1,    greenDim);
    }
  }

  doomDrawFontText(vmX + vmW + 6, vmCY - 8, moveBuf,
                   &scry_font_funnel_display_16, readoutCol);

  // ── Divider + USE button ──────────────────────────────────────────────
  const int16_t btnH = 30, btnM = 10, btnY = bh - btnH - 6;
  doomFillRect(8, btnY - 6, lw - 18, 1, frameDim);
  {
    const int16_t  bw   = lw - (btnM * 2) - 4;
    const uint16_t bg   = leftActive ? greenDim : lv_color_make(10, 12, 8).full;
    const uint16_t bord = leftActive ? greenHi  : frameDim;
    const uint16_t glow = leftActive ? greenMid : lv_color_make(18, 20, 14).full;
    const uint16_t txt  = leftActive ? textWhite : lv_color_make(110, 100, 78).full;
    doomFillRect(btnM, btnY, bw, btnH, bg);
    doomDrawRectOutline(btnM, btnY, bw, btnH, bord);
    doomFillRect(btnM + 1, btnY + 1,        bw - 2, 2, glow);
    doomFillRect(btnM + 1, btnY + btnH - 3, bw - 2, 2, shadowCol);
    doomDrawFontTextCentered(lcx - 2, btnY + 5, "USE", &scry_font_funnel_display_20, txt);
  }

  // ═══════════════════════════════════════════════════════════════════════
  //  RIGHT BAND — COMBAT
  // ═══════════════════════════════════════════════════════════════════════

  const uint16_t rightFrame = rightActive ? redMid : frameDim;
  doomDrawRectOutline(rx + 4, 2, rw - 6, bh - 4, rightFrame);
  doomDrawFontText(rx + 8, 6, "TURN", &scry_font_funnel_display_12, textMuted);

  // ── Horizontal tilt meter (large) ─────────────────────────────────────
  const int16_t hmW = rw - 28, hmH = 26;
  const int16_t hmX = rx + 14, hmY = 24;
  const int16_t hmCX = hmX + (hmW / 2);

  doomFillRect(hmX, hmY, hmW, hmH, meterBg);
  doomDrawRectOutline(hmX, hmY, hmW, hmH, rightFrame);

  for (int8_t t = 1; t < kDoomTurnBinMax; ++t) {
    const int16_t off = (int16_t)((t * ((hmW / 2) - 6)) / kDoomTurnBinMax);
    doomFillRect(hmCX + off, hmY + 1,        1, 4, tickCol);
    doomFillRect(hmCX + off, hmY + hmH - 5,  1, 4, tickCol);
    doomFillRect(hmCX - off, hmY + 1,        1, 4, tickCol);
    doomFillRect(hmCX - off, hmY + hmH - 5,  1, 4, tickCol);
  }

  doomFillRect(hmCX, hmY - 3, 2, hmH + 6, centerMark);

  if (g_doom.turnBin != 0) {
    const int16_t halfW = (hmW / 2) - 6;
    const int16_t span  = max<int16_t>(2, (int16_t)((abs(g_doom.turnBin) * halfW) / kDoomTurnBinMax));
    const uint16_t fillC = rightActive ? textAmber : lv_color_make(180, 120, 0).full;
    if (g_doom.turnBin < 0)
      doomFillRect(hmCX - span, hmY + 4, span, hmH - 8, fillC);
    else
      doomFillRect(hmCX + 2,    hmY + 4, span, hmH - 8, fillC);
  }

  doomDrawFontText(hmX + 3,        hmY + 7, "<", &scry_font_funnel_display_12, textMuted);
  doomDrawFontText(hmX + hmW - 11, hmY + 7, ">", &scry_font_funnel_display_12, textMuted);
  doomDrawFontTextCentered(rcx, hmY + hmH + 5, turnBuf,
                           &scry_font_funnel_display_16, readoutCol);

  // ── Status badge (pre-game) ───────────────────────────────────────────
#if DB_HAS_PRBOOM_DONOR
  if (!doomPrboomHasFrame()) {
    char promptBuf[24];
    if (!g_doom.launchRequested) snprintf(promptBuf, sizeof(promptBuf), "PRESS FIRE");
    else { const char *s = doomPrboomStatus();
           snprintf(promptBuf, sizeof(promptBuf), "%s", s ? s : "BOOTING"); }
    const int16_t bdW = rw - 36, bdX = rx + 18, bdY = 82, bdH = 24;
    doomFillRect(bdX, bdY, bdW, bdH, lv_color_make(14, 10, 4).full);
    doomDrawRectOutline(bdX, bdY, bdW, bdH,
                        g_doom.launchRequested ? textAmber : redHi);
    doomFillRect(bdX + 1, bdY + 1, bdW - 2, 1, lv_color_make(36, 26, 10).full);
    doomDrawFontTextCentered(rcx, bdY + 4, promptBuf,
                             &scry_font_funnel_display_16, textAmber);
  }
#endif

  // ── Divider + FIRE button ─────────────────────────────────────────────
  doomFillRect(rx + 10, btnY - 6, rw - 22, 1, frameDim);
  {
    const int16_t  bw   = rw - (btnM * 2) - 4;
    const uint16_t bg   = rightActive ? redDim  : lv_color_make(12, 8, 6).full;
    const uint16_t bord = rightActive ? redHi   : frameDim;
    const uint16_t glow = rightActive ? redMid  : lv_color_make(20, 14, 10).full;
    const uint16_t txt  = rightActive ? textWhite : lv_color_make(110, 84, 66).full;
    doomFillRect(rx + btnM + 2, btnY, bw, btnH, bg);
    doomDrawRectOutline(rx + btnM + 2, btnY, bw, btnH, bord);
    doomFillRect(rx + btnM + 3, btnY + 1,        bw - 2, 2, glow);
    doomFillRect(rx + btnM + 3, btnY + btnH - 3, bw - 2, 2, shadowCol);
    doomDrawFontTextCentered(rcx, btnY + 5, "FIRE", &scry_font_funnel_display_20, txt);
  }
}

bool doomScrybarPageVisible() {
  return g_uiPageMode == UI_PAGE_DOOM;
}

bool doomScrybarGetInputState(int8_t *moveBin, int8_t *turnBin, uint8_t *touchZone) {
  const bool visible = (g_uiPageMode == UI_PAGE_DOOM);
  if (moveBin) *moveBin = visible ? g_doom.moveBin : 0;
  if (turnBin) *turnBin = visible ? g_doom.turnBin : 0;
  if (touchZone) *touchZone = visible ? g_doom.touchZone : DOOM_TOUCH_NONE;
  return visible;
}

bool doomScrybarBlitIndexedFrame(const uint8_t *pixels,
                                 int srcWidth,
                                 int srcHeight,
                                 const uint16_t *palette565be,
                                 bool forceFlush) {
  if (!pixels || !palette565be) return false;
  if (g_uiPageMode != UI_PAGE_DOOM) return false;
  if (!g_dispHw.canvasBuf) return false;
  if (!initDisplay()) return false;

  memset(g_dispHw.canvasBuf, 0, (size_t)DB_CANVAS_W * DB_CANVAS_H * sizeof(uint16_t));
  for (int16_t y = 0; y < kDoomFrameH; ++y) {
    const int srcY = ((int32_t)y * srcHeight) / kDoomFrameH;
    const size_t srcRow = (size_t)srcY * (size_t)srcWidth;
    uint16_t *dst = g_dispHw.canvasBuf + ((size_t)(kDoomFrameY + y) * DB_CANVAS_W) + kDoomFrameX;
    for (int16_t x = 0; x < kDoomFrameW; ++x) {
      const int srcX = ((int32_t)x * srcWidth) / kDoomFrameW;
      dst[x] = palette565be[pixels[srcRow + (size_t)srcX]];
    }
  }

  doomDrawBandOverlay();
  if (forceFlush) {
    setBacklightPercent(100);
    dispFlush();
  }
  return true;
}

static void doomBuildPalette() {
  if (g_doom.paletteReady) return;
  for (uint16_t i = 0; i < 256; ++i) {
    const uint8_t r = pgm_read_byte(kDoomTitlePicPalette + (i * 3) + 0);
    const uint8_t g = pgm_read_byte(kDoomTitlePicPalette + (i * 3) + 1);
    const uint8_t b = pgm_read_byte(kDoomTitlePicPalette + (i * 3) + 2);
    const uint16_t raw565 = doomRgb888To565(r, g, b);
    g_doom.palette565[i] = (uint16_t)((raw565 << 8) | (raw565 >> 8));
  }
  g_doom.paletteReady = true;
}

static void doomBlitTitlePicToCanvas() {
  if (!g_dispHw.canvasBuf) return;
  doomBuildPalette();
  memset(g_dispHw.canvasBuf, 0, (size_t)DB_CANVAS_W * DB_CANVAS_H * sizeof(uint16_t));

  for (int16_t y = 0; y < kDoomFrameH; ++y) {
    const int16_t srcY = (int16_t)(((int32_t)y * kDoomTitlePicHeight) / kDoomFrameH);
    const size_t srcRow = (size_t)srcY * kDoomTitlePicWidth;
    uint16_t *dst = g_dispHw.canvasBuf + ((size_t)(kDoomFrameY + y) * DB_CANVAS_W) + kDoomFrameX;
    for (int16_t x = 0; x < kDoomFrameW; ++x) {
      const int16_t srcX = (int16_t)(((int32_t)x * kDoomTitlePicWidth) / kDoomFrameW);
      const uint8_t idx = pgm_read_byte(kDoomTitlePicPixels + srcRow + srcX);
      const uint16_t color = g_doom.palette565[idx];
      dst[x] = color;
    }
  }

  doomDrawBandOverlay();
}

static void doomRenderSpike(bool force) {
  if (g_uiPageMode != UI_PAGE_DOOM) return;
  if (!initDisplay()) return;
#if DB_HAS_PRBOOM_DONOR
  if (g_doom.launchRequested) {
    doomPrboomEnsureStarted();
    if (doomPrboomHasFrame()) {
      g_doom.frameDirty = false;
      return;
    }
  }
#endif
  if (!force && !g_doom.frameDirty) return;

  setBacklightPercent(100);
  doomBlitTitlePicToCanvas();
  dispFlush();
  g_doom.frameDirty = false;

  const uint32_t now = millis();
  if (force || g_doom.lastRenderLogMs == 0 || (now - g_doom.lastRenderLogMs) >= 2000) {
    g_doom.lastRenderLogMs = now;
    const char *status = doomPrboomStatus();
    Serial.printf("[DOOM] TITLEPIC rendered src=%ux%u frame=%dx%d@x=%d bands=%d/%d move=%d turn=%d status=%s donor=prboom-go\n",
                  (unsigned)kDoomTitlePicWidth,
                  (unsigned)kDoomTitlePicHeight,
                  (int)kDoomFrameW,
                  (int)kDoomFrameH,
                  (int)kDoomFrameX,
                  (int)kDoomLeftBandW,
                  (int)(DB_CANVAS_W - kDoomRightBandX),
                  (int)g_doom.moveBin,
                  (int)g_doom.turnBin,
                  status ? status : "boot");
  }
}
#elif TEST_DISPLAY && DOOM_SPIKE_ENABLED
static void doomRenderSpike(bool force) {
  (void)force;
  if (g_uiPageMode != UI_PAGE_DOOM) return;
  Serial.println("[DOOM][ERR] spike assets missing (src/doom/doom_titlepic.h)");
}
#else
static void doomRenderSpike(bool force) {
  (void)force;
}
#endif
// ── fine DOOM spike ───────────────────────────────────────────────────────────

// Tile-based transpose+flip of one chunk from canvasBuf into a DMA buffer.
// 16×16 tiles aligned to PSRAM cache line (32B = 16 px) reduce cache misses vs pixel stride.
static inline void dispRotateChunk(uint16_t *dst, int16_t colBase) {
  constexpr int T = 16;  // was 8 — matches PSRAM cache line (32B = 16 px)
  for (int16_t dj0 = 0; dj0 < DB_CHUNK_ROWS; dj0 += T) {
    for (int16_t di0 = 0; di0 < DB_CANVAS_H; di0 += T) {
      const int16_t diEnd = (di0 + T <= DB_CANVAS_H) ? (di0 + T) : DB_CANVAS_H;
      for (int16_t dj = dj0; dj < dj0 + T && dj < DB_CHUNK_ROWS; ++dj) {
        uint16_t *d = &dst[dj * DB_NATIVE_W + di0];
        for (int16_t di = di0; di < diEnd; ++di) {
          d[di - di0] = g_dispHw.canvasBuf[(DB_CANVAS_H - 1 - di) * DB_CANVAS_W + colBase + dj];
        }
      }
    }
  }
}

static bool dispFlush() {
  if (!g_dispHw.panel || !g_dispHw.canvasBuf || !g_dispHw.dmaBuf || !g_dispHw.dmaBuf2 || !g_dispHw.flushSem) return false;

  const uint32_t t0 = micros();
  const int chunks = DB_NATIVE_H / DB_CHUNK_ROWS;  // 640/64 = 10 chunks
  uint16_t *bufCur = g_dispHw.dmaBuf;
  uint16_t *bufNext = g_dispHw.dmaBuf2;

  // Rotate first chunk (no DMA overlap yet)
  dispRotateChunk(bufCur, 0);

  // Start DMA on first chunk
  xSemaphoreGive(g_dispHw.flushSem);
  xSemaphoreTake(g_dispHw.flushSem, portMAX_DELAY);
  esp_lcd_panel_draw_bitmap(g_dispHw.panel, 0, 0, DB_NATIVE_W, DB_CHUNK_ROWS, bufCur);

  // Pipeline: rotate chunk c into bufNext while DMA sends chunk c-1 from bufCur
  for (int c = 1; c < chunks; ++c) {
    dispRotateChunk(bufNext, c * DB_CHUNK_ROWS);

    xSemaphoreTake(g_dispHw.flushSem, portMAX_DELAY);
    esp_lcd_panel_draw_bitmap(g_dispHw.panel, 0, c * DB_CHUNK_ROWS, DB_NATIVE_W, (c + 1) * DB_CHUNK_ROWS, bufNext);

    uint16_t *tmp = bufCur;
    bufCur = bufNext;
    bufNext = tmp;
  }

  // Wait for last DMA to complete
  xSemaphoreTake(g_dispHw.flushSem, portMAX_DELAY);

  const uint32_t dt = micros() - t0;
  g_perf.flushCount++;
  g_perf.flushTotalUs += dt;
  if (dt > g_perf.flushMaxUs) g_perf.flushMaxUs = dt;

  return true;
}
#endif

static bool initDisplay() {
#if TEST_DISPLAY && DISPLAY_BACKEND_ESP_LCD
  if (g_dispHw.panel != nullptr) return true;

  gpio_config_t rst_cfg = {};
  rst_cfg.intr_type = GPIO_INTR_DISABLE;
  rst_cfg.mode = GPIO_MODE_OUTPUT;
  rst_cfg.pin_bit_mask = (1ULL << LCD_RST_PIN);
  rst_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  rst_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
  if (gpio_config(&rst_cfg) != ESP_OK) {
    Serial.println("[ERR] GPIO config LCD_RST failed.");
    return false;
  }

  spi_bus_config_t buscfg = {};
  buscfg.sclk_io_num = LCD_QSPI_SCK_PIN;
  buscfg.data0_io_num = LCD_QSPI_D0_PIN;
  buscfg.data1_io_num = LCD_QSPI_D1_PIN;
  buscfg.data2_io_num = LCD_QSPI_D2_PIN;
  buscfg.data3_io_num = LCD_QSPI_D3_PIN;
  buscfg.max_transfer_sz = DB_NATIVE_W * DB_CHUNK_ROWS * 2;
  if (spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO) != ESP_OK) {
    Serial.println("[ERR] spi_bus_initialize failed.");
    return false;
  }

  g_dispHw.flushSem = xSemaphoreCreateBinary();
  if (!g_dispHw.flushSem) {
    Serial.println("[ERR] flush semaphore alloc failed.");
    return false;
  }

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
  if (esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &g_dispHw.panelIo) != ESP_OK) {
    Serial.println("[ERR] esp_lcd_new_panel_io_spi failed.");
    return false;
  }

  static const uint8_t kLcdCmd11Data[] = {0x00};
  static const uint8_t kLcdCmd29Data[] = {0x00};
  static const axs15231b_lcd_init_cmd_t lcd_init_cmds[] = {
      {0x11, kLcdCmd11Data, 0, 100},
      {0x29, kLcdCmd29Data, 0, 100},
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
  if (esp_lcd_new_panel_axs15231b(g_dispHw.panelIo, &panel_config, &g_dispHw.panel) != ESP_OK) {
    Serial.println("[ERR] esp_lcd_new_panel_axs15231b failed.");
    return false;
  }

  gpio_set_level((gpio_num_t)LCD_RST_PIN, 1);
  delay(30);
  gpio_set_level((gpio_num_t)LCD_RST_PIN, 0);
  delay(250);
  gpio_set_level((gpio_num_t)LCD_RST_PIN, 1);
  delay(30);
  if (esp_lcd_panel_init(g_dispHw.panel) != ESP_OK) {
    Serial.println("[ERR] esp_lcd_panel_init failed.");
    return false;
  }
#if DISPLAY_FLIP_180
  if (esp_lcd_panel_mirror(g_dispHw.panel, true, true) != ESP_OK) {
    Serial.println("[ERR] esp_lcd_panel_mirror(180) failed.");
    return false;
  }
#endif

  g_dispHw.canvasBuf = (uint16_t *)heap_caps_malloc(DB_CANVAS_W * DB_CANVAS_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
  g_dispHw.dmaBuf = (uint16_t *)heap_caps_malloc(DB_NATIVE_W * DB_CHUNK_ROWS * sizeof(uint16_t), MALLOC_CAP_DMA);
  g_dispHw.dmaBuf2 = (uint16_t *)heap_caps_malloc(DB_NATIVE_W * DB_CHUNK_ROWS * sizeof(uint16_t), MALLOC_CAP_DMA);
  if (!g_dispHw.canvasBuf || !g_dispHw.dmaBuf || !g_dispHw.dmaBuf2) {
    Serial.println("[ERR] display buffers alloc failed.");
    return false;
  }

  dispFillScreen(DB_COLOR_BLACK);
  dispFlush();
  Serial.printf("[DISPLAY] esp_lcd init ok. native=%dx%d canvas=%dx%d flip180=%d\n",
                DB_NATIVE_W, DB_NATIVE_H, DB_CANVAS_W, DB_CANVAS_H, DISPLAY_FLIP_180 ? 1 : 0);
  return true;
#elif HAS_ARDUINO_GFX
  if (g_gfx != nullptr) return true;
  const uint8_t runtimeRotation = (uint8_t)((DISPLAY_ROTATION + (DISPLAY_FLIP_180 ? 2 : 0)) & 0x03);

  g_qspiBus = new Arduino_ESP32QSPI(
      LCD_QSPI_CS_PIN /* cs */,
      LCD_QSPI_SCK_PIN /* sck */,
      LCD_QSPI_D0_PIN /* d0 */,
      LCD_QSPI_D1_PIN /* d1 */,
      LCD_QSPI_D2_PIN /* d2 */,
      LCD_QSPI_D3_PIN /* d3 */);

  g_gfx = new Arduino_AXS15231B(
      g_qspiBus,
      LCD_RST_PIN /* rst */,
      runtimeRotation /* rotation */,
      false /* ips */,
      LCD_WIDTH,
      LCD_HEIGHT);

  bool ok = g_gfx->begin(40000000UL);
  if (!ok) {
    Serial.println("[ERR] Display begin() failed.");
    return false;
  }
  Serial.printf("[DISPLAY] init ok. rotation=%d (cfg=%d flip180=%d) native=%dx%d canvas=%dx%d mode=%d (cfg=%dx%d)\n",
                runtimeRotation,
                DISPLAY_ROTATION,
                DISPLAY_FLIP_180 ? 1 : 0,
                g_gfx->width(),
                g_gfx->height(),
                (DISPLAY_COORD_MODE == 1) ? LCD_HEIGHT : g_gfx->width(),
                (DISPLAY_COORD_MODE == 1) ? LCD_WIDTH : g_gfx->height(),
                DISPLAY_COORD_MODE,
                LCD_WIDTH,
                LCD_HEIGHT);
  g_gfx->fillScreen(DB_COLOR_BLACK);
  return true;
#else
  Serial.println("[ERR] Arduino_GFX_Library non trovata. Installa 'GFX Library for Arduino'.");
  return false;
#endif
}

#if TEST_WIFI || TEST_NTP
static const char *wlStatusToStr(wl_status_t s) {
  switch (s) {
    case WL_IDLE_STATUS: return "IDLE";
    case WL_NO_SSID_AVAIL: return "NO_SSID";
    case WL_SCAN_COMPLETED: return "SCAN_DONE";
    case WL_CONNECTED: return "CONNECTED";
    case WL_CONNECT_FAILED: return "CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED: return "DISCONNECTED";
    default: return "UNKNOWN";
  }
}

static bool applyWiFiDnsOverrideIfEnabled(bool verbose);
#if WEB_CONFIG_ENABLED
static void webConfigStartCaptiveDnsIfNeeded();
static void webConfigStopCaptiveDns();
#endif
static bool wifiSetupModeIsOff() { return strcmp(g_wifiSt.setupMode, "off") == 0; }
static bool wifiSetupModeIsOn() { return strcmp(g_wifiSt.setupMode, "on") == 0; }
static bool wifiSetupModeIsAuto() { return !wifiSetupModeIsOff() && !wifiSetupModeIsOn(); }

static void wifiBuildSetupApSsid() {
  if (g_wifiSt.setupApSsid[0]) return;
  const uint64_t mac = ESP.getEfuseMac();
  const uint8_t tailA = (uint8_t)((mac >> 8) & 0xFF);
  const uint8_t tailB = (uint8_t)(mac & 0xFF);
  snprintf(g_wifiSt.setupApSsid, sizeof(g_wifiSt.setupApSsid), "%s-%02X%02X", WIFI_SETUP_AP_SSID_PREFIX, tailA, tailB);
}

static void wifiBuildSetupPortalUrl(char *out, size_t outLen) {
  if (!out || outLen == 0) return;
  IPAddress apIp = WiFi.softAPIP();
  if ((uint32_t)apIp == 0U) apIp = IPAddress(192, 168, 4, 1);
  snprintf(out, outLen, "http://%s:%u", apIp.toString().c_str(), (unsigned)WEB_CONFIG_PORT);
  out[outLen - 1] = '\0';
}

static bool wifiStartSetupAp(bool autoStart) {
  if (g_wifiSt.setupApActive) return true;
  wifiBuildSetupApSsid();
  WiFi.mode(WIFI_AP_STA);
  bool ok = false;
  if (strlen(WIFI_SETUP_AP_PASSWORD) >= 8) {
    ok = WiFi.softAP(g_wifiSt.setupApSsid, WIFI_SETUP_AP_PASSWORD, WIFI_SETUP_AP_CHANNEL, false, WIFI_SETUP_AP_MAX_CLIENTS);
  } else {
    ok = WiFi.softAP(g_wifiSt.setupApSsid, nullptr, WIFI_SETUP_AP_CHANNEL, false, WIFI_SETUP_AP_MAX_CLIENTS);
  }
  if (!ok) {
    Serial.println("[WIFI][AP][ERR] impossibile avviare setup AP");
    return false;
  }
  g_wifiSt.setupApActive = true;
  g_wifiSt.setupApAutoStarted = autoStart;
#if WEB_CONFIG_ENABLED
  webConfigStartCaptiveDnsIfNeeded();
#endif
  Serial.printf("[WIFI][AP] setup active ssid='%s' ip=%s mode=%s\n",
                g_wifiSt.setupApSsid,
                WiFi.softAPIP().toString().c_str(),
                autoStart ? "auto" : "manual");
  return true;
}

static void wifiStopSetupAp() {
  if (!g_wifiSt.setupApActive) return;
  WiFi.softAPdisconnect(true);
  g_wifiSt.setupApActive = false;
  g_wifiSt.setupApAutoStarted = false;
#if WEB_CONFIG_ENABLED
  webConfigStopCaptiveDns();
#endif
  WiFi.mode(WIFI_STA);
  Serial.println("[WIFI][AP] setup disabled");
}

static void wifiScheduleNextAttempt(uint32_t nowMs, uint32_t delayMs) {
  g_wifiSt.reconnectAttemptActive = false;
  g_wifiSt.reconnectNextAttemptMs = nowMs + delayMs;
}

static void wifiRotateCredentialIndex() {
  if (g_wifiSt.credCount <= 1) return;
  g_wifiSt.reconnectIdx = (uint8_t)((g_wifiSt.reconnectIdx + 1U) % g_wifiSt.credCount);
}

static void wifiRearmStationRadio(uint32_t nowMs, const char *cause) {
  if ((nowMs - g_wifiSt.lastRadioResetMs) < 1500UL) {
    wifiScheduleNextAttempt(nowMs, WIFI_RETRY_AFTER_RADIO_RESET_MS);
    return;
  }
  Serial.printf("[WIFI][HEAL] radio reset cause=%s\n", cause ? cause : "-");
  g_wifiSt.internalDisconnect = true;
  WiFi.disconnect(true, false);
  delay(20);
  WiFi.mode(WIFI_OFF);
  g_wifiSt.setupApActive = false;
  g_wifiSt.setupApAutoStarted = false;
  delay(60);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  g_wifiSt.internalDisconnect = false;
  g_wifiSt.lastDiscReason = -1;
  g_wifiSt.lastRadioResetMs = millis();
  wifiScheduleNextAttempt(g_wifiSt.lastRadioResetMs, WIFI_RETRY_AFTER_RADIO_RESET_MS);
}

static void wifiHandleFailure(uint32_t nowMs, const char *cause) {
  const uint8_t failedIdx = g_wifiSt.reconnectIdx;
  const char *failedSsid = g_wifiSt.credSsids[failedIdx] ? g_wifiSt.credSsids[failedIdx] : "-";
  wifiRotateCredentialIndex();
  const char *nextSsid = g_wifiSt.credSsids[g_wifiSt.reconnectIdx] ? g_wifiSt.credSsids[g_wifiSt.reconnectIdx] : "-";
  ++g_wifiSt.consecutiveFailCount;
  Serial.printf("[WIFI][RETRY] cause=%s fail_streak=%u failed='%s' next='%s'\n",
                cause ? cause : "-",
                (unsigned)g_wifiSt.consecutiveFailCount,
                failedSsid,
                nextSsid);
  if (g_wifiSt.consecutiveFailCount >= WIFI_RETRY_FAILS_BEFORE_RADIO_RESET) {
    g_wifiSt.consecutiveFailCount = 0;
    wifiRearmStationRadio(nowMs, cause);
    return;
  }
  wifiScheduleNextAttempt(nowMs, WIFI_RETRY_STEP_DELAY_MS);
}

static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    g_wifiSt.connected = true;
    g_wifiSt.everConnected = true;
    g_wifiSt.lastConnectMs = millis();
    g_wifiSt.noLinkSinceMs = 0;
    g_wifiSt.consecutiveFailCount = 0;
    Serial.printf("[WIFI][GOT_IP] ip=%s\n", WiFi.localIP().toString().c_str());
    const String activeSsid = WiFi.SSID();
    for (uint8_t i = 0; i < g_wifiSt.credCount; ++i) {
      const char *known = g_wifiSt.credSsids[i];
      if (known && activeSsid.equals(known)) {
        g_wifiSt.reconnectIdx = i;
        break;
      }
    }
    const bool dnsOk = applyWiFiDnsOverrideIfEnabled(false);
    Serial.printf("[WIFI][DNS] active=%s/%s (%s)\n",
                  WiFi.dnsIP(0).toString().c_str(),
                  WiFi.dnsIP(1).toString().c_str(),
                  dnsOk ? "OK" : "ERR");
    wifiScheduleNextAttempt(millis(), 0UL);
#if WEB_CONFIG_ENABLED
    ensureScryBarMdnsStarted();
#endif
  } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    const bool attemptWasActive = g_wifiSt.reconnectAttemptActive;
    g_wifiSt.connected = false;
    g_wifiSt.lastDisconnectMs = millis();
    if (g_wifiSt.noLinkSinceMs == 0) g_wifiSt.noLinkSinceMs = g_wifiSt.lastDisconnectMs;
    g_wifiSt.lastDiscReason = (int)info.wifi_sta_disconnected.reason;
#if WEB_CONFIG_ENABLED
    stopScryBarMdns();
#endif
    Serial.printf("[WIFI][DISC] reason=%d (%s)\n",
                  g_wifiSt.lastDiscReason,
                  WiFi.disconnectReasonName((wifi_err_reason_t)g_wifiSt.lastDiscReason));
    if (g_wifiSt.credCount == 0) return;
    if (g_wifiSt.internalDisconnect) {
      wifiScheduleNextAttempt(millis(), WIFI_RETRY_STEP_DELAY_MS);
      return;
    }
    if (attemptWasActive) {
      wifiHandleFailure(millis(), "event-disconnect");
      return;
    }
    // Link dropped after having been connected: retry soon on last known good SSID.
    wifiScheduleNextAttempt(millis(), WIFI_RETRY_STEP_DELAY_MS);
  }
}

static size_t buildWiFiStaticCredentialList(const char **ssidOut, const char **passOut, size_t cap) {
  size_t n = 0;
#if defined(WIFI_SSID) && defined(WIFI_PASSWORD)
  if (n < cap && strlen(WIFI_SSID) > 0) {
    ssidOut[n] = WIFI_SSID;
    passOut[n] = WIFI_PASSWORD;
    ++n;
  }
#endif
#if defined(WIFI_SSID_2) && defined(WIFI_PASSWORD_2)
  if (n < cap && strlen(WIFI_SSID_2) > 0) {
    ssidOut[n] = WIFI_SSID_2;
    passOut[n] = WIFI_PASSWORD_2;
    ++n;
  }
#endif
#if defined(WIFI_SSID_3) && defined(WIFI_PASSWORD_3)
  if (n < cap && strlen(WIFI_SSID_3) > 0) {
    ssidOut[n] = WIFI_SSID_3;
    passOut[n] = WIFI_PASSWORD_3;
    ++n;
  }
#endif
#if defined(WIFI_SSID_4) && defined(WIFI_PASSWORD_4)
  if (n < cap && strlen(WIFI_SSID_4) > 0) {
    ssidOut[n] = WIFI_SSID_4;
    passOut[n] = WIFI_PASSWORD_4;
    ++n;
  }
#endif
#if defined(WIFI_SSID_5) && defined(WIFI_PASSWORD_5)
  if (n < cap && strlen(WIFI_SSID_5) > 0) {
    ssidOut[n] = WIFI_SSID_5;
    passOut[n] = WIFI_PASSWORD_5;
    ++n;
  }
#endif
  return n;
}

static int8_t findWiFiCredentialIndexBySsid(const char *ssid) {
  if (!ssid || !ssid[0]) return -1;
  for (uint8_t i = 0; i < g_wifiSt.credCount; ++i) {
    const char *known = g_wifiSt.credSsids[i];
    if (!known || !known[0]) continue;
    if (strcmp(known, ssid) == 0) return (int8_t)i;
  }
  return -1;
}

static void wifiPrepareCredentialCache() {
  g_wifiSt.staticCredCount = buildWiFiStaticCredentialList(g_wifiSt.staticSsids, g_wifiSt.staticPasswords, WIFI_STATIC_CREDENTIALS_MAX);
  g_wifiSt.credCount = 0;
  for (uint8_t i = 0; i < g_wifiSt.runtimeCredCount && g_wifiSt.credCount < WIFI_TOTAL_CREDENTIALS_MAX; ++i) {
    const char *ssid = g_wifiSt.runtimeCreds[i].ssid;
    if (!ssid[0]) continue;
    g_wifiSt.credSsids[g_wifiSt.credCount] = g_wifiSt.runtimeCreds[i].ssid;
    g_wifiSt.credPasswords[g_wifiSt.credCount] = g_wifiSt.runtimeCreds[i].password;
    ++g_wifiSt.credCount;
  }
  for (size_t i = 0; i < g_wifiSt.staticCredCount && g_wifiSt.credCount < WIFI_TOTAL_CREDENTIALS_MAX; ++i) {
    const char *ssid = g_wifiSt.staticSsids[i];
    if (!ssid || !ssid[0]) continue;
    bool duplicate = false;
    for (size_t j = 0; j < g_wifiSt.credCount; ++j) {
      if (g_wifiSt.credSsids[j] && strcmp(g_wifiSt.credSsids[j], ssid) == 0) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) continue;
    g_wifiSt.credSsids[g_wifiSt.credCount] = ssid;
    g_wifiSt.credPasswords[g_wifiSt.credCount] = g_wifiSt.staticPasswords[i];
    ++g_wifiSt.credCount;
  }
  if (g_wifiSt.credCount == 0) {
    Serial.println("[WIFI][WARN] Nessuna rete nota configurata (secrets + NVS)");
  } else {
    Serial.printf("[WIFI] reti note: %u (secrets=%u runtime=%u)\n",
                  (unsigned)g_wifiSt.credCount,
                  (unsigned)g_wifiSt.staticCredCount,
                  (unsigned)g_wifiSt.runtimeCredCount);
  }
  g_wifiSt.reconnectIdx = 0;
  if (g_wifiSt.preferredSsid[0]) {
    const int8_t preferredIdx = findWiFiCredentialIndexBySsid(g_wifiSt.preferredSsid);
    if (preferredIdx >= 0) g_wifiSt.reconnectIdx = (uint8_t)preferredIdx;
  }
  g_wifiSt.reconnectAttemptActive = false;
  g_wifiSt.reconnectAttemptStartMs = 0;
  g_wifiSt.reconnectNextAttemptMs = 0;
  g_wifiSt.consecutiveFailCount = 0;
  g_wifiSt.lastRadioResetMs = 0;
  g_wifiSt.internalDisconnect = false;
}

static bool wifiIsConnectedNow();

static void wifiHandleSetupModeLoop(uint32_t nowMs) {
  if (wifiSetupModeIsOn()) {
    (void)wifiStartSetupAp(false);
    if (wifiIsConnectedNow()) g_wifiSt.noLinkSinceMs = 0;
    return;
  }

  if (wifiIsConnectedNow()) {
    g_wifiSt.noLinkSinceMs = 0;
    if (g_wifiSt.setupApActive) wifiStopSetupAp();
    return;
  }
  if (g_wifiSt.noLinkSinceMs == 0) g_wifiSt.noLinkSinceMs = nowMs;

  if (wifiSetupModeIsOff()) {
    if (g_wifiSt.setupApActive) wifiStopSetupAp();
    return;
  }
  if (!wifiSetupModeIsAuto()) return;

  const uint32_t bootstrapDelay = (g_wifiSt.credCount == 0) ? 500UL : WIFI_SETUP_AP_AUTOSTART_MS;
  if (!g_wifiSt.setupApActive && (nowMs - g_wifiSt.noLinkSinceMs) >= bootstrapDelay) {
    (void)wifiStartSetupAp(true);
  }
}

static bool wifiIsConnectedNow() {
  return (WiFi.status() == WL_CONNECTED) && g_wifiSt.connected;
}

static void wifiBeginAttempt(uint8_t idx) {
  if (idx >= g_wifiSt.credCount) return;
  const char *ssid = g_wifiSt.credSsids[idx];
  const char *password = g_wifiSt.credPasswords[idx];
  if (!ssid || !ssid[0]) return;

  g_wifiSt.connected = false;
  g_wifiSt.lastDiscReason = -1;
  g_wifiSt.internalDisconnect = true;
  WiFi.disconnect(true, false);
  delay(20);
  g_wifiSt.internalDisconnect = false;

  Serial.printf("[WIFI] try %u/%u ssid='%s' timeout=%ums\n",
                (unsigned)(idx + 1),
                (unsigned)g_wifiSt.credCount,
                ssid,
                (unsigned)WIFI_CONNECT_TIMEOUT_MS);
  if (password && password[0]) WiFi.begin(ssid, password);
  else WiFi.begin(ssid);

  g_wifiSt.reconnectAttemptActive = true;
  g_wifiSt.reconnectAttemptStartMs = millis();
}

static void handleWiFiReconnectLoop(uint32_t nowMs) {
  if (g_wifiSt.credCount == 0) return;
  if (wifiIsConnectedNow()) {
    g_wifiSt.reconnectAttemptActive = false;
    return;
  }

  if (!g_wifiSt.reconnectAttemptActive) {
    if (nowMs < g_wifiSt.reconnectNextAttemptMs) return;
    wifiBeginAttempt(g_wifiSt.reconnectIdx);
    return;
  }

  if ((nowMs - g_wifiSt.reconnectAttemptStartMs) < (uint32_t)WIFI_CONNECT_TIMEOUT_MS) return;

  const uint8_t failedIdx = g_wifiSt.reconnectIdx;
  const char *failedSsid = g_wifiSt.credSsids[failedIdx] ? g_wifiSt.credSsids[failedIdx] : "-";
  Serial.printf("[WIFI][FAIL] ssid='%s' status=%s (%d)\n",
                failedSsid,
                wlStatusToStr(WiFi.status()),
                (int)WiFi.status());
  if (g_wifiSt.lastDiscReason >= 0) {
    Serial.printf("[WIFI][FAIL] reason=%d (%s)\n",
                  g_wifiSt.lastDiscReason,
                  WiFi.disconnectReasonName((wifi_err_reason_t)g_wifiSt.lastDiscReason));
  }

  // Close current attempt first; avoid event-side double handling.
  g_wifiSt.reconnectAttemptActive = false;
  g_wifiSt.internalDisconnect = true;
  WiFi.disconnect(true, false);
  delay(10);
  g_wifiSt.internalDisconnect = false;
  wifiHandleFailure(nowMs, "timeout");
}

static bool applyWiFiDnsOverrideIfEnabled(bool verbose) {
#if WIFI_DNS_OVERRIDE_ENABLED
  const IPAddress dns1 = WIFI_DNS1_IP;
  const IPAddress dns2 = WIFI_DNS2_IP;
  const bool ok = WiFi.setDNS(dns1, dns2);
  if (verbose) {
    Serial.printf("[WIFI] DNS policy %s target=%s/%s active=%s/%s\n",
                  ok ? "OK" : "ERR",
                  dns1.toString().c_str(),
                  dns2.toString().c_str(),
                  WiFi.dnsIP(0).toString().c_str(),
                  WiFi.dnsIP(1).toString().c_str());
  }
  return ok;
#else
  (void)verbose;
  return true;
#endif
}

static bool runWiFiConnectTest() {
  Serial.println();
  Serial.println("=================================================");
  Serial.println("ScryBar | M0.6 WiFi connect");
  Serial.println("=================================================");

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.disconnect(true, false);
  delay(100);

  g_wifiSt.connected = false;
  g_wifiSt.lastDiscReason = -1;
  if (!g_wifiSt.eventRegistered) {
    WiFi.onEvent(onWiFiEvent);
    g_wifiSt.eventRegistered = true;
  }
  normalizeWifiSetupMode();
  wifiPrepareCredentialCache();
  g_wifiSt.noLinkSinceMs = millis();

  if (g_wifiSt.credCount > 0) {
    // Non-blocking strategy: one SSID attempt at a time, cycled in loop().
    wifiBeginAttempt(g_wifiSt.reconnectIdx);
  } else {
    Serial.println("[WIFI][INFO] Nessuna rete nota: avvio setup AP fallback.");
    (void)wifiStartSetupAp(true);
  }
  wifiHandleSetupModeLoop(millis());
  return false;
}

static bool runNtpTimeTest() {
  Serial.println();
  Serial.println("=================================================");
  Serial.println("ScryBar | M0.7 NTP time");
  Serial.println("=================================================");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[FAIL] WiFi non connesso: salto NTP.");
    return false;
  }

  Serial.printf("[STEP] NTP sync tz='%s' servers='%s','%s'\n", NTP_TZ_INFO, NTP_SERVER_1, NTP_SERVER_2);
  configTzTime(NTP_TZ_INFO, NTP_SERVER_1, NTP_SERVER_2);

  struct tm timeinfo;
  uint32_t start = millis();
  while ((millis() - start) < NTP_SYNC_TIMEOUT_MS) {
    if (getLocalTime(&timeinfo, 1000)) {
      char buf[64];
      strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
      Serial.printf("[OK] NTP sync riuscita. local_time=%s\n", buf);
      g_clock.ntpSynced = true;
      g_clock.lastSecond = -1;
      g_clock.lastDateKey = -1;
      g_clock.staticDrawn = false;
      return true;
    }
  }

  g_clock.ntpSynced = false;
  Serial.printf("[FAIL] NTP timeout dopo %u ms\n", NTP_SYNC_TIMEOUT_MS);
  return false;
}

#if TEST_WIFI
static void copyStringSafe(char *dst, size_t dstLen, const char *src) {
  if (!dst || dstLen == 0) return;
  if (!src) src = "";
  strncpy(dst, src, dstLen - 1);
  dst[dstLen - 1] = '\0';
}

#if WEB_CONFIG_ENABLED && DB_HAS_MDNS
static void buildScryBarMdnsIdentity(char *hostOut, size_t hostLen, char *instanceOut, size_t instanceLen) {
  const uint64_t mac = ESP.getEfuseMac();
  const uint8_t tailA = (uint8_t)((mac >> 8) & 0xFFu);
  const uint8_t tailB = (uint8_t)(mac & 0xFFu);
  if (hostOut && hostLen > 0) {
    snprintf(hostOut, hostLen, "scrybar-%02x%02x", tailA, tailB);
    hostOut[hostLen - 1] = '\0';
  }
  if (instanceOut && instanceLen > 0) {
    snprintf(instanceOut, instanceLen, "ScryBar %02X%02X", tailA, tailB);
    instanceOut[instanceLen - 1] = '\0';
  }
}

static void ensureScryBarMdnsStarted() {
  if (g_webCfg.mdnsStarted) return;
  if (!g_wifiSt.connected || WiFi.status() != WL_CONNECTED) return;

  char host[sizeof(g_webCfg.mdnsHost)] = {0};
  char instance[sizeof(g_webCfg.mdnsInstanceName)] = {0};
  buildScryBarMdnsIdentity(host, sizeof(host), instance, sizeof(instance));
  if (!host[0]) return;

  if (!MDNS.begin(host)) {
    Serial.printf("[MDNS][ERR] begin failed for '%s'\n", host);
    return;
  }

  MDNS.setInstanceName(instance);
  if (!MDNS.addService("scrybar", "tcp", WEB_CONFIG_PORT)) {
    Serial.printf("[MDNS][ERR] addService failed for '%s'\n", host);
    MDNS.end();
    return;
  }
  MDNS.addServiceTxt("scrybar", "tcp", "device", "ScryBar");
  MDNS.addServiceTxt("scrybar", "tcp", "fw", FW_BUILD_TAG);
  MDNS.addServiceTxt("scrybar", "tcp", "api", "/api/now-playing");

  copyStringSafe(g_webCfg.mdnsHost, sizeof(g_webCfg.mdnsHost), host);
  copyStringSafe(g_webCfg.mdnsInstanceName, sizeof(g_webCfg.mdnsInstanceName), instance);
  g_webCfg.mdnsStarted = true;
  Serial.printf("[MDNS] service='%s' host=%s.local type=_scrybar._tcp port=%u\n",
                g_webCfg.mdnsInstanceName,
                g_webCfg.mdnsHost,
                (unsigned)WEB_CONFIG_PORT);
}

static void stopScryBarMdns() {
  if (!g_webCfg.mdnsStarted) return;
  MDNS.end();
  g_webCfg.mdnsStarted = false;
  g_webCfg.mdnsHost[0] = '\0';
  g_webCfg.mdnsInstanceName[0] = '\0';
  Serial.println("[MDNS] stopped");
}
#elif WEB_CONFIG_ENABLED
static void ensureScryBarMdnsStarted() {}
static void stopScryBarMdns() {}
#endif

static bool startsWithHttp(const char *url) {
  if (!url) return false;
  return (strncmp(url, "http://", 7) == 0) || (strncmp(url, "https://", 8) == 0);
}

static void normalizeWifiSetupMode() {
  if (wifiSetupModeIsOff() || wifiSetupModeIsOn() || wifiSetupModeIsAuto()) return;
  copyStringSafe(g_wifiSt.setupMode, sizeof(g_wifiSt.setupMode), WIFI_SETUP_MODE_DEFAULT);
  if (!wifiSetupModeIsOff() && !wifiSetupModeIsOn() && !wifiSetupModeIsAuto()) {
    copyStringSafe(g_wifiSt.setupMode, sizeof(g_wifiSt.setupMode), "auto");
  }
}

static int8_t findRuntimeWiFiCredentialBySsid(const char *ssid) {
  if (!ssid || !ssid[0]) return -1;
  for (uint8_t i = 0; i < g_wifiSt.runtimeCredCount; ++i) {
    if (strcmp(g_wifiSt.runtimeCreds[i].ssid, ssid) == 0) return (int8_t)i;
  }
  return -1;
}

static bool upsertRuntimeWiFiCredential(const char *ssid, const char *password) {
  if (!ssid || !ssid[0]) return false;
  if (strlen(ssid) > WIFI_MAX_SSID_LEN) return false;
  const char *safePass = password ? password : "";
  if (strlen(safePass) > WIFI_MAX_PASSWORD_LEN) return false;

  const int8_t existing = findRuntimeWiFiCredentialBySsid(ssid);
  if (existing >= 0) {
    copyStringSafe(g_wifiSt.runtimeCreds[(uint8_t)existing].password, sizeof(g_wifiSt.runtimeCreds[(uint8_t)existing].password), safePass);
    return true;
  }

  if (g_wifiSt.runtimeCredCount < WIFI_RUNTIME_CREDENTIALS_MAX) {
    copyStringSafe(g_wifiSt.runtimeCreds[g_wifiSt.runtimeCredCount].ssid, sizeof(g_wifiSt.runtimeCreds[g_wifiSt.runtimeCredCount].ssid), ssid);
    copyStringSafe(g_wifiSt.runtimeCreds[g_wifiSt.runtimeCredCount].password, sizeof(g_wifiSt.runtimeCreds[g_wifiSt.runtimeCredCount].password), safePass);
    ++g_wifiSt.runtimeCredCount;
    return true;
  }

  // FIFO eviction when runtime slots are full.
  for (uint8_t i = 1; i < WIFI_RUNTIME_CREDENTIALS_MAX; ++i) {
    copyStringSafe(g_wifiSt.runtimeCreds[i - 1].ssid, sizeof(g_wifiSt.runtimeCreds[i - 1].ssid), g_wifiSt.runtimeCreds[i].ssid);
    copyStringSafe(g_wifiSt.runtimeCreds[i - 1].password, sizeof(g_wifiSt.runtimeCreds[i - 1].password), g_wifiSt.runtimeCreds[i].password);
  }
  const uint8_t tail = WIFI_RUNTIME_CREDENTIALS_MAX - 1;
  copyStringSafe(g_wifiSt.runtimeCreds[tail].ssid, sizeof(g_wifiSt.runtimeCreds[tail].ssid), ssid);
  copyStringSafe(g_wifiSt.runtimeCreds[tail].password, sizeof(g_wifiSt.runtimeCreds[tail].password), safePass);
  return true;
}

static void loadRuntimeWiFiCredentialsFromPrefs(Preferences &prefs, bool &loadedAny) {
  g_wifiSt.runtimeCredCount = 0;
  const uint8_t rawCount = prefs.getUChar("wifi_dyn_n", 0);
  const uint8_t count = (rawCount > WIFI_RUNTIME_CREDENTIALS_MAX) ? WIFI_RUNTIME_CREDENTIALS_MAX : rawCount;
  for (uint8_t i = 0; i < count; ++i) {
    char keySsid[16];
    char keyPass[16];
    snprintf(keySsid, sizeof(keySsid), "wifi_ds%u", (unsigned)(i + 1));
    snprintf(keyPass, sizeof(keyPass), "wifi_dp%u", (unsigned)(i + 1));
    const String ssid = prefs.getString(keySsid, "");
    if (ssid.length() == 0 || ssid.length() > WIFI_MAX_SSID_LEN) continue;
    const String pass = prefs.getString(keyPass, "");
    if (pass.length() > WIFI_MAX_PASSWORD_LEN) continue;
    copyStringSafe(g_wifiSt.runtimeCreds[g_wifiSt.runtimeCredCount].ssid, sizeof(g_wifiSt.runtimeCreds[g_wifiSt.runtimeCredCount].ssid), ssid.c_str());
    copyStringSafe(g_wifiSt.runtimeCreds[g_wifiSt.runtimeCredCount].password, sizeof(g_wifiSt.runtimeCreds[g_wifiSt.runtimeCredCount].password), pass.c_str());
    ++g_wifiSt.runtimeCredCount;
    loadedAny = true;
  }
}

static size_t saveRuntimeWiFiCredentialsToPrefs(Preferences &prefs) {
  size_t bytes = prefs.putUChar("wifi_dyn_n", g_wifiSt.runtimeCredCount);
  for (uint8_t i = 0; i < WIFI_RUNTIME_CREDENTIALS_MAX; ++i) {
    char keySsid[16];
    char keyPass[16];
    snprintf(keySsid, sizeof(keySsid), "wifi_ds%u", (unsigned)(i + 1));
    snprintf(keyPass, sizeof(keyPass), "wifi_dp%u", (unsigned)(i + 1));
    if (i < g_wifiSt.runtimeCredCount) {
      bytes += prefs.putString(keySsid, g_wifiSt.runtimeCreds[i].ssid);
      bytes += prefs.putString(keyPass, g_wifiSt.runtimeCreds[i].password);
    } else {
      bytes += prefs.putString(keySsid, "");
      bytes += prefs.putString(keyPass, "");
    }
  }
  return bytes;
}

static void ensureRuntimeNetConfig();

static constexpr uint8_t RSS_FEED_MIN_ITEMS = 1;
static const char *kWebStudioLogoUrl = "https://netmi.lk/wp-content/uploads/2024/10/netmilk.svg";

static void normalizeRuntimeUiTheme(RuntimeNetConfig &cfg) {
  const int8_t idx = findUiThemeIndexById(cfg.uiTheme);
  if (idx >= 0) copyStringSafe(cfg.uiTheme, sizeof(cfg.uiTheme), kUiThemes[idx].id);
  else copyStringSafe(cfg.uiTheme, sizeof(cfg.uiTheme), kUiThemes[0].id);
}

static void syncActiveUiThemeFromRuntimeConfig(const RuntimeNetConfig &cfg) {
  setActiveUiThemeById(cfg.uiTheme);
}

static uint8_t normalizeRuntimeViewMask(uint8_t mask) {
  return mask & UI_VIEW_MASK_DEFAULT;
}

static const char *runtimeUiThemeId() {
  ensureRuntimeNetConfig();
  return g_runtimeNetConfig.uiTheme[0] ? g_runtimeNetConfig.uiTheme : kUiThemes[0].id;
}

static const char *runtimeUiThemeLabel() {
  ensureRuntimeNetConfig();
  const int8_t idx = findUiThemeIndexById(g_runtimeNetConfig.uiTheme);
  return (idx >= 0) ? kUiThemes[idx].label : kUiThemes[0].label;
}

static uint8_t clampRssFeedMaxItems(uint8_t n) {
  if (n < RSS_FEED_MIN_ITEMS) return RSS_FEED_MIN_ITEMS;
  if (n > RSS_MAX_ITEMS) return RSS_MAX_ITEMS;
  return n;
}

static void defaultRssFeedName(uint8_t slot, char *out, size_t outLen) {
  if (!out || outLen == 0) return;
  snprintf(out, outLen, "Feed %u", (unsigned)(slot + 1));
  out[outLen - 1] = '\0';
}

static void resetRuntimeRssFeedsToDefaults(RuntimeNetConfig &cfg) {
  for (uint8_t i = 0; i < RSS_FEED_SLOT_COUNT; ++i) {
    char defName[16];
    defaultRssFeedName(i, defName, sizeof(defName));
    copyStringSafe(cfg.rssFeeds[i].name, sizeof(cfg.rssFeeds[i].name), defName);
    cfg.rssFeeds[i].url[0] = '\0';
    cfg.rssFeeds[i].maxItems = RSS_DEFAULT_FEED_ITEMS;
  }
  copyStringSafe(cfg.rssFeeds[0].url, sizeof(cfg.rssFeeds[0].url), RSS_FEED_URL);
}

static void normalizeRuntimeRssFeeds(RuntimeNetConfig &cfg) {
  for (uint8_t i = 0; i < RSS_FEED_SLOT_COUNT; ++i) {
    RuntimeRssFeedConfig &feed = cfg.rssFeeds[i];
    feed.maxItems = clampRssFeedMaxItems(feed.maxItems);
    if (!startsWithHttp(feed.url)) feed.url[0] = '\0';
    if (!feed.name[0]) {
      char defName[16];
      defaultRssFeedName(i, defName, sizeof(defName));
      copyStringSafe(feed.name, sizeof(feed.name), defName);
    }
  }
}

static void buildRssNvsKey(char *out, size_t outLen, uint8_t slot, const char *suffix) {
  if (!out || outLen == 0) return;
  snprintf(out, outLen, "rss%u_%s", (unsigned)slot, suffix ? suffix : "");
  out[outLen - 1] = '\0';
}

static bool runtimeRssFeedEntriesEqual(const RuntimeRssFeedConfig &a, const RuntimeRssFeedConfig &b) {
  return (strncmp(a.name, b.name, sizeof(a.name)) == 0) &&
         (strncmp(a.url, b.url, sizeof(a.url)) == 0) &&
         (a.maxItems == b.maxItems);
}

static int runtimeFirstConfiguredRssFeedIndexNoEnsure() {
  for (uint8_t i = 0; i < RSS_FEED_SLOT_COUNT; ++i) {
    if (startsWithHttp(g_runtimeNetConfig.rssFeeds[i].url)) return (int)i;
  }
  return -1;
}

static uint8_t runtimeRssConfiguredFeedCountNoEnsure() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < RSS_FEED_SLOT_COUNT; ++i) {
    if (startsWithHttp(g_runtimeNetConfig.rssFeeds[i].url)) ++n;
  }
  return n;
}

static int runtimeFirstConfiguredRssFeedIndex() {
  ensureRuntimeNetConfig();
  return runtimeFirstConfiguredRssFeedIndexNoEnsure();
}

static const RuntimeRssFeedConfig *runtimeRssFeedBySlot(uint8_t slot) {
  ensureRuntimeNetConfig();
  if (slot >= RSS_FEED_SLOT_COUNT) return nullptr;
  return &g_runtimeNetConfig.rssFeeds[slot];
}

static uint8_t runtimeRssConfiguredFeedCount() {
  ensureRuntimeNetConfig();
  return runtimeRssConfiguredFeedCountNoEnsure();
}

// ---------- NVS load helpers (M10 extraction) ----------

/// Load RSS feed slots from NVS (legacy single-URL key + multi-slot loop).
static void nvsLoadRssFeeds(Preferences &prefs, bool &loadedAny) {
  // Legacy key compatibility (single feed URL).
  if (prefs.isKey("rss_url")) {
    const String rss = prefs.getString("rss_url", "");
    if (rss.length() > 0 && startsWithHttp(rss.c_str())) {
      copyStringSafe(g_runtimeNetConfig.rssFeeds[0].url, sizeof(g_runtimeNetConfig.rssFeeds[0].url), rss.c_str());
      loadedAny = true;
    }
  }
  for (uint8_t i = 0; i < RSS_FEED_SLOT_COUNT; ++i) {
    char key[16];
    buildRssNvsKey(key, sizeof(key), i, "name");
    if (prefs.isKey(key)) {
      const String name = prefs.getString(key, "");
      if (name.length() > 0) {
        copyStringSafe(g_runtimeNetConfig.rssFeeds[i].name, sizeof(g_runtimeNetConfig.rssFeeds[i].name), name.c_str());
      }
      loadedAny = true;
    }
    buildRssNvsKey(key, sizeof(key), i, "url");
    if (prefs.isKey(key)) {
      const String url = prefs.getString(key, "");
      if (url.length() > 0 && startsWithHttp(url.c_str())) {
        copyStringSafe(g_runtimeNetConfig.rssFeeds[i].url, sizeof(g_runtimeNetConfig.rssFeeds[i].url), url.c_str());
      } else {
        g_runtimeNetConfig.rssFeeds[i].url[0] = '\0';
      }
      loadedAny = true;
    }
    buildRssNvsKey(key, sizeof(key), i, "max");
    if (prefs.isKey(key)) {
      const uint8_t maxItems = prefs.getUChar(key, RSS_DEFAULT_FEED_ITEMS);
      g_runtimeNetConfig.rssFeeds[i].maxItems = clampRssFeedMaxItems(maxItems);
      loadedAny = true;
    }
  }
}

/// Load system language from NVS with legacy migration (genz→bellazio, bellazi→bellazio).
static void nvsLoadLanguageConfig(Preferences &prefs, bool &langNeedsPersist) {
  if (!prefs.isKey("wc_lang")) return;
  String lang = prefs.getString("wc_lang", WORD_CLOCK_LANG_DEFAULT);
  lang.trim();
  lang.toLowerCase();
  String langLower(lang);
  langLower.toLowerCase();
  const bool legacyAlias =
      (langLower.length() == 4) &&
      (langLower.charAt(0) == 'g') &&
      (langLower.charAt(1) == 'e') &&
      (langLower.charAt(2) == 'n') &&
      (langLower.charAt(3) == 'z');
  if (legacyAlias) {
    lang = "bellazio";
    langNeedsPersist = true;
    Serial.println("[CFG][NVS] wc_lang legacy alias -> 'bellazio'");
  }
  if (lang.equalsIgnoreCase("bellazi")) {
    lang = "bellazio";
    langNeedsPersist = true;
    Serial.println("[CFG][NVS] wc_lang legacy 'bellazi' -> 'bellazio'");
  }
  bool valid = isValidLangCode(lang);
  if (valid && lang.length() > 0 && lang.length() < sizeof(g_wordClockLang)) {
    strncpy(g_wordClockLang, lang.c_str(), sizeof(g_wordClockLang) - 1);
    g_wordClockLang[sizeof(g_wordClockLang) - 1] = '\0';
  } else if (lang.length() > 0) {
    Serial.printf("[CFG][NVS] wc_lang invalid '%s', fallback '%s'\n", lang.c_str(), WORD_CLOCK_LANG_DEFAULT);
    copyStringSafe(g_wordClockLang, sizeof(g_wordClockLang), WORD_CLOCK_LANG_DEFAULT);
  }
}

// ---------- Main NVS loader ----------

static void loadRuntimeNetConfigFromNvs() {
  if (g_runtimeNetConfigNvsLoaded) return;
  g_runtimeNetConfigNvsLoaded = true;

  Preferences prefs;
  if (!prefs.begin("scrybar_cfg", true)) {
    Serial.println("[CFG][NVS] begin(ro) failed, uso default");
    return;
  }

  bool loadedAny = false;
  bool langNeedsPersist = false;
  if (prefs.isKey("w_city")) {
    const String city = prefs.getString("w_city", "");
    if (city.length() > 0) {
      copyStringSafe(g_runtimeNetConfig.weatherCity, sizeof(g_runtimeNetConfig.weatherCity), city.c_str());
      loadedAny = true;
    }
  }
  if (prefs.isKey("w_lat")) {
    const float lat = prefs.getFloat("w_lat", WEATHER_LAT);
    if (isfinite(lat) && lat >= -90.0f && lat <= 90.0f) {
      g_runtimeNetConfig.weatherLat = lat;
      loadedAny = true;
    }
  }
  if (prefs.isKey("w_lon")) {
    const float lon = prefs.getFloat("w_lon", WEATHER_LON);
    if (isfinite(lon) && lon >= -180.0f && lon <= 180.0f) {
      g_runtimeNetConfig.weatherLon = lon;
      loadedAny = true;
    }
  }

  nvsLoadRssFeeds(prefs, loadedAny);

  if (prefs.isKey("logo_url")) {
    const String logo = prefs.getString("logo_url", "");
    if (logo.length() > 0 && startsWithHttp(logo.c_str())) {
      copyStringSafe(g_runtimeNetConfig.logoUrl, sizeof(g_runtimeNetConfig.logoUrl), logo.c_str());
      loadedAny = true;
    }
  }
  if (prefs.isKey("wifi_pref")) {
    String wifiPref = prefs.getString("wifi_pref", "");
    wifiPref.trim();
    if (wifiPref.length() < sizeof(g_wifiSt.preferredSsid)) {
      copyStringSafe(g_wifiSt.preferredSsid, sizeof(g_wifiSt.preferredSsid), wifiPref.c_str());
      loadedAny = true;
    }
  }
  if (prefs.isKey("wifi_setup_mode")) {
    String setupMode = prefs.getString("wifi_setup_mode", WIFI_SETUP_MODE_DEFAULT);
    setupMode.trim();
    setupMode.toLowerCase();
    if (setupMode.length() < sizeof(g_wifiSt.setupMode)) {
      copyStringSafe(g_wifiSt.setupMode, sizeof(g_wifiSt.setupMode), setupMode.c_str());
      loadedAny = true;
    }
  }
  loadRuntimeWiFiCredentialsFromPrefs(prefs, loadedAny);
  nvsLoadLanguageConfig(prefs, langNeedsPersist);
  if (prefs.isKey("ui_theme")) {
    const String theme = prefs.getString("ui_theme", "");
    if (theme.length() > 0 && theme.length() < sizeof(g_runtimeNetConfig.uiTheme)) {
      copyStringSafe(g_runtimeNetConfig.uiTheme, sizeof(g_runtimeNetConfig.uiTheme), theme.c_str());
      loadedAny = true;
    }
  }
  if (prefs.isKey("ui_views")) {
    uint8_t stored = prefs.getUChar("ui_views", UI_VIEW_MASK_DEFAULT);
    // Which flags were known when this value was last saved?
    // Flags added after that save should default to ON (their bit in
    // UI_VIEW_MASK_DEFAULT) instead of inheriting an implicit 0.
    const uint8_t knownAtSave = prefs.isKey("ui_views_gen")
        ? prefs.getUChar("ui_views_gen", UI_VIEW_MASK_DEFAULT)
        : (uint8_t)(UI_VIEW_FLAG_INFO | UI_VIEW_FLAG_AUX | UI_VIEW_FLAG_WIKI | UI_VIEW_FLAG_DOOM);
    const uint8_t newFlags = UI_VIEW_MASK_DEFAULT & ~knownAtSave;
    g_runtimeNetConfig.enabledViewsMask = normalizeRuntimeViewMask(stored | newFlags);
    loadedAny = true;
  }
  // Wiki language (independent from system language)
  {
    char wl[8] = "en";
    prefs.getString("wiki_lang", wl, sizeof(wl));
    const char* kWikiLangs[] = {"en","it","fr","de","es","pt","la","eo",nullptr};
    bool wlValid = false;
    for (const char **p = kWikiLangs; *p; ++p) { if (strcmp(wl, *p) == 0) { wlValid = true; break; } }
    if (wlValid) strncpy(g_wikiLang, wl, sizeof(g_wikiLang) - 1);
  }
  // Transit stations + Transitous stop ID
  if (prefs.isKey("transit_stn")) {
    char stn[TRANSIT_STATION_LEN] = {0};
    prefs.getString("transit_stn", stn, sizeof(stn));
    if (stn[0]) {
      copyStringSafe(g_transitConfig.station, sizeof(g_transitConfig.station), stn);
      loadedAny = true;
    }
  }
  if (prefs.isKey("transit_arr")) {
    char arr[TRANSIT_STATION_LEN] = {0};
    prefs.getString("transit_arr", arr, sizeof(arr));
    copyStringSafe(g_transitConfig.arrStation, sizeof(g_transitConfig.arrStation), arr);
  }
  if (prefs.isKey("transit_sid")) {
    char sid[TRANSIT_STOP_ID_LEN] = {0};
    prefs.getString("transit_sid", sid, sizeof(sid));
    copyStringSafe(g_transitConfig.stopId, sizeof(g_transitConfig.stopId), sid);
  }
  // configured = true only when both station name AND stop ID are present
  g_transitConfig.configured = (g_transitConfig.station[0] && g_transitConfig.stopId[0]);

  prefs.end();

  normalizeWifiSetupMode();
  wifiPrepareCredentialCache();
  if (g_wifiSt.preferredSsid[0]) {
    const int8_t preferredIdx = findWiFiCredentialIndexBySsid(g_wifiSt.preferredSsid);
    if (preferredIdx >= 0) g_wifiSt.reconnectIdx = (uint8_t)preferredIdx;
    else g_wifiSt.preferredSsid[0] = '\0';
  }

  if (langNeedsPersist) {
    Preferences prefsRw;
    if (prefsRw.begin("scrybar_cfg", false)) {
      prefsRw.putString("wc_lang", "bellazio");
      prefsRw.end();
      Serial.println("[CFG][NVS] wc_lang migration persisted");
    }
  }

  normalizeRuntimeRssFeeds(g_runtimeNetConfig);
  normalizeRuntimeUiTheme(g_runtimeNetConfig);
  g_runtimeNetConfig.enabledViewsMask = normalizeRuntimeViewMask(g_runtimeNetConfig.enabledViewsMask);

  if (loadedAny) {
    const int activeIdx = runtimeFirstConfiguredRssFeedIndexNoEnsure();
    Serial.printf("[CFG][NVS] loaded city='%s' lat=%.4f lon=%.4f rss_feeds=%u active='%s' theme='%s' views=0x%02X wiki_lang='%s' wifi_pref='%s' wifi_setup='%s' wifi_dyn=%u\n",
                  g_runtimeNetConfig.weatherCity,
                  g_runtimeNetConfig.weatherLat,
                  g_runtimeNetConfig.weatherLon,
                  (unsigned)runtimeRssConfiguredFeedCountNoEnsure(),
                  activeIdx >= 0 ? g_runtimeNetConfig.rssFeeds[activeIdx].url : "-",
                  g_runtimeNetConfig.uiTheme,
                  (unsigned)g_runtimeNetConfig.enabledViewsMask,
                  g_wikiLang,
                  g_wifiSt.preferredSsid[0] ? g_wifiSt.preferredSsid : "auto",
                  g_wifiSt.setupMode,
                  (unsigned)g_wifiSt.runtimeCredCount);
  } else {
    Serial.println("[CFG][NVS] no saved config, uso default");
  }
}

static bool saveRuntimeNetConfigToNvs() {
  Preferences prefs;
  if (!prefs.begin("scrybar_cfg", false)) {
    Serial.println("[CFG][NVS] begin(rw) failed");
    return false;
  }
  const size_t n1 = prefs.putString("w_city", g_runtimeNetConfig.weatherCity);
  const size_t n2 = prefs.putFloat("w_lat", g_runtimeNetConfig.weatherLat);
  const size_t n3 = prefs.putFloat("w_lon", g_runtimeNetConfig.weatherLon);
  const size_t n4 = prefs.putString("rss_url", runtimeRssFeedUrl());
  size_t nFeedUrl = 0;
  size_t nFeedName = 0;
  size_t nFeedMax = 0;
  for (uint8_t i = 0; i < RSS_FEED_SLOT_COUNT; ++i) {
    char key[16];
    buildRssNvsKey(key, sizeof(key), i, "name");
    nFeedName += prefs.putString(key, g_runtimeNetConfig.rssFeeds[i].name);
    buildRssNvsKey(key, sizeof(key), i, "url");
    nFeedUrl += prefs.putString(key, g_runtimeNetConfig.rssFeeds[i].url);
    buildRssNvsKey(key, sizeof(key), i, "max");
    nFeedMax += prefs.putUChar(key, g_runtimeNetConfig.rssFeeds[i].maxItems);
  }
  const size_t n5 = prefs.putString("logo_url", g_runtimeNetConfig.logoUrl);
  const size_t n6 = prefs.putString("wc_lang", g_wordClockLang);
  const size_t n7 = prefs.putString("ui_theme", g_runtimeNetConfig.uiTheme);
  const size_t nViewMask = prefs.putUChar("ui_views", normalizeRuntimeViewMask(g_runtimeNetConfig.enabledViewsMask));
  prefs.putUChar("ui_views_gen", UI_VIEW_MASK_DEFAULT);  // track known flags for future migrations
  const size_t nWikiLang = prefs.putString("wiki_lang", g_wikiLang);
  const size_t n8 = prefs.putString("wifi_pref", g_wifiSt.preferredSsid);
  const size_t n9 = prefs.putString("wifi_setup_mode", g_wifiSt.setupMode);
  const size_t n10 = saveRuntimeWiFiCredentialsToPrefs(prefs);
  prefs.putString("transit_stn", g_transitConfig.station);
  prefs.putString("transit_arr", g_transitConfig.arrStation);
  prefs.putString("transit_sid", g_transitConfig.stopId);
  prefs.end();
  const bool ok = (n1 > 0) && (n2 > 0) && (n3 > 0);
  Serial.printf("[CFG][NVS] save %s (city=%u lat=%u lon=%u rss_legacy=%u feed_name=%u feed_url=%u feed_max=%u logo=%u lang=%u theme=%u views=%u wiki_lang=%u wifi_pref=%u wifi_setup=%u wifi_dyn=%u)\n",
                ok ? "OK" : "ERR",
                (unsigned)n1, (unsigned)n2, (unsigned)n3, (unsigned)n4,
                (unsigned)nFeedName, (unsigned)nFeedUrl, (unsigned)nFeedMax, (unsigned)n5,
                (unsigned)n6, (unsigned)n7, (unsigned)nViewMask, (unsigned)nWikiLang, (unsigned)n8, (unsigned)n9, (unsigned)n10);
  return ok;
}

static void ensureRuntimeNetConfig() {
  if (g_runtimeNetConfig.ready) return;
  copyStringSafe(g_runtimeNetConfig.weatherCity, sizeof(g_runtimeNetConfig.weatherCity), WEATHER_CITY_LABEL);
  g_runtimeNetConfig.weatherLat = WEATHER_LAT;
  g_runtimeNetConfig.weatherLon = WEATHER_LON;
  resetRuntimeRssFeedsToDefaults(g_runtimeNetConfig);
  copyStringSafe(g_runtimeNetConfig.uiTheme, sizeof(g_runtimeNetConfig.uiTheme), kUiThemes[0].id);
  if (WEB_CONFIG_LOGO_URL[0]) {
    copyStringSafe(g_runtimeNetConfig.logoUrl, sizeof(g_runtimeNetConfig.logoUrl), WEB_CONFIG_LOGO_URL);
  } else {
    copyStringSafe(g_runtimeNetConfig.logoUrl, sizeof(g_runtimeNetConfig.logoUrl), kWebStudioLogoUrl);
  }
  loadRuntimeNetConfigFromNvs();
  normalizeRuntimeRssFeeds(g_runtimeNetConfig);
  normalizeRuntimeUiTheme(g_runtimeNetConfig);
  g_runtimeNetConfig.enabledViewsMask = normalizeRuntimeViewMask(g_runtimeNetConfig.enabledViewsMask);
  syncActiveUiThemeFromRuntimeConfig(g_runtimeNetConfig);
  g_runtimeNetConfig.ready = true;
}

static const char *runtimeWeatherCityLabel() {
  ensureRuntimeNetConfig();
  return g_runtimeNetConfig.weatherCity[0] ? g_runtimeNetConfig.weatherCity : WEATHER_CITY_LABEL;
}

static float runtimeWeatherLat() {
  ensureRuntimeNetConfig();
  return g_runtimeNetConfig.weatherLat;
}

static float runtimeWeatherLon() {
  ensureRuntimeNetConfig();
  return g_runtimeNetConfig.weatherLon;
}

static const char *runtimeRssFeedUrl() {
  ensureRuntimeNetConfig();
  const int idx = runtimeFirstConfiguredRssFeedIndex();
  if (idx >= 0) return g_runtimeNetConfig.rssFeeds[idx].url;
  return RSS_FEED_URL;
}

static uint8_t runtimeRssActiveMaxItems() {
  ensureRuntimeNetConfig();
  const int idx = runtimeFirstConfiguredRssFeedIndex();
  if (idx >= 0) return clampRssFeedMaxItems(g_runtimeNetConfig.rssFeeds[idx].maxItems);
  return RSS_DEFAULT_FEED_ITEMS;
}

#if WEB_CONFIG_ENABLED
static const char *runtimeLogoUrl() {
  ensureRuntimeNetConfig();
  return g_runtimeNetConfig.logoUrl[0] ? g_runtimeNetConfig.logoUrl : kWebStudioLogoUrl;
}

static bool isHttpUrl(const String &url) {
  return startsWithHttp(url.c_str());
}

static bool parseStrictFloat(const String &text, float &outValue) {
  String t = text;
  t.trim();
  if (t.length() == 0) return false;
  char buf[32];
  t.toCharArray(buf, sizeof(buf));
  char *endPtr = nullptr;
  const float v = strtof(buf, &endPtr);
  if (endPtr == buf) return false;
  while (endPtr && *endPtr && isspace((unsigned char)*endPtr)) ++endPtr;
  if (endPtr && *endPtr != '\0') return false;
  outValue = v;
  return true;
}

static bool parseStrictUint8(const String &text, uint8_t &outValue) {
  String t = text;
  t.trim();
  if (t.length() == 0) return false;
  char buf[16];
  t.toCharArray(buf, sizeof(buf));
  char *endPtr = nullptr;
  const long v = strtol(buf, &endPtr, 10);
  if (endPtr == buf) return false;
  while (endPtr && *endPtr && isspace((unsigned char)*endPtr)) ++endPtr;
  if (endPtr && *endPtr != '\0') return false;
  if (v < 0 || v > 255) return false;
  outValue = (uint8_t)v;
  return true;
}

static bool parseStrictBool(const String &text, bool &outValue) {
  String t = text;
  t.trim();
  t.toLowerCase();
  if (t == "1" || t == "true" || t == "on" || t == "yes") {
    outValue = true;
    return true;
  }
  if (t == "0" || t == "false" || t == "off" || t == "no") {
    outValue = false;
    return true;
  }
  return false;
}

static void appendHtmlEscaped(String &out, const char *text) {
  if (!text) return;
  for (const char *p = text; *p; ++p) {
    const char c = *p;
    if (c == '&') out += F("&amp;");
    else if (c == '<') out += F("&lt;");
    else if (c == '>') out += F("&gt;");
    else if (c == '"') out += F("&quot;");
    else if (c == '\'') out += F("&#39;");
    else out += c;
  }
}

static void appendJsonEscaped(String &out, const char *text) {
  if (!text) return;
  for (const char *p = text; *p; ++p) {
    const char c = *p;
    if (c == '\\') out += F("\\\\");
    else if (c == '"') out += F("\\\"");
    else if (c == '\n') out += F("\\n");
    else if (c == '\r') out += F("\\r");
    else if (c == '\t') out += F("\\t");
    else out += c;
  }
}

static void appendWebThemeCssVars(String &out, const UiThemeWebTokens &t) {
  out += F(":root{");
  out += F("--font-main:"); out += t.fontMain; out += ';';
  out += F("--font-mono:"); out += t.fontMono; out += ';';
  out += F("--bg-deepest:"); out += t.bgDeepest; out += ';';
  out += F("--bg-deep:"); out += t.bgDeep; out += ';';
  out += F("--bg-surface:"); out += t.bgSurface; out += ';';
  out += F("--line:"); out += t.line; out += ';';
  out += F("--line-soft:"); out += t.lineSoft; out += ';';
  out += F("--txt:"); out += t.txt; out += ';';
  out += F("--txt2:"); out += t.txt2; out += ';';
  out += F("--txt3:"); out += t.txt3; out += ';';
  out += F("--acc1:"); out += t.acc1; out += ';';
  out += F("--acc2:"); out += t.acc2; out += ';';
  out += F("--okbg:"); out += t.okbg; out += ';';
  out += F("--sec-bg:"); out += t.secBg; out += ';';
  out += F("--border:"); out += t.border; out += ';';
  out += F("--hero-border:"); out += t.heroBorder; out += ';';
  out += F("--hero-copy-border:"); out += t.heroCopyBorder; out += ';';
  out += F("--hero-copy-bg:"); out += t.heroCopyBg; out += ';';
  out += F("--release-border:"); out += t.releaseBorder; out += ';';
  out += F("--release-bg:"); out += t.releaseBg; out += ';';
  out += F("--release-key:"); out += t.releaseKey; out += ';';
  out += F("--release-value:"); out += t.releaseValue; out += ';';
  out += F("--grid-line-a:"); out += t.gridLineA; out += ';';
  out += F("--grid-line-b:"); out += t.gridLineB; out += ';';
  out += F("--grid-glow-a:"); out += t.gridGlowA; out += ';';
  out += F("--grid-glow-b:"); out += t.gridGlowB; out += ';';
  out += F("--grid-horizon-a:"); out += t.gridHorizonA; out += ';';
  out += F("--grid-horizon-b:"); out += t.gridHorizonB; out += ';';
  out += F("--scanline:"); out += t.scanline; out += ';';
  out += F("--vline-a:"); out += t.vlineA; out += ';';
  out += F("--vline-b:"); out += t.vlineB; out += ';';
  out += F("--btn-ghost-bg:"); out += t.btnGhostBg; out += ';';
  out += F("--btn-ghost-text:"); out += t.btnGhostText; out += ';';
  out += F("--radius:10px;--gap:1.5rem;");
  out += '}';
}

#if WEB_CONFIG_ENABLED && DB_HAS_QRCODEGEN
static bool ensureWebQrBuffers() {
  if (g_webCfg.qrTempBuf && g_webCfg.qrDataBuf) return true;
  if (!g_webCfg.qrTempBuf) {
    g_webCfg.qrTempBuf = (uint8_t *)ps_malloc(qrcodegen_BUFFER_LEN_MAX);
    if (!g_webCfg.qrTempBuf) g_webCfg.qrTempBuf = (uint8_t *)malloc(qrcodegen_BUFFER_LEN_MAX);
  }
  if (!g_webCfg.qrDataBuf) {
    g_webCfg.qrDataBuf = (uint8_t *)ps_malloc(qrcodegen_BUFFER_LEN_MAX);
    if (!g_webCfg.qrDataBuf) g_webCfg.qrDataBuf = (uint8_t *)malloc(qrcodegen_BUFFER_LEN_MAX);
  }
  if (!g_webCfg.qrTempBuf || !g_webCfg.qrDataBuf) {
    Serial.printf("[WEB][QR][ERR] alloc failed temp=%d data=%d heap=%u psram=%u\n",
                  g_webCfg.qrTempBuf ? 1 : 0,
                  g_webCfg.qrDataBuf ? 1 : 0,
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getFreePsram());
    return false;
  }
  return true;
}
#endif

// ── M3 PROGMEM: static CSS (vibemilk DS subset + component classes) ──
static const char kWebCssCore[] PROGMEM = R"rawliteral(
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
:root{--font-family:'Montserrat',-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;--font-mono:ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,'Liberation Mono',monospace;--text-primary:#fff;--text-secondary:#A3AED0;--text-tertiary:#707EAE;--accent-primary:#7551FF;--accent-secondary:#39B8FF;--bg-deepest:#070D2D;--bg-surface:#111C44;--bg-input:#0B1437;--bg-elevated:#111C44;--stroke:rgba(255,255,255,.11);--stroke-soft:rgba(255,255,255,.07);--okbg:rgba(1,181,116,.14);--shadow-sm:0 2px 8px rgba(0,0,0,.25);--shadow-md:0 4px 16px rgba(0,0,0,.3);--r-sm:8px;--r-md:12px;--r-lg:14px;--focus-ring:0 0 0 3px rgba(57,184,255,.18)}
body{font-family:var(--font-family);font-size:14px;font-weight:400;line-height:1.5;color:var(--text-secondary);background:var(--bg-deepest);-webkit-font-smoothing:antialiased}
a{color:var(--accent-primary);text-decoration:none}::selection{background:rgba(57,184,255,.24);color:var(--text-primary)}
.vm-wrap{max-width:780px;margin:0 auto;padding:20px 16px 32px}
.vm-card{background:var(--bg-surface);border:1px solid var(--stroke-soft);border-radius:var(--r-lg);padding:16px 18px;margin-bottom:12px}.vm-card--muted{background:var(--bg-deepest);border-color:var(--stroke-soft)}
.vm-card__hd{display:flex;align-items:center;gap:8px;margin-bottom:12px;font-size:13px;font-weight:700;letter-spacing:.06em;text-transform:uppercase;color:var(--text-primary)}
.vm-card__hd .vm-badge{margin-left:auto;text-transform:none;letter-spacing:0}
.vm-btn{display:inline-flex;align-items:center;justify-content:center;gap:6px;font-family:var(--font-family);font-weight:600;border:0;cursor:pointer;transition:all .15s ease;white-space:nowrap;font-size:13px;height:40px;padding:0 18px;border-radius:var(--r-sm)}
.vm-btn--sm{height:34px;padding:0 12px;font-size:12px;border-radius:6px}
.vm-btn--primary{background:var(--accent-primary);color:#fff}.vm-btn--primary:hover{filter:brightness(1.15);box-shadow:var(--shadow-sm)}
.vm-btn--secondary{background:var(--bg-elevated);color:var(--text-secondary);border:1px solid var(--stroke)}.vm-btn--secondary:hover{color:var(--text-primary);border-color:var(--accent-secondary)}
.vm-btn--danger{background:rgba(238,93,80,.12);color:#f26a5e;border:1px solid rgba(238,93,80,.3)}.vm-btn--danger:hover{background:rgba(238,93,80,.22)}
.vm-btn--warn{background:rgba(117,81,255,.12);color:#b8a8ff;border:1px solid rgba(117,81,255,.35)}.vm-btn--warn:hover{background:rgba(117,81,255,.22)}
.vm-btn:disabled{opacity:.4;cursor:not-allowed;pointer-events:none}
.vm-input,.vm-select{width:100%;height:44px;padding:0 16px;font-family:var(--font-family);font-size:16px;font-weight:500;color:var(--text-primary);background:var(--bg-input);border:1px solid var(--stroke);border-radius:var(--r-sm);outline:none;transition:border-color .15s ease;margin:0 0 4px;touch-action:manipulation}
.vm-input:focus,.vm-select:focus{border-color:var(--accent-secondary);box-shadow:var(--focus-ring)}
.vm-input::placeholder{color:var(--text-tertiary)}
.vm-select{cursor:pointer;appearance:none;padding-right:40px;background-image:linear-gradient(45deg,transparent 50%,var(--text-tertiary) 50%),linear-gradient(135deg,var(--text-tertiary) 50%,transparent 50%);background-repeat:no-repeat;background-size:6px 6px,6px 6px;background-position:calc(100% - 18px) 52%,calc(100% - 13px) 52%}
.vm-label{display:block;font-size:11px;font-weight:600;letter-spacing:.06em;text-transform:uppercase;color:var(--text-tertiary);margin:0 0 6px}
.vm-help{font-size:12px;color:var(--text-tertiary);line-height:1.45;margin:6px 0 0}
.vm-badge{display:inline-flex;align-items:center;gap:4px;height:22px;padding:0 10px;border-radius:999px;font-size:11px;font-weight:600}
.vm-badge--brand{background:rgba(117,81,255,.14);color:var(--accent-primary)}.vm-badge--info{background:rgba(57,184,255,.14);color:var(--accent-secondary)}
.pill{display:inline-block;padding:4px 10px;border-radius:999px;background:rgba(57,184,255,.12);color:var(--accent-secondary);font-size:11px;font-weight:700}
.vm-alert{padding:12px 16px;border-radius:var(--r-md);border-left:4px solid #01B574;background:var(--okbg);color:#c9fce9;font-weight:600;font-size:13px}
.vm-toast-fixed{position:fixed;top:12px;left:50%;transform:translateX(-50%);width:min(94vw,680px);z-index:9999;box-shadow:var(--shadow-md);background:rgba(4,52,34,.95);border-left-color:#01B574}
.msg{margin:0 0 12px;padding:10px 12px;border-radius:var(--r-md);border:1px solid rgba(1,181,116,.45);background:var(--okbg);color:#c9fce9;font-weight:600}
.panel{background:transparent;border:0;padding:0}
.hero{padding:0 0 14px;margin-bottom:14px;border-bottom:1px solid var(--stroke-soft)}
.hero-top{display:flex;align-items:flex-start;justify-content:space-between;gap:14px;flex-wrap:wrap}.hero-left{min-width:290px;flex:1 1 560px}
.logo{height:56px;display:block;object-fit:contain}.hero-right{display:grid;gap:8px;justify-items:end}
.release-box{display:inline-flex;gap:14px;padding:0;border:0;background:0;font:600 11px var(--font-mono)}
.release-box .k{color:var(--accent-secondary);text-transform:uppercase;letter-spacing:.08em}.release-box .v{color:var(--text-primary);letter-spacing:.01em}
.lede{margin:10px 0 0;color:var(--text-secondary);font-size:13px;line-height:1.46}
.vm-grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.vm-views{display:grid;gap:0}
.vm-view{display:flex;gap:10px;align-items:flex-start;padding:10px 0;border:0;border-bottom:1px solid var(--stroke-soft);border-radius:0;background:0}
.vm-view input[type=checkbox]{width:18px;height:18px;margin:2px 0 0;accent-color:var(--accent-secondary);flex:0 0 auto}
.vm-view__copy{display:grid;gap:3px}.vm-view__copy strong{font-size:13px;color:var(--text-primary)}.vm-view__copy small{color:var(--text-tertiary);line-height:1.35;font-size:12px}
.vm-view--fixed{border-bottom-style:dashed}.vm-view--off{opacity:.55}.vm-view:last-child{border-bottom:0}
.vm-secret{display:flex;gap:8px;align-items:stretch;margin:0 0 4px}.vm-secret .vm-input{margin:0}
.vm-setup-grid{display:grid;grid-template-columns:auto 1fr;gap:12px;align-items:center;margin-top:10px}
.vm-setup-qr{width:138px;height:138px;border:1px solid var(--stroke);background:#fff;padding:6px;border-radius:var(--r-sm);display:block}
.vm-setup-url{font:600 13px var(--font-mono);word-break:break-all;color:var(--text-primary)}
.vm-rss-composer{display:grid;grid-template-columns:1fr 1.9fr .55fr auto auto;gap:10px;align-items:end;margin-top:4px}
.vm-rss-list{display:grid;gap:8px;margin-top:10px}
.rss-row{display:grid;grid-template-columns:1fr auto;gap:8px;align-items:center;border:0;border-left:3px solid var(--accent-primary);border-radius:0;padding:10px 0 10px 12px;background:0;border-bottom:1px solid var(--stroke-soft)}.rss-row:last-child{border-bottom:0}
.rss-title{display:flex;align-items:center;gap:6px;font-size:14px;font-weight:700;color:var(--text-primary);margin:0 0 2px}
.rss-meta{font-size:12px;color:var(--text-tertiary);margin:0;word-break:break-all}
.rss-chip{display:inline-block;margin-left:7px;padding:2px 8px;border-radius:999px;background:rgba(57,184,255,.14);color:var(--accent-secondary);font-size:11px;font-weight:600}
.rss-actions{display:flex;gap:6px;flex-wrap:wrap;justify-content:flex-end}
.rss-status{margin:6px 0 2px;color:var(--text-tertiary);font-size:12px;min-height:16px}
.rss-empty{padding:12px;border:1px dashed var(--stroke);border-radius:var(--r-md);color:var(--text-tertiary);font-size:12px;background:rgba(255,255,255,.02)}
.hidden{display:none}
.vm-kv{font-size:13px;line-height:1.7}.vm-kv small{color:var(--text-tertiary)}.vm-kv code{color:var(--text-primary);font-family:var(--font-mono);font-size:12px}
.vm-footer{margin-top:20px;padding:14px 0 4px;border-top:1px solid var(--stroke-soft);font-size:12px;color:var(--text-tertiary);line-height:1.5}.vm-footer strong{color:var(--text-secondary)}.vm-footer a{color:var(--accent-secondary)}
.vm-actions{display:flex;gap:10px;flex-wrap:wrap;margin-top:16px;margin-bottom:8px}
.vm-actions-sticky{position:sticky;bottom:0;z-index:100;background:var(--bg-deepest);padding:10px 16px 8px;border-top:1px solid var(--stroke-soft);margin:0 -16px}
.vm-clp>*:not(h2){display:none!important}
.vm-card h2{display:flex;align-items:center;gap:6px;font-size:15px;font-weight:600;margin:0}
.vm-collapse-arr{margin-left:auto;font-size:18px;opacity:1;flex-shrink:0;color:var(--accent-secondary);line-height:1;transition:transform .15s}
.vm-api-note{margin-top:12px;padding:6px 0;border-radius:0;background:0;border:0;font-size:12px;color:var(--text-tertiary)}.vm-api-note code{color:var(--text-secondary)}
#wifi_new_password{font-family:var(--font-mono),ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,monospace;letter-spacing:.02em}
.geo-status{margin:2px 0 8px;color:var(--text-tertiary);font-size:12px;min-height:16px}
@media(max-width:768px){.vm-wrap{padding:12px 10px 24px}.vm-card{padding:14px 12px}.vm-grid{grid-template-columns:1fr;gap:8px}.vm-rss-composer{grid-template-columns:1fr;gap:8px}.hero-top{flex-wrap:wrap}.hero-right{width:100%;justify-items:start}.vm-actions,.vm-actions-sticky{flex-direction:column}.vm-actions .vm-btn,.vm-actions-sticky .vm-btn{width:100%;justify-content:center}.logo{height:40px}}
small{color:var(--text-tertiary)}code{color:var(--text-secondary)}
)rawliteral";

// ── M3 PROGMEM: static JS (before initialFeeds injection) ──
static const char kWebJsCorePre[] PROGMEM = R"rawliteral(
<script>(function(){
(function(){const t=document.querySelector('.vm-toast-fixed');if(t){setTimeout(function(){t.style.transition='opacity .6s';t.style.opacity='0';setTimeout(function(){t.style.display='none';},650);},4200);}})();
const q=document.getElementById('geo_query');const dl=document.getElementById('geo_hits');const st=document.getElementById('geo_status');const city=document.getElementById('weather_city');const lat=document.getElementById('weather_lat');const lon=document.getElementById('weather_lon');
if(!q||!dl||!city||!lat||!lon)return;let t=0;let map={};function setStatus(msg){if(st)st.textContent=msg||'';}function clearHits(){dl.innerHTML='';map={};}
function applyPick(key){const r=map[key];if(!r)return false;city.value=r.name||city.value;lat.value=Number(r.latitude).toFixed(4);lon.value=Number(r.longitude).toFixed(4);setStatus('Coordinates filled in automatically.');return true;}
q.addEventListener('change',function(){applyPick(q.value);});q.addEventListener('blur',function(){applyPick(q.value);});
q.addEventListener('input',function(){const term=q.value.trim();if(term.length<2){clearHits();setStatus('');return;}clearTimeout(t);t=setTimeout(async function(){try{setStatus('Searching...');const u='https://geocoding-api.open-meteo.com/v1/search?count=6&language=en&format=json&name='+encodeURIComponent(term);const r=await fetch(u,{cache:'no-store'});if(!r.ok)throw new Error('http '+r.status);const data=await r.json();const rows=(data&&data.results)?data.results:[];clearHits();if(!rows.length){setStatus('No results found.');return;}rows.forEach(function(it){const label=[it.name,it.admin1,it.country].filter(Boolean).join(', ');const opt=document.createElement('option');opt.value=label;opt.label=(Number(it.latitude).toFixed(4)+', '+Number(it.longitude).toFixed(4));dl.appendChild(opt);map[label]=it;});setStatus('Select a result to fill city / lat / lon.');if(rows.length===1){const one=[rows[0].name,rows[0].admin1,rows[0].country].filter(Boolean).join(', ');q.value=one;applyPick(one);}}catch(e){clearHits();setStatus('Search unavailable, try again later.');}} ,280);});
const wifiScanBtn=document.getElementById('wifi_scan_btn');const wifiScanResults=document.getElementById('wifi_scan_results');const wifiScanStatus=document.getElementById('wifi_scan_status');const wifiNewSsid=document.getElementById('wifi_new_ssid');const wifiNewPassword=document.getElementById('wifi_new_password');const wifiPwdToggle=document.getElementById('wifi_pwd_toggle');
function setWifiStatus(msg){if(wifiScanStatus)wifiScanStatus.textContent=msg||'';}function escHtml(s){return (s||'').replace(/[&<>]/g,function(c){return({'&':'&amp;','<':'&lt;','>':'&gt;'})[c];});}
async function scanWifiNow(){if(!wifiScanResults)return;const ctl=(window.AbortController?new AbortController():null);let tm=0;try{setWifiStatus('Scanning 2.4 GHz networks...');wifiScanResults.innerHTML="<option value=''>Scanning...</option>";if(ctl){tm=window.setTimeout(function(){ctl.abort();},9000);}const res=await fetch('/api/wifi/scan',{cache:'no-store',signal:ctl?ctl.signal:undefined});if(tm)window.clearTimeout(tm);if(!res.ok)throw new Error('http '+res.status);const data=await res.json();const rows=(data&&data.networks)?data.networks:[];wifiScanResults.innerHTML="";if(!rows.length){wifiScanResults.innerHTML="<option value=''>No 2.4 GHz network found</option>";setWifiStatus((data&&data.message==='scan_timeout')?'Scan timed out, retry in a few seconds.':'No 2.4 GHz networks found.');return;}rows.forEach(function(n){const opt=document.createElement('option');const lock=n.secure?'SEC':'OPEN';opt.value=n.ssid||'';opt.dataset.secure=n.secure?'1':'0';opt.dataset.channel=String(n.channel||0);opt.innerHTML=escHtml((n.ssid||'(hidden)')+'  '+lock+'  ch'+(n.channel||'?')+'  '+(n.rssi||0)+'dBm');wifiScanResults.appendChild(opt);});setWifiStatus('Scan complete. Pick an SSID and press Save Config.');if(rows[0]&&rows[0].ssid&&wifiNewSsid&&!wifiNewSsid.value){wifiNewSsid.value=rows[0].ssid;}}catch(e){if(tm)window.clearTimeout(tm);wifiScanResults.innerHTML="<option value=''>Scan failed</option>";setWifiStatus((e&&e.name==='AbortError')?'Scan timeout. Retry.':'Scan unavailable right now.');}}
function syncWifiPwdToggle(){if(!wifiPwdToggle||!wifiNewPassword)return;const visible=wifiNewPassword.type==='text';wifiPwdToggle.textContent=visible?'Hide':'Show';wifiPwdToggle.title=visible?'Hide password':'Show password';wifiPwdToggle.setAttribute('aria-label',wifiPwdToggle.title);}if(wifiPwdToggle&&wifiNewPassword){wifiPwdToggle.addEventListener('click',function(){wifiNewPassword.type=(wifiNewPassword.type==='password')?'text':'password';syncWifiPwdToggle();});syncWifiPwdToggle();}if(wifiScanBtn)wifiScanBtn.addEventListener('click',function(){scanWifiNow();});if(wifiScanResults)wifiScanResults.addEventListener('change',function(){const v=wifiScanResults.value||'';if(wifiNewSsid&&v)wifiNewSsid.value=v;const sel=wifiScanResults.options[wifiScanResults.selectedIndex];if(wifiNewPassword&&sel&&sel.dataset.secure==='0'){wifiNewPassword.value='';wifiNewPassword.placeholder='Open network (no password)';}else if(wifiNewPassword){wifiNewPassword.placeholder='Password (WPA/WPA2)';}});
const form=document.getElementById('cfg_form');const rssName=document.getElementById('rss_name');const rssUrl=document.getElementById('rss_url');const rssMax=document.getElementById('rss_max');const rssAdd=document.getElementById('rss_add');const rssReset=document.getElementById('rss_reset');const rssList=document.getElementById('rss_list');const rssEmpty=document.getElementById('rss_empty');const rssStatus=document.getElementById('rss_status');const rssHidden=document.getElementById('rss_hidden_inputs');const rssPill=document.getElementById('rss_count_pill');const viewHidden=document.getElementById('view_hidden_inputs');
const maxSlots=5;const minPosts=1;const maxPosts=8;let editIndex=-1;
const initialFeeds=[)rawliteral";

// ── M3 PROGMEM: static JS (after initialFeeds injection) ──
static const char kWebJsCorePost[] PROGMEM = R"rawliteral(];
let feeds=initialFeeds.filter(f=>f&&f.url&&/^https?:\/\//i.test(f.url));
function clampPosts(n){n=parseInt(n,10);if(isNaN(n))n=maxPosts;if(n<minPosts)n=minPosts;if(n>maxPosts)n=maxPosts;return n;}function startsHttp(v){return /^https?:\/\//i.test((v||'').trim());}
function defName(i){return 'Feed '+(i+1);}function setRssStatus(m){if(rssStatus)rssStatus.textContent=m||'';}
function clearComposer(){editIndex=-1;rssName.value='';rssUrl.value='';rssMax.value='8';rssAdd.innerHTML="+ Add";setRssStatus('');}
function renderFeeds(){if(!rssList)return;rssList.innerHTML='';if(rssPill)rssPill.innerHTML="RSS feeds "+feeds.length+'/5';if(rssEmpty)rssEmpty.style.display=feeds.length?'none':'block';feeds.forEach(function(f,idx){const row=document.createElement('div');row.className='rss-row';const left=document.createElement('div');const t=document.createElement('p');t.className='rss-title';t.textContent='';t.appendChild(document.createTextNode(f.name||defName(idx)));const chip=document.createElement('span');chip.className='rss-chip';chip.textContent='max '+clampPosts(f.max);t.appendChild(chip);const m=document.createElement('p');m.className='rss-meta';m.textContent=f.url||'';left.appendChild(t);left.appendChild(m);const act=document.createElement('div');act.className='rss-actions';const bEdit=document.createElement('button');bEdit.type='button';bEdit.className='vm-btn vm-btn--sm vm-btn--warn';bEdit.textContent='Edit';bEdit.addEventListener('click',function(){editIndex=idx;rssName.value=f.name||'';rssUrl.value=f.url||'';rssMax.value=String(clampPosts(f.max));rssAdd.textContent='Update';setRssStatus('Editing feed '+(idx+1));});const bDel=document.createElement('button');bDel.type='button';bDel.className='vm-btn vm-btn--sm vm-btn--danger';bDel.textContent='Delete';bDel.addEventListener('click',function(){feeds.splice(idx,1);if(editIndex===idx)clearComposer();else if(editIndex>idx)editIndex-=1;renderFeeds();setRssStatus('Feed removed.');});act.appendChild(bEdit);act.appendChild(bDel);row.appendChild(left);row.appendChild(act);rssList.appendChild(row);});}
function pushOrUpdate(){const name=(rssName.value||'').trim();const url=(rssUrl.value||'').trim();const max=clampPosts(rssMax.value);if(!url){setRssStatus('Please enter a feed URL.');return;}if(!startsHttp(url)){setRssStatus('URL must start with http:// or https://');return;}const item={name:name||defName(editIndex>=0?editIndex:feeds.length),url:url,max:max};if(editIndex>=0){feeds[editIndex]=item;clearComposer();setRssStatus('Feed updated.');renderFeeds();return;}if(feeds.length>=maxSlots){setRssStatus('Maximum limit: 5 feeds.');return;}feeds.push(item);clearComposer();renderFeeds();setRssStatus('Feed added.');}
function addHidden(k,v){const i=document.createElement('input');i.type='hidden';i.name=k;i.value=v;rssHidden.appendChild(i);}function buildHiddenInputs(){if(!rssHidden)return;rssHidden.innerHTML='';for(let i=0;i<maxSlots;i+=1){const f=feeds[i]||{name:defName(i),url:'',max:maxPosts};addHidden('rss_feed_name_'+(i+1),f.name||defName(i));addHidden('rss_feed_url_'+(i+1),f.url||'');addHidden('rss_feed_items_'+(i+1),String(clampPosts(f.max)));}const f0=feeds[0]||{name:defName(0),url:'',max:maxPosts};addHidden('rss_feed_name',f0.name||defName(0));addHidden('rss_feed_url',f0.url||'');addHidden('rss_feed_items',String(clampPosts(f0.max)));}
function addViewHidden(k,v){if(!viewHidden)return;const i=document.createElement('input');i.type='hidden';i.name=k;i.value=v;viewHidden.appendChild(i);}function buildViewHiddenInputs(){if(!viewHidden)return;viewHidden.innerHTML='';[['view_info','view_info_cb'],['view_aux','view_aux_cb'],['view_wiki','view_wiki_cb'],['view_now_playing','view_now_playing_cb'],['view_transit','view_transit_cb']].forEach(function(pair){const el=document.getElementById(pair[1]);addViewHidden(pair[0],(el&&el.checked)?'1':'0');});}
if(rssAdd)rssAdd.addEventListener('click',function(){pushOrUpdate();});if(rssReset)rssReset.addEventListener('click',function(){clearComposer();setRssStatus('Composer cleared.');});if(form)form.addEventListener('submit',function(){buildHiddenInputs();buildViewHiddenInputs();});renderFeeds();
// Collapsible sections — start collapsed; ▶ = closed, ▼ = open
document.querySelectorAll('.vm-card').forEach(function(card){
  var h=card.querySelector('h2');if(!h)return;
  var arr=document.createElement('span');arr.className='vm-collapse-arr';
  card.classList.add('vm-clp');arr.textContent='\u25B6';
  h.appendChild(arr);
  h.style.cursor='pointer';h.style.userSelect='none';
  h.addEventListener('click',function(e){if(e.target.tagName==='INPUT'||e.target.tagName==='SELECT')return;var c=card.classList.toggle('vm-clp');arr.textContent=c?'\u25B6':'\u25BC';});
});
})();</script>)rawliteral";

// ── M3: sub-functions for buildWebConfigPage decomposition ──

static void buildWebCssBlock(String &html) {
  html += FPSTR(kWebCssCore);
}

static void buildWebHeroSection(String &html, const char *statusMsg) {
  html += F("<section class='hero'><div class='hero-top'><div class='hero-left'><img class='logo' alt='Netmilk Studio' src='");
  appendHtmlEscaped(html, runtimeLogoUrl());
  html += F("'></div><div class='hero-right'><div class='release-box'><span><span class='k'>release</span> <span class='v'>");
  appendHtmlEscaped(html, FW_RELEASE_DATE);
  html += F("</span></span><span><span class='k'>version</span> <span class='v'>");
  appendHtmlEscaped(html, FW_BUILD_TAG);
  html += F("</span></span></div></div></div><p class='lede'>\xE2\x9C\xA8 <b>ScryBar</b> is a mass of sensors, pixels, and unresolved ambition, pretending to be furniture.<br>Time, weather, news, and a talking oracle. Everything you could faster check on your phone, but won't.<br>Overengineered with pride by <b>enuzzo</b>, stealing billable hours at <b>Netmilk Studio</b>. Reflashed at 2 AM with no regrets.<br><em>Your desk knows things now.</em></p></section>");
  html += F("<section class='panel'>");
  if (statusMsg && statusMsg[0]) {
    html += F("<p class='vm-alert vm-toast-fixed'>");
    appendHtmlEscaped(html, statusMsg);
    html += F("</p>");
  }
}

static void buildWebThemeSelector(String &html) {
  html += F("<div class='vm-card'><h2>&#x1F3A8;&ensp;Visual Theme</h2><div class='vm-label'>THEME</div><select class='vm-select' name='ui_theme'>");
  for (size_t i = 0; i < UI_THEME_COUNT; ++i) {
    html += F("<option value='");
    html += kUiThemes[i].id;
    html += '\'';
    if (strcmp(runtimeUiThemeId(), kUiThemes[i].id) == 0) html += F(" selected");
    html += '>';
    html += kUiThemes[i].label;
    html += F("</option>");
  }
  html += F("</select><p class='vm-help'>One selector drives both interfaces: this web control surface and the ESP32 display UI. Switching theme applies instantly and persists in NVS.</p></div>");
}

static void buildWebViewToggles(String &html) {
  const bool infoViewOn = (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_INFO) != 0;
  const bool auxViewOn = (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_AUX) != 0;
  const bool wikiViewOn = (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_WIKI) != 0;
  const bool nowPlayingViewOn = (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_NOW_PLAYING) != 0;
  const bool transitViewOn = (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_TRANSIT) != 0;
  html += F("<div class='vm-card'><h2>&#x1F4F1;&ensp;Views</h2><div class='vm-views'>");
  html += F("<label class='vm-view'><input id='view_info_cb' type='checkbox'");
  if (infoViewOn) html += F(" checked");
  html += F("><span class='vm-view__copy'><strong>Info</strong><small>Word clock and ambient status page.</small></span></label>");
  html += F("<label class='vm-view vm-view--fixed'><input type='checkbox' checked disabled><span class='vm-view__copy'><strong>Home</strong><small>Always on. Safe fallback when other pages are disabled.</small></span></label>");
  html += F("<label class='vm-view'><input id='view_aux_cb' type='checkbox'");
  if (auxViewOn) html += F(" checked");
  html += F("><span class='vm-view__copy'><strong>RSS / AUX</strong><small>News feed deck, QR and refresh actions.</small></span></label>");
  html += F("<label class='vm-view'><input id='view_wiki_cb' type='checkbox'");
  if (wikiViewOn) html += F(" checked");
  html += F("><span class='vm-view__copy'><strong>Wikipedia</strong><small>Featured, On This Day and random article cards.</small></span></label>");
  html += F("<label class='vm-view'><input id='view_now_playing_cb' type='checkbox'");
  if (nowPlayingViewOn) html += F(" checked");
  html += F("><span class='vm-view__copy'><strong>Now Playing</strong><small>Live track info from macOS companion app.</small></span></label>");
  html += F("<label class='vm-view'><input id='view_transit_cb' type='checkbox'");
  if (transitViewOn) html += F(" checked");
  html += F("><span class='vm-view__copy'><strong>Departures</strong><small>Live transit departure board via Transitous API.</small></span></label>");
  html += F("</div><p class='vm-help'>Swipe navigation only includes enabled pages.</p><div id='view_hidden_inputs' class='hidden'></div></div>");
}

#if TEST_WIFI
static void buildWebWifiSection(String &html) {
  const bool wifiOk = (WiFi.status() == WL_CONNECTED) && g_wifiSt.connected;
  const String activeSsid = wifiOk ? WiFi.SSID() : String("");
  char setupUrl[96] = "";
  wifiBuildSetupPortalUrl(setupUrl, sizeof(setupUrl));
  html += F("<div class='vm-card'><h2>&#x1F4F6;&ensp;Wi-Fi Known Networks</h2><div class='vm-label'>PREFERRED SSID</div><select class='vm-select' name='wifi_pref_ssid'>");
  html += F("<option value=''");
  if (!g_wifiSt.preferredSsid[0]) html += F(" selected");
  html += F(">Auto (smart rotation)</option>");
  for (uint8_t i = 0; i < g_wifiSt.credCount; ++i) {
    const char *ssid = g_wifiSt.credSsids[i];
    if (!ssid || !ssid[0]) continue;
    html += F("<option value='");
    appendHtmlEscaped(html, ssid);
    html += '\'';
    const bool selected = (strcmp(g_wifiSt.preferredSsid, ssid) == 0);
    if (selected) html += F(" selected");
    html += '>';
    appendHtmlEscaped(html, ssid);
    if (wifiOk && activeSsid.equals(ssid)) html += F(" (connected)");
    html += F("</option>");
  }
  html += F("</select><div class='vm-label'>WIFI DIRECT MODE</div><select class='vm-select' name='wifi_setup_mode'>");
  html += F("<option value='off'");
  if (wifiSetupModeIsOff()) html += F(" selected");
  html += F(">Off</option>");
  html += F("<option value='auto'");
  if (wifiSetupModeIsAuto()) html += F(" selected");
  html += F(">Auto fallback</option>");
  html += F("<option value='on'");
  if (wifiSetupModeIsOn()) html += F(" selected");
  html += F(">Always on</option>");
  html += F("</select>");
  html += F("<p class='vm-help'>Auto mode cycles known SSIDs first, then starts setup AP if disconnected too long. ScryBar supports <b>2.4 GHz only</b> (5 GHz is ignored).</p>");
  html += F("<div class='vm-label' style='margin-top:14px'>PROVISION NEW NETWORK (2.4 GHZ)</div><div class='vm-grid'><div><button id='wifi_scan_btn' class='vm-btn vm-btn--sm vm-btn--secondary' type='button'>Scan networks</button><p id='wifi_scan_status' class='rss-status'></p><div class='vm-label'>SCAN RESULTS</div><select class='vm-select' id='wifi_scan_results'><option value=''>Press scan first...</option></select></div><div><div class='vm-label'>SSID</div><input class='vm-input' id='wifi_new_ssid' name='wifi_new_ssid' maxlength='32' placeholder='MyPhone Hotspot'><div class='vm-label'>PASSWORD</div><div class='vm-secret'><input class='vm-input' id='wifi_new_password' name='wifi_new_password' maxlength='64' type='password' placeholder='Leave empty if open network'><button id='wifi_pwd_toggle' class='vm-btn vm-btn--sm vm-btn--secondary' type='button' aria-label='Show password' title='Show password'>Show</button></div></div></div>");
  if (g_wifiSt.setupApActive) {
    html += F("<p class='vm-help'>Setup AP active: <code>");
    appendHtmlEscaped(html, g_wifiSt.setupApSsid);
    html += F("</code> @ <code>");
    html += WiFi.softAPIP().toString();
    html += F("</code></p>");
    html += F("<div class='vm-setup-grid'><img class='vm-setup-qr' src='/api/wifi/setup-qr.svg' alt='Setup QR'><div><div class='vm-label'>SETUP URL</div><div class='vm-setup-url'>");
    appendHtmlEscaped(html, setupUrl);
    html += F("</div><p class='vm-help'>Scan this QR or open the URL manually to access setup instantly. Captive portal probes are redirected to this page.</p></div></div>");
  }
  html += F("<p class='vm-help'>Save Config to store SSID/password in NVS (persistent across reboot/reflash; cleared only by NVS erase).</p></div>");
}
#endif

static void buildWebLangSelectors(String &html) {
  // System Language section
  {
    struct { const char *code; const char *label; } kLangsFun[] = {
      {"bellazio", "Bellazio"},
      {"val",  "Valley Girl"},
      {"l33t", "1337 5P34K"},
      {"sha",  "Shakespearean English"},
      {"eo",   "Esperanto"},
      {"la",   "Latina"},
      {"tlh",  "tlhIngan Hol (Klingon)"},
      {"pir",  "Pirate"},
    };
    struct { const char *code; const char *label; } kLangsStd[] = {
      {"en",  "English"},
      {"it",  "Italiano"},
      {"es",  "Espa\xC3\xB1" "ol"},
      {"fr",  "Fran\xC3\xA7" "ais"},
      {"de",  "Deutsch"},
      {"pt",  "Portugu\xC3\xAA" "s"},
    };
    html += F("<div class='vm-card'><h2>&#x1F310;&ensp;System Language</h2><div class='vm-label'>LANGUAGE</div><select class='vm-select' name='wc_lang'>");
    html += F("<optgroup label='Creative &amp; Constructed'>");
    for (unsigned i = 0; i < sizeof(kLangsFun)/sizeof(kLangsFun[0]); ++i) {
      html += F("<option value='");
      html += kLangsFun[i].code;
      html += '\'';
      if (strcmp(g_wordClockLang, kLangsFun[i].code) == 0) html += F(" selected");
      html += '>';
      html += kLangsFun[i].label;
      html += F("</option>");
    }
    html += F("</optgroup><optgroup label='Modern Languages'>");
    for (unsigned i = 0; i < sizeof(kLangsStd)/sizeof(kLangsStd[0]); ++i) {
      html += F("<option value='");
      html += kLangsStd[i].code;
      html += '\'';
      if (strcmp(g_wordClockLang, kLangsStd[i].code) == 0) html += F(" selected");
      html += '>';
      html += kLangsStd[i].label;
      html += F("</option>");
    }
    html += F("</optgroup></select><p class='vm-help'>Controls the language of the entire display UI: word clock, weather labels, RSS status and touch hints. Saved to NVS, persists across reboots.</p></div>");
  }
  // Wikipedia Language section
  {
    struct { const char *code; const char *label; } kWikiLangs[] = {
      {"en", "English"}, {"it", "Italiano"},
      {"fr", "Fran\xC3\xA7" "ais"}, {"de", "Deutsch"},
      {"es", "Espa\xC3\xB1" "ol"}, {"pt", "Portugu\xC3\xAA" "s"},
      {"la", "Latina"}, {"eo", "Esperanto"},
    };
    html += F("<div class='vm-card'><h2>&#x1F4D6;&ensp;Wikipedia Language</h2>"
              "<div class='vm-label'>WIKI LANGUAGE</div><select class='vm-select' name='wiki_lang'>");
    for (unsigned i = 0; i < sizeof(kWikiLangs)/sizeof(kWikiLangs[0]); ++i) {
      html += F("<option value='");
      html += kWikiLangs[i].code;
      html += '\'';
      if (strcmp(g_wikiLang, kWikiLangs[i].code) == 0) html += F(" selected");
      html += '>';
      html += kWikiLangs[i].label;
      html += F("</option>");
    }
    html += F("</select><p class='vm-help'>"
              "Language for Wikipedia feeds (Featured, On This Day, Random). Independent from the system language.</p></div>");
  }
}

static void buildWebWeatherSection(String &html) {
  char latBuf[24];
  char lonBuf[24];
  snprintf(latBuf, sizeof(latBuf), "%.4f", runtimeWeatherLat());
  snprintf(lonBuf, sizeof(lonBuf), "%.4f", runtimeWeatherLon());
  html += F("<div class='vm-card'><h2>&#x2600;&ensp;Weather &amp; Location</h2><div class='vm-grid'><div><div class='vm-label'>PLACE SEARCH</div><input class='vm-input' id='geo_query' type='search' list='geo_hits' placeholder='Search city or place'><datalist id='geo_hits'></datalist><p id='geo_status' class='geo-status'></p><div class='vm-label'>CITY LABEL</div><input class='vm-input' id='weather_city' name='weather_city' maxlength='31' value='");
  appendHtmlEscaped(html, runtimeWeatherCityLabel());
  html += F("'></div><div class='vm-grid'><div><div class='vm-label'>LATITUDE</div><input class='vm-input' id='weather_lat' name='weather_lat' value='");
  appendHtmlEscaped(html, latBuf);
  html += F("'></div><div><div class='vm-label'>LONGITUDE</div><input class='vm-input' id='weather_lon' name='weather_lon' value='");
  appendHtmlEscaped(html, lonBuf);
  html += F("'></div></div></div></div>");
}

static void buildWebRssBuilder(String &html) {
  const uint8_t configuredFeeds = runtimeRssConfiguredFeedCount();
  html += F("<div class='vm-card'><h2>&#x1F4E1;&ensp;RSS Feed Builder <span id='rss_count_pill' class='vm-badge vm-badge--info'>RSS feeds ");
  html += configuredFeeds;
  html += F("/5</span></h2><p class='vm-help'>One composer for name, URL and max posts. Press + to add to the list (max 5 feeds).</p>");
  html += F("<div class='vm-rss-composer'><div><div class='vm-label'>FRIENDLY NAME</div><input class='vm-input' id='rss_name' maxlength='23' placeholder='Nintendo'></div><div><div class='vm-label'>FEED URL</div><input class='vm-input' id='rss_url' type='url' placeholder='https://example.com/feed.xml'></div><div><div class='vm-label'>MAX POSTS</div><input class='vm-input' id='rss_max' type='number' min='1' max='8' value='8'></div><button id='rss_add' class='vm-btn vm-btn--primary' type='button'>+ Add</button><button id='rss_reset' class='vm-btn vm-btn--secondary' type='button'>Reset</button></div><p id='rss_status' class='rss-status'></p>");
  html += F("<div id='rss_list' class='vm-rss-list'></div><p id='rss_empty' class='rss-empty'>No feeds configured.</p><div id='rss_hidden_inputs' class='hidden'></div></div>");
}

static void buildWebTransitSection(String &html) {
  html += F("<div class='vm-card'><h2>&#x1F689;&ensp;Transit Departure Board</h2>");
  html += F("<p class='vm-help'>Search for any stop worldwide. "
            "Powered by <a href='https://transitous.org' target='_blank' rel='noopener noreferrer'>Transitous</a> "
            "(global free GTFS data — trains, buses, trams). "
            "Type at least 3 chars and pick from the list; destination filter is optional.</p>");
  html += F("<div class='vm-grid'>");

  // --- Departure station ---
  html += F("<div><div class='vm-label'>DEPARTURE STATION</div>");
  html += F("<input class='vm-input' type='search' id='transit_from_q' list='transit_from_hits' "
            "autocomplete='off' placeholder='e.g. Luino, Milano Centrale, Zurich HB'");
  if (g_transitConfig.station[0]) {
    html += F(" value='");
    appendHtmlEscaped(html, g_transitConfig.station);
    html += '\'';
  }
  html += F("><datalist id='transit_from_hits'></datalist>");
  // Hidden: display name submitted as transit_from
  html += F("<input type='hidden' id='transit_from_val' name='transit_from' value='");
  appendHtmlEscaped(html, g_transitConfig.station);
  html += F("'>");
  // Hidden: Transitous stop ID submitted as transit_from_id
  html += F("<input type='hidden' id='transit_from_id' name='transit_from_id' value='");
  appendHtmlEscaped(html, g_transitConfig.stopId);
  html += F("'><p id='transit_from_st' class='geo-status'></p></div>");

  // --- Destination filter (optional) ---
  html += F("<div><div class='vm-label'>FILTER BY DESTINATION <small>(optional)</small></div>");
  html += F("<input class='vm-input' type='text' id='transit_to_q' name='transit_to' "
            "autocomplete='off' placeholder='Leave empty for all departures'");
  if (g_transitConfig.arrStation[0]) {
    html += F(" value='");
    appendHtmlEscaped(html, g_transitConfig.arrStation);
    html += '\'';
  }
  html += F("><p class='vm-help'><small>Partial match on headsign, e.g. \"Gallarate\" or \"Milano\".</small></p></div>");

  html += F("</div>"); // vm-grid

  if (g_transitConfig.configured) {
    html += F("<p class='vm-help'><small>Last fetch: ");
    html += g_transitState.fetchedAt;
    html += F(" &bull; ");
    html += g_transitState.count;
    html += F(" departure(s)");
    if (g_transitConfig.stopId[0]) {
      html += F(" &bull; id: <code>");
      appendHtmlEscaped(html, g_transitConfig.stopId);
      html += F("</code>");
    }
    html += F(".</small></p>");
  }
  html += F("</div>"); // vm-card

  // Inline autocomplete JS — Transitous geocode API
  // Bug-fixes vs r240: filter type=STOP only (avoids city POI overwriting train stop in map);
  // check map in 'input' handler before re-searching (iOS never fires 'change' until blur).
  html += F("<script>(function(){"
    "var q=document.getElementById('transit_from_q');"
    "var v=document.getElementById('transit_from_val');"
    "var idI=document.getElementById('transit_from_id');"
    "var dl=document.getElementById('transit_from_hits');"
    "var st=document.getElementById('transit_from_st');"
    "if(!q||!dl)return;"
    "var t=0,map={};"
    "function setS(m){if(st)st.innerHTML=m||'';}"
    "function clr(){dl.innerHTML='';map={};}"
    // tryCapture: if current value is an exact map entry, store ID and return true
    "function tryCapture(val){"
      "var s=map[val];"
      "if(s&&s.id){"
        "if(v)v.value=s.name||val;"
        "if(idI)idI.value=s.id;"
        "var modes=(s.modes&&s.modes.length)?s.modes.slice(0,2).join('+'):'';"
        "setS('&#x2713; <b>'+s.name+'</b>'+(modes?' ['+modes+']':'')+'<br><small style=\"opacity:.6\">'+s.id+'</small>');"
        "return true;"
      "}"
      "return false;"
    "}"
    "q.addEventListener('input',function(){"
      "var term=q.value.trim();"
      // On datalist pick, 'input' fires before 'change' (esp. on iOS) — capture immediately
      "if(tryCapture(term)){clearTimeout(t);return;}"
      "if(term.length<3){clr();setS('');return;}"
      "clearTimeout(t);"
      "t=setTimeout(async function(){"
        "try{"
          "setS('Searching...');"
          "var u='https://api.transitous.org/api/v1/geocode?text='+encodeURIComponent(term)+'&limit=12';"
          "var r=await fetch(u,{cache:'no-store'});"
          "if(!r.ok)throw new Error('HTTP '+r.status);"
          "var d=await r.json();"
          // API returns plain array
          "var rows=Array.isArray(d)?d:[];"
          "clr();"
          // Filter to transit STOPs only (exclude type=PLACE city markers)
          "var stops=rows.filter(function(s){return s.type==='STOP';});"
          "if(!stops.length){setS('No transit stop found. Try a city or station name.');return;}"
          "stops.forEach(function(s){"
            "var n=s.name||'';"
            "if(!n)return;"
            "var modes=(s.modes&&s.modes.length)?s.modes.slice(0,2).join('+'):'';"
            "var label=n+(modes?' ['+modes+']':'')+(s.country?' \u2022 '+s.country:'');"
            "var o=document.createElement('option');"
            "o.value=n;"
            "o.label=label;"
            "dl.appendChild(o);"
            // Keep first occurrence on name collision (API sorts by relevance, best first)
            "if(!map[n])map[n]=s;"
          "});"
          "setS(stops.length+' stop(s) found \u2014 pick from list.');"
          // Auto-fill if single result
          "if(stops.length===1){"
            "q.value=stops[0].name||'';"
            "tryCapture(q.value);"
          "}"
        "}catch(e){clr();setS('Search error: '+e.message);}"
      "},400);"
    "});"
    // Fallback change handler (desktop: fires after blur when datalist selected)
    "q.addEventListener('change',function(){"
      "if(!tryCapture(q.value.trim())&&!q.value.trim()){if(v)v.value='';if(idI)idI.value='';setS('');}"
    "});"
  "})();</script>");
}

static void buildWebSystemInfo(String &html) {
  char siBuf[48];
  html += F("<div class='vm-card vm-card--muted'><h2>&#x2699;&ensp;System Info</h2><div class='vm-grid'>");
  // Network card
  html += F("<div><div class='vm-label'>NETWORK</div>");
#if TEST_WIFI
  {
    const bool wOk = (WiFi.status() == WL_CONNECTED) && g_wifiSt.connected;
    html += F("<small>ip: </small><code>");
    html += wOk ? WiFi.localIP().toString() : "--";
    html += F("</code><br><small>ssid: </small><code>");
    if (wOk) {
      appendHtmlEscaped(html, WiFi.SSID().c_str());
    } else {
      html += F("--");
    }
    html += F("</code><br><small>rssi: </small><code>");
    if (wOk) { snprintf(siBuf, sizeof(siBuf), "%d dBm", WiFi.RSSI()); html += siBuf; }
    else { html += F("--"); }
    html += F("</code><br><small>mac: </small><code>");
    html += wOk ? WiFi.macAddress() : "--";
    html += F("</code><br><small>dns: </small><code>");
    if (wOk) {
      html += WiFi.dnsIP(0).toString();
      html += F(" / ");
      html += WiFi.dnsIP(1).toString();
    } else { html += F("--"); }
    html += F("</code><br><small>preferred: </small><code>");
    if (g_wifiSt.preferredSsid[0]) appendHtmlEscaped(html, g_wifiSt.preferredSsid);
    else html += F("auto");
    html += F("</code><br><small>direct mode: </small><code>");
    appendHtmlEscaped(html, g_wifiSt.setupMode);
    html += F("</code><br><small>setup ap: </small><code>");
    if (g_wifiSt.setupApActive) {
      appendHtmlEscaped(html, g_wifiSt.setupApSsid);
      html += F(" @ ");
      html += WiFi.softAPIP().toString();
    } else {
      html += F("off");
    }
    html += F("</code>");
  }
#else
  html += F("<code>wifi disabled</code>");
#endif
  html += F("</div>");
  // Firmware / runtime card
  html += F("<div><div class='vm-label'>FIRMWARE &amp; RUNTIME</div>");
  html += F("<small>fw: </small><code>"); appendHtmlEscaped(html, FW_BUILD_TAG); html += F("</code><br>");
  html += F("<small>date: </small><code>"); appendHtmlEscaped(html, FW_RELEASE_DATE); html += F("</code><br>");
  html += F("<small>lang: </small><code>"); appendHtmlEscaped(html, g_wordClockLang); html += F("</code><br>");
  html += F("<small>theme: </small><code>"); appendHtmlEscaped(html, runtimeUiThemeLabel()); html += F("</code><br>");
  snprintf(siBuf, sizeof(siBuf), "%lus", (unsigned long)(millis() / 1000UL));
  html += F("<small>uptime: </small><code>"); html += siBuf; html += F("</code><br>");
#if TEST_NTP
  html += F("<small>ntp: </small><code>"); html += g_clock.ntpSynced ? "SYNCED" : "WAIT"; html += F("</code><br>");
#endif
  snprintf(siBuf, sizeof(siBuf), "%u KB", (unsigned)(ESP.getFreeHeap() / 1024));
  html += F("<small>free heap: </small><code>"); html += siBuf; html += F("</code>");
#if TEST_BATTERY
  html += F("<br><small>battery: </small><code>");
  if (g_batt.hasSample) {
    snprintf(siBuf, sizeof(siBuf), "%d%%", g_batt.percent);
    html += siBuf;
    if (g_batt.chargingLikely) html += F(" +CHG");
  } else { html += F("N/A"); }
  html += F("</code>");
#endif
  html += F("</div>");
  html += F("</div></div>");
}

static void buildWebJsBlock(String &html) {
  html += FPSTR(kWebJsCorePre);
  for (uint8_t i = 0; i < RSS_FEED_SLOT_COUNT; ++i) {
    const RuntimeRssFeedConfig *feed = runtimeRssFeedBySlot(i);
    if (i) html += F(",");
    html += F("{name:\"");
    appendJsonEscaped(html, feed ? feed->name : "");
    html += F("\",url:\"");
    appendJsonEscaped(html, feed ? feed->url : "");
    html += F("\",max:");
    html += (unsigned)(feed ? clampRssFeedMaxItems(feed->maxItems) : RSS_DEFAULT_FEED_ITEMS);
    html += F("}");
  }
  html += FPSTR(kWebJsCorePost);
}

static String buildWebConfigPage(const char *statusMsg) {
  ensureRuntimeNetConfig();

  String html;
  html.reserve(22000);
  // ── Head: meta + fonts + CSS ──
  html += F("<!doctype html><html lang='en'><head><meta charset='utf-8'>");
  html += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>ScryBar Control Surface</title>");
  html += F("<link rel='preconnect' href='https://fonts.googleapis.com'>");
  html += F("<link rel='preconnect' href='https://fonts.gstatic.com' crossorigin>");
  html += F("<link rel='stylesheet' href='https://fonts.googleapis.com/css2?family=Montserrat:wght@400;500;600;700&display=swap' media='print' onload=\"this.media='all'\">");
  html += F("<noscript><link href='https://fonts.googleapis.com/css2?family=Montserrat:wght@400;500;600;700&display=swap' rel='stylesheet'></noscript>");
  html += F("<style>");
  buildWebCssBlock(html);
  html += F("</style></head><body><main class='vm-wrap'>");
  // ── Body: hero + form sections ──
  buildWebHeroSection(html, statusMsg);
  html += F("<form id='cfg_form' method='post' action='/config'>");
  buildWebThemeSelector(html);
  buildWebViewToggles(html);
#if TEST_WIFI
  buildWebWifiSection(html);
#endif
  buildWebLangSelectors(html);
  buildWebWeatherSection(html);
  buildWebTransitSection(html);   // Transit before RSS; Save button moved to sticky footer
  buildWebRssBuilder(html);
  // Sticky Save bar — always visible at the bottom of the viewport while scrolling
  html += F("<div class='vm-actions vm-actions-sticky'>"
            "<button class='vm-btn vm-btn--primary' type='submit'>&#x1F4BE; Save Config</button>"
            "<button class='vm-btn vm-btn--secondary' type='submit' formaction='/reload' formmethod='post'>Force Reload</button>"
            "</div>");
  html += F("</form>");
  // ── System info + footer + JS ──
  buildWebSystemInfo(html);
  html += F("<p class='vm-api-note'><small>API ready: <code>GET /api/config</code>, <code>POST /api/config</code>, <code>GET /api/wifi/scan</code>, <code>GET /api/wifi/setup-qr.svg</code>.</small></p>");
  html += F("<footer class='vm-footer'><strong>A project by Netmilk Studio sagl</strong> | Copyright 2026<br>Open Source under the <a href='https://opensource.org/license/mit' target='_blank' rel='noopener noreferrer'>MIT License</a> | Feel free to steal, fork, remix, and ship. \xF0\x9F\x96\x96</footer>");
  buildWebJsBlock(html);
  html += F("</section></main></body></html>");
  return html;
}

static void sendWebConfigJson(int code, bool ok, const char *message = nullptr) {
  ensureRuntimeNetConfig();
  char latBuf[24];
  char lonBuf[24];
  snprintf(latBuf, sizeof(latBuf), "%.4f", runtimeWeatherLat());
  snprintf(lonBuf, sizeof(lonBuf), "%.4f", runtimeWeatherLon());

  String out;
  out.reserve(3600);
  out += F("{\"ok\":");
  out += ok ? F("true") : F("false");
  if (message && message[0]) {
    out += F(",\"message\":\"");
    appendJsonEscaped(out, message);
    out += '"';
  }
  out += F(",\"weather\":{\"city\":\"");
  appendJsonEscaped(out, runtimeWeatherCityLabel());
  out += F("\",\"lat\":");
  out += latBuf;
  out += F(",\"lon\":");
  out += lonBuf;
  out += F("},\"wifi\":{\"preferred_ssid\":\"");
  appendJsonEscaped(out, g_wifiSt.preferredSsid);
  out += F("\",\"known\":[");
  bool firstSsid = true;
  for (uint8_t i = 0; i < g_wifiSt.credCount; ++i) {
    const char *ssid = g_wifiSt.credSsids[i];
    if (!ssid || !ssid[0]) continue;
    if (!firstSsid) out += ',';
    firstSsid = false;
    out += '"';
    appendJsonEscaped(out, ssid);
    out += '"';
  }
  out += F("],\"setup_mode\":\"");
  appendJsonEscaped(out, g_wifiSt.setupMode);
  out += F("\",\"setup_ap_active\":");
  out += g_wifiSt.setupApActive ? F("true") : F("false");
  out += F(",\"setup_ap_ssid\":\"");
  appendJsonEscaped(out, g_wifiSt.setupApSsid);
  out += F("\",\"setup_ap_ip\":\"");
  appendJsonEscaped(out, WiFi.softAPIP().toString().c_str());
  out += F("\",\"runtime_known\":");
  out += (unsigned)g_wifiSt.runtimeCredCount;
  out += F("},\"rss\":{\"feed_url\":\"");
  appendJsonEscaped(out, runtimeRssFeedUrl());
  out += F("\",\"active_max_items\":");
  out += (unsigned)runtimeRssActiveMaxItems();
  out += F(",\"configured\":");
  out += (unsigned)runtimeRssConfiguredFeedCount();
  out += F(",\"feeds\":[");
  for (uint8_t i = 0; i < RSS_FEED_SLOT_COUNT; ++i) {
    const RuntimeRssFeedConfig *feed = runtimeRssFeedBySlot(i);
    if (i) out += ',';
    out += F("{\"slot\":");
    out += (unsigned)(i + 1);
    out += F(",\"name\":\"");
    appendJsonEscaped(out, feed ? feed->name : "");
    out += F("\",\"url\":\"");
    appendJsonEscaped(out, feed ? feed->url : "");
    out += F("\",\"max_items\":");
    out += (unsigned)(feed ? clampRssFeedMaxItems(feed->maxItems) : RSS_DEFAULT_FEED_ITEMS);
    out += '}';
  }
  out += F("]},\"branding\":{\"logo_url\":\"");
  appendJsonEscaped(out, runtimeLogoUrl());
  out += F("\"},\"word_clock\":{\"lang\":\"");
  out += g_wordClockLang;
  out += F("\"},\"ui\":{\"theme\":\"");
  appendJsonEscaped(out, runtimeUiThemeId());
  out += F("\",\"theme_label\":\"");
  appendJsonEscaped(out, runtimeUiThemeLabel());
  out += F("\",\"views\":{\"info\":");
  out += (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_INFO) ? F("true") : F("false");
  out += F(",\"home\":true,\"aux\":");
  out += (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_AUX) ? F("true") : F("false");
  out += F(",\"wiki\":");
  out += (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_WIKI) ? F("true") : F("false");
  out += F(",\"now_playing\":");
  out += (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_NOW_PLAYING) ? F("true") : F("false");
  out += F(",\"transit\":");
  out += (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_TRANSIT) ? F("true") : F("false");
  out += F("},\"themes\":[");
  for (size_t i = 0; i < UI_THEME_COUNT; ++i) {
    if (i) out += ',';
    out += F("{\"id\":\"");
    appendJsonEscaped(out, kUiThemes[i].id);
    out += F("\",\"label\":\"");
    appendJsonEscaped(out, kUiThemes[i].label);
    out += F("\"}");
  }
  out += F("]}}");
  g_webCfg.server.send(code, "application/json", out);
}

static void webConfigRedirect(const char *status) {
  String location = "/";
  if (status && status[0]) {
    location += "?status=";
    location += status;
  }
  g_webCfg.server.sendHeader("Location", location, true);
  g_webCfg.server.send(303, "text/plain", "");
}

// ── M5: Config parser sub-functions ──
// (ConfigDiffResult is defined in config.h for Arduino auto-prototype visibility)

static bool parseWeatherConfig(RuntimeNetConfig &next, String &errorOut, bool &hasInput) {
  if (g_webCfg.server.hasArg("weather_city")) {
    hasInput = true;
    String city = g_webCfg.server.arg("weather_city");
    city.trim();
    if (city.length() == 0) { errorOut = "weather_city vuota"; return false; }
    copyStringSafe(next.weatherCity, sizeof(next.weatherCity), city.c_str());
  }
  if (g_webCfg.server.hasArg("weather_lat")) {
    hasInput = true;
    float lat = 0.0f;
    if (!parseStrictFloat(g_webCfg.server.arg("weather_lat"), lat) || !isfinite(lat) || lat < -90.0f || lat > 90.0f) {
      errorOut = "weather_lat non valida"; return false;
    }
    next.weatherLat = lat;
  }
  if (g_webCfg.server.hasArg("weather_lon")) {
    hasInput = true;
    float lon = 0.0f;
    if (!parseStrictFloat(g_webCfg.server.arg("weather_lon"), lon) || !isfinite(lon) || lon < -180.0f || lon > 180.0f) {
      errorOut = "weather_lon non valida"; return false;
    }
    next.weatherLon = lon;
  }
  return true;
}

static bool parseRssFeedConfig(RuntimeNetConfig &next, String &errorOut, bool &hasInput) {
  bool rssInput = false;
  if (g_webCfg.server.hasArg("rss_feed_url")) {
    hasInput = true; rssInput = true;
    String rss = g_webCfg.server.arg("rss_feed_url"); rss.trim();
    if (rss.length() > 0 && !isHttpUrl(rss)) { errorOut = "rss_feed_url deve iniziare con http:// o https://"; return false; }
    if (rss.length() == 0) next.rssFeeds[0].url[0] = '\0';
    else copyStringSafe(next.rssFeeds[0].url, sizeof(next.rssFeeds[0].url), rss.c_str());
  }
  if (g_webCfg.server.hasArg("rss_feed_name")) {
    hasInput = true; rssInput = true;
    String name = g_webCfg.server.arg("rss_feed_name"); name.trim();
    copyStringSafe(next.rssFeeds[0].name, sizeof(next.rssFeeds[0].name), name.c_str());
  }
  if (g_webCfg.server.hasArg("rss_feed_items")) {
    hasInput = true; rssInput = true;
    uint8_t maxItems = 0;
    if (!parseStrictUint8(g_webCfg.server.arg("rss_feed_items"), maxItems)) { errorOut = "rss_feed_items non valido"; return false; }
    next.rssFeeds[0].maxItems = clampRssFeedMaxItems(maxItems);
  }
  for (uint8_t i = 0; i < RSS_FEED_SLOT_COUNT; ++i) {
    char keyName[32], keyUrl[32], keyItems[32];
    snprintf(keyName, sizeof(keyName), "rss_feed_name_%u", (unsigned)(i + 1));
    snprintf(keyUrl, sizeof(keyUrl), "rss_feed_url_%u", (unsigned)(i + 1));
    snprintf(keyItems, sizeof(keyItems), "rss_feed_items_%u", (unsigned)(i + 1));
    if (g_webCfg.server.hasArg(keyName)) {
      hasInput = true; rssInput = true;
      String name = g_webCfg.server.arg(keyName); name.trim();
      copyStringSafe(next.rssFeeds[i].name, sizeof(next.rssFeeds[i].name), name.c_str());
    }
    if (g_webCfg.server.hasArg(keyUrl)) {
      hasInput = true; rssInput = true;
      String url = g_webCfg.server.arg(keyUrl); url.trim();
      if (url.length() > 0 && !isHttpUrl(url)) { errorOut = "rss_feed_url_N deve iniziare con http:// o https://"; return false; }
      if (url.length() == 0) next.rssFeeds[i].url[0] = '\0';
      else copyStringSafe(next.rssFeeds[i].url, sizeof(next.rssFeeds[i].url), url.c_str());
    }
    if (g_webCfg.server.hasArg(keyItems)) {
      hasInput = true; rssInput = true;
      uint8_t maxItems = 0;
      if (!parseStrictUint8(g_webCfg.server.arg(keyItems), maxItems)) { errorOut = "rss_feed_items_N non valido"; return false; }
      next.rssFeeds[i].maxItems = clampRssFeedMaxItems(maxItems);
    }
  }
  if (rssInput) normalizeRuntimeRssFeeds(next);
  return true;
}

static bool parseLogoConfig(RuntimeNetConfig &next, String &errorOut, bool &hasInput) {
  if (g_webCfg.server.hasArg("logo_url")) {
    hasInput = true;
    String logo = g_webCfg.server.arg("logo_url"); logo.trim();
    if (logo.length() > 0 && !isHttpUrl(logo)) { errorOut = "logo_url deve iniziare con http:// o https://"; return false; }
    copyStringSafe(next.logoUrl, sizeof(next.logoUrl), logo.c_str());
  }
  return true;
}

static bool parseThemeConfig(RuntimeNetConfig &next, String &errorOut, bool &hasInput) {
  if (g_webCfg.server.hasArg("ui_theme")) {
    hasInput = true;
    String theme = g_webCfg.server.arg("ui_theme"); theme.trim();
    if (theme.length() == 0) { errorOut = "ui_theme vuoto"; return false; }
    if (findUiThemeIndexById(theme.c_str()) < 0) { errorOut = "ui_theme non valido"; return false; }
    copyStringSafe(next.uiTheme, sizeof(next.uiTheme), theme.c_str());
  }
  return true;
}

static bool parseViewsConfig(RuntimeNetConfig &next, String &errorOut, bool &hasInput) {
  bool viewsInput = false;
  struct ViewArgDef { const char *key; uint8_t bit; };
  static const ViewArgDef kViewArgs[] = {
      {"view_info",        UI_VIEW_FLAG_INFO},
      {"view_aux",         UI_VIEW_FLAG_AUX},
      {"view_wiki",        UI_VIEW_FLAG_WIKI},
      {"view_now_playing", UI_VIEW_FLAG_NOW_PLAYING},
      {"view_transit",     UI_VIEW_FLAG_TRANSIT},
  };
  for (const ViewArgDef &viewArg : kViewArgs) {
    if (!g_webCfg.server.hasArg(viewArg.key)) continue;
    hasInput = true; viewsInput = true;
    bool enabled = false;
    if (!parseStrictBool(g_webCfg.server.arg(viewArg.key), enabled)) {
      errorOut = String(viewArg.key) + " non valido"; return false;
    }
    if (enabled) next.enabledViewsMask |= viewArg.bit;
    else next.enabledViewsMask &= (uint8_t)~viewArg.bit;
  }
  if (viewsInput) next.enabledViewsMask = normalizeRuntimeViewMask(next.enabledViewsMask);
  return true;
}

static bool parseWifiSetupModeConfig(String &errorOut, bool &hasInput, bool &wifiSetupModeChanged) {
  if (g_webCfg.server.hasArg("wifi_setup_mode")) {
    hasInput = true;
    String setupMode = g_webCfg.server.arg("wifi_setup_mode");
    setupMode.trim(); setupMode.toLowerCase();
    if (!(setupMode == "off" || setupMode == "auto" || setupMode == "on")) {
      errorOut = "wifi_setup_mode non valido"; return false;
    }
    if (strncmp(g_wifiSt.setupMode, setupMode.c_str(), sizeof(g_wifiSt.setupMode)) != 0) {
      copyStringSafe(g_wifiSt.setupMode, sizeof(g_wifiSt.setupMode), setupMode.c_str());
      wifiSetupModeChanged = true;
    }
  }
  return true;
}

static bool parseWifiCredentialConfig(String &errorOut, bool &hasInput, bool &wifiPrefChanged, bool &wifiProvisioned, int8_t &wifiPrefIdx) {
  if (g_webCfg.server.hasArg("wifi_new_ssid") || g_webCfg.server.hasArg("wifi_new_password")) {
    hasInput = true;
    String ssid = g_webCfg.server.arg("wifi_new_ssid");
    String pass = g_webCfg.server.arg("wifi_new_password");
    ssid.trim();
    if (ssid.length() > 0) {
      if (ssid.length() > WIFI_MAX_SSID_LEN) { errorOut = "wifi_new_ssid troppo lungo"; return false; }
      if (pass.length() > WIFI_MAX_PASSWORD_LEN) { errorOut = "wifi_new_password troppo lunga"; return false; }
      if (!upsertRuntimeWiFiCredential(ssid.c_str(), pass.c_str())) { errorOut = "impossibile salvare rete runtime"; return false; }
      wifiPrepareCredentialCache();
      char previousPref[sizeof(g_wifiSt.preferredSsid)] = {0};
      copyStringSafe(previousPref, sizeof(previousPref), g_wifiSt.preferredSsid);
      copyStringSafe(g_wifiSt.preferredSsid, sizeof(g_wifiSt.preferredSsid), ssid.c_str());
      wifiPrefChanged = true;
      wifiProvisioned = true;
      wifiPrefIdx = findWiFiCredentialIndexBySsid(g_wifiSt.preferredSsid);
      if (wifiPrefIdx < 0) wifiPrefChanged = (strcmp(previousPref, g_wifiSt.preferredSsid) != 0);
      Serial.printf("[WIFI][PROVISION] added ssid='%s' runtime_known=%u\n",
                    g_wifiSt.preferredSsid, (unsigned)g_wifiSt.runtimeCredCount);
    }
  }
  return true;
}

static bool parseWifiPreferredConfig(String &errorOut, bool &hasInput, bool wifiProvisioned, bool &wifiPrefChanged, int8_t &wifiPrefIdx) {
  if (!wifiProvisioned && g_webCfg.server.hasArg("wifi_pref_ssid")) {
    hasInput = true;
    String preferred = g_webCfg.server.arg("wifi_pref_ssid"); preferred.trim();
    char previousPref[sizeof(g_wifiSt.preferredSsid)] = {0};
    copyStringSafe(previousPref, sizeof(previousPref), g_wifiSt.preferredSsid);
    if (preferred.length() == 0) {
      g_wifiSt.preferredSsid[0] = '\0';
      wifiPrefChanged = (previousPref[0] != '\0');
    } else {
      if (preferred.length() >= sizeof(g_wifiSt.preferredSsid)) { errorOut = "wifi_pref_ssid troppo lungo"; return false; }
      wifiPrefIdx = findWiFiCredentialIndexBySsid(preferred.c_str());
      if (wifiPrefIdx < 0) { errorOut = "wifi_pref_ssid non presente nelle reti note"; return false; }
      copyStringSafe(g_wifiSt.preferredSsid, sizeof(g_wifiSt.preferredSsid), preferred.c_str());
      wifiPrefChanged = (strcmp(previousPref, g_wifiSt.preferredSsid) != 0);
    }
  }
  return true;
}

static bool parseLangConfig(String &errorOut, bool &hasInput, bool &langChanged) {
  if (g_webCfg.server.hasArg("wc_lang")) {
    hasInput = true;
    String lang = g_webCfg.server.arg("wc_lang");
    lang.trim(); lang.toLowerCase();
    if (!isValidLangCode(lang)) { errorOut = "wc_lang non valido"; return false; }
    if (strncmp(g_wordClockLang, lang.c_str(), sizeof(g_wordClockLang)) != 0) {
      copyStringSafe(g_wordClockLang, sizeof(g_wordClockLang), lang.c_str());
      langChanged = true;
      Serial.printf("[CFG][WEB] wc_lang='%s'\n", g_wordClockLang);
    }
  }
  return true;
}

static bool parseWikiLangConfig(String &errorOut, bool &hasInput, bool &wikiLangChanged) {
  if (g_webCfg.server.hasArg("wiki_lang")) {
    hasInput = true;
    String wl = g_webCfg.server.arg("wiki_lang");
    wl.trim(); wl.toLowerCase();
    const char* kWikiLangs[] = {"en","it","fr","de","es","pt","la","eo",nullptr};
    bool wlValid = false;
    for (const char **p = kWikiLangs; *p; ++p) { if (wl == *p) { wlValid = true; break; } }
    if (wlValid && strncmp(g_wikiLang, wl.c_str(), sizeof(g_wikiLang)) != 0) {
      strncpy(g_wikiLang, wl.c_str(), sizeof(g_wikiLang) - 1);
      g_wikiLang[sizeof(g_wikiLang) - 1] = '\0';
      wikiLangChanged = true;
      Serial.printf("[CFG][WEB] wiki_lang='%s'\n", g_wikiLang);
    }
  }
  return true;
}

static bool parseTransitConfig(String &errorOut, bool &hasInput, bool &transitChanged) {
  if (!g_webCfg.server.hasArg("transit_from")) return true;
  hasInput = true;
  String stn = g_webCfg.server.arg("transit_from");
  stn.trim();
  if (stn.length() >= TRANSIT_STATION_LEN) { errorOut = "departure station name too long"; return false; }

  String sid = g_webCfg.server.hasArg("transit_from_id") ? g_webCfg.server.arg("transit_from_id") : "";
  sid.trim();
  if (sid.length() >= TRANSIT_STOP_ID_LEN) { errorOut = "stop ID too long"; return false; }

  String arr = g_webCfg.server.hasArg("transit_to") ? g_webCfg.server.arg("transit_to") : "";
  arr.trim();
  if (arr.length() >= TRANSIT_STATION_LEN) { errorOut = "destination station name too long"; return false; }

  // configured = name AND stop ID both present
  const bool newConfigured = (stn.length() > 0 && sid.length() > 0);
  const bool stnChanged = (strncmp(g_transitConfig.station, stn.c_str(), TRANSIT_STATION_LEN) != 0);
  const bool sidChanged = (strncmp(g_transitConfig.stopId,  sid.c_str(), TRANSIT_STOP_ID_LEN)  != 0);
  const bool arrChanged = (strncmp(g_transitConfig.arrStation, arr.c_str(), TRANSIT_STATION_LEN) != 0);
  const bool cfgChanged = (g_transitConfig.configured != newConfigured);

  if (stnChanged || sidChanged || arrChanged || cfgChanged) {
    copyStringSafe(g_transitConfig.station,    sizeof(g_transitConfig.station),    stn.c_str());
    copyStringSafe(g_transitConfig.stopId,     sizeof(g_transitConfig.stopId),     sid.c_str());
    copyStringSafe(g_transitConfig.arrStation, sizeof(g_transitConfig.arrStation), arr.c_str());
    g_transitConfig.configured = newConfigured;
    // Invalidate cached state so next fetch is triggered immediately
    if (stnChanged || sidChanged) {
      g_transitState.valid      = false;
      g_transitState.count      = 0;
      g_transitState.lastFetchMs = 0;
    }
    transitChanged = true;
    Serial.printf("[CFG][WEB] transit_from='%s' transit_from_id='%s' transit_to='%s' configured=%d\n",
                  g_transitConfig.station, g_transitConfig.stopId,
                  g_transitConfig.arrStation, (int)g_transitConfig.configured);
  }
  return true;
}

// ── M5: Commit phase — diff detection + NVS save ──

static ConfigDiffResult commitConfigToNvs(RuntimeNetConfig &next) {
  normalizeRuntimeUiTheme(next);
  const bool weatherChanged =
      (strncmp(g_runtimeNetConfig.weatherCity, next.weatherCity, sizeof(next.weatherCity)) != 0) ||
      (fabsf(g_runtimeNetConfig.weatherLat - next.weatherLat) > 0.00005f) ||
      (fabsf(g_runtimeNetConfig.weatherLon - next.weatherLon) > 0.00005f);
  bool rssChanged = false;
  for (uint8_t i = 0; i < RSS_FEED_SLOT_COUNT; ++i) {
    if (!runtimeRssFeedEntriesEqual(g_runtimeNetConfig.rssFeeds[i], next.rssFeeds[i])) { rssChanged = true; break; }
  }
  const bool brandingChanged = (strncmp(g_runtimeNetConfig.logoUrl, next.logoUrl, sizeof(next.logoUrl)) != 0);
  const bool themeChanged = (strncmp(g_runtimeNetConfig.uiTheme, next.uiTheme, sizeof(next.uiTheme)) != 0);
  const bool viewsChanged = (g_runtimeNetConfig.enabledViewsMask != next.enabledViewsMask);

  g_runtimeNetConfig = next;
  normalizeRuntimeUiTheme(g_runtimeNetConfig);
  g_runtimeNetConfig.enabledViewsMask = normalizeRuntimeViewMask(g_runtimeNetConfig.enabledViewsMask);
  syncActiveUiThemeFromRuntimeConfig(g_runtimeNetConfig);
  g_runtimeNetConfig.ready = true;
  const bool nvsSaved = saveRuntimeNetConfigToNvs();
  if (!nvsSaved) Serial.println("[CFG][NVS] warning: config aggiornata in RAM ma non salvata su flash");

  return { weatherChanged, rssChanged, brandingChanged, themeChanged, viewsChanged };
}

// ── M5: Side-effect phase — cache invalidation, WiFi, live reload ──

static void applyConfigSideEffects(const ConfigDiffResult &diff, bool langChanged, bool wikiLangChanged,
                                    bool wifiPrefChanged, bool wifiSetupModeChanged, int8_t wifiPrefIdx) {
  if (diff.weatherChanged) {
    g_weather.valid = false;
    g_weather.lastFetchMs = 0;
  }
  if (diff.rssChanged) {
    g_rss.valid = false; g_rss.itemCount = 0; g_rss.currentIndex = 0;
    g_rss.lastFetchMs = 0; g_rss.lastAttemptMs = 0; g_rss.lastRotateMs = 0; g_rss.lastHttpCode = 0;
    strncpy(g_rss.fetchedAt, "--/-- --:--", sizeof(g_rss.fetchedAt) - 1);
    g_rss.fetchedAt[sizeof(g_rss.fetchedAt) - 1] = '\0';
  }
  if (wikiLangChanged) {
    g_wiki.valid = false; g_wiki.itemCount = 0; g_wiki.currentIndex = 0;
    g_wiki.lastFetchMs = 0; g_wiki.lastAttemptMs = 0; g_wiki.lastRotateMs = 0;
    g_wiki.lastHttpCode = 0;
    strncpy(g_wiki.fetchedAt, "--/-- --:--", sizeof(g_wiki.fetchedAt) - 1);
    g_wiki.fetchedAt[sizeof(g_wiki.fetchedAt) - 1] = '\0';
  }
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI
  if (diff.themeChanged && g_lvglReady) lvglApplyThemeStyles(true);
#endif
#if TEST_NTP
  if (diff.weatherChanged || diff.rssChanged || diff.brandingChanged || diff.themeChanged || langChanged || wikiLangChanged || diff.viewsChanged) g_uiNeedsRedraw = true;
#endif
  if (wifiPrefChanged) {
    if (wifiPrefIdx >= 0) g_wifiSt.reconnectIdx = (uint8_t)wifiPrefIdx;
    else if (g_wifiSt.reconnectIdx >= g_wifiSt.credCount) g_wifiSt.reconnectIdx = 0;
    if (g_wifiSt.credCount > 0) {
      const bool wifiUp = wifiIsConnectedNow();
      bool alreadyOnTarget = false;
      if (wifiUp && wifiPrefIdx >= 0) alreadyOnTarget = WiFi.SSID().equals(g_wifiSt.credSsids[wifiPrefIdx]);
      if (!alreadyOnTarget) {
        wifiScheduleNextAttempt(millis(), 0UL);
        if (wifiUp) {
          g_wifiSt.internalDisconnect = true;
          WiFi.disconnect(true, false);
          delay(20);
          g_wifiSt.internalDisconnect = false;
        }
      }
    }
  }
  if (wifiSetupModeChanged) {
    normalizeWifiSetupMode();
    if (wifiSetupModeIsOff()) wifiStopSetupAp();
    else wifiHandleSetupModeLoop(millis());
  }
  if (diff.weatherChanged) (void)updateWeatherFromApi(true);
  if (diff.rssChanged) (void)updateRssFromFeed(true);
  if (wikiLangChanged) (void)updateWikiFromFeed(true);
  if (diff.viewsChanged && !uiPageEnabledNoEnsure(g_uiPageMode)) {
    setUiPage(uiLastEnabledMainViewNoEnsure());
  }
}

// ── M5: Orchestrator ──

static bool applyRuntimeConfigFromRequest(String &errorOut) {
  ensureRuntimeNetConfig();
  RuntimeNetConfig next = g_runtimeNetConfig;
  bool hasInput = false, langChanged = false, wikiLangChanged = false;
  bool wifiPrefChanged = false, wifiSetupModeChanged = false, wifiProvisioned = false;
  bool transitChanged = false;
  int8_t wifiPrefIdx = -1;

  if (!parseWeatherConfig(next, errorOut, hasInput)) return false;
  if (!parseRssFeedConfig(next, errorOut, hasInput)) return false;
  if (!parseLogoConfig(next, errorOut, hasInput)) return false;
  if (!parseThemeConfig(next, errorOut, hasInput)) return false;
  if (!parseViewsConfig(next, errorOut, hasInput)) return false;
  if (!parseWifiSetupModeConfig(errorOut, hasInput, wifiSetupModeChanged)) return false;
  if (!parseWifiCredentialConfig(errorOut, hasInput, wifiPrefChanged, wifiProvisioned, wifiPrefIdx)) return false;
  if (!parseWifiPreferredConfig(errorOut, hasInput, wifiProvisioned, wifiPrefChanged, wifiPrefIdx)) return false;
  if (!parseLangConfig(errorOut, hasInput, langChanged)) return false;
  if (!parseWikiLangConfig(errorOut, hasInput, wikiLangChanged)) return false;
  if (!parseTransitConfig(errorOut, hasInput, transitChanged)) return false;

  if (!hasInput) { errorOut = "nessun parametro"; return false; }

  const ConfigDiffResult diff = commitConfigToNvs(next);
  applyConfigSideEffects(diff, langChanged, wikiLangChanged, wifiPrefChanged, wifiSetupModeChanged, wifiPrefIdx);

  if (transitChanged) {
    g_transitState.valid = false;
    g_transitState.lastFetchMs = 0;
    g_transitState.count = 0;
    g_uiNeedsRedraw = true;
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI
    if (g_lvglReady && g_transitUi.station)
      lv_label_set_text(g_transitUi.station,
          g_transitConfig.station[0] ? g_transitConfig.station : "--");
    if (g_lvglReady && g_transitUi.noData)
      lv_label_set_text(g_transitUi.noData,
          g_transitConfig.configured ? "Loading..." : "Set station in web UI");
#endif
    if (g_transitConfig.configured) (void)updateTransitFromApi(true);
  }
  return true;
}

static const char *statusMessageFromCode(const String &status) {
  if (status == "saved") return "Configuration saved to NVS.";
  if (status == "reloaded") return "Weather / RSS / Wiki reload complete.";
  if (status == "invalid") return "Invalid request: please check the fields.";
  return "";
}

static void handleWebConfigRoot() {
  String msg;
  if (g_webCfg.server.hasArg("status")) msg = g_webCfg.server.arg("status");
  const String html = buildWebConfigPage(statusMessageFromCode(msg));
  g_webCfg.server.send(200, "text/html; charset=utf-8", html);
}

static void sendWebCaptiveRedirect() {
  char setupUrl[96] = "";
  wifiBuildSetupPortalUrl(setupUrl, sizeof(setupUrl));
  g_webCfg.server.sendHeader("Cache-Control", "no-store", true);
  g_webCfg.server.sendHeader("Location", setupUrl, true);
  g_webCfg.server.send(302, "text/plain", "");
}

static void handleWebCaptivePortalProbe() {
  sendWebCaptiveRedirect();
}

#if WEB_CONFIG_ENABLED
static void webConfigStartCaptiveDnsIfNeeded() {
  if (!g_wifiSt.setupApActive || g_webCfg.dnsStarted) return;
  if (!g_webCfg.dnsServer.start(53, "*", WiFi.softAPIP())) {
    Serial.println("[WEB][DNS][ERR] captive dns start failed");
    return;
  }
  g_webCfg.dnsStarted = true;
  Serial.printf("[WEB][DNS] captive resolver active on %s\n", WiFi.softAPIP().toString().c_str());
}

static void webConfigStopCaptiveDns() {
  if (!g_webCfg.dnsStarted) return;
  g_webCfg.dnsServer.stop();
  g_webCfg.dnsStarted = false;
  Serial.println("[WEB][DNS] captive resolver stopped");
}
#endif

static void handleWebConfigGet() {
  sendWebConfigJson(200, true);
}

static void handleWebWifiScanApi() {
  String out;
  out.reserve(2600);
  out += F("{\"ok\":true,\"only_24ghz\":true,\"networks\":[");

  const uint32_t scanStartMs = millis();
  constexpr uint32_t WIFI_SCAN_API_TIMEOUT_MS = 6500UL;

  const int prior = WiFi.scanComplete();
  if (prior == WIFI_SCAN_RUNNING) {
    WiFi.scanDelete();
    delay(20);
  } else if (prior >= 0 || prior == WIFI_SCAN_FAILED) {
    WiFi.scanDelete();
  }

  int found = WIFI_SCAN_FAILED;
  int startRc = WiFi.scanNetworks(true, true, false, 160);
  if (startRc == WIFI_SCAN_RUNNING) {
    while (true) {
      found = WiFi.scanComplete();
      if (found != WIFI_SCAN_RUNNING) break;
      if ((millis() - scanStartMs) > WIFI_SCAN_API_TIMEOUT_MS) {
        found = WIFI_SCAN_FAILED;
        break;
      }
#if WEB_CONFIG_ENABLED
      if (g_webCfg.dnsStarted) g_webCfg.dnsServer.processNextRequest();
#endif
      delay(35);
    }
  } else if (startRc >= 0) {
    found = startRc;
  } else {
    found = WIFI_SCAN_FAILED;
  }

  const bool scanTimedOut = (found == WIFI_SCAN_FAILED) && ((millis() - scanStartMs) > WIFI_SCAN_API_TIMEOUT_MS);
  Serial.printf("[WEB][WIFI] scan rc=%d elapsed=%lums mode=%d ap=%d sta=%d timeout=%d\n",
                found,
                (unsigned long)(millis() - scanStartMs),
                (int)WiFi.getMode(),
                g_wifiSt.setupApActive ? 1 : 0,
                wifiIsConnectedNow() ? 1 : 0,
                scanTimedOut ? 1 : 0);

  bool first = true;
  if (found > 0) {
    for (int i = 0; i < found; ++i) {
      const int ch = WiFi.channel(i);
      const String ssid = WiFi.SSID(i);
      if (ch < 1 || ch > 14) continue;
      if (ssid.length() == 0) continue;
      if (!first) out += ',';
      first = false;
      out += F("{\"ssid\":\"");
      appendJsonEscaped(out, ssid.c_str());
      out += F("\",\"rssi\":");
      out += WiFi.RSSI(i);
      out += F(",\"channel\":");
      out += ch;
      out += F(",\"secure\":");
      out += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? F("false") : F("true");
      out += '}';
    }
  }
  WiFi.scanDelete();
  if (scanTimedOut) out += F("],\"message\":\"scan_timeout\"}");
  else if (found == WIFI_SCAN_FAILED) out += F("],\"message\":\"scan_failed\"}");
  else out += F("]}");
  g_webCfg.server.send(200, "application/json", out);
}

static void handleWebWifiSetupQrSvgApi() {
  char setupUrl[96];
  wifiBuildSetupPortalUrl(setupUrl, sizeof(setupUrl));

#if DB_HAS_QRCODEGEN
  if (!ensureWebQrBuffers()) {
    g_webCfg.server.send(500, "text/plain", "QR buffers unavailable");
    return;
  }
  const bool ok = qrcodegen_encodeText(
      setupUrl,
      g_webCfg.qrTempBuf,
      g_webCfg.qrDataBuf,
      qrcodegen_Ecc_MEDIUM,
      1,
      8,
      qrcodegen_Mask_AUTO,
      true);
  if (!ok) {
    g_webCfg.server.send(500, "text/plain", "QR encode failed");
    return;
  }

  const int qrSize = qrcodegen_getSize(g_webCfg.qrDataBuf);
  const int border = 3;
  const int scale = 5;
  const int dim = (qrSize + border * 2) * scale;

  String svg;
  svg.reserve(18000);
  svg += F("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 ");
  svg += dim;
  svg += ' ';
  svg += dim;
  svg += F("' shape-rendering='crispEdges'>");
  svg += F("<rect width='100%' height='100%' fill='#ffffff'/>");
  for (int y = 0; y < qrSize; ++y) {
    for (int x = 0; x < qrSize; ++x) {
      if (!qrcodegen_getModule(g_webCfg.qrDataBuf, x, y)) continue;
      const int px = (x + border) * scale;
      const int py = (y + border) * scale;
      svg += F("<rect x='");
      svg += px;
      svg += F("' y='");
      svg += py;
      svg += F("' width='");
      svg += scale;
      svg += F("' height='");
      svg += scale;
      svg += F("' fill='#000000'/>");
    }
  }
  svg += F("</svg>");
  g_webCfg.server.send(200, "image/svg+xml", svg);
#else
  String fallback;
  fallback.reserve(256);
  fallback += F("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 360 80'><rect width='100%' height='100%' fill='#111827'/>");
  fallback += F("<text x='12' y='34' font-family='monospace' font-size='14' fill='#f9fafb'>QR encoder not available</text>");
  fallback += F("<text x='12' y='56' font-family='monospace' font-size='12' fill='#93c5fd'>");
  appendHtmlEscaped(fallback, setupUrl);
  fallback += F("</text></svg>");
  g_webCfg.server.send(200, "image/svg+xml", fallback);
#endif
}

static void handleWebConfigApplyApi() {
  String err;
  if (!applyRuntimeConfigFromRequest(err)) {
    sendWebConfigJson(400, false, err.c_str());
    return;
  }
  sendWebConfigJson(200, true, "updated");
}

static void handleWebConfigApplyForm() {
  String err;
  if (!applyRuntimeConfigFromRequest(err)) {
    webConfigRedirect("invalid");
    return;
  }
  webConfigRedirect("saved");
}

static bool webRequestHasConfigParams() {
  if (g_webCfg.server.hasArg("weather_city")) return true;
  if (g_webCfg.server.hasArg("weather_lat")) return true;
  if (g_webCfg.server.hasArg("weather_lon")) return true;
  if (g_webCfg.server.hasArg("wc_lang")) return true;
  if (g_webCfg.server.hasArg("wiki_lang")) return true;
  if (g_webCfg.server.hasArg("wifi_pref_ssid")) return true;
  if (g_webCfg.server.hasArg("wifi_setup_mode")) return true;
  if (g_webCfg.server.hasArg("wifi_new_ssid")) return true;
  if (g_webCfg.server.hasArg("wifi_new_password")) return true;
  if (g_webCfg.server.hasArg("ui_theme")) return true;
  if (g_webCfg.server.hasArg("view_info")) return true;
  if (g_webCfg.server.hasArg("view_aux")) return true;
  if (g_webCfg.server.hasArg("view_wiki")) return true;
  if (g_webCfg.server.hasArg("rss_feed_url")) return true;
  if (g_webCfg.server.hasArg("logo_url")) return true;
  if (g_webCfg.server.hasArg("transit_from")) return true;
  for (uint8_t i = 1; i <= RSS_FEED_SLOT_COUNT; ++i) {
    const String keyUrl = String("rss_feed_url_") + String(i);
    if (g_webCfg.server.hasArg(keyUrl)) return true;
  }
  return false;
}

static void handleWebReloadForm() {
  if (webRequestHasConfigParams()) {
    String err;
    if (!applyRuntimeConfigFromRequest(err)) {
      webConfigRedirect("invalid");
      return;
    }
  }
  netEnqueue(NET_REQ_WEATHER, 0);
  netEnqueue(NET_REQ_RSS, 0);
  netEnqueue(NET_REQ_WIKI, 0);
  g_uiNeedsRedraw = true;
  Serial.println("[WEB] reload queued (weather+rss+wiki)");
  webConfigRedirect("reloaded");
}

static void handleWebReloadApi() {
  if (webRequestHasConfigParams()) {
    String err;
    if (!applyRuntimeConfigFromRequest(err)) {
      sendWebConfigJson(400, false, err.c_str());
      return;
    }
  }
  netEnqueue(NET_REQ_WEATHER, 0);
  netEnqueue(NET_REQ_RSS, 0);
  netEnqueue(NET_REQ_WIKI, 0);
  sendWebConfigJson(200, true, "reload queued");
}

static void ensureWebConfigServerStarted() {
  ensureRuntimeNetConfig();
  const bool staUp = (WiFi.status() == WL_CONNECTED) && g_wifiSt.connected;
  if (!staUp && !g_wifiSt.setupApActive) {
    if (g_webCfg.dnsStarted) webConfigStopCaptiveDns();
    stopScryBarMdns();
    return;
  }
  if (g_webCfg.serverStarted) {
    if (g_wifiSt.setupApActive) webConfigStartCaptiveDnsIfNeeded();
    else if (g_webCfg.dnsStarted) webConfigStopCaptiveDns();
    if (staUp) ensureScryBarMdnsStarted();
    else stopScryBarMdns();
    return;
  }

  if (!g_webCfg.routesRegistered) {
    g_webCfg.server.on("/", HTTP_GET, handleWebConfigRoot);
    g_webCfg.server.on("/generate_204", HTTP_GET, handleWebCaptivePortalProbe);
    g_webCfg.server.on("/gen_204", HTTP_GET, handleWebCaptivePortalProbe);
    g_webCfg.server.on("/hotspot-detect.html", HTTP_GET, handleWebCaptivePortalProbe);
    g_webCfg.server.on("/connecttest.txt", HTTP_GET, handleWebCaptivePortalProbe);
    g_webCfg.server.on("/ncsi.txt", HTTP_GET, handleWebCaptivePortalProbe);
    g_webCfg.server.on("/fwlink", HTTP_GET, handleWebCaptivePortalProbe);
    g_webCfg.server.on("/success.txt", HTTP_GET, handleWebCaptivePortalProbe);
    g_webCfg.server.on("/config", HTTP_POST, handleWebConfigApplyForm);
    g_webCfg.server.on("/reload", HTTP_POST, handleWebReloadForm);
    g_webCfg.server.on("/api/config", HTTP_GET, handleWebConfigGet);
    g_webCfg.server.on("/api/config", HTTP_POST, handleWebConfigApplyApi);
    g_webCfg.server.on("/api/now-playing", HTTP_GET, handleWebNowPlayingGetApi);
    g_webCfg.server.on("/api/now-playing", HTTP_POST, handleWebNowPlayingPostApi);
    g_webCfg.server.on("/api/wifi/scan", HTTP_GET, handleWebWifiScanApi);
    g_webCfg.server.on("/api/wifi/setup-qr.svg", HTTP_GET, handleWebWifiSetupQrSvgApi);
    g_webCfg.server.on("/api/reload", HTTP_POST, handleWebReloadApi);
    g_webCfg.server.onNotFound([]() {
      if (g_wifiSt.setupApActive) {
        sendWebCaptiveRedirect();
        return;
      }
      g_webCfg.server.send(404, "text/plain", "Not found");
    });
    g_webCfg.routesRegistered = true;
  }

  g_webCfg.server.begin();
  g_webCfg.serverStarted = true;
  if (staUp) ensureScryBarMdnsStarted();
  if (staUp) {
    Serial.printf("[WEB] config ui ready (STA): http://%s:%u\n",
                  WiFi.localIP().toString().c_str(),
                  (unsigned)WEB_CONFIG_PORT);
  }
  if (g_wifiSt.setupApActive) {
    webConfigStartCaptiveDnsIfNeeded();
    Serial.printf("[WEB] config ui ready (AP): ssid='%s' url=http://%s:%u\n",
                  g_wifiSt.setupApSsid,
                  WiFi.softAPIP().toString().c_str(),
                  (unsigned)WEB_CONFIG_PORT);
  }
}

static void handleWebConfigServerLoop() {
  ensureWebConfigServerStarted();
  if (g_webCfg.dnsStarted) g_webCfg.dnsServer.processNextRequest();
  if (g_webCfg.serverStarted) g_webCfg.server.handleClient();
}
#else
static void ensureWebConfigServerStarted() {
  ensureRuntimeNetConfig();
}

static void handleWebConfigServerLoop() {}
#endif

static bool extractJsonNumberField(const char *json, const char *key, float &out) {
  if (!json || !key) return false;
  const char *p = json;
  while (true) {
    p = strstr(p, key);
    if (!p) return false;
    p += strlen(key);
    while (*p == ' ' || *p == '\t' || *p == ':' || *p == '\n' || *p == '\r') ++p;
    if (!*p) return false;
    if (*p == '"') { ++p; continue; }  // skip units string occurrence, keep searching numeric one
    char *endp = nullptr;
    const float val = strtof(p, &endp);
    if (!endp || endp == p) { ++p; continue; }
    out = val;
    return true;
  }
}

static bool extractJsonBoolFieldLoose(const String &json, const char *key, bool &out) {
  int pos = json.indexOf(key);
  if (pos < 0) return false;
  pos = json.indexOf(':', pos);
  if (pos < 0) return false;
  ++pos;
  while (pos < (int)json.length() &&
         (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) {
    ++pos;
  }
  if (pos >= (int)json.length()) return false;
  if (json.startsWith("true", pos)) {
    out = true;
    return true;
  }
  if (json.startsWith("false", pos)) {
    out = false;
    return true;
  }
  if (json[pos] == '1' || json[pos] == '0') {
    out = (json[pos] == '1');
    return true;
  }
  if (json[pos] != '"') return false;
  const int end = json.indexOf('"', pos + 1);
  if (end <= pos) return false;
  String token = json.substring(pos + 1, end);
  token.trim();
  return parseStrictBool(token, out);
}

static bool extractJsonArrayNumberAt(const char *json, const char *key, int index, float &out) {
  if (!json || !key || index < 0) return false;
  const char *keyP = strstr(json, key);
  if (!keyP) return false;
  const char *p = strchr(keyP, '[');
  if (!p) return false;
  ++p;

  for (int i = 0; i <= index; ++i) {
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') ++p;
    if (!*p || *p == ']') return false;
    const char *start = p;
    while (*p && *p != ',' && *p != ']') ++p;
    if (i == index) {
      const char *s = start;
      while (s < p && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t')) ++s;
      if (s == p) return false;
      char *endp = nullptr;
      out = strtof(s, &endp);
      return (endp && endp > s);
    }
    if (*p == ',') ++p;
  }
  return false;
}

static bool extractJsonArrayStringAt(const char *json, const char *key, int index, char *out, size_t outLen) {
  if (!json || !key || index < 0 || !out || outLen == 0) return false;
  const char *keyP = strstr(json, key);
  if (!keyP) return false;
  const char *p = strchr(keyP, '[');
  if (!p) return false;
  ++p;

  for (int i = 0; i <= index; ++i) {
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') ++p;
    if (!*p || *p == ']') return false;
    if (*p != '"') return false;
    ++p;
    const char *end = strchr(p, '"');
    if (!end) return false;
    if (i == index) {
      size_t len = (size_t)(end - p);
      if (len >= outLen) len = outLen - 1;
      memcpy(out, p, len);
      out[len] = '\0';
      return true;
    }
    p = end + 1;
    while (*p && *p != ',' && *p != ']') ++p;
    if (*p == ',') ++p;
  }
  return false;
}

static void isoToHhMm(const char *iso, char out[6]) {
  const size_t len = iso ? strlen(iso) : 0;
  if (len >= 16) {
    out[0] = iso[11];
    out[1] = iso[12];
    out[2] = ':';
    out[3] = iso[14];
    out[4] = iso[15];
    out[5] = '\0';
  } else {
    strcpy(out, "--:--");
  }
}

static const char* weatherCodeLabelIt(int code) {
  if (code == 0) return "Cielo sereno";
  if (code == 1) return "Prevalentemente sereno";
  if (code == 2) return "Parzialmente nuvoloso";
  if (code == 3) return "Coperto";
  if (code == 45) return "Nebbia";
  if (code == 48) return "Nebbia brinata";
  if (code == 51) return "Pioviggine debole";
  if (code == 53) return "Pioviggine moderata";
  if (code == 55) return "Pioviggine intensa";
  if (code == 56 || code == 57) return "Pioviggine gelata";
  if (code == 61) return "Pioggia debole";
  if (code == 63) return "Pioggia moderata";
  if (code == 65) return "Pioggia forte";
  if (code == 66 || code == 67) return "Pioggia gelata";
  if (code == 71) return "Nevicata debole";
  if (code == 73) return "Nevicata moderata";
  if (code == 75) return "Nevicata forte";
  if (code == 77) return "Granuli di neve";
  if (code == 80) return "Rovesci deboli";
  if (code == 81) return "Rovesci moderati";
  if (code == 82) return "Rovesci violenti";
  if (code == 85 || code == 86) return "Rovesci di neve";
  if (code == 95) return "Temporale";
  if (code == 96 || code == 99) return "Temporale con grandine";
  return "N/D";
}

// ---------------------------------------------------------------------------
// Weather label data tables — table-driven language dispatch (M2)
// ---------------------------------------------------------------------------

static const char* weatherCodeShortFromLabels(int code, const WeatherShortLabels* l) {
  if (code == 0 || code == 1) return l->clear;
  if (code == 2) return l->cloudy;
  if (code == 3) return l->overcast;
  if (code == 45 || code == 48) return l->fog;
  if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return l->rain;
  if (code >= 71 && code <= 77) return l->snow;
  if (code >= 95) return l->storm;
  return l->na;
}

static const WeatherShortLabels kWeatherShortIt       = {"Sereno",    "Nuvoloso",  "Coperto",   "Nebbia",     "Pioggia",  "Neve",     "Temporale",   "N/D"};
static const WeatherShortLabels kWeatherShortEn       = {"Clear",     "Cloudy",    "Overcast",  "Fog",        "Rain",     "Snow",     "Storm",       "N/A"};
static const WeatherShortLabels kWeatherShortFr       = {"Clair",     "Nuageux",   "Couvert",   "Brouillard", "Pluie",    "Neige",    "Orage",       "N/D"};
static const WeatherShortLabels kWeatherShortDe       = {"Klar",      "Bewoelkt",  "Bedeckt",   "Nebel",      "Regen",    "Schnee",   "Gewitter",    "N/V"};
static const WeatherShortLabels kWeatherShortEs       = {"Despejado", "Nublado",   "Cubierto",  "Niebla",     "Lluvia",   "Nieve",    "Tormenta",    "N/D"};
static const WeatherShortLabels kWeatherShortPt       = {"Limpo",     "Nublado",   "Encoberto", "Nevoeiro",   "Chuva",    "Neve",     "Temporal",    "N/D"};
static const WeatherShortLabels kWeatherShortLa       = {"Serenum",   "Nubilum",   "Opertum",   "Nebula",     "Imber",    "Nix",      "Procella",    "N/D"};
static const WeatherShortLabels kWeatherShortEo       = {"Klara",     "Nuba",      "Kovrita",   "Nebulo",     "Pluvo",    "Nego",     "Fulmotondro", "N/D"};
static const WeatherShortLabels kWeatherShortTlh      = {"muD QaQ",   "muD Hurgh", "muD Hurgh", "muD Duj",    "SIS",      "chuch",    "muD QeH",     "Duj"};
static const WeatherShortLabels kWeatherShortL33t     = {"CL34R",     "CL0UDY",    "0VCST",     "F09",        "R41N",     "5N0W",     "570RM",       "N/4"};
static const WeatherShortLabels kWeatherShortSha      = {"Faire",     "Cloudie",   "Overcast",  "Mist",       "Raineth",  "Snoweth",  "Tempest",     "N/A"};
static const WeatherShortLabels kWeatherShortVal      = {"Sunny!",    "Cloudy",    "Ugh Gray",  "Like Fog",   "Ugh Rain", "OMG Snow", "Storm!",      "N/A"};
static const WeatherShortLabels kWeatherShortBellazio = {"Sereno",    "Nuvoloso",  "Coperto",   "Nebbia",     "Pioggia",  "Neve",     "Temporale",   "N/D"};
static const WeatherShortLabels kWeatherShortPir      = {"Fair Winds","Gloomy",    "Grey Skies","Fog Bank",   "Squall",   "Blizzard", "Tempest",     "????"};

// ---------------------------------------------------------------------------
// Detailed WMO UI labels — indexed by WmoUiIdx
// ---------------------------------------------------------------------------

static int8_t wmoCodeToUiIdx(int code) {
  switch (code) {
    case 0:           return WMO_CLEAR;
    case 1:           return WMO_MAINLY_CLEAR;
    case 2:           return WMO_PARTLY_CLOUDY;
    case 3:           return WMO_OVERCAST;
    case 45:          return WMO_FOG;
    case 48:          return WMO_ICY_FOG;
    case 51:          return WMO_DRIZZLE_L;
    case 53:          return WMO_DRIZZLE_M;
    case 55:          return WMO_DRIZZLE_H;
    case 56: case 57: return WMO_FREEZE_DRIZZLE;
    case 61:          return WMO_RAIN_L;
    case 63:          return WMO_RAIN_M;
    case 65:          return WMO_RAIN_H;
    case 66: case 67: return WMO_FREEZE_RAIN;
    case 71:          return WMO_SNOW_L;
    case 73:          return WMO_SNOW_M;
    case 75:          return WMO_SNOW_H;
    case 77:          return WMO_SNOW_GRAINS;
    case 80:          return WMO_SHOWER_L;
    case 81:          return WMO_SHOWER_M;
    case 82:          return WMO_SHOWER_H;
    case 85: case 86: return WMO_SNOW_SHOWER;
    case 95:          return WMO_THUNDER;
    case 96: case 99: return WMO_HAIL;
    default:          return -1;
  }
}

static const char* weatherCodeUiLabelFromTable(int code, const char* const labels[], const char* na) {
  int8_t idx = wmoCodeToUiIdx(code);
  return (idx >= 0) ? labels[idx] : na;
}

// Per-language WMO UI label arrays (order matches WmoUiIdx)
static const char* const kWeatherUiIt[WMO_UI_COUNT] = {
  "Sereno","Sole prevalente","Parz. nuvoloso","Coperto",
  "Nebbia","Nebbia gelata",
  "Pioviggine","Pioviggia mod.","Pioviggia forte","Pioviggia gel.",
  "Pioggia debole","Pioggia mod.","Pioggia forte","Pioggia gelata",
  "Neve debole","Neve moderata","Neve forte","Granuli neve",
  "Rovesci deboli","Rovesci mod.","Rovesci forti","Rovesci neve",
  "Temporale","Temp. grandine"
};
static const char* const kWeatherUiEn[WMO_UI_COUNT] = {
  "Clear","Mainly clear","Partly cloudy","Overcast",
  "Fog","Icy fog",
  "Light drizzle","Mod. drizzle","Heavy drizzle","Freezing drizzle",
  "Light rain","Moderate rain","Heavy rain","Freezing rain",
  "Light snow","Moderate snow","Heavy snow","Snow grains",
  "Light showers","Mod. showers","Heavy showers","Snow showers",
  "Thunderstorm","Storm w/ hail"
};
static const char* const kWeatherUiFr[WMO_UI_COUNT] = {
  "Clair","Principalement clair","Part. nuageux","Couvert",
  "Brouillard","Brouillard glac.",
  "Bruine legere","Bruine mod.","Bruine forte","Bruine verglac.",
  "Pluie faible","Pluie mod.","Pluie forte","Pluie verglac.",
  "Neige faible","Neige mod.","Neige forte","Grains de neige",
  "Averses faibles","Averses mod.","Averses fortes","Averses de neige",
  "Orage","Orage avec grele"
};
static const char* const kWeatherUiDe[WMO_UI_COUNT] = {
  "Klar","Ueberwiegend klar","Teils bewoelkt","Bedeckt",
  "Nebel","Eisnebel",
  "Leichter Nieseln","Maess. Nieseln","Starkes Nieseln","Gefrierender Niesel",
  "Leichter Regen","Maess. Regen","Starker Regen","Gefrierender Regen",
  "Leichter Schnee","Maess. Schnee","Starker Schnee","Schneekristalle",
  "Leichte Schauer","Maess. Schauer","Starke Schauer","Schneeschauer",
  "Gewitter","Gewitter m. Hagel"
};
static const char* const kWeatherUiEs[WMO_UI_COUNT] = {
  "Despejado","Mainly despejado","Parc. nublado","Cubierto",
  "Niebla","Niebla helada",
  "Llovizna leve","Llovizna mod.","Llovizna fuerte","Llovizna helada",
  "Lluvia leve","Lluvia mod.","Lluvia fuerte","Lluvia helada",
  "Nieve leve","Nieve mod.","Nieve fuerte","Granulos nieve",
  "Chubascos leves","Chubascos mod.","Chubascos fuertes","Chubascos nieve",
  "Tormenta","Torm. con granizo"
};
static const char* const kWeatherUiPt[WMO_UI_COUNT] = {
  "Limpo","Principalmente limpo","Parc. nublado","Encoberto",
  "Nevoeiro","Nevoeiro gelado",
  "Chuvisco fraco","Chuvisco mod.","Chuvisco forte","Chuvisco gelado",
  "Chuva fraca","Chuva mod.","Chuva forte","Chuva gelada",
  "Neve fraca","Neve mod.","Neve forte","Graos de neve",
  "Aguaceiros fracos","Aguaceiros mod.","Aguaceiros fortes","Aguaceiros neve",
  "Temporal","Temp. com granizo"
};
static const char* const kWeatherUiLa[WMO_UI_COUNT] = {
  "Serenum","Fere serenum","Part. nubilum","Opertum",
  "Nebula","Nebula glacialis",
  "Pluvia levis","Pluvia mod.","Pluvia magna","Pluvia glacialis",
  "Imber levis","Imber mod.","Imber magnus","Imber glacialis",
  "Nix levis","Nix mod.","Nix magna","Grana nivis",
  "Imbres leves","Imbres mod.","Imbres magni","Imbres nivis",
  "Procella","Proc. cum grandine"
};
static const char* const kWeatherUiEo[WMO_UI_COUNT] = {
  "Klara","Cefe klara","Part. nuba","Kovrita",
  "Nebulo","Glacia nebulo",
  "Malpeza drizzle","Mod. drizzle","Peza drizzle","Glacia drizzle",
  "Malpeza pluvo","Mod. pluvo","Peza pluvo","Glacia pluvo",
  "Malpeza nego","Mod. nego","Peza nego","Negaj grenoj",
  "Malpezaj soversoj","Mod. soversoj","Pezaj soversoj","Negaj soversoj",
  "Fulmotondro","Fulmont. kun hajlo"
};
static const char* const kWeatherUiTlh[WMO_UI_COUNT] = {
  "muD QaQ","muD QaQ law'","muD Hurgh","muD Hurgh HoS",
  "muD Duj","muD chuch Duj",
  "SIS mach","SIS mod.","SIS HoS","SIS chuch",
  "bIQ mach","bIQ mod.","bIQ HoS","bIQ chuch",
  "chuch mach","chuch mod.","chuch HoS","chuch Hap",
  "SIS mach bIQ","SIS mod. bIQ","SIS HoS bIQ","SIS chuch bIQ",
  "muD QeH","muD QeH begh"
};
static const char* const kWeatherUiL33t[WMO_UI_COUNT] = {
  "CL34R 5KY","M41NLY CL34R","P4R7LY CL0UDY","0V3RC457",
  "F09","1CY F09",
  "L1GH7 DR1ZZL3","M0D DR1ZZL3","H34VY DR1ZZL3","FR33Z1N9 DR1ZZ",
  "L1GH7 R41N","M0D R41N","H34VY R41N","FR33Z1N9 R41N",
  "L1GH7 5N0W","M0D 5N0W","H34VY 5N0W","5N0W 9R41N5",
  "L1GH7 5H0W3R","M0D 5H0W3R","H34VY 5H0W3R","5N0W 5H0W3R",
  "7HuND3R570RM","570RM+H41L"
};
static const char* const kWeatherUiSha[WMO_UI_COUNT] = {
  "Faire skies","Mainly faire","Partly cloudie","Overcast",
  "Mist","Icy mist",
  "Light drizzle","Mod. drizzle","Heavy drizzle","Freezing driz.",
  "Light rain","Moderate rain","Heavy rain","Freezing rain",
  "Light snoweth","Mod. snoweth","Heavy snoweth","Snow grains",
  "Light showers","Mod. showers","Heavy showers","Snow showers",
  "Thunderstorm","Storm+hail"
};
static const char* const kWeatherUiVal[WMO_UI_COUNT] = {
  "Totally Sunny","Like Sunny","Kinda Cloudy","So Overcast",
  "Like Foggy","Icy Fog Ew",
  "Light Drizzle","Some Drizzle","Heavy Drizzle","Freezing Rain",
  "Light Rain","Moderate Rain","Heavy Rain","Freezing Rain",
  "Light Snow","Like Snow","Heavy Snow!","Snow Grains",
  "Light Shower","Mod. Shower","Heavy Shower","Snow Shower",
  "Thunderstorm","Storm+Hail"
};
static const char* const kWeatherUiBellazio[WMO_UI_COUNT] = {
  "Sereno pieno","Sole, tipo","Un po' nuv.","Tutto coperto",
  "Nebbia ugh","Nebbia gelata",
  "Pioggerella","Piovigg. mid","Pioggia forte","Pioggia ghiac.",
  "Pioggia lieve","Pioggia boh","Pioggia forte","Pioggia gel.",
  "Neve lowkey","Neve mod.","Neve fr fr","Granuli neve",
  "Rovesci lievi","Rovesci mid","Rovesci forti","Rovesci neve",
  "Temporale!","Temp.+grandine"
};
static const char* const kWeatherUiPir[WMO_UI_COUNT] = {
  "Clear skies, smooth sailin'!","Mostly clear, fine day to plunder!","Partly cloudy, eyes peeled!","Grey as Davy Jones' locker!",
  "Fog bank rollin' in!","Icy fog — the sea be cursed!",
  "Light drizzle on the poop deck","Drizzle comin' down steady","Heavy drizzle, batten down!","Freezin' drizzle — riggin's icin'!",
  "Light rain, nothin' fer a pirate","Rain like cannonballs!","Heavy rain — bilge pumps!","Freezin' rain — ship be glazed!",
  "Light snow on the quarterdeck","Snow thick as stolen gold","Blizzard on the high seas!","Snow grains peltin' the crew",
  "Light showers, spit from the sky","Showers blowin' sideways!","Deluge — all hands below!","Snow mixin' with the squall",
  "Thunderin' tempest — all hands!","Tempest with cannonball hail!"
};

// ---------------------------------------------------------------------------
// LangVtable — table-driven language dispatch (M2)
// ---------------------------------------------------------------------------

// Forward declarations for per-language word clock and date functions
// (defined later in the file; function pointers resolved at link time)
static void composeWordClockSentenceIt(const tm&, char*, size_t);
static void composeWordClockSentenceTlh(const tm&, char*, size_t);
static void composeWordClockSentenceEn(const tm&, char*, size_t);
static void composeWordClockSentenceFr(const tm&, char*, size_t);
static void composeWordClockSentenceDe(const tm&, char*, size_t);
static void composeWordClockSentenceEs(const tm&, char*, size_t);
static void composeWordClockSentencePt(const tm&, char*, size_t);
static void composeWordClockSentenceLa(const tm&, char*, size_t);
static void composeWordClockSentenceEo(const tm&, char*, size_t);
static void composeWordClockSentenceL33t(const tm&, char*, size_t);
static void composeWordClockSentenceSha(const tm&, char*, size_t);
static void composeWordClockSentenceVal(const tm&, char*, size_t);
static void composeWordClockSentenceBellazio(const tm&, char*, size_t);
static void composeWordClockSentencePir(const tm&, char*, size_t);
static void formatDateIt(const tm&, char*, size_t);
static void formatDateEn(const tm&, char*, size_t);
static void formatDateFr(const tm&, char*, size_t);
static void formatDateDe(const tm&, char*, size_t);
static void formatDateEs(const tm&, char*, size_t);
static void formatDatePt(const tm&, char*, size_t);
static void formatDateLa(const tm&, char*, size_t);
static void formatDateEo(const tm&, char*, size_t);
static void formatDateTlh(const tm&, char*, size_t);
static void formatDateL33t(const tm&, char*, size_t);
static void formatDateSha(const tm&, char*, size_t);
static void formatDateVal(const tm&, char*, size_t);
static void formatDateBellazio(const tm&, char*, size_t);
static void formatDatePir(const tm&, char*, size_t);

static const LangVtable kLangTable[] = {
  {"it",       composeWordClockSentenceIt,       &kWeatherShortIt,       kWeatherUiIt,       "N/D", formatDateIt,       &kUiLang_it},
  {"en",       composeWordClockSentenceEn,       &kWeatherShortEn,       kWeatherUiEn,       "N/A", formatDateEn,       &kUiLang_en},
  {"fr",       composeWordClockSentenceFr,       &kWeatherShortFr,       kWeatherUiFr,       "N/D", formatDateFr,       &kUiLang_fr},
  {"de",       composeWordClockSentenceDe,       &kWeatherShortDe,       kWeatherUiDe,       "N/V", formatDateDe,       &kUiLang_de},
  {"es",       composeWordClockSentenceEs,       &kWeatherShortEs,       kWeatherUiEs,       "N/D", formatDateEs,       &kUiLang_es},
  {"pt",       composeWordClockSentencePt,       &kWeatherShortPt,       kWeatherUiPt,       "N/D", formatDatePt,       &kUiLang_pt},
  {"la",       composeWordClockSentenceLa,       &kWeatherShortLa,       kWeatherUiLa,       "N/D", formatDateLa,       &kUiLang_la},
  {"eo",       composeWordClockSentenceEo,       &kWeatherShortEo,       kWeatherUiEo,       "N/D", formatDateEo,       &kUiLang_eo},
  {"tlh",      composeWordClockSentenceTlh,      &kWeatherShortTlh,      kWeatherUiTlh,      "Duj", formatDateTlh,      &kUiLang_tlh},
  {"l33t",     composeWordClockSentenceL33t,     &kWeatherShortL33t,     kWeatherUiL33t,     "N/4", formatDateL33t,     &kUiLang_l33t},
  {"sha",      composeWordClockSentenceSha,      &kWeatherShortSha,      kWeatherUiSha,      "N/A", formatDateSha,      &kUiLang_sha},
  {"val",      composeWordClockSentenceVal,      &kWeatherShortVal,      kWeatherUiVal,      "N/A", formatDateVal,      &kUiLang_val},
  {"bellazio", composeWordClockSentenceBellazio, &kWeatherShortBellazio, kWeatherUiBellazio, "N/D", formatDateBellazio, &kUiLang_bellazio},
  {"pir",      composeWordClockSentencePir,      &kWeatherShortPir,      kWeatherUiPir,      "????", formatDatePir,      &kUiLang_pir},
};
static constexpr size_t kLangCount = sizeof(kLangTable) / sizeof(kLangTable[0]);

static const LangVtable* findLangVtable() {
  for (size_t i = 0; i < kLangCount; ++i) {
    if (strcmp(g_wordClockLang, kLangTable[i].code) == 0) return &kLangTable[i];
  }
  return &kLangTable[0]; // default = Italian
}

// Slim dispatcher wrappers — same API as before, now backed by vtable lookup
static const char* weatherCodeUiLabel(int code) {
  const LangVtable* v = findLangVtable();
  return weatherCodeUiLabelFromTable(code, v->weatherUi, v->weatherUiNa);
}

static const char* weatherCodeShort(int code) {
  const LangVtable* v = findLangVtable();
  return weatherCodeShortFromLabels(code, v->weatherShort);
}

static void appendAsciiFoldedCodepoint(String &out, uint32_t cp) {
  if (cp == 0) return;
  if (cp < 0x20U) { out += ' '; return; }
  if (cp < 0x80U) { out += (char)cp; return; }

  switch (cp) {
    case 0x00A0: out += ' '; return;     // NBSP
    case 0x00AB: case 0x00BB: out += '"'; return;
    case 0x2010: case 0x2011: case 0x2012: case 0x2013: case 0x2014: case 0x2015: out += '-'; return;
    case 0x2018: case 0x2019: case 0x201A: case 0x201B: case 0x2032: out += '\''; return;
    case 0x201C: case 0x201D: case 0x201E: case 0x201F: case 0x2033: out += '"'; return;
    case 0x2022: case 0x00B7: case 0x2219: out += '*'; return;
    case 0x2026: out += "..."; return;
    case 0x202F: case 0x2009: case 0x200A: case 0x2002: case 0x2003: out += ' '; return;
    case 0x200B: case 0x200C: case 0x200D: case 0xFEFF: return; // zero-width
    case 0x2044: out += '/'; return;
    case 0x20AC: out += "EUR"; return;
    case 0x00A3: out += "GBP"; return;
    case 0x00A5: out += "YEN"; return;
    case 0x00A2: out += "cent"; return;
    case 0x00A9: out += "(c)"; return;
    case 0x00AE: out += "(r)"; return;
    case 0x2122: out += "(tm)"; return;
    case 0x00DF: out += "ss"; return;
    case 0x00C6: out += "AE"; return;
    case 0x00E6: out += "ae"; return;
    case 0x0152: out += "OE"; return;
    case 0x0153: out += "oe"; return;
    case 0x00D0: out += "D"; return;
    case 0x00F0: out += "d"; return;
    case 0x00DE: out += "TH"; return;
    case 0x00FE: out += "th"; return;
  }

  switch (cp) {
    case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3: case 0x00C4: case 0x00C5:
    case 0x0100: case 0x0102: case 0x0104: out += 'A'; return;
    case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E3: case 0x00E4: case 0x00E5:
    case 0x0101: case 0x0103: case 0x0105: out += 'a'; return;
    case 0x00C7: case 0x0106: case 0x0108: case 0x010A: case 0x010C: out += 'C'; return;
    case 0x00E7: case 0x0107: case 0x0109: case 0x010B: case 0x010D: out += 'c'; return;
    case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB:
    case 0x0112: case 0x0114: case 0x0116: case 0x0118: case 0x011A: out += 'E'; return;
    case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB:
    case 0x0113: case 0x0115: case 0x0117: case 0x0119: case 0x011B: out += 'e'; return;
    case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF:
    case 0x0128: case 0x012A: case 0x012C: case 0x012E: case 0x0130: out += 'I'; return;
    case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF:
    case 0x0129: case 0x012B: case 0x012D: case 0x012F: case 0x0131: out += 'i'; return;
    case 0x00D1: case 0x0143: case 0x0145: case 0x0147: out += 'N'; return;
    case 0x00F1: case 0x0144: case 0x0146: case 0x0148: out += 'n'; return;
    case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5: case 0x00D6: case 0x00D8:
    case 0x014C: case 0x014E: case 0x0150: out += 'O'; return;
    case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F5: case 0x00F6: case 0x00F8:
    case 0x014D: case 0x014F: case 0x0151: out += 'o'; return;
    case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC:
    case 0x0168: case 0x016A: case 0x016C: case 0x016E: case 0x0170: case 0x0172: out += 'U'; return;
    case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC:
    case 0x0169: case 0x016B: case 0x016D: case 0x016F: case 0x0171: case 0x0173: out += 'u'; return;
    case 0x00DD: case 0x0176: case 0x0178: out += 'Y'; return;
    case 0x00FD: case 0x00FF: case 0x0177: out += 'y'; return;
    case 0x015A: case 0x015C: case 0x015E: case 0x0160: out += 'S'; return;
    case 0x015B: case 0x015D: case 0x015F: case 0x0161: out += 's'; return;
    case 0x0179: case 0x017B: case 0x017D: out += 'Z'; return;
    case 0x017A: case 0x017C: case 0x017E: out += 'z'; return;
  }

  out += ' ';
}

static bool decodeNextUtf8Codepoint(const String &in, int &idx, uint32_t &cp) {
  const int len = (int)in.length();
  if (idx >= len) return false;
  const uint8_t b0 = (uint8_t)in[idx];
  if (b0 < 0x80U) { cp = b0; ++idx; return true; }
  if ((b0 & 0xE0U) == 0xC0U && (idx + 1) < len) {
    const uint8_t b1 = (uint8_t)in[idx + 1];
    if ((b1 & 0xC0U) == 0x80U) {
      cp = (uint32_t)(b0 & 0x1FU) << 6;
      cp |= (uint32_t)(b1 & 0x3FU);
      idx += 2;
      return true;
    }
  } else if ((b0 & 0xF0U) == 0xE0U && (idx + 2) < len) {
    const uint8_t b1 = (uint8_t)in[idx + 1];
    const uint8_t b2 = (uint8_t)in[idx + 2];
    if (((b1 & 0xC0U) == 0x80U) && ((b2 & 0xC0U) == 0x80U)) {
      cp = (uint32_t)(b0 & 0x0FU) << 12;
      cp |= (uint32_t)(b1 & 0x3FU) << 6;
      cp |= (uint32_t)(b2 & 0x3FU);
      idx += 3;
      return true;
    }
  } else if ((b0 & 0xF8U) == 0xF0U && (idx + 3) < len) {
    const uint8_t b1 = (uint8_t)in[idx + 1];
    const uint8_t b2 = (uint8_t)in[idx + 2];
    const uint8_t b3 = (uint8_t)in[idx + 3];
    if (((b1 & 0xC0U) == 0x80U) && ((b2 & 0xC0U) == 0x80U) && ((b3 & 0xC0U) == 0x80U)) {
      cp = (uint32_t)(b0 & 0x07U) << 18;
      cp |= (uint32_t)(b1 & 0x3FU) << 12;
      cp |= (uint32_t)(b2 & 0x3FU) << 6;
      cp |= (uint32_t)(b3 & 0x3FU);
      idx += 4;
      return true;
    }
  }
  cp = b0;
  ++idx;
  return false;
}

static void foldUtf8ToAscii(String &text) {
  String out;
  out.reserve(text.length() + 8);
  int i = 0;
  while (i < (int)text.length()) {
    uint32_t cp = 0;
    (void)decodeNextUtf8Codepoint(text, i, cp);
    appendAsciiFoldedCodepoint(out, cp);
  }
  text = out;
}

static bool htmlNamedEntityToCodepoint(const String &entityRaw, uint32_t &cpOut) {
  String entity(entityRaw);
  entity.toLowerCase();
  if (entity == "amp") { cpOut = '&'; return true; }
  if (entity == "quot") { cpOut = '"'; return true; }
  if (entity == "apos") { cpOut = '\''; return true; }
  if (entity == "lt") { cpOut = '<'; return true; }
  if (entity == "gt") { cpOut = '>'; return true; }
  if (entity == "nbsp") { cpOut = 0x00A0; return true; }
  if (entity == "copy") { cpOut = 0x00A9; return true; }
  if (entity == "reg") { cpOut = 0x00AE; return true; }
  if (entity == "trade") { cpOut = 0x2122; return true; }
  if (entity == "hellip") { cpOut = 0x2026; return true; }
  if (entity == "bull") { cpOut = 0x2022; return true; }
  if (entity == "middot") { cpOut = 0x00B7; return true; }
  if (entity == "ndash") { cpOut = 0x2013; return true; }
  if (entity == "mdash") { cpOut = 0x2014; return true; }
  if (entity == "lsquo") { cpOut = 0x2018; return true; }
  if (entity == "rsquo") { cpOut = 0x2019; return true; }
  if (entity == "ldquo") { cpOut = 0x201C; return true; }
  if (entity == "rdquo") { cpOut = 0x201D; return true; }
  if (entity == "laquo") { cpOut = 0x00AB; return true; }
  if (entity == "raquo") { cpOut = 0x00BB; return true; }
  if (entity == "euro") { cpOut = 0x20AC; return true; }
  if (entity == "pound") { cpOut = 0x00A3; return true; }
  if (entity == "yen") { cpOut = 0x00A5; return true; }
  if (entity == "cent") { cpOut = 0x00A2; return true; }
  if (entity == "deg") { cpOut = 0x00B0; return true; }
  if (entity == "aacute") { cpOut = 0x00E1; return true; }
  if (entity == "agrave") { cpOut = 0x00E0; return true; }
  if (entity == "acirc") { cpOut = 0x00E2; return true; }
  if (entity == "atilde") { cpOut = 0x00E3; return true; }
  if (entity == "auml") { cpOut = 0x00E4; return true; }
  if (entity == "aring") { cpOut = 0x00E5; return true; }
  if (entity == "eacute") { cpOut = 0x00E9; return true; }
  if (entity == "egrave") { cpOut = 0x00E8; return true; }
  if (entity == "ecirc") { cpOut = 0x00EA; return true; }
  if (entity == "euml") { cpOut = 0x00EB; return true; }
  if (entity == "iacute") { cpOut = 0x00ED; return true; }
  if (entity == "igrave") { cpOut = 0x00EC; return true; }
  if (entity == "icirc") { cpOut = 0x00EE; return true; }
  if (entity == "iuml") { cpOut = 0x00EF; return true; }
  if (entity == "oacute") { cpOut = 0x00F3; return true; }
  if (entity == "ograve") { cpOut = 0x00F2; return true; }
  if (entity == "ocirc") { cpOut = 0x00F4; return true; }
  if (entity == "otilde") { cpOut = 0x00F5; return true; }
  if (entity == "ouml") { cpOut = 0x00F6; return true; }
  if (entity == "uacute") { cpOut = 0x00FA; return true; }
  if (entity == "ugrave") { cpOut = 0x00F9; return true; }
  if (entity == "ucirc") { cpOut = 0x00FB; return true; }
  if (entity == "uuml") { cpOut = 0x00FC; return true; }
  if (entity == "ntilde") { cpOut = 0x00F1; return true; }
  if (entity == "ccedil") { cpOut = 0x00E7; return true; }
  if (entity == "yacute") { cpOut = 0x00FD; return true; }
  if (entity == "yuml") { cpOut = 0x00FF; return true; }
  if (entity == "szlig") { cpOut = 0x00DF; return true; }
  if (entity == "aelig") { cpOut = 0x00E6; return true; }
  if (entity == "oelig") { cpOut = 0x0153; return true; }
  return false;
}

static void decodeHtmlEntitiesToAscii(String &text) {
  String out;
  out.reserve(text.length() + 8);
  for (int i = 0; i < (int)text.length(); ++i) {
    const char c = text[i];
    if (c != '&') {
      out += c;
      continue;
    }
    int semi = text.indexOf(';', i + 1);
    if (semi < 0 || (semi - i) > 14) {
      out += '&';
      continue;
    }
    const String entity = text.substring(i + 1, semi);
    uint32_t cp = 0;
    bool ok = false;
    if (entity.startsWith("#x") || entity.startsWith("#X")) {
      char *endPtr = nullptr;
      const unsigned long v = strtoul(entity.substring(2).c_str(), &endPtr, 16);
      if (endPtr && *endPtr == '\0') { cp = (uint32_t)v; ok = true; }
    } else if (entity.startsWith("#")) {
      char *endPtr = nullptr;
      const unsigned long v = strtoul(entity.substring(1).c_str(), &endPtr, 10);
      if (endPtr && *endPtr == '\0') { cp = (uint32_t)v; ok = true; }
    } else {
      ok = htmlNamedEntityToCodepoint(entity, cp);
    }
    if (ok) {
      appendAsciiFoldedCodepoint(out, cp);
      i = semi;
      continue;
    }
    out += '&';
    out += entity;
    out += ';';
    i = semi;
  }
  text = out;
}

static void sanitizeAsciiBuffer(char *buf, size_t bufLen) {
  if (!buf || bufLen == 0 || buf[0] == '\0') return;
  String tmp(buf);
  decodeHtmlEntitiesToAscii(tmp);
  foldUtf8ToAscii(tmp);
  strncpy(buf, tmp.c_str(), bufLen - 1);
  buf[bufLen - 1] = '\0';
}

#if RSS_ENABLED
static void stripHtmlTags(String &text) {
  String out;
  out.reserve(text.length());
  bool inTag = false;
  for (int i = 0; i < (int)text.length(); ++i) {
    const char c = text[i];
    if (c == '<') {
      inTag = true;
      if (out.length() > 0 && out[out.length() - 1] != ' ') out += ' ';
      continue;
    }
    if (c == '>') {
      inTag = false;
      continue;
    }
    if (!inTag) out += c;
  }
  text = out;
}

static void normalizeRssText(String &text) {
  String cleaned = text;
  decodeHtmlEntitiesToAscii(cleaned);
  stripHtmlTags(cleaned);
  decodeHtmlEntitiesToAscii(cleaned);
  stripHtmlTags(cleaned);
  foldUtf8ToAscii(cleaned);

  String collapsed;
  collapsed.reserve(cleaned.length());
  bool prevSpace = true;
  for (int i = 0; i < (int)cleaned.length(); ++i) {
    char c = cleaned[i];
    if ((uint8_t)c <= 0x20U) c = ' ';
    const bool isSpace = (c == ' ');
    if (isSpace) {
      if (!prevSpace) collapsed += ' ';
    } else {
      collapsed += c;
    }
    prevSpace = isSpace;
  }
  collapsed.trim();
  text = collapsed;
}

static bool extractXmlTagText(const String &xml, const char *tag, String &out) {
  const String openTag = String("<") + tag + ">";
  const String closeTag = String("</") + tag + ">";
  int t0 = xml.indexOf(openTag);
  if (t0 < 0) {
    const String openTagAttr = String("<") + tag + " ";
    t0 = xml.indexOf(openTagAttr);
    if (t0 < 0) return false;
    t0 = xml.indexOf('>', t0);
    if (t0 < 0) return false;
    t0 += 1;
  } else {
    t0 += (int)openTag.length();
  }
  const int t1 = xml.indexOf(closeTag, t0);
  if (t1 < 0) return false;
  out = xml.substring(t0, t1);
  out.trim();
  if (out.startsWith("<![CDATA[")) {
    out.remove(0, 9);
    const int cdataEnd = out.indexOf("]]>");
    if (cdataEnd >= 0) out.remove(cdataEnd);
  }
  out.trim();
  return out.length() > 0;
}

static void decodeHtmlMarkupEntities(String &text) {
  text.replace("&lt;", "<");
  text.replace("&gt;", ">");
  text.replace("&quot;", "\"");
  text.replace("&apos;", "'");
  text.replace("&#39;", "'");
  text.replace("&amp;", "&");
}

static bool extractFirstWikiArticleHrefFromHtml(const String &html, String &out) {
  int cursor = 0;
  while (true) {
    int href0 = html.indexOf("href=", cursor);
    if (href0 < 0) return false;
    href0 += 5;
    if (href0 >= (int)html.length()) return false;
    const char quote = html.charAt(href0);
    if (quote != '"' && quote != '\'') {
      cursor = href0;
      continue;
    }
    ++href0;
    const int href1 = html.indexOf(quote, href0);
    if (href1 <= href0) return false;
    String href = html.substring(href0, href1);
    href.trim();
    if (href.startsWith("/wiki/") &&
        !href.startsWith("/wiki/Special:") &&
        !href.startsWith("/wiki/File:") &&
        !href.startsWith("/wiki/Help:") &&
        !href.startsWith("/wiki/Template:") &&
        !href.startsWith("/wiki/Wikipedia:")) {
      out = href;
      return true;
    }
    if ((href.startsWith("https://") || href.startsWith("http://")) &&
        href.indexOf(".wikipedia.org/wiki/") > 0 &&
        href.indexOf("/wiki/Special:") < 0 &&
        href.indexOf("/wiki/File:") < 0) {
      out = href;
      return true;
    }
    cursor = href1 + 1;
  }
}

static bool resolveWikipediaArticleUrlFromDescription(const String &descriptionRaw, const String &feedItemLink, String &outUrl) {
  String html = descriptionRaw;
  decodeHtmlMarkupEntities(html);
  String href;
  if (!extractFirstWikiArticleHrefFromHtml(html, href)) return false;
  if (href.startsWith("http://") || href.startsWith("https://")) {
    outUrl = href;
    return true;
  }
  if (!href.startsWith("/")) return false;

  const int scheme = feedItemLink.indexOf("://");
  const int hostStart = (scheme >= 0) ? (scheme + 3) : 0;
  const int hostEnd = feedItemLink.indexOf('/', hostStart);
  if (hostEnd <= hostStart) return false;
  const String origin = feedItemLink.substring(0, hostEnd);
  outUrl = origin + href;
  return true;
}

static uint8_t parseRssItems(const String &xml, RssItem *items, uint8_t maxItems) {
  uint8_t count = 0;
  int cursor = 0;
  while (count < maxItems) {
    int itemStart = xml.indexOf("<item", cursor);
    if (itemStart < 0) break;
    itemStart = xml.indexOf('>', itemStart);
    if (itemStart < 0) break;
    itemStart += 1;
    const int itemEnd = xml.indexOf("</item>", itemStart);
    if (itemEnd < 0) break;
    const String itemXml = xml.substring(itemStart, itemEnd);
    cursor = itemEnd + 7;

    String title, link, pubDate, summary;
    if (!extractXmlTagText(itemXml, "title", title)) continue;
    (void)extractXmlTagText(itemXml, "link", link);
    (void)extractXmlTagText(itemXml, "pubDate", pubDate);
    if (!extractXmlTagText(itemXml, "description", summary)) {
      if (!extractXmlTagText(itemXml, "content:encoded", summary)) {
        (void)extractXmlTagText(itemXml, "summary", summary);
      }
    }
    const String descriptionRaw = summary;
    normalizeRssText(title);
    normalizeRssText(pubDate);
    normalizeRssText(summary);
    if (summary.length() > 0 && title.length() > 0) {
      String t = title;
      String s = summary;
      t.toLowerCase();
      s.toLowerCase();
      if (s.startsWith(t)) {
        summary.remove(0, title.length());
        summary.trim();
        if (summary.startsWith("-")) {
          summary.remove(0, 1);
          summary.trim();
        }
      }
    }
    link.trim();
    link.replace("&amp;", "&");
    link.replace("&quot;", "\"");
    link.replace("&apos;", "'");
    link.replace("&#39;", "'");
    if (link.indexOf("/wiki/Special:FeedItem/") >= 0) {
      String articleUrl;
      if (resolveWikipediaArticleUrlFromDescription(descriptionRaw, link, articleUrl)) {
        link = articleUrl;
      }
    }
    strncpy(items[count].title, title.c_str(), sizeof(items[count].title) - 1);
    items[count].title[sizeof(items[count].title) - 1] = '\0';
    strncpy(items[count].link, link.c_str(), sizeof(items[count].link) - 1);
    items[count].link[sizeof(items[count].link) - 1] = '\0';
    strncpy(items[count].pubDate, pubDate.c_str(), sizeof(items[count].pubDate) - 1);
    items[count].pubDate[sizeof(items[count].pubDate) - 1] = '\0';
    strncpy(items[count].summary, summary.c_str(), sizeof(items[count].summary) - 1);
    items[count].summary[sizeof(items[count].summary) - 1] = '\0';
    items[count].wikiMetaReady = false;
    items[count].wikiMetaTried = false;
    ++count;
  }
  return count;
}

static bool extractJsonStringFieldLoose(const String &json, const char *key, String &out) {
  int p = json.indexOf(key);
  if (p < 0) return false;
  p = json.indexOf(':', p);
  if (p < 0) return false;
  ++p;
  while (p < (int)json.length() && (json[p] == ' ' || json[p] == '\t' || json[p] == '\n' || json[p] == '\r')) ++p;
  if (p >= (int)json.length() || json[p] != '"') return false;
  ++p;
  int end = p;
  while (end < (int)json.length()) {
    if (json[end] == '"' && json[end - 1] != '\\') break;
    ++end;
  }
  if (end <= p || end >= (int)json.length()) return false;
  out = json.substring(p, end);
  out.replace("\\/", "/");
  out.replace("\\\"", "\"");
  out.replace("\\\\", "\\");
  out.trim();
  return out.length() > 0;
}

static String jsonEscape(const String &in) {
  String out;
  out.reserve(in.length() + 8);
  for (int i = 0; i < (int)in.length(); ++i) {
    const char c = in[i];
    if (c == '\\' || c == '"') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else {
      out += c;
    }
  }
  return out;
}

#if WEB_CONFIG_ENABLED
static bool applyNowPlayingPayloadJson(const String &body, String &err) {
  String title;
  String artist;
  String album;
  String source;
  String appName;
  String artworkUrl;
  String artworkId;
  String artworkRgb565B64;
  float durationSecF = 0.0f;
  float elapsedSecF = 0.0f;
  float artworkWidthF = 0.0f;
  float artworkHeightF = 0.0f;
  bool isPlaying = true;
  bool inSync = true;

  if (!extractJsonStringFieldLoose(body, "\"title\"", title) || title.length() == 0) {
    err = "Missing required field: title";
    return false;
  }
  (void)extractJsonStringFieldLoose(body, "\"artist\"", artist);
  (void)extractJsonStringFieldLoose(body, "\"album\"", album);
  (void)extractJsonStringFieldLoose(body, "\"source\"", source);
  (void)extractJsonStringFieldLoose(body, "\"appName\"", appName);
  const bool hasArtworkUrl = extractJsonStringFieldLoose(body, "\"artworkURL\"", artworkUrl);
  bool hasArtworkId = extractJsonStringFieldLoose(body, "\"artworkID\"", artworkId);
  const bool hasArtworkBase64 = extractJsonStringFieldLoose(body, "\"artworkRGB565B64\"", artworkRgb565B64);
  (void)extractJsonBoolFieldLoose(body, "\"isPlaying\"", isPlaying);
  (void)extractJsonBoolFieldLoose(body, "\"inSync\"", inSync);
  (void)extractJsonNumberField(body.c_str(), "\"durationSec\"", durationSecF);
  (void)extractJsonNumberField(body.c_str(), "\"elapsedSec\"", elapsedSecF);
  (void)extractJsonNumberField(body.c_str(), "\"artworkWidth\"", artworkWidthF);
  (void)extractJsonNumberField(body.c_str(), "\"artworkHeight\"", artworkHeightF);

  if (!hasArtworkId && hasArtworkUrl && artworkUrl.length() > 0) {
    artworkId = artworkUrl;
    hasArtworkId = true;
  }

  if (durationSecF < 0.0f) durationSecF = 0.0f;
  if (elapsedSecF < 0.0f) elapsedSecF = 0.0f;
  if (durationSecF > 65535.0f) durationSecF = 65535.0f;
  if (elapsedSecF > 65535.0f) elapsedSecF = 65535.0f;

  uint8_t *decodedArtwork = nullptr;
  size_t decodedArtworkSize = 0u;
  const uint16_t artworkWidth = (artworkWidthF > 0.0f) ? (uint16_t)lroundf(artworkWidthF) : 0u;
  const uint16_t artworkHeight = (artworkHeightF > 0.0f) ? (uint16_t)lroundf(artworkHeightF) : 0u;
  const bool currentArtworkMatches = hasArtworkId && (strcmp(g_liveNowPlaying.artworkId, artworkId.c_str()) == 0);
  bool installArtwork = false;
  bool keepArtwork = false;
  bool clearArtwork = false;

  auto releaseDecodedArtwork = [&]() {
    if (decodedArtwork) {
      free(decodedArtwork);
      decodedArtwork = nullptr;
      decodedArtworkSize = 0u;
    }
  };

  if (hasArtworkBase64) {
    if (!hasArtworkId || artworkId.length() == 0) {
      err = "Artwork payload missing artworkID";
      return false;
    }
    if (!decodeNowPlayingArtworkBase64(artworkRgb565B64, artworkWidth, artworkHeight,
                                       &decodedArtwork, decodedArtworkSize, err)) {
      return false;
    }
    installArtwork = true;
  } else if (hasArtworkId && currentArtworkMatches && g_liveNowPlayingArtwork.valid) {
    keepArtwork = true;
  } else if (!hasArtworkId && g_liveNowPlayingArtwork.valid &&
             g_liveNowPlaying.valid &&
             strcmp(g_liveNowPlaying.title, title.c_str()) == 0 &&
             strcmp(g_liveNowPlaying.artist, artist.c_str()) == 0) {
    // Same track but artworkID missing in this update (e.g. pause) — keep existing artwork
    keepArtwork = true;
    artworkId = g_liveNowPlaying.artworkId;
    hasArtworkId = true;
  } else if (!hasArtworkUrl) {
    clearArtwork = true;
  } else if (hasArtworkId && !currentArtworkMatches) {
    clearArtwork = true;
  }

  LiveNowPlayingState next = {};
  next.valid = true;
  next.isPlaying = isPlaying;
  next.inSync = inSync;
  next.receivedAtMs = millis();
  next.durationSec = (uint16_t)lroundf(durationSecF);
  next.elapsedSec = (uint16_t)lroundf(elapsedSecF);
  if (next.durationSec > 0 && next.elapsedSec > next.durationSec) next.elapsedSec = next.durationSec;
  copyStringSafe(next.title, sizeof(next.title), title.c_str());
  copyStringSafe(next.artist, sizeof(next.artist), artist.c_str());
  copyStringSafe(next.album, sizeof(next.album), album.c_str());
  copyStringSafe(next.source, sizeof(next.source), source.c_str());
  copyStringSafe(next.appName, sizeof(next.appName), appName.c_str());
  copyStringSafe(next.artworkUrl, sizeof(next.artworkUrl), artworkUrl.c_str());
  if (installArtwork || keepArtwork) {
    copyStringSafe(next.artworkId, sizeof(next.artworkId), artworkId.c_str());
  }

  const bool contentChanged =
      !g_liveNowPlaying.valid ||
      strcmp(g_liveNowPlaying.title, next.title) != 0 ||
      strcmp(g_liveNowPlaying.artist, next.artist) != 0 ||
      strcmp(g_liveNowPlaying.album, next.album) != 0 ||
      strcmp(g_liveNowPlaying.source, next.source) != 0 ||
      strcmp(g_liveNowPlaying.appName, next.appName) != 0 ||
      strcmp(g_liveNowPlaying.artworkUrl, next.artworkUrl) != 0 ||
      strcmp(g_liveNowPlaying.artworkId, next.artworkId) != 0 ||
      g_liveNowPlaying.durationSec != next.durationSec ||
      g_liveNowPlaying.isPlaying != next.isPlaying;
  if (contentChanged) {
    ++g_liveNowPlayingTokenSeq;
    if (g_liveNowPlayingTokenSeq == 0) ++g_liveNowPlayingTokenSeq;
    next.contentToken = g_liveNowPlayingTokenSeq;
  } else {
    next.contentToken = g_liveNowPlaying.contentToken;
  }

  if (installArtwork) {
    installLiveNowPlayingArtwork(decodedArtwork, decodedArtworkSize, artworkWidth, artworkHeight, artworkId.c_str());
    decodedArtwork = nullptr;
    decodedArtworkSize = 0u;
  } else if (clearArtwork) {
    clearLiveNowPlayingArtwork();
  }
  g_liveNowPlaying = next;
#if TEST_NTP
  g_uiNeedsRedraw = true;
#endif
  Serial.printf("[NOWPLAYING][API] title='%s' artist='%s' source='%s' elapsed=%us duration=%us\n",
                g_liveNowPlaying.title,
                g_liveNowPlaying.artist,
                liveNowPlayingSourceLabel(),
                (unsigned)g_liveNowPlaying.elapsedSec,
                (unsigned)g_liveNowPlaying.durationSec);
  releaseDecodedArtwork();
  return true;
}

static void handleWebNowPlayingGetApi() {
  const uint32_t nowMs = millis();
  const bool active = liveNowPlayingAvailable();
  const bool displaySync = liveNowPlayingDisplayInSync(nowMs);
  String out;
  out.reserve(1200);
  out += F("{\"ok\":true,\"service\":{\"type\":\"_scrybar._tcp\",\"port\":");
  out += (unsigned)WEB_CONFIG_PORT;
  out += F(",\"path\":\"/api/now-playing\"");
#if DB_HAS_MDNS
  if (g_webCfg.mdnsHost[0]) {
    out += F(",\"host\":\"");
    appendJsonEscaped(out, g_webCfg.mdnsHost);
    out += '"';
  }
  if (g_webCfg.mdnsInstanceName[0]) {
    out += F(",\"instance\":\"");
    appendJsonEscaped(out, g_webCfg.mdnsInstanceName);
    out += '"';
  }
#endif
  out += F("},\"nowPlaying\":{\"active\":");
  out += active ? F("true") : F("false");
  if (active) {
    out += F(",\"title\":\"");
    appendJsonEscaped(out, g_liveNowPlaying.title);
    out += F("\",\"artist\":\"");
    appendJsonEscaped(out, g_liveNowPlaying.artist);
    out += F("\",\"album\":\"");
    appendJsonEscaped(out, g_liveNowPlaying.album);
    out += F("\",\"source\":\"");
    appendJsonEscaped(out, g_liveNowPlaying.source);
    out += F("\",\"appName\":\"");
    appendJsonEscaped(out, g_liveNowPlaying.appName);
    out += F("\",\"durationSec\":");
    out += (unsigned)g_liveNowPlaying.durationSec;
    out += F(",\"elapsedSec\":");
    out += (unsigned)g_liveNowPlaying.elapsedSec;
    out += F(",\"isPlaying\":");
    out += g_liveNowPlaying.isPlaying ? F("true") : F("false");
    out += F(",\"inSync\":");
    out += g_liveNowPlaying.inSync ? F("true") : F("false");
    out += F(",\"displayInSync\":");
    out += displaySync ? F("true") : F("false");
    out += F(",\"ageMs\":");
    out += (unsigned long)(nowMs - g_liveNowPlaying.receivedAtMs);
    if (g_liveNowPlaying.artworkUrl[0]) {
      out += F(",\"artworkURL\":\"");
      appendJsonEscaped(out, g_liveNowPlaying.artworkUrl);
      out += '"';
    }
    if (g_liveNowPlaying.artworkId[0]) {
      out += F(",\"artworkID\":\"");
      appendJsonEscaped(out, g_liveNowPlaying.artworkId);
      out += '"';
    }
    if (g_liveNowPlayingArtwork.valid) {
      out += F(",\"artworkWidth\":");
      out += (unsigned)g_liveNowPlayingArtwork.width;
      out += F(",\"artworkHeight\":");
      out += (unsigned)g_liveNowPlayingArtwork.height;
    }
  }
  out += F("}}");
  g_webCfg.server.sendHeader("Cache-Control", "no-store", true);
  g_webCfg.server.send(200, "application/json", out);
}

static void handleWebNowPlayingPostApi() {
  const String body = g_webCfg.server.arg("plain");
  if (body.length() == 0) {
    g_webCfg.server.send(400, "application/json", "{\"ok\":false,\"message\":\"Empty JSON body\"}");
    return;
  }

  String err;
  if (!applyNowPlayingPayloadJson(body, err)) {
    String out;
    out.reserve(160);
    out += F("{\"ok\":false,\"message\":\"");
    appendJsonEscaped(out, err.c_str());
    out += F("\"}");
    g_webCfg.server.send(400, "application/json", out);
    return;
  }

  String out;
  out.reserve(200);
  out += F("{\"ok\":true,\"message\":\"updated\",\"inSync\":");
  out += liveNowPlayingDisplayInSync(millis()) ? F("true") : F("false");
  out += F(",\"source\":\"");
  appendJsonEscaped(out, liveNowPlayingSourceLabel());
  out += F("\"}");
  g_webCfg.server.sendHeader("Cache-Control", "no-store", true);
  g_webCfg.server.send(200, "application/json", out);
}
#endif

// ---------- PSRAM-based TLS allocator ----------
// ESP32 Arduino 3.x compiles mbedtls with CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC,
// forcing all TLS buffers into internal DRAM (~52KB free).  The 16KB+16KB
// default I/O buffers + handshake temporaries exceed what's available,
// causing "SSL - Memory allocation failed".
//
// Fix: redirect mbedtls allocations to PSRAM (7.8MB free) for the duration
// of each HTTPS request, then restore the default internal allocator.
static void *psramCalloc(size_t n, size_t size) {
  void *p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = calloc(n, size);  // fallback to internal if PSRAM somehow fails
  return p;
}
static void psramFree(void *ptr) { free(ptr); }

// Replicate the default ESP internal allocator (same as esp_mbedtls_mem_calloc)
// to avoid C/C++ linkage issues with the pre-compiled ESP-IDF symbol.
static void *internalCalloc(size_t n, size_t size) {
  return heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
static void internalFree(void *ptr) { free(ptr); }

// RAII guard: redirect mbedtls to PSRAM on construction, restore on destruction.
struct ScopedPsramTls {
  bool active;
  ScopedPsramTls() : active(!g_netTaskReady) {
    if (active) mbedtls_platform_set_calloc_free(psramCalloc, psramFree);
  }
  ~ScopedPsramTls() {
    if (active) mbedtls_platform_set_calloc_free(internalCalloc, internalFree);
  }
};

static void buildRssWhenLabel(const char *pubDate, char *out, size_t outLen) {
  if (!out || outLen == 0) return;
  out[0] = '\0';
  if (!pubDate || !pubDate[0]) {
    strncpy(out, "--/-- --:--", outLen - 1);
    out[outLen - 1] = '\0';
    return;
  }
  char wd[12] = {0}, mon[8] = {0}, tz[12] = {0};
  int day = 0, year = 0, hh = 0, mm = 0, ss = 0;
  if (sscanf(pubDate, "%11[^,], %d %7s %d %d:%d:%d %11s", wd, &day, mon, &year, &hh, &mm, &ss, tz) >= 7) {
    snprintf(out, outLen, "%02d %s %02d:%02d", day, mon, hh, mm);
    return;
  }
  String p(pubDate);
  p.trim();
  if (p.length() > 22) p = p.substring(0, 22) + "...";
  snprintf(out, outLen, "%s", p.c_str());
}

static bool extractJsonObjectFieldLoose(const String &json, const char *key, String &out) {
  int p = json.indexOf(key);
  if (p < 0) return false;
  p = json.indexOf('{', p);
  if (p < 0) return false;
  int depth = 0;
  bool inString = false;
  bool escape = false;
  for (int i = p; i < (int)json.length(); ++i) {
    const char c = json[i];
    if (inString) {
      if (escape) {
        escape = false;
      } else if (c == '\\') {
        escape = true;
      } else if (c == '"') {
        inString = false;
      }
      continue;
    }
    if (c == '"') {
      inString = true;
      continue;
    }
    if (c == '{') {
      ++depth;
      continue;
    }
    if (c == '}') {
      --depth;
      if (depth == 0) {
        out = json.substring(p, i + 1);
        return out.length() > 0;
      }
    }
  }
  return false;
}

static bool rssExtractWikipediaArticleRef(const char *url, String &hostOut, String &titlePathOut) {
  if (!url || !url[0]) return false;
  String full(url);
  int scheme = full.indexOf("://");
  int hostStart = (scheme >= 0) ? (scheme + 3) : 0;
  int hostEnd = full.indexOf('/', hostStart);
  if (hostEnd < 0) return false;
  String host = full.substring(hostStart, hostEnd);
  host.toLowerCase();
  if (host.startsWith("www.")) host.remove(0, 4);
  if (!host.endsWith(".wikipedia.org")) return false;
  String path = full.substring(hostEnd);
  int q = path.indexOf('?');
  if (q >= 0) path = path.substring(0, q);
  int h = path.indexOf('#');
  if (h >= 0) path = path.substring(0, h);
  if (!path.startsWith("/wiki/")) return false;
  String titlePath = path.substring(6);
  titlePath.trim();
  if (titlePath.length() == 0) return false;
  if (titlePath.indexOf(':') >= 0) return false;
  hostOut = host;
  titlePathOut = titlePath;
  return true;
}

static bool buildHttpDowngradeUrl(const char *srcUrl, char *out, size_t outLen) {
  if (!srcUrl || !out || outLen == 0) return false;
  if (strncmp(srcUrl, "https://", 8) != 0) return false;
  snprintf(out, outLen, "http://%s", srcUrl + 8);
  out[outLen - 1] = '\0';
  return true;
}

static bool rssFetchWikipediaSummaryMeta(const char *articleUrl, String &outSummary) {
  outSummary = "";
  if (WiFi.status() != WL_CONNECTED || !g_wifiSt.connected) return false;
  ScopedPsramTls psramTls;  // redirect mbedtls allocations to PSRAM

  String wikiHost, wikiTitlePath;
  if (!rssExtractWikipediaArticleRef(articleUrl, wikiHost, wikiTitlePath)) return false;

  const String apiUrl = String("https://") + wikiHost + "/api/rest_v1/page/summary/" + wikiTitlePath;
  char httpFallback[256];
  const char *requestUrl = apiUrl.c_str();
  bool triedHttpFallback = false;

  while (true) {
    HTTPClient http;
    http.setConnectTimeout(RSS_HTTP_TIMEOUT_MS);
    http.setTimeout(RSS_HTTP_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    bool beginOk = false;
    WiFiClientSecure tls;
    if (strncmp(requestUrl, "https://", 8) == 0) {
      tls.setInsecure();

      tls.setHandshakeTimeout((RSS_HTTP_TIMEOUT_MS + 999U) / 1000U);
      beginOk = http.begin(tls, requestUrl);
    } else {
      beginOk = http.begin(requestUrl);
    }
    if (!beginOk) {
      if (!triedHttpFallback && buildHttpDowngradeUrl(requestUrl, httpFallback, sizeof(httpFallback))) {
        triedHttpFallback = true;
        requestUrl = httpFallback;
        continue;
      }
      return false;
    }
    http.addHeader("Accept", "application/json");
    http.addHeader("User-Agent", "ScryBar/1.0 (ESP32)");
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
      http.end();
      if (code < 0 && !triedHttpFallback && buildHttpDowngradeUrl(requestUrl, httpFallback, sizeof(httpFallback))) {
        triedHttpFallback = true;
        requestUrl = httpFallback;
        continue;
      }
      return false;
    }
    const String body = http.getString();
    http.end();
    if (body.length() == 0) return false;

    String summary;
    if (extractJsonStringFieldLoose(body, "\"extract\"", summary)) {
      normalizeRssText(summary);
      if (summary.length() > 0) outSummary = summary;
    }
    return outSummary.length() > 0;
  }
}

static bool rssTryEnrichItemWikipediaMeta(RssItem &item) {
  if (!item.link[0]) return false;
  if (item.wikiMetaTried && item.wikiMetaReady) return false;
  if (item.summary[0]) {
    item.wikiMetaReady = true;
    item.wikiMetaTried = true;
    return false;
  }

  item.wikiMetaTried = true;
  String summary;
  if (!rssFetchWikipediaSummaryMeta(item.link, summary)) return false;

  bool changed = false;
  if (!item.summary[0] && summary.length() > 0) {
    strncpy(item.summary, summary.c_str(), sizeof(item.summary) - 1);
    item.summary[sizeof(item.summary) - 1] = '\0';
    changed = true;
  }
  item.wikiMetaReady = (item.summary[0] != 0);
  return changed;
}

static void extractRssHost(const char *url, char *outHost, size_t outLen) {
  if (!outHost || outLen == 0) return;
  outHost[0] = '\0';
  if (!url || !url[0]) {
    strncpy(outHost, "unknown", outLen - 1);
    outHost[outLen - 1] = '\0';
    return;
  }
  const char *start = strstr(url, "://");
  start = start ? (start + 3) : url;
  size_t n = 0;
  while (start[n] && start[n] != '/' && start[n] != '?' && start[n] != '#' && n + 1 < outLen) {
    outHost[n] = (char)tolower((unsigned char)start[n]);
    ++n;
  }
  outHost[n] = '\0';
  char *port = strchr(outHost, ':');
  if (port) *port = '\0';
  if (strncmp(outHost, "www.", 4) == 0) {
    memmove(outHost, outHost + 4, strlen(outHost + 4) + 1);
  }
  if (!outHost[0]) {
    strncpy(outHost, "unknown", outLen - 1);
    outHost[outLen - 1] = '\0';
  }
}

static void buildRssSiteShortName(const char *host, char *out, size_t outLen) {
  if (!out || outLen == 0) return;
  out[0] = '\0';
  if (!host || !host[0]) {
    strncpy(out, "WEB", outLen - 1);
    out[outLen - 1] = '\0';
    return;
  }

  char tmp[96];
  strncpy(tmp, host, sizeof(tmp) - 1);
  tmp[sizeof(tmp) - 1] = '\0';

  char *parts[8] = {nullptr};
  uint8_t count = 0;
  char *tok = strtok(tmp, ".");
  while (tok && count < 8) {
    parts[count++] = tok;
    tok = strtok(nullptr, ".");
  }

  const char *base = host;
  if (count >= 2) {
    uint8_t idx = count - 2;
    if (count >= 3) {
      const size_t tldLen = strlen(parts[count - 1]);
      const size_t sldLen = strlen(parts[count - 2]);
      if (tldLen == 2 && sldLen <= 3) idx = count - 3;
    }
    base = parts[idx];
  } else if (count == 1) {
    base = parts[0];
  }

  size_t j = 0;
  for (size_t i = 0; base[i] && j + 1 < outLen && j < 10; ++i) {
    const char c = base[i];
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      out[j++] = (char)toupper((unsigned char)c);
    }
  }
  if (j == 0) {
    strncpy(out, "WEB", outLen - 1);
    out[outLen - 1] = '\0';
  } else {
    out[j] = '\0';
  }
}

// ── Favicon cache — Google Favicon API → RGB565 via pngle ────────────────────
static constexpr uint8_t  kFaviconCacheSlots = 8;
static constexpr uint16_t kFaviconSize       = 32;  // 32×32 px from Google API
static constexpr size_t   kFaviconBytes      = kFaviconSize * kFaviconSize * 2;  // RGB565

struct FaviconCacheEntry {
  char     host[64]       = {0};
  uint8_t *rgb565         = nullptr;  // PSRAM-allocated, kFaviconBytes
  lv_img_dsc_t imgDsc     = {};
  uint32_t lastAccessMs   = 0;        // LRU: updated on every cache hit
  bool     valid          = false;
};
static FaviconCacheEntry g_faviconCache[kFaviconCacheSlots];

/// pngle user context for streaming decode into RGB565 buffer.
struct PngleRgb565Ctx {
  uint8_t *buf;
  uint16_t bufW;   // output buffer width (kFaviconSize)
  uint16_t bufH;   // output buffer height (kFaviconSize)
  int16_t  offX;   // centering offset — set after PNG header parsed
  int16_t  offY;
  bool     inited; // true after init callback sets offsets
};

static void pngleOnInit(pngle_t *pngle, uint32_t imgW, uint32_t imgH) {
  auto *ctx = (PngleRgb565Ctx *)pngle_get_user_data(pngle);
  if (!ctx) return;
  ctx->offX = (int16_t)((ctx->bufW > imgW) ? (ctx->bufW - imgW) / 2 : 0);
  ctx->offY = (int16_t)((ctx->bufH > imgH) ? (ctx->bufH - imgH) / 2 : 0);
  ctx->inited = true;
  Serial.printf("[FAV] png %ux%u → offset +%d,+%d in %ux%u\n",
                imgW, imgH, ctx->offX, ctx->offY, ctx->bufW, ctx->bufH);
}

static void pngleOnDraw(pngle_t *pngle, uint32_t x, uint32_t y,
                         uint32_t w, uint32_t h,
                         const uint8_t rgba[4]) {
  auto *ctx = (PngleRgb565Ctx *)pngle_get_user_data(pngle);
  if (!ctx || !ctx->buf) return;
  const uint32_t dx = x + (uint32_t)ctx->offX;
  const uint32_t dy = y + (uint32_t)ctx->offY;
  if (dx >= ctx->bufW || dy >= ctx->bufH) return;
  const uint8_t a = rgba[3];
  if (a == 0) return;  // fully transparent → leave as background
  // Alpha-blend onto white background (favicons are designed for light bg)
  uint8_t r = rgba[0], g = rgba[1], b = rgba[2];
  if (a < 255) {
    r = (uint8_t)((r * a + 255 * (255 - a)) / 255);
    g = (uint8_t)((g * a + 255 * (255 - a)) / 255);
    b = (uint8_t)((b * a + 255 * (255 - a)) / 255);
  }
  const uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
  const size_t off = ((size_t)dy * ctx->bufW + dx) * 2;
  // LV_COLOR_16_SWAP=1 → store big-endian (high byte first)
  ctx->buf[off]     = (uint8_t)(rgb565 >> 8);
  ctx->buf[off + 1] = (uint8_t)(rgb565 & 0xFF);
}

/// Find or allocate a cache slot for `host`.  Returns slot index.
/// LRU eviction: when full, evicts the least-recently-accessed entry in-place
/// (no memmove — LVGL may hold pointers to lv_img_dsc_t inside entries).
static uint8_t faviconCacheSlot(const char *host) {
  // Check existing (hit → touch access time)
  for (uint8_t i = 0; i < kFaviconCacheSlots; ++i) {
    if (g_faviconCache[i].valid && strcmp(g_faviconCache[i].host, host) == 0) {
      g_faviconCache[i].lastAccessMs = millis();
      return i;
    }
  }
  // Find empty
  for (uint8_t i = 0; i < kFaviconCacheSlots; ++i) {
    if (!g_faviconCache[i].valid) return i;
  }
  // Evict LRU (smallest lastAccessMs)
  uint8_t lruIdx = 0;
  for (uint8_t i = 1; i < kFaviconCacheSlots; ++i) {
    if (g_faviconCache[i].lastAccessMs < g_faviconCache[lruIdx].lastAccessMs) lruIdx = i;
  }
  Serial.printf("[FAV] LRU evict slot %u host=%s (age %lums)\n",
                lruIdx, g_faviconCache[lruIdx].host,
                (unsigned long)(millis() - g_faviconCache[lruIdx].lastAccessMs));
  if (g_faviconCache[lruIdx].rgb565) heap_caps_free(g_faviconCache[lruIdx].rgb565);
  g_faviconCache[lruIdx] = FaviconCacheEntry{};
  return lruIdx;
}

/// Download favicon from Google API, decode PNG → RGB565, cache in PSRAM.
/// Returns pointer to lv_img_dsc_t on success, nullptr on failure.
static const lv_img_dsc_t *faviconFetchAndCache(const char *host) {
  if (!host || !host[0]) return nullptr;

  // Already cached? → touch LRU timestamp
  for (uint8_t i = 0; i < kFaviconCacheSlots; ++i) {
    if (g_faviconCache[i].valid && strcmp(g_faviconCache[i].host, host) == 0) {
      g_faviconCache[i].lastAccessMs = millis();
      return &g_faviconCache[i].imgDsc;
    }
  }

  // Build Google Favicon API URL
  char url[192];
  snprintf(url, sizeof(url),
           "https://www.google.com/s2/favicons?domain=%s&sz=%u", host, kFaviconSize);
  Serial.printf("[FAV] fetch %s\n", url);

  // Allocate RGB565 buffer in PSRAM, fill white (default bg for transparent pixels)
  uint8_t *rgb565Buf = (uint8_t *)heap_caps_malloc(kFaviconBytes, MALLOC_CAP_SPIRAM);
  if (!rgb565Buf) {
    Serial.println("[FAV] PSRAM alloc failed");
    return nullptr;
  }
  // White in RGB565 big-endian (LV_COLOR_16_SWAP=1): 0xFFFF
  memset(rgb565Buf, 0xFF, kFaviconBytes);

  // HTTP GET
  ScopedPsramTls psramTls;
  WiFiClientSecure tls;
  tls.setInsecure();
  tls.setHandshakeTimeout(5);
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(tls, url)) {
    Serial.println("[FAV] http begin fail");
    heap_caps_free(rgb565Buf);
    return nullptr;
  }
  int code = http.GET();
  if (code != 200) {
    Serial.printf("[FAV] http %d\n", code);
    http.end();
    heap_caps_free(rgb565Buf);
    return nullptr;
  }

  // Stream-decode PNG via pngle
  PngleRgb565Ctx ctx = { rgb565Buf, kFaviconSize, kFaviconSize, 0, 0, false };
  pngle_t *pngle = pngle_new();
  if (!pngle) {
    http.end();
    heap_caps_free(rgb565Buf);
    return nullptr;
  }
  pngle_set_user_data(pngle, &ctx);
  pngle_set_init_callback(pngle, pngleOnInit);
  pngle_set_draw_callback(pngle, pngleOnDraw);

  WiFiClient *stream = http.getStreamPtr();
  uint8_t chunk[256];
  bool decodeOk = true;
  while (http.connected() && stream->available()) {
    int n = stream->readBytes(chunk, sizeof(chunk));
    if (n <= 0) break;
    if (pngle_feed(pngle, chunk, n) < 0) {
      Serial.printf("[FAV] pngle error: %s\n", pngle_error(pngle));
      decodeOk = false;
      break;
    }
  }
  pngle_destroy(pngle);
  http.end();

  if (!decodeOk) {
    heap_caps_free(rgb565Buf);
    return nullptr;
  }

  // Store in cache
  uint8_t slot = faviconCacheSlot(host);
  auto &entry = g_faviconCache[slot];
  if (entry.rgb565 && entry.rgb565 != rgb565Buf) heap_caps_free(entry.rgb565);
  entry.valid = true;
  entry.lastAccessMs = millis();
  copyStringSafe(entry.host, sizeof(entry.host), host);
  entry.rgb565 = rgb565Buf;
  entry.imgDsc.header.cf = LV_IMG_CF_TRUE_COLOR;
  entry.imgDsc.header.always_zero = 0;
  entry.imgDsc.header.reserved = 0;
  entry.imgDsc.header.w = kFaviconSize;
  entry.imgDsc.header.h = kFaviconSize;
  entry.imgDsc.data_size = kFaviconBytes;
  entry.imgDsc.data = rgb565Buf;

  Serial.printf("[FAV] cached slot %u host=%s\n", slot, host);
  return &entry.imgDsc;
}

static void buildRssSiteBadge(const char *siteShort, char *out, size_t outLen) {
  if (!out || outLen == 0) return;
  out[0] = '\0';
  if (!siteShort || !siteShort[0]) {
    strncpy(out, "WEB", outLen - 1);
    out[outLen - 1] = '\0';
    return;
  }
  size_t n = 0;
  while (siteShort[n] && n < 4 && n + 1 < outLen) {
    out[n] = siteShort[n];
    ++n;
  }
  out[n] = '\0';
}

static uint32_t rssSiteColorHexFromHost(const char *host) {
  if (!host || !host[0]) return 0x2B468E;
  uint32_t h = 2166136261UL;
  for (size_t i = 0; host[i]; ++i) {
    h ^= (uint8_t)host[i];
    h *= 16777619UL;
  }
  static const uint32_t palette[] = {
      0x1F3B87, 0x274B96, 0x3558A0, 0x2D5F8F, 0x3D4FA3, 0x2A4E7A};
  return palette[h % (sizeof(palette) / sizeof(palette[0]))];
}

static uint8_t fetchRssItemsFromUrl(const char *feedUrl, RssItem *outItems, uint8_t cap, int *httpCodeOut) {
  if (httpCodeOut) *httpCodeOut = -1;
  if (!feedUrl || !feedUrl[0] || !outItems || cap == 0) return 0;
  ScopedPsramTls psramTls;  // redirect mbedtls allocations to PSRAM
  char httpFallback[RSS_FEED_URL_LEN];
  const char *requestUrl = feedUrl;
  bool triedHttpFallback = false;

  while (true) {
    HTTPClient http;
    http.setConnectTimeout(RSS_HTTP_TIMEOUT_MS);
    http.setTimeout(RSS_HTTP_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    bool beginOk = false;
    WiFiClientSecure tls;
    if (strncmp(requestUrl, "https://", 8) == 0) {
      tls.setInsecure();

      tls.setHandshakeTimeout((RSS_HTTP_TIMEOUT_MS + 999U) / 1000U);
      beginOk = http.begin(tls, requestUrl);
    } else {
      beginOk = http.begin(requestUrl);
    }
    if (!beginOk) {
      Serial.printf("[RSS][HTTP] begin failed feed=%s heap=%u psram=%u\n",
                    requestUrl,
                    (unsigned)ESP.getFreeHeap(),
                    (unsigned)ESP.getFreePsram());
      if (!triedHttpFallback && buildHttpDowngradeUrl(requestUrl, httpFallback, sizeof(httpFallback))) {
        triedHttpFallback = true;
        requestUrl = httpFallback;
        continue;
      }
      return 0;
    }

    http.addHeader("Accept", "application/rss+xml, application/xml, text/xml, application/atom+xml;q=0.9, */*;q=0.1");
    http.addHeader("User-Agent", "ScryBar/1.0 (ESP32)");

    const int code = http.GET();
    if (httpCodeOut) *httpCodeOut = code;
    if (code != HTTP_CODE_OK) {
      Serial.printf("[RSS][HTTP] GET fail feed=%s code=%d heap=%u psram=%u\n",
                    requestUrl,
                    code,
                    (unsigned)ESP.getFreeHeap(),
                    (unsigned)ESP.getFreePsram());
      http.end();
      if (code < 0 && !triedHttpFallback && buildHttpDowngradeUrl(requestUrl, httpFallback, sizeof(httpFallback))) {
        triedHttpFallback = true;
        requestUrl = httpFallback;
        continue;
      }
      return 0;
    }

    String payload = http.getString();
    http.end();
    if (payload.length() <= 0) return 0;
    const uint8_t parsed = parseRssItems(payload, outItems, cap);
    if (parsed == 0) {
      String preview = payload;
      preview.replace('\n', ' ');
      preview.replace('\r', ' ');
      if (preview.length() > 160) preview = preview.substring(0, 160) + "...";
      Serial.printf("[RSS][PARSE] zero-items url=%s code=%d preview=%s\n", requestUrl, code, preview.c_str());
    }
    return parsed;
  }
}

static RssItem *ensureRssParseBuf() {
  if (g_rssParseBuf) return g_rssParseBuf;
  g_rssParseBuf = (RssItem *)ps_calloc(RSS_MAX_ITEMS, sizeof(RssItem));
  if (!g_rssParseBuf) g_rssParseBuf = (RssItem *)calloc(RSS_MAX_ITEMS, sizeof(RssItem));
  if (!g_rssParseBuf) {
    Serial.printf("[RSS][OOM] parse buf alloc failed bytes=%u heap=%u psram=%u\n",
                  (unsigned)(RSS_MAX_ITEMS * sizeof(RssItem)),
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getFreePsram());
    return nullptr;
  }
  Serial.printf("[RSS] parse buf ready bytes=%u heap=%u psram=%u\n",
                (unsigned)(RSS_MAX_ITEMS * sizeof(RssItem)),
                (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getFreePsram());
  return g_rssParseBuf;
}

static bool updateRssFromFeed(bool force) {
  if (WiFi.status() != WL_CONNECTED || !g_wifiSt.connected) return false;
  const uint32_t now = millis();
  const uint32_t waitMs = g_rss.valid ? rssRefreshIntervalByEnergy() : rssRetryIntervalByEnergy();
  if (!force && g_rss.lastAttemptMs != 0 && (now - g_rss.lastAttemptMs) < waitMs) return g_rss.valid;
  g_rss.lastAttemptMs = now;

  if (g_netTaskReady) {
    netEnqueue(NET_REQ_RSS, 0);
    return g_rss.valid;
  }
  // Fallback: inline (during boot before netTask starts)
  netFetchRss();
  return g_rss.valid;
}

// Extract a JSON string value by key. Handles simple "key":"value" patterns.
// For nested keys like "thumbnail.source", call with the inner key on a substring.
static bool extractJsonStringField(const String &json, const char *key, String &out) {
  String needle = String("\"") + key + "\"";
  int pos = json.indexOf(needle);
  if (pos < 0) return false;
  int i = pos + needle.length();
  // skip whitespace and colon
  while (i < (int)json.length() && (json[i] == ' ' || json[i] == ':' || json[i] == '\t' || json[i] == '\n' || json[i] == '\r')) ++i;
  if (i >= (int)json.length() || json[i] != '"') return false;
  ++i; // skip opening quote
  int start = i;
  while (i < (int)json.length() && json[i] != '"') {
    if (json[i] == '\\') ++i; // skip escaped char
    ++i;
  }
  if (i >= (int)json.length()) return false;
  out = json.substring(start, i);
  return out.length() > 0;
}
// char* overload used by HA fetch (avoids Arduino String heap churn)
static bool extractJsonStringField(const char *json, const char *key, char *buf, size_t bufLen) {
  String out;
  if (!extractJsonStringField(String(json), key, out)) return false;
  copyStringSafe(buf, bufLen, out.c_str());
  return buf[0] != '\0';
}

// Fetch a random Wikipedia article via REST API. Returns true if item populated.
static bool fetchWikiRandomArticle(RssItem &item) {
  ScopedPsramTls psramTls;  // redirect mbedtls allocations to PSRAM
  char url[128];
  snprintf(url, sizeof(url),
           "https://%s.wikipedia.org/api/rest_v1/page/random/summary",
           g_wikiLang);
  char httpFallback[160];
  const char *requestUrl = url;
  bool triedHttpFallback = false;
  String payload;

  while (true) {
    HTTPClient http;
    http.setConnectTimeout(RSS_HTTP_TIMEOUT_MS);
    http.setTimeout(RSS_HTTP_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    bool beginOk = false;
    WiFiClientSecure tls;
    if (strncmp(requestUrl, "https://", 8) == 0) {
      tls.setInsecure();

      tls.setHandshakeTimeout((RSS_HTTP_TIMEOUT_MS + 999U) / 1000U);
      beginOk = http.begin(tls, requestUrl);
    } else {
      beginOk = http.begin(requestUrl);
    }
    if (!beginOk) {
      if (!triedHttpFallback && buildHttpDowngradeUrl(requestUrl, httpFallback, sizeof(httpFallback))) {
        triedHttpFallback = true;
        requestUrl = httpFallback;
        continue;
      }
      return false;
    }
    http.addHeader("Accept", "application/json; charset=utf-8");
    http.addHeader("User-Agent", "ScryBar/1.0 (ESP32)");

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
      http.end();
      if (code < 0 && !triedHttpFallback && buildHttpDowngradeUrl(requestUrl, httpFallback, sizeof(httpFallback))) {
        triedHttpFallback = true;
        requestUrl = httpFallback;
        continue;
      }
      return false;
    }

    payload = http.getString();
    http.end();
    if (payload.length() == 0) return false;
    break;
  }

  String title, extract, pageUrl, thumbUrl;
  if (!extractJsonStringField(payload, "title", title)) return false;
  if (!extractJsonStringField(payload, "extract", extract)) return false;

  // content_urls.desktop.page — find "desktop" block first, then "page" within it
  int desktopPos = payload.indexOf("\"desktop\"");
  if (desktopPos >= 0) {
    String sub = payload.substring(desktopPos);
    extractJsonStringField(sub, "page", pageUrl);
  }
  if (pageUrl.length() == 0) return false;

  // thumbnail.source (optional)
  int thumbPos = payload.indexOf("\"thumbnail\"");
  if (thumbPos >= 0) {
    String sub = payload.substring(thumbPos);
    extractJsonStringField(sub, "source", thumbUrl);
  }

  normalizeRssText(title);
  normalizeRssText(extract);

  copyStringSafe(item.title, sizeof(item.title), title.c_str());
  copyStringSafe(item.summary, sizeof(item.summary), extract.c_str());
  copyStringSafe(item.link, sizeof(item.link), pageUrl.c_str());
  item.feedSlot = 2;
  item.pubDate[0] = '\0';

  return true;
}

static bool updateWikiFromFeed(bool force) {
  if (WiFi.status() != WL_CONNECTED || !g_wifiSt.connected) return false;
  const uint32_t now = millis();
  const uint32_t waitMs = g_wiki.valid ? rssRefreshIntervalByEnergy() : rssRetryIntervalByEnergy();
  if (!force && g_wiki.lastAttemptMs != 0 && (now - g_wiki.lastAttemptMs) < waitMs) return g_wiki.valid;
  g_wiki.lastAttemptMs = now;

  if (g_netTaskReady) {
    netEnqueue(NET_REQ_WIKI, 0);
    return g_wiki.valid;
  }
  // Fallback: inline (during boot before netTask starts)
  netFetchWiki();
  return g_wiki.valid;
}

static bool contentAdvanceToNextFeed(RssState &state, uint8_t feedSlotCount) {
  if (!state.valid || state.itemCount == 0 || feedSlotCount == 0) return false;
  int16_t firstIdx[RSS_FEED_SLOT_COUNT];
  for (uint8_t i = 0; i < RSS_FEED_SLOT_COUNT; ++i) firstIdx[i] = -1;

  uint8_t curSlot = 0xFF;
  if (state.currentIndex < state.itemCount) curSlot = state.items[state.currentIndex].feedSlot;

  for (uint8_t i = 0; i < state.itemCount; ++i) {
    const uint8_t slot = state.items[i].feedSlot;
    if (slot < RSS_FEED_SLOT_COUNT && firstIdx[slot] < 0) firstIdx[slot] = (int16_t)i;
  }

  if (curSlot >= feedSlotCount) {
    for (uint8_t s = 0; s < feedSlotCount; ++s) {
      if (firstIdx[s] >= 0) {
        state.currentIndex = (uint8_t)firstIdx[s];
        state.lastRotateMs = millis();
        return true;
      }
    }
    return false;
  }

  for (uint8_t step = 1; step < feedSlotCount; ++step) {
    const uint8_t nextSlot = (uint8_t)((curSlot + step) % feedSlotCount);
    if (firstIdx[nextSlot] >= 0) {
      state.currentIndex = (uint8_t)firstIdx[nextSlot];
      state.lastRotateMs = millis();
      return true;
    }
  }
  return false;
}

static bool contentAdvanceToNextItem(RssState &state) {
  if (!state.valid || state.itemCount <= 1) return false;
  state.currentIndex = (uint8_t)((state.currentIndex + 1) % state.itemCount);
  state.lastRotateMs = millis();
  return true;
}

static void rssResolveSourceHost(const RssItem &item, char *outHost, size_t outLen) {
  if (!outHost || outLen == 0) return;
  outHost[0] = '\0';
  if (item.feedSlot < RSS_FEED_SLOT_COUNT) {
    const RuntimeRssFeedConfig *feed = runtimeRssFeedBySlot(item.feedSlot);
    if (feed && startsWithHttp(feed->url)) {
      extractRssHost(feed->url, outHost, outLen);
      if (outHost[0] && strcmp(outHost, "unknown") != 0) return;
    }
  }
  extractRssHost(item.link, outHost, outLen);
}

static bool rssAdvanceToNextFeed() {
  return contentAdvanceToNextFeed(g_rss, RSS_FEED_SLOT_COUNT);
}

static bool rssAdvanceToNextItem() {
  return contentAdvanceToNextItem(g_rss);
}

static bool wikiAdvanceToNextFeed() {
  return contentAdvanceToNextFeed(g_wiki, WIKI_FEED_SLOT_COUNT);
}

static bool wikiAdvanceToNextItem() {
  return contentAdvanceToNextItem(g_wiki);
}

static void wikiPreloadMetaStep() {
  if (!g_wiki.valid || g_wiki.itemCount == 0) return;
  if (WiFi.status() != WL_CONNECTED || !g_wifiSt.connected) return;
  const uint32_t now = millis();
  if (g_wikiMetaPreloadLastMs != 0 && (now - g_wikiMetaPreloadLastMs) < 2000) return;
  g_wikiMetaPreloadLastMs = now;
  if (g_netTaskReady) {
    netEnqueue(NET_REQ_WIKI_META, 0);
    return;
  }
  netFetchWikiMeta();
}
// Keep Wiki visuals progressing even while user stays on WIKI view.
// Enqueues wiki-meta enrichment for the currently visible item.
static void wikiPreloadVisibleItemStep() {
  if (g_uiPageMode != UI_PAGE_WIKI) return;
  if (!g_wiki.valid || g_wiki.itemCount == 0) return;
  if (WiFi.status() != WL_CONNECTED || !g_wifiSt.connected) return;

  const uint32_t now = millis();
  if ((now - g_wikiVisiblePreloadLastMs) < 2200UL) return;
  g_wikiVisiblePreloadLastMs = now;

  if (g_netTaskReady) {
    netEnqueue(NET_REQ_WIKI_META, 0);
    return;
  }
  netFetchWikiMeta();
}

// --- netFetch stubs (Phase 1 — replaced in Tasks 1.3-1.5) ---
static bool netFetchWeather(WeatherState &out) {
  if (WiFi.status() != WL_CONNECTED || !g_wifiSt.connected) return false;

  const float lat = runtimeWeatherLat();
  const float lon = runtimeWeatherLon();
  char url[400];
  snprintf(url, sizeof(url),
    "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
    "&current=temperature_2m,relative_humidity_2m,weather_code,is_day,wind_speed_10m"
    "&hourly=temperature_2m,weather_code"
    "&daily=sunrise,sunset"
    "&timezone=Europe%%2FRome&forecast_hours=36&forecast_days=2",
    lat, lon);

  HTTPClient http;
  http.setConnectTimeout(7000);
  http.setTimeout(7000);
  if (!http.begin(url)) {
    Serial.println("[METEO][ERR] HTTP begin fallita.");
    return false;
  }
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[METEO][ERR] HTTP GET code=%d\n", code);
    http.end();
    return false;
  }

  // getString() handles both Content-Length and chunked encoding correctly.
  // Copy to PSRAM so the heap-allocated String can be freed before parsing.
  const String responseStr = http.getString();
  http.end();
  const char *payload = responseStr.c_str();
  if (responseStr.length() == 0) {
    return false;
  }

  float temp = 0.0f, hum = 0.0f, wc = 0.0f, day = 1.0f, wind = 0.0f;
  const bool okTemp = extractJsonNumberField(payload, "\"temperature_2m\"", temp);
  const bool okHum = extractJsonNumberField(payload, "\"relative_humidity_2m\"", hum);
  const bool okCode = extractJsonNumberField(payload, "\"weather_code\"", wc);
  const bool okDay = extractJsonNumberField(payload, "\"is_day\"", day);
  const bool okWind = extractJsonNumberField(payload, "\"wind_speed_10m\"", wind);

  char sunriseIso[32] = {}, sunsetIso[32] = {};
  const bool okSunrise = extractJsonArrayStringAt(payload, "\"sunrise\":[", 0, sunriseIso, sizeof(sunriseIso));
  const bool okSunset  = extractJsonArrayStringAt(payload, "\"sunset\":[",  0, sunsetIso,  sizeof(sunsetIso));

  float next0 = 0.0f, next3 = 0.0f, next6 = 0.0f;
  float nextCode0 = 0.0f, nextCode3 = 0.0f, nextCode6 = 0.0f;
  const bool okNext0 = extractJsonArrayNumberAt(payload, "\"temperature_2m\":[", 0, next0) &&
                       extractJsonArrayNumberAt(payload, "\"weather_code\":[", 0, nextCode0);
  const bool okNext3 = extractJsonArrayNumberAt(payload, "\"temperature_2m\":[", 3, next3) &&
                       extractJsonArrayNumberAt(payload, "\"weather_code\":[", 3, nextCode3);
  const bool okNext6 = extractJsonArrayNumberAt(payload, "\"temperature_2m\":[", 6, next6) &&
                       extractJsonArrayNumberAt(payload, "\"weather_code\":[", 6, nextCode6);

  float tomTemp = 0.0f, tomCode = 0.0f;
  const bool okTomorrow = extractJsonArrayNumberAt(payload, "\"temperature_2m\":[", 24, tomTemp) &&
                          extractJsonArrayNumberAt(payload, "\"weather_code\":[", 24, tomCode);

  if (!(okTemp && okHum && okCode && okDay && okWind)) {
    Serial.println("[METEO][ERR] parse JSON fallita.");
    return false;
  }

  out.valid = true;
  out.tempC = temp;
  out.humidity = (int)lroundf(hum);
  out.weatherCode = (int)lroundf(wc);
  out.isDay = ((int)lroundf(day) != 0);
  out.windKmh = wind;
  if (okSunrise) isoToHhMm(sunriseIso, out.sunrise);
  if (okSunset) isoToHhMm(sunsetIso, out.sunset);

  out.nextValid[0] = okNext0;
  out.nextValid[1] = okNext3;
  out.nextValid[2] = okNext6;
  if (okNext0) { out.nextTemp[0] = (int)lroundf(next0); out.nextCode[0] = (int)lroundf(nextCode0); }
  if (okNext3) { out.nextTemp[1] = (int)lroundf(next3); out.nextCode[1] = (int)lroundf(nextCode3); }
  if (okNext6) { out.nextTemp[2] = (int)lroundf(next6); out.nextCode[2] = (int)lroundf(nextCode6); }

  out.tomorrowValid = okTomorrow;
  if (okTomorrow) { out.tomorrowTemp = (int)lroundf(tomTemp); out.tomorrowCode = (int)lroundf(tomCode); }

  Serial.printf("[METEO] %s %.1fC RH=%d%% wind=%.1fkm/h code=%d day=%d sun=%s/%s\n",
                runtimeWeatherCityLabel(), out.tempC, out.humidity, out.windKmh,
                out.weatherCode, out.isDay ? 1 : 0, out.sunrise, out.sunset);
  return true;
}
static void netFetchRss() {
  if (WiFi.status() != WL_CONNECTED || !g_wifiSt.connected) return;
  RssItem *parseBuf = ensureRssParseBuf();
  if (!parseBuf) return;
  const uint32_t now = millis();

  memset(parseBuf, 0, sizeof(RssItem) * RSS_MAX_ITEMS);
  uint8_t count = 0;
  uint8_t feedsTried = 0;
  uint8_t feedsWithItems = 0;
  const uint8_t configuredFeeds = runtimeRssConfiguredFeedCount();
  uint8_t configuredSeen = 0;
  int firstHttpErr = 0;

  for (uint8_t slot = 0; slot < RSS_FEED_SLOT_COUNT && count < RSS_MAX_ITEMS; ++slot) {
    const RuntimeRssFeedConfig *feed = runtimeRssFeedBySlot(slot);
    if (!feed || !startsWithHttp(feed->url)) continue;
    ++feedsTried;
    ++configuredSeen;

    const uint8_t remaining = (uint8_t)(RSS_MAX_ITEMS - count);
    uint8_t feedCap = clampRssFeedMaxItems(feed->maxItems);
    if (configuredFeeds > 0 && configuredSeen <= configuredFeeds) {
      const uint8_t feedsLeft = (uint8_t)(configuredFeeds - configuredSeen + 1);
      const uint8_t reserveForOthers = (feedsLeft > 1) ? (uint8_t)(feedsLeft - 1) : 0;
      uint8_t fairCap = (remaining > reserveForOthers) ? (uint8_t)(remaining - reserveForOthers) : 1;
      if (fairCap == 0) fairCap = 1;
      if (feedCap > fairCap) feedCap = fairCap;
    }
    if (feedCap > remaining) feedCap = remaining;
    int httpCode = 0;
    const uint8_t got = fetchRssItemsFromUrl(feed->url, &parseBuf[count], feedCap, &httpCode);
    if (got > 0) {
      for (uint8_t k = 0; k < got; ++k) parseBuf[count + k].feedSlot = slot;
      count = (uint8_t)(count + got);
      ++feedsWithItems;
    } else if (firstHttpErr == 0 && httpCode != 0) {
      firstHttpErr = httpCode;
    }
  }

  // Fallback to compile-time default feed
  if (count == 0 && startsWithHttp(RSS_FEED_URL)) {
    bool alreadyConfigured = false;
    for (uint8_t slot = 0; slot < RSS_FEED_SLOT_COUNT; ++slot) {
      const RuntimeRssFeedConfig *feed = runtimeRssFeedBySlot(slot);
      if (!feed || !startsWithHttp(feed->url)) continue;
      if (strncmp(feed->url, RSS_FEED_URL, sizeof(feed->url)) == 0) { alreadyConfigured = true; break; }
    }
    if (!alreadyConfigured || configuredFeeds == 0) {
      ++feedsTried;
      int httpCode = 0;
      const uint8_t got = fetchRssItemsFromUrl(RSS_FEED_URL, &parseBuf[count],
                                               clampRssFeedMaxItems(RSS_DEFAULT_FEED_ITEMS), &httpCode);
      if (got > 0) {
        for (uint8_t k = 0; k < got; ++k) parseBuf[count + k].feedSlot = 0;
        count = (uint8_t)(count + got);
        ++feedsWithItems;
      } else if (firstHttpErr == 0 && httpCode != 0) {
        firstHttpErr = httpCode;
      }
    }
  }

  if (count == 0) {
    xSemaphoreTake(g_netMutex, portMAX_DELAY);
    g_rss.lastHttpCode = (firstHttpErr != 0) ? firstHttpErr : -1;
    xSemaphoreGive(g_netMutex);
    return;
  }

  // Copy results to shared state under mutex
  xSemaphoreTake(g_netMutex, portMAX_DELAY);
  g_rss.lastHttpCode = HTTP_CODE_OK;
  g_rss.itemCount = count;
  for (uint8_t i = 0; i < count; ++i) g_rss.items[i] = parseBuf[i];
  for (uint8_t i = count; i < RSS_MAX_ITEMS; ++i) {
    g_rss.items[i].title[0] = '\0';
    g_rss.items[i].link[0] = '\0';
    g_rss.items[i].pubDate[0] = '\0';
    g_rss.items[i].summary[0] = '\0';
    g_rss.items[i].feedSlot = 0xFF;
    g_rss.items[i].wikiMetaReady = false;
    g_rss.items[i].wikiMetaTried = false;
  }
  g_rss.currentIndex = 0;
  g_rss.lastRotateMs = now;
  g_rss.valid = true;
  g_rss.lastFetchMs = now;
  struct tm tinfo;
  if (getLocalTime(&tinfo, 50)) {
    snprintf(g_rss.fetchedAt, sizeof(g_rss.fetchedAt), "%02d/%02d %02d:%02d",
             tinfo.tm_mday, tinfo.tm_mon + 1, tinfo.tm_hour, tinfo.tm_min);
  } else {
    strncpy(g_rss.fetchedAt, "--/-- --:--", sizeof(g_rss.fetchedAt) - 1);
  }
  g_rss.dirty = true;
  xSemaphoreGive(g_netMutex);

  Serial.printf("[RSS] feeds=%u/%u items=%u first='%s'\n",
                (unsigned)feedsWithItems, (unsigned)feedsTried,
                (unsigned)count, parseBuf[0].title);

  // Trigger favicon prefetch
  netEnqueue(NET_REQ_FAVICON, 0);
}
static void netFetchWiki() {
  if (WiFi.status() != WL_CONNECTED || !g_wifiSt.connected) return;
  RssItem *parseBuf = ensureRssParseBuf();
  if (!parseBuf) return;
  const uint32_t now = millis();

  memset(parseBuf, 0, sizeof(RssItem) * RSS_MAX_ITEMS);
  uint8_t count = 0;
  uint8_t feedsTried = 0;
  uint8_t feedsWithItems = 0;
  int firstHttpErr = 0;

  // Build language-parameterized URLs for Featured (slot 0) and On This Day (slot 1).
  char wikiFeedUrls[2][160];
  snprintf(wikiFeedUrls[0], sizeof(wikiFeedUrls[0]),
           "https://%s.wikipedia.org/w/api.php?action=featuredfeed&feed=featured&feedformat=rss",
           g_wikiLang);
  {
    struct tm tNow;
    if (getLocalTime(&tNow, 50)) {
      snprintf(wikiFeedUrls[1], sizeof(wikiFeedUrls[1]),
               "https://%s.wikipedia.org/w/api.php?action=featuredfeed&feed=onthisday"
               "&feedformat=rss&month=%02d&day=%02d",
               g_wikiLang, tNow.tm_mon + 1, tNow.tm_mday);
    } else {
      snprintf(wikiFeedUrls[1], sizeof(wikiFeedUrls[1]),
               "https://%s.wikipedia.org/w/api.php?action=featuredfeed&feed=onthisday&feedformat=rss",
               g_wikiLang);
    }
  }

  // Slots 0-1: RSS feeds (Featured, On This Day)
  for (uint8_t slot = 0; slot < 2 && count < RSS_MAX_ITEMS; ++slot) {
    const char *feedUrl = wikiFeedUrls[slot];
    if (!startsWithHttp(feedUrl)) continue;
    ++feedsTried;

    const uint8_t remaining = (uint8_t)(RSS_MAX_ITEMS - count);
    uint8_t feedCap = RSS_DEFAULT_FEED_ITEMS;
    if (feedCap > remaining) feedCap = remaining;
    int httpCode = 0;

    uint8_t got = 0;
    {
      static constexpr uint8_t WIKI_PARSE_MAX = 10;
      RssItem *tmpBuf = (RssItem *)ps_calloc(WIKI_PARSE_MAX, sizeof(RssItem));
      if (tmpBuf) {
        const uint8_t parsed = fetchRssItemsFromUrl(feedUrl, tmpBuf, WIKI_PARSE_MAX, &httpCode);
        if (parsed > 0) {
          const uint8_t take = (parsed < feedCap) ? parsed : feedCap;
          const uint8_t skip = parsed - take;
          for (uint8_t k = 0; k < take; ++k) {
            parseBuf[count + k] = tmpBuf[skip + k];
          }
          got = take;
        }
        free(tmpBuf);
      } else {
        got = fetchRssItemsFromUrl(feedUrl, &parseBuf[count], feedCap, &httpCode);
      }
    }

    if (got > 0) {
      for (uint8_t k = 0; k < got; ++k) {
        parseBuf[count + k].feedSlot = slot;
      }
      count = (uint8_t)(count + got);
      ++feedsWithItems;
    } else if (firstHttpErr == 0 && httpCode != 0) {
      firstHttpErr = httpCode;
    }
  }

  // Slot 2: Random article via REST API (JSON, not RSS).
  const uint8_t randomTarget = (count == 0) ? 3 : 1;
  for (uint8_t attempt = 0; attempt < randomTarget && count < RSS_MAX_ITEMS; ++attempt) {
    ++feedsTried;
    RssItem randomItem;
    memset(&randomItem, 0, sizeof(randomItem));
    if (fetchWikiRandomArticle(randomItem)) {
      parseBuf[count] = randomItem;
      ++count;
      ++feedsWithItems;
    }
  }

  if (count == 0) {
    xSemaphoreTake(g_netMutex, portMAX_DELAY);
    g_wiki.lastHttpCode = (firstHttpErr != 0) ? firstHttpErr : -1;
    xSemaphoreGive(g_netMutex);
    return;
  }

  // Copy results to shared state under mutex
  xSemaphoreTake(g_netMutex, portMAX_DELAY);
  g_wiki.lastHttpCode = HTTP_CODE_OK;
  g_wiki.itemCount = count;
  for (uint8_t i = 0; i < count; ++i) g_wiki.items[i] = parseBuf[i];
  for (uint8_t i = count; i < RSS_MAX_ITEMS; ++i) {
    g_wiki.items[i].title[0] = '\0';
    g_wiki.items[i].link[0] = '\0';
    g_wiki.items[i].pubDate[0] = '\0';
    g_wiki.items[i].summary[0] = '\0';
    g_wiki.items[i].feedSlot = 0xFF;
    g_wiki.items[i].wikiMetaReady = false;
    g_wiki.items[i].wikiMetaTried = false;
  }
  g_wiki.currentIndex = 0;
  g_wiki.lastRotateMs = now;
  g_wiki.valid = true;
  g_wiki.lastFetchMs = now;
  g_wikiMetaPreloadLastMs = 0;
  struct tm tinfo;
  if (getLocalTime(&tinfo, 50)) {
    snprintf(g_wiki.fetchedAt, sizeof(g_wiki.fetchedAt), "%02d/%02d %02d:%02d",
             tinfo.tm_mday, tinfo.tm_mon + 1, tinfo.tm_hour, tinfo.tm_min);
  } else {
    strncpy(g_wiki.fetchedAt, "--/-- --:--", sizeof(g_wiki.fetchedAt) - 1);
    g_wiki.fetchedAt[sizeof(g_wiki.fetchedAt) - 1] = '\0';
  }
  g_wiki.dirty = true;
  xSemaphoreGive(g_netMutex);

  Serial.printf("[WIKI] feeds=%u/%u items=%u first='%s'\n",
                (unsigned)feedsWithItems, (unsigned)feedsTried,
                (unsigned)count, parseBuf[0].title);

  // Trigger favicon prefetch for wiki hosts
  netEnqueue(NET_REQ_FAVICON, 0);
}

static void netFetchFavicons() {
  // Read item links under mutex to collect unique hosts
  char hosts[16][96];
  uint8_t hostCount = 0;

  xSemaphoreTake(g_netMutex, portMAX_DELAY);
  for (uint8_t i = 0; i < g_rss.itemCount && hostCount < 16; ++i) {
    char host[96];
    rssResolveSourceHost(g_rss.items[i], host, sizeof(host));
    if (!host[0]) continue;
    bool dup = false;
    for (uint8_t s = 0; s < hostCount; ++s) { if (strcmp(hosts[s], host) == 0) { dup = true; break; } }
    if (!dup) { copyStringSafe(hosts[hostCount++], sizeof(hosts[0]), host); }
  }
  for (uint8_t i = 0; i < g_wiki.itemCount && hostCount < 16; ++i) {
    char host[96];
    rssResolveSourceHost(g_wiki.items[i], host, sizeof(host));
    if (!host[0]) continue;
    bool dup = false;
    for (uint8_t s = 0; s < hostCount; ++s) { if (strcmp(hosts[s], host) == 0) { dup = true; break; } }
    if (!dup) { copyStringSafe(hosts[hostCount++], sizeof(hosts[0]), host); }
  }
  xSemaphoreGive(g_netMutex);

  // Fetch favicons without holding mutex
  uint8_t fetched = 0;
  for (uint8_t i = 0; i < hostCount; ++i) {
    bool cached = false;
    for (uint8_t s = 0; s < kFaviconCacheSlots; ++s) {
      if (g_faviconCache[s].valid && strcmp(g_faviconCache[s].host, hosts[i]) == 0) { cached = true; break; }
    }
    if (cached) continue;
    faviconFetchAndCache(hosts[i]);
    ++fetched;
  }
  if (fetched) Serial.printf("[FAV] prefetched %u new favicons\n", (unsigned)fetched);
}

static void netFetchWikiMeta() {
  // Read which items need meta enrichment under mutex
  xSemaphoreTake(g_netMutex, portMAX_DELAY);
  uint8_t wikiCount = g_wiki.itemCount;
  RssItem localItems[RSS_MAX_ITEMS];
  for (uint8_t i = 0; i < wikiCount; ++i) localItems[i] = g_wiki.items[i];
  xSemaphoreGive(g_netMutex);

  bool anyUpdated = false;
  for (uint8_t i = 0; i < wikiCount; ++i) {
    if (localItems[i].wikiMetaTried && localItems[i].wikiMetaReady) continue;
    if (localItems[i].summary[0]) {
      localItems[i].wikiMetaReady = true;
      localItems[i].wikiMetaTried = true;
      continue;
    }
    if (!localItems[i].link[0]) continue;
    localItems[i].wikiMetaTried = true;
    String summary;
    if (rssFetchWikipediaSummaryMeta(localItems[i].link, summary)) {
      if (summary.length() > 0) {
        copyStringSafe(localItems[i].summary, sizeof(localItems[i].summary), summary.c_str());
        localItems[i].wikiMetaReady = true;
        anyUpdated = true;
      }
    }
    break;  // one item per call (same as original wikiPreloadMetaStep behavior)
  }

  if (anyUpdated) {
    xSemaphoreTake(g_netMutex, portMAX_DELAY);
    for (uint8_t i = 0; i < wikiCount; ++i) g_wiki.items[i] = localItems[i];
    g_wiki.dirty = true;
    xSemaphoreGive(g_netMutex);
  }
}

// ── Network background task (Core 1) ──────────────────────────────────────────
static bool netEnqueue(NetRequestType type, uint8_t param) {
  if (!g_netQueue) return false;
  NetRequest req = { type, param };
  if (xQueueSend(g_netQueue, &req, 0) != pdTRUE) {
    Serial.printf("[NET] queue full, dropping %d\n", (int)type);
    return false;
  }
  return true;
}

static void netTaskMain(void *param) {
  (void)param;

  // Activate PSRAM for mbedtls on this task's lifetime.
  // INVARIANT: Core 0 must NEVER use WiFiClientSecure while this is active.
  mbedtls_platform_set_calloc_free(psramCalloc, psramFree);

  Serial.printf("[NET] task started on core %d\n", xPortGetCoreID());
  g_netTaskReady = true;

  uint32_t lastStackCheckMs = 0;
  NetRequest req;

  for (;;) {
    if (xQueueReceive(g_netQueue, &req, pdMS_TO_TICKS(500)) == pdTRUE) {
      const uint32_t t0 = millis();
      switch (req.type) {
        case NET_REQ_WEATHER: {
          WeatherState local = {};
          const bool ok = netFetchWeather(local);
          if (ok) {
            xSemaphoreTake(g_netMutex, portMAX_DELAY);
            g_weather = local;
            g_weather.lastFetchMs = millis();
            g_weather.dirty = true;
            xSemaphoreGive(g_netMutex);
          }
          Serial.printf("[NET] weather done ok=%d dt=%lu ms\n", ok ? 1 : 0, millis() - t0);
          break;
        }
        case NET_REQ_RSS: {
          netFetchRss();
          Serial.printf("[NET] rss done dt=%lu ms\n", millis() - t0);
          break;
        }
        case NET_REQ_WIKI: {
          netFetchWiki();
          Serial.printf("[NET] wiki done dt=%lu ms\n", millis() - t0);
          break;
        }
        case NET_REQ_FAVICON: {
          netFetchFavicons();
          Serial.printf("[NET] favicons done dt=%lu ms\n", millis() - t0);
          break;
        }
        case NET_REQ_WIKI_META: {
          netFetchWikiMeta();
          const uint32_t wikiMetaDt = millis() - t0;
          if (wikiMetaDt >= 50) Serial.printf("[NET] wiki_meta done dt=%lu ms\n", wikiMetaDt);
          break;
        }
        case NET_REQ_TRANSIT_POLL: {
          netFetchTransitDepartures();
          Serial.printf("[NET] transit_poll done dt=%lu ms\n", millis() - t0);
          break;
        }
        case NET_REQ_LAUNCH_POLL: {
          netFetchLaunchData();
          Serial.printf("[NET] launch_poll done dt=%lu ms\n", millis() - t0);
          break;
        }
      }
    }

    // Stack high-water mark monitoring
    const uint32_t now = millis();
    if ((now - lastStackCheckMs) >= NET_STACK_MONITOR_MS) {
      lastStackCheckMs = now;
      const UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
      Serial.printf("[NET] stack high-water: %u bytes remaining\n", (unsigned)(hwm * sizeof(StackType_t)));
    }
  }
}

static void printTlsDiagResult(const char *label, WiFiClientSecure &client, bool ok) {
  char errBuf[96] = {0};
  const int lastErr = client.lastError(errBuf, sizeof(errBuf));
  Serial.printf("[RSSDIAG] tls %s ok=%d last_err=%d detail=%s heap=%u psram=%u\n",
                label ? label : "?",
                ok ? 1 : 0,
                lastErr,
                errBuf[0] ? errBuf : "-",
                (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getFreePsram());
}

static void runRssDiag() {
  Serial.println("[RSSDIAG] begin");
  if (g_netTaskReady) {
    Serial.println("[RSSDIAG] TLS diag disabled while netTask active — use web UI");
    return;
  }
#if WIFI_DNS_OVERRIDE_ENABLED
  {
    const IPAddress dns1 = WIFI_DNS1_IP;
    const IPAddress dns2 = WIFI_DNS2_IP;
    Serial.printf("[RSSDIAG] dns target=%s/%s active=%s/%s\n",
                  dns1.toString().c_str(),
                  dns2.toString().c_str(),
                  WiFi.dnsIP(0).toString().c_str(),
                  WiFi.dnsIP(1).toString().c_str());
  }
#else
  Serial.printf("[RSSDIAG] dns active=%s/%s\n",
                WiFi.dnsIP(0).toString().c_str(),
                WiFi.dnsIP(1).toString().c_str());
#endif
  IPAddress ipAnsa;
  const bool okAnsa = WiFi.hostByName("www.ansa.it", ipAnsa);
  Serial.printf("[RSSDIAG] dns www.ansa.it ok=%d ip=%s\n", okAnsa ? 1 : 0, okAnsa ? ipAnsa.toString().c_str() : "-");

  ScopedPsramTls psramTls;  // redirect mbedtls allocations to PSRAM for all TLS diag
  {
    WiFiClientSecure tls;
    tls.setInsecure();

    tls.setHandshakeTimeout((RSS_HTTP_TIMEOUT_MS + 999U) / 1000U);
    const bool ok = tls.connect("rss.nytimes.com", 443);
    printTlsDiagResult("rss.nytimes.com:443", tls, ok);
    if (ok) tls.stop();
  }
  {
    WiFiClientSecure tls;
    tls.setInsecure();

    tls.setHandshakeTimeout((RSS_HTTP_TIMEOUT_MS + 999U) / 1000U);
    const bool ok = tls.connect("it.wikipedia.org", 443);
    printTlsDiagResult("it.wikipedia.org:443", tls, ok);
    if (ok) tls.stop();
  }
  Serial.println("[RSSDIAG] end");
}
#else
static bool updateRssFromFeed(bool force) {
  (void)force;
  return false;
}

static void runRssDiag() {
  Serial.println("[RSSDIAG] RSS disabled");
}
#endif

// ── Transit Departure Board — net fetch ────────────────────────────────────

// Full percent-encoding for URL query parameters (handles ':' '/' etc. in GTFS stop IDs).
static void urlEncodeParam(const char *src, char *dst, size_t dstLen) {
  static const char kHex[] = "0123456789ABCDEF";
  size_t di = 0;
  for (size_t i = 0; src[i] && di + 3 < dstLen; ++i) {
    const uint8_t c = (uint8_t)src[i];
    // unreserved chars per RFC 3986: A-Z a-z 0-9 - _ . ~
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      dst[di++] = (char)c;
    } else {
      dst[di++] = '%';
      dst[di++] = kHex[c >> 4];
      dst[di++] = kHex[c & 0x0F];
    }
  }
  dst[di] = '\0';
}

// Kept for any existing callers that only need space→'+' encoding.
static void urlEncodeSpaces(const char *src, char *dst, size_t dstLen) {
  size_t di = 0;
  for (size_t i = 0; src[i] && di + 2 < dstLen; ++i) {
    dst[di++] = (src[i] == ' ') ? '+' : src[i];
  }
  dst[di] = '\0';
}

// Parse Transitous UTC ISO timestamp "2026-04-01T12:45:00Z" into local hour/minute.
// Returns false if the string is malformed.
static bool parseIsoUtcToLocal(const char *iso, uint8_t &hourOut, uint8_t &minOut) {
  if (!iso || strlen(iso) < 19) return false;
  // Fast path: parse digits directly.
  struct tm t = {};
  t.tm_year = ((iso[0]-'0')*1000 + (iso[1]-'0')*100 + (iso[2]-'0')*10 + (iso[3]-'0')) - 1900;
  t.tm_mon  = ((iso[5]-'0')*10  + (iso[6]-'0')) - 1;
  t.tm_mday =  (iso[8]-'0')*10  + (iso[9]-'0');
  t.tm_hour =  (iso[11]-'0')*10 + (iso[12]-'0');
  t.tm_min  =  (iso[14]-'0')*10 + (iso[15]-'0');
  t.tm_sec  =  (iso[17]-'0')*10 + (iso[18]-'0');
  t.tm_isdst = 0;
  time_t utc = mktime(&t);   // treated as local by mktime; we compensate below
  // mktime treats struct as local time; we have UTC — get timezone offset and subtract.
  struct tm local = {};
  getLocalTime(&local, 0);
  time_t localNow = mktime(&local);
  time_t utcNow;
  {
    struct tm utcTm = {};
    getLocalTime(&utcTm, 0);
    utcNow = mktime(&utcTm);
  }
  // Simpler: parse UTC as-is, then use gmtime offset trick via configTzTime set timezone.
  // Actually the cleanest on ESP-IDF: interpret string as UTC with timegm equivalent.
  // ESP32 doesn't have timegm(), so: set tm_isdst=0, use mktime, then subtract UTC offset.
  (void)localNow; (void)utcNow; (void)utc;

  // Reliable approach: use strptime on the string and rely on configured tz.
  // We'll parse the UTC epoch manually: days since epoch + time.
  // Zeller / Julian day approach for year/month/day → epoch.
  static const int kDaysPerMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  int y = t.tm_year + 1900, m = t.tm_mon + 1, d = t.tm_mday;
  long days = 0;
  for (int yr = 1970; yr < y; ++yr)
    days += ((yr % 4 == 0 && (yr % 100 != 0 || yr % 400 == 0)) ? 366 : 365);
  bool leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
  for (int mo = 1; mo < m; ++mo)
    days += (mo == 2 && leap) ? 29 : kDaysPerMonth[mo-1];
  days += d - 1;
  time_t epoch = (time_t)(days * 86400L + t.tm_hour * 3600L + t.tm_min * 60L + t.tm_sec);

  struct tm localTm = {};
  localtime_r(&epoch, &localTm);
  hourOut = (uint8_t)localTm.tm_hour;
  minOut  = (uint8_t)localTm.tm_min;
  return true;
}

// Parse 6-char hex color string (no '#') → uint32_t RGB. Returns fallback on error.
static uint32_t parseHexColor(const char *hex, uint32_t fallback = 0x555577) {
  if (!hex || strlen(hex) < 6) return fallback;
  uint32_t v = 0;
  for (int i = 0; i < 6; ++i) {
    char c = hex[i];
    uint8_t nib;
    if      (c >= '0' && c <= '9') nib = c - '0';
    else if (c >= 'a' && c <= 'f') nib = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') nib = c - 'A' + 10;
    else return fallback;
    v = (v << 4) | nib;
  }
  return v;
}

// Fallback badge color by Transitous mode string (used when routeColor == 0).
static uint32_t transitCategoryColor(const char *cat) {
  if (!cat || !cat[0]) return 0x777777;
  // Transitous mode names (from global API testing r247)
  if (strcmp(cat, "HIGH_SPEED_RAIL") == 0 || strcmp(cat, "INTERCITY_RAIL") == 0) return 0xCC2222;
  if (strcmp(cat, "HIGHSPEED_RAIL")  == 0) return 0xCC2222;
  if (strcmp(cat, "LONG_DISTANCE")   == 0) return 0xCC2222;
  if (strcmp(cat, "NIGHT_RAIL")      == 0) return 0x443388;
  if (strcmp(cat, "REGIONAL_RAIL")   == 0) return 0x118833;
  if (strcmp(cat, "SUBURBAN_RAILWAY")== 0) return 0x118833;
  if (strcmp(cat, "METRO")           == 0) return 0x884499;
  if (strcmp(cat, "SUBWAY")          == 0) return 0x884499;
  if (strcmp(cat, "BUS")             == 0) return 0x1155CC;
  if (strcmp(cat, "COACH")           == 0) return 0x1155CC;
  if (strcmp(cat, "TRAM")            == 0) return 0xCC6600;
  if (strcmp(cat, "FERRY")           == 0) return 0x0077AA;
  // Legacy opendata.ch codes (kept in case of fallback)
  if (strncmp(cat, "IC", 2) == 0 || strncmp(cat, "EC", 2) == 0) return 0xCC2222;
  if (strncmp(cat, "IR", 2) == 0) return 0xDD5500;
  if (strncmp(cat, "RE", 2) == 0 || strncmp(cat, "RB", 2) == 0) return 0xDD8800;
  if (cat[0] == 'S') return 0x118833;
  return 0x555577;
}

// Case-insensitive substring search (strcasestr not available on ESP32).
static bool transitHeadsignContains(const char *headsign, const char *filter) {
  const size_t hl = strlen(headsign);
  const size_t fl = strlen(filter);
  if (!fl || fl > hl) return (fl == 0);
  for (size_t i = 0; i <= hl - fl; ++i) {
    if (strncasecmp(headsign + i, filter, fl) == 0) return true;
  }
  return false;
}

// Returns true for road/urban modes that get pill-shaped badge + blue fallback color.
static bool transitIsBus(const char *cat) {
  if (!cat || !cat[0]) return false;
  return (strncmp(cat, "BUS",   3) == 0 ||
          strncmp(cat, "TRAM",  4) == 0 ||
          strncmp(cat, "COACH", 5) == 0);
}

/// Extract a JSON string value for the first occurrence of key in a bounded region.
static void transitExtractStr(const char *start, const char *stop,
                               const char *key, char *out, size_t outLen) {
  const size_t kl = strlen(key);
  out[0] = '\0';
  for (const char *p = start; p < stop - (ptrdiff_t)kl; ++p) {
    if (strncmp(p, key, kl) != 0) continue;
    p += kl;
    while (p < stop && (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
    if (p >= stop || *p != '"') continue;
    ++p;
    size_t di = 0;
    while (p < stop && *p != '"' && di + 1 < outLen) {
      if (*p == '\\') { ++p; if (p < stop && di + 1 < outLen) out[di++] = *p++; else if (p < stop) ++p; }
      else out[di++] = *p++;
    }
    out[di] = '\0';
    return;
  }
}

static void netFetchTransitDepartures() {
  // Require a Transitous stop ID (set via web UI autocomplete).
  if (!g_transitConfig.configured || !g_transitConfig.stopId[0]) {
    Serial.println("[TRANSIT] no stopId configured — open web UI to search for station");
    return;
  }

  char encId[256];
  urlEncodeParam(g_transitConfig.stopId, encId, sizeof(encId));

  // Current UTC time in ISO 8601 for the ?time= parameter.
  time_t nowUtc = 0;
  {
    struct tm utcTm = {};
    getLocalTime(&utcTm, 0);   // local — we convert back to UTC via mktime + tz offset
    // Build UTC epoch: use the same Zeller trick as parseIsoUtcToLocal in reverse.
    // Simplest: mktime gives local epoch; subtract UTC offset.
    time_t localEpoch = mktime(&utcTm);
    struct tm gmCheck = {};
    gmtime_r(&localEpoch, &gmCheck);
    // offset = local - gm
    time_t gmEpoch = mktime(&gmCheck);
    long tzOff = (long)(localEpoch - gmEpoch);
    nowUtc = localEpoch - tzOff;
  }
  char timeParam[32];
  {
    struct tm utcFmt = {};
    gmtime_r(&nowUtc, &utcFmt);
    snprintf(timeParam, sizeof(timeParam), "%04d-%02d-%02dT%02d:%02d:%02dZ",
             utcFmt.tm_year + 1900, utcFmt.tm_mon + 1, utcFmt.tm_mday,
             utcFmt.tm_hour, utcFmt.tm_min, utcFmt.tm_sec);
  }
  char encTime[48];
  urlEncodeParam(timeParam, encTime, sizeof(encTime));

  char url[512];
  snprintf(url, sizeof(url),
           "https://api.transitous.org/api/v1/stoptimes?stopId=%s&n=8&time=%s",
           encId, encTime);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(TRANSIT_HTTP_TIMEOUT_MS);
  http.setTimeout(TRANSIT_HTTP_TIMEOUT_MS);
  http.begin(client, url);
  http.addHeader("User-Agent", "ScryBar/" FW_BUILD_TAG " (https://github.com/enuzzo/scrybar)");
  http.addHeader("Accept", "application/json");

  const uint32_t t0 = millis();
  const int code = http.GET();

  TransitState local = {};
  local.lastFetchMs = millis();
  local.lastHttpCode = code;
  // Copy station display name from config (Transitous doesn't echo it back).
  copyStringSafe(local.stationName, sizeof(local.stationName), g_transitConfig.station);
  // r242: store tripId per slot for post-loop trip endpoint calls
  char tripIds[TRANSIT_MAX_DEPARTURES][128] = {};

  if (code == 200) {
    const String payload = http.getString();
    const char *data = payload.c_str();
    const size_t dataLen = payload.length();

    struct tm ti = {};
    getLocalTime(&ti, 0);

    // Find "stopTimes":[ array
    const char *stTag = strstr(data, "\"stopTimes\":[");
    const char *p     = stTag ? strchr(stTag, '[') : nullptr;
    if (p) {
      ++p;
      while (*p && local.count < TRANSIT_MAX_DEPARTURES) {
        // Advance to next entry '{'
        while (*p && *p != '{' && *p != ']') ++p;
        if (!*p || *p == ']') break;
        const char *entryStart = p;

        // Find matching top-level '}'
        int depth = 1; ++p;
        while (*p && depth > 0) {
          if (*p == '{') ++depth;
          else if (*p == '}') --depth;
          ++p;
        }
        const char *entryEnd = p;

        // Extract top-level string fields
        char headsign[48]      = {};
        char routeShortName[16]= {};
        char mode[32]          = {};
        char routeColorHex[8]  = {};
        char routeTextHex[8]   = {};
        transitExtractStr(entryStart, entryEnd, "\"headsign\":",       headsign,       sizeof(headsign));
        transitExtractStr(entryStart, entryEnd, "\"routeShortName\":", routeShortName, sizeof(routeShortName));
        transitExtractStr(entryStart, entryEnd, "\"mode\":",           mode,           sizeof(mode));
        transitExtractStr(entryStart, entryEnd, "\"routeColor\":",     routeColorHex,  sizeof(routeColorHex));
        transitExtractStr(entryStart, entryEnd, "\"routeTextColor\":", routeTextHex,   sizeof(routeTextHex));

        // "cancelled" and "realTime" boolean fields
        bool cancelled = false, realTime = false;
        {
          const char *cTag = (const char *)memmem(entryStart, (size_t)(entryEnd - entryStart),
                                                   "\"cancelled\":", 12);
          if (cTag) cancelled = (strncmp(cTag + 12, "true", 4) == 0);
          const char *rtTag = (const char *)memmem(entryStart, (size_t)(entryEnd - entryStart),
                                                    "\"realTime\":", 11);
          if (rtTag) realTime = (strncmp(rtTag + 11, "true", 4) == 0);
        }

        // "place":{ sub-block for departure timestamp, scheduled time and platform
        char depIso[32] = {}, schedIso[32] = {}, platform[8] = {};
        {
          const char *placeTag = (const char *)memmem(entryStart, (size_t)(entryEnd - entryStart),
                                                       "\"place\":{", 9);
          if (placeTag) {
            const char *placeOpen = strchr(placeTag, '{');
            if (placeOpen && placeOpen < entryEnd) {
              int pd = 1; const char *pp = placeOpen + 1;
              while (pp < entryEnd && pd > 0) {
                if (*pp == '{') ++pd; else if (*pp == '}') --pd; ++pp;
              }
              const char *placeEnd = pp;
              transitExtractStr(placeOpen, placeEnd, "\"departure\":",          depIso,   sizeof(depIso));
              transitExtractStr(placeOpen, placeEnd, "\"scheduledDeparture\":", schedIso, sizeof(schedIso));
              transitExtractStr(placeOpen, placeEnd, "\"track\":",              platform, sizeof(platform));
            }
          }
        }
        // tripId (top-level string in each stopTimes entry)
        char tripId[128] = {};
        transitExtractStr(entryStart, entryEnd, "\"tripId\":", tripId, sizeof(tripId));
        // tripFrom.name — origin station for the trip
        char tripFromName[32] = {};
        {
          const char *tfTag = (const char *)memmem(entryStart, (size_t)(entryEnd - entryStart),
                                                    "\"tripFrom\":{", 12);
          if (tfTag) {
            const char *tfOpen = strchr(tfTag, '{');
            if (tfOpen && tfOpen < entryEnd) {
              int tfd = 1; const char *tfp = tfOpen + 1;
              while (tfp < entryEnd && tfd > 0) {
                if (*tfp == '{') ++tfd; else if (*tfp == '}') --tfd; ++tfp;
              }
              transitExtractStr(tfOpen, tfp, "\"name\":", tripFromName, sizeof(tripFromName));
            }
          }
        }
        // tripTo.name — destination (Trenitalia NeTEx feed has headsign="" but populates this)
        char tripToName[48] = {};
        {
          const char *ttTag = (const char *)memmem(entryStart, (size_t)(entryEnd - entryStart),
                                                    "\"tripTo\":{", 10);
          if (ttTag) {
            const char *ttOpen = strchr(ttTag, '{');
            if (ttOpen && ttOpen < entryEnd) {
              int ttd = 1; const char *ttp = ttOpen + 1;
              while (ttp < entryEnd && ttd > 0) {
                if (*ttp == '{') ++ttd; else if (*ttp == '}') --ttd; ++ttp;
              }
              transitExtractStr(ttOpen, ttp, "\"name\":", tripToName, sizeof(tripToName));
            }
          }
        }
        // Effective destination: headsign → tripTo.name → skip.
        // French feeds use non-destination headsigns — prefer tripTo.name when detected:
        //  - RER/Transilien mission codes: exactly 4 uppercase letters (e.g., "ROPO")
        //  - SNCF train numbers: all digits (e.g., "5470" at CDG)
        bool looksLikeCode = false;
        if (headsign[0] && tripToName[0]) {
          const size_t hl = strlen(headsign);
          bool allUpper = (hl == 4);
          bool allDigit = (hl > 0);
          for (size_t j = 0; j < hl; ++j) {
            if (!(headsign[j] >= 'A' && headsign[j] <= 'Z')) allUpper = false;
            if (!(headsign[j] >= '0' && headsign[j] <= '9')) allDigit = false;
          }
          looksLikeCode = allUpper || allDigit;
        }
        const char *dest = (headsign[0] && !looksLikeCode) ? headsign : tripToName;

        // Destination filter: substring match anywhere in effective destination
        if (dest[0] && g_transitConfig.arrStation[0]) {
          if (!transitHeadsignContains(dest, g_transitConfig.arrStation)) {
            p = entryEnd; continue;
          }
        }

        // Need at least a departure time and a destination to display
        if (!depIso[0] || !dest[0]) { p = entryEnd; continue; }

        TransitDeparture &d = local.departures[local.count];
        // routeShortName is the badge line (e.g. "S30", "R21"); fallback to mode
        copyStringSafe(d.line,         sizeof(d.line),         routeShortName[0] ? routeShortName : mode);
        copyStringSafe(d.category,     sizeof(d.category),     mode);
        copyStringSafe(d.destination,  sizeof(d.destination),  dest);
        copyStringSafe(d.platform,     sizeof(d.platform),     platform);
        copyStringSafe(d.tripFromName, sizeof(d.tripFromName), tripFromName);
        d.cancelled      = cancelled;
        d.realTime       = realTime;
        d.routeColor     = parseHexColor(routeColorHex, 0);
        d.routeTextColor = parseHexColor(routeTextHex,  0);
        parseIsoUtcToLocal(depIso, d.depHour, d.depMinute);
        // Delay = actual departure − scheduled departure (minutes)
        d.hasDelay = false;
        d.delayMin = 0;
        if (schedIso[0]) {
          uint8_t sh = 0, sm = 0;
          if (parseIsoUtcToLocal(schedIso, sh, sm)) {
            int diff = ((int)d.depHour * 60 + (int)d.depMinute) - ((int)sh * 60 + (int)sm);
            if (diff >  12 * 60) diff -= 24 * 60;
            if (diff < -12 * 60) diff += 24 * 60;
            if (diff <  -99) diff = -99;
            if (diff >   99) diff =  99;
            d.delayMin = (int8_t)diff;
            d.hasDelay = true;
          }
        }
        d.hasArr = false;   // filled by trip endpoint calls below
        d.valid  = true;
        // Save tripId for post-loop trip fetch
        copyStringSafe(tripIds[local.count], sizeof(tripIds[local.count]), tripId);
        local.count++;
        p = entryEnd;
      }
      local.valid = (local.count > 0);
      snprintf(local.fetchedAt, sizeof(local.fetchedAt), "%02d:%02d", ti.tm_hour, ti.tm_min);
      // Diagnostics: when count=0 log the raw payload start and active filter
      if (local.count == 0) {
        Serial.printf("[TRANSIT] count=0 stopId='%.64s' arrFilter='%s'\n",
                      g_transitConfig.stopId, g_transitConfig.arrStation);
        Serial.printf("[TRANSIT] payload[0..399]: %.400s\n", data);
      }

      // r242: fetch destination arrival time for each departure via /api/v1/trip
      for (uint8_t j = 0; j < local.count; ++j) {
        if (!tripIds[j][0]) continue;
        char encTripId[200];
        urlEncodeParam(tripIds[j], encTripId, sizeof(encTripId));
        char tripUrl[400];
        snprintf(tripUrl, sizeof(tripUrl),
                 "https://api.transitous.org/api/v1/trip?tripId=%s", encTripId);
        WiFiClientSecure tCl;
        tCl.setInsecure();
        HTTPClient tHttp;
        tHttp.setConnectTimeout(3000);
        tHttp.setTimeout(3000);
        tHttp.begin(tCl, tripUrl);
        tHttp.addHeader("User-Agent", "ScryBar/" FW_BUILD_TAG " (https://github.com/enuzzo/scrybar)");
        tHttp.addHeader("Accept", "application/json");
        const int tc = tHttp.GET();
        if (tc == 200) {
          const String tPay = tHttp.getString();
          const char  *td   = tPay.c_str();
          // Parse: {"legs":[{"to":{"arrival":"...Z","name":"..."},...},...]}
          const char *legsTag = strstr(td, "\"legs\":[");
          if (legsTag) {
            const char *lb  = strchr(legsTag, '[');
            const char *leg = lb ? strchr(lb + 1, '{') : nullptr;
            if (leg) {
              // Find end of first leg object
              int ld = 1; const char *lp = leg + 1;
              while (*lp && ld > 0) { if (*lp == '{') ++ld; else if (*lp == '}') --ld; ++lp; }
              const char *legEnd = lp;
              // Find "to":{ within first leg
              const char *toTag = (const char *)memmem(leg, (size_t)(legEnd - leg), "\"to\":{", 6);
              if (toTag) {
                const char *toOpen = strchr(toTag + 5, '{');
                if (toOpen && toOpen < legEnd) {
                  int td2 = 1; const char *tp2 = toOpen + 1;
                  while (*tp2 && td2 > 0) { if (*tp2 == '{') ++td2; else if (*tp2 == '}') --td2; ++tp2; }
                  char arrIso[32] = {};
                  transitExtractStr(toOpen, tp2, "\"arrival\":", arrIso, sizeof(arrIso));
                  if (arrIso[0]) {
                    if (parseIsoUtcToLocal(arrIso, local.departures[j].arrHour,
                                                    local.departures[j].arrMinute)) {
                      local.departures[j].hasArr = true;
                    }
                  }
                }
              }
            }
          }
        }
        tHttp.end();
        Serial.printf("[TRANSIT] trip[%u] code=%d hasArr=%d arr=%02u:%02u\n",
                      j, tc, (int)local.departures[j].hasArr,
                      local.departures[j].arrHour, local.departures[j].arrMinute);
      }

    } else {
      Serial.printf("[TRANSIT] 'stopTimes' key not found — payload: %.120s\n", data);
    }
  } else {
    Serial.printf("[TRANSIT] HTTP error %d\n", code);
  }
  http.end();

  if (g_netMutex) xSemaphoreTake(g_netMutex, portMAX_DELAY);
  g_transitState = local;
  g_transitState.dirty = true;
  if (g_netMutex) xSemaphoreGive(g_netMutex);

  Serial.printf("[NET] transit done code=%d count=%d dt=%lu ms\n",
                code, local.count, millis() - t0);
}

static bool updateTransitFromApi(bool force) {
  if (!g_transitConfig.configured) return false;
  if (WiFi.status() != WL_CONNECTED || !g_wifiSt.connected) return false;
  // Only poll Transitous when the transit page is visible (or forced by config save).
  if (!force && g_uiPageMode != UI_PAGE_TRANSIT) return g_transitState.valid;
  const uint32_t now = millis();
  const uint32_t waitMs = g_transitState.valid ? TRANSIT_REFRESH_MS : TRANSIT_RETRY_MS;
  if (!force && g_transitState.lastFetchMs != 0 && (now - g_transitState.lastFetchMs) < waitMs)
    return g_transitState.valid;
  g_transitState.lastFetchMs = now;
  if (g_netTaskReady) {
    netEnqueue(NET_REQ_TRANSIT_POLL, 0);
    return g_transitState.valid;
  }
  netFetchTransitDepartures();
  return g_transitState.valid;
}

// ── Launch net fetch ────────────────────────────────────────────────────────

// Parse ISO 8601 UTC "YYYY-MM-DDTHH:MMZ" or "YYYY-MM-DDTHH:MM:SSZ" → UTC epoch.
static time_t launchIsoToEpoch(const char *iso) {
  if (!iso || strlen(iso) < 16) return 0;
  struct tm t = {};
  t.tm_year = atoi(iso) - 1900;
  t.tm_mon  = atoi(iso + 5) - 1;
  t.tm_mday = atoi(iso + 8);
  t.tm_hour = atoi(iso + 11);
  t.tm_min  = atoi(iso + 14);
  if (strlen(iso) >= 19 && iso[16] == ':') t.tm_sec = atoi(iso + 17);
  // Manual UTC epoch (ESP32 lacks timegm)
  static const int kDpm[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  int y = t.tm_year + 1900, m = t.tm_mon + 1, d = t.tm_mday;
  long days = 0;
  for (int yr = 1970; yr < y; ++yr)
    days += ((yr % 4 == 0 && (yr % 100 != 0 || yr % 400 == 0)) ? 366 : 365);
  bool leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
  for (int mo = 1; mo < m; ++mo)
    days += (mo == 2 && leap) ? 29 : kDpm[mo - 1];
  days += d - 1;
  return (time_t)(days * 86400L + t.tm_hour * 3600L + t.tm_min * 60L + t.tm_sec);
}

static void netFetchLaunchData() {
  HTTPClient http;
  http.setConnectTimeout(LAUNCH_HTTP_TIMEOUT_MS);
  http.setTimeout(LAUNCH_HTTP_TIMEOUT_MS);
  http.setUserAgent("ScryBar/" FW_BUILD_TAG);

  const char *url = "https://fdo.rocketlaunch.live/json/launches/next/5";
  if (!http.begin(url)) {
    Serial.println("[LAUNCH] http.begin failed");
    g_launchState.lastHttpCode = -1;
    return;
  }
  http.addHeader("Accept", "application/json");

  const int code = http.GET();
  g_launchState.lastHttpCode = code;
  if (code != 200) {
    Serial.printf("[LAUNCH] HTTP %d\n", code);
    http.end();
    return;
  }

  const String body = http.getString();
  http.end();
  const char *json = body.c_str();
  const size_t jsonLen = body.length();

  // Find "result":[ array
  const char *arr = strstr(json, "\"result\":[");
  if (!arr) { Serial.println("[LAUNCH] no result array"); return; }
  arr = strchr(arr, '[');
  if (!arr) return;
  arr++;

  uint8_t count = 0;
  const char *cur = arr;

  while (count < LAUNCH_MAX_ITEMS && cur < json + jsonLen) {
    const char *objStart = strchr(cur, '{');
    if (!objStart) break;

    // Find matching close brace (handle nesting)
    int depth = 0;
    const char *objEnd = objStart;
    for (; objEnd < json + jsonLen; objEnd++) {
      if (*objEnd == '{') depth++;
      else if (*objEnd == '}') { depth--; if (depth == 0) break; }
    }
    if (depth != 0) break;

    LaunchItem item = {};

    // Find end of JSON string value, skipping escaped quotes (\")
    auto jsonStrEnd = [&](const char *p, const char *limit) -> const char * {
      for (; p < limit; p++) {
        if (*p == '"') return p;
        if (*p == '\\' && p + 1 < limit) p++;  // skip escaped char
      }
      return nullptr;
    };

    // Decode JSON string escapes: \" -> ", \\ -> \, \uXXXX -> UTF-8, strip others
    auto jsonDecode = [&](const char *src, size_t srcLen, char *dst, size_t maxLen) {
      size_t di = 0;
      for (size_t si = 0; si < srcLen && di < maxLen - 1; si++) {
        if (src[si] == '\\' && si + 1 < srcLen) {
          si++;
          if (src[si] == '"') dst[di++] = '"';
          else if (src[si] == '\\') dst[di++] = '\\';
          else if (src[si] == 'n') dst[di++] = ' ';
          else if (src[si] == 'u' && si + 4 < srcLen) {
            // Decode \uXXXX to UTF-8
            char hex[5] = { src[si+1], src[si+2], src[si+3], src[si+4], 0 };
            uint16_t cp = (uint16_t)strtol(hex, nullptr, 16);
            si += 4;
            if (cp < 0x80 && di < maxLen - 1) {
              dst[di++] = (char)cp;
            } else if (cp < 0x800 && di + 1 < maxLen - 1) {
              dst[di++] = (char)(0xC0 | (cp >> 6));
              dst[di++] = (char)(0x80 | (cp & 0x3F));
            } else if (di + 2 < maxLen - 1) {
              dst[di++] = (char)(0xE0 | (cp >> 12));
              dst[di++] = (char)(0x80 | ((cp >> 6) & 0x3F));
              dst[di++] = (char)(0x80 | (cp & 0x3F));
            }
          } else {
            dst[di++] = src[si];  // unknown escape, keep char
          }
        } else {
          dst[di++] = src[si];
        }
      }
      dst[di] = '\0';
    };

    // Helper: extract flat string "key":"value" with JSON unescape
    auto extractStr = [&](const char *key, char *dst, size_t maxLen) {
      char pattern[64];
      snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
      const char *p = strstr(objStart, pattern);
      if (!p || p > objEnd) { dst[0] = '\0'; return; }
      p += strlen(pattern);
      const char *end = jsonStrEnd(p, objEnd);
      if (!end) { dst[0] = '\0'; return; }
      jsonDecode(p, (size_t)(end - p), dst, maxLen);
    };

    // Helper: extract nested "outerKey":{"innerKey":"value"}
    auto extractNested = [&](const char *outerKey, const char *innerKey,
                             char *dst, size_t maxLen) {
      char pattern[64];
      snprintf(pattern, sizeof(pattern), "\"%s\":{", outerKey);
      const char *p = strstr(objStart, pattern);
      if (!p || p > objEnd) { dst[0] = '\0'; return; }
      const char *subEnd = strchr(p, '}');
      if (!subEnd || subEnd > objEnd) { dst[0] = '\0'; return; }
      char innerPat[64];
      snprintf(innerPat, sizeof(innerPat), "\"%s\":\"", innerKey);
      const char *q = strstr(p, innerPat);
      if (!q || q > subEnd) { dst[0] = '\0'; return; }
      q += strlen(innerPat);
      const char *valEnd = jsonStrEnd(q, subEnd);
      if (!valEnd) { dst[0] = '\0'; return; }
      jsonDecode(q, (size_t)(valEnd - q), dst, maxLen);
    };

    extractStr("name", item.name, LAUNCH_NAME_LEN);
    extractStr("launch_description", item.description, LAUNCH_DESC_LEN);
    extractStr("weather_condition", item.weatherCondition, LAUNCH_WEATHER_LEN);
    extractStr("weather_temp", item.weatherTemp, sizeof(item.weatherTemp));

    extractNested("provider", "name", item.provider, LAUNCH_PROVIDER_LEN);
    extractNested("provider", "slug", item.providerSlug, LAUNCH_SLUG_LEN);
    extractNested("vehicle", "name", item.vehicle, LAUNCH_VEHICLE_LEN);
    extractNested("pad", "name", item.pad, LAUNCH_PAD_LEN);
    extractNested("location", "name", item.location, LAUNCH_LOCATION_LEN);
    extractNested("location", "country", item.country, 32);

    // t0 (can be null)
    {
      const char *t0p = strstr(objStart, "\"t0\":\"");
      if (t0p && t0p < objEnd) {
        t0p += 6;
        char t0buf[32] = {};
        const char *t0end = strchr(t0p, '"');
        if (t0end && t0end < objEnd) {
          size_t len = min((size_t)(t0end - t0p), sizeof(t0buf) - 1);
          memcpy(t0buf, t0p, len); t0buf[len] = '\0';
          item.t0Epoch = launchIsoToEpoch(t0buf);
          item.hasT0 = (item.t0Epoch > 0);
        }
      }
    }

    // win_open / win_close (optional)
    {
      const char *wp = strstr(objStart, "\"win_open\":\"");
      if (wp && wp < objEnd) {
        wp += 12; char buf[32] = {};
        const char *we = strchr(wp, '"');
        if (we && we < objEnd) {
          size_t len = min((size_t)(we - wp), sizeof(buf) - 1);
          memcpy(buf, wp, len); buf[len] = '\0';
          item.winOpen = launchIsoToEpoch(buf);
        }
      }
    }
    {
      const char *wp = strstr(objStart, "\"win_close\":\"");
      if (wp && wp < objEnd) {
        wp += 13; char buf[32] = {};
        const char *we = strchr(wp, '"');
        if (we && we < objEnd) {
          size_t len = min((size_t)(we - wp), sizeof(buf) - 1);
          memcpy(buf, wp, len); buf[len] = '\0';
          item.winClose = launchIsoToEpoch(buf);
        }
      }
    }

    // result (integer, can be null)
    {
      const char *rp = strstr(objStart, "\"result\":");
      if (rp && rp < objEnd) {
        rp += 9;
        if (*rp == 'n') item.result = 0;
        else item.result = (int8_t)atoi(rp);
      }
    }

    // tags: [{"text":"..."},...]
    {
      const char *tp = strstr(objStart, "\"tags\":[");
      if (tp && tp < objEnd) {
        tp += 7;
        item.tagCount = 0;
        while (item.tagCount < LAUNCH_MAX_TAGS) {
          const char *txt = strstr(tp, "\"text\":\"");
          if (!txt || txt > objEnd) break;
          txt += 8;
          const char *txtEnd = strchr(txt, '"');
          if (!txtEnd || txtEnd > objEnd) break;
          size_t len = min((size_t)(txtEnd - txt), (size_t)(LAUNCH_TAG_LEN - 1));
          memcpy(item.tags[item.tagCount], txt, len);
          item.tags[item.tagCount][len] = '\0';
          item.tagCount++;
          tp = txtEnd + 1;
        }
      }
    }

    // Skip already-launched (result == 1)
    if (item.result == 1) { cur = objEnd + 1; continue; }

    g_launchState.items[count] = item;
    count++;
    cur = objEnd + 1;
  }

  g_launchState.count = count;
  g_launchState.valid = (count > 0);
  g_launchState.dirty = true;
  g_launchState.lastFetchMs = millis();

  struct tm ti;
  time_t now = time(nullptr);
  localtime_r(&now, &ti);
  snprintf(g_launchState.fetchedAt, sizeof(g_launchState.fetchedAt),
           "%02d:%02d", ti.tm_hour, ti.tm_min);
  Serial.printf("[LAUNCH] parsed %u launches\n", count);
}

static bool updateLaunchFromApi(bool force) {
  if (WiFi.status() != WL_CONNECTED || !g_wifiSt.connected) return false;
  if (!force && g_uiPageMode != UI_PAGE_LAUNCH) return g_launchState.valid;
  const uint32_t now = millis();
  const uint32_t waitMs = g_launchState.valid ? LAUNCH_REFRESH_MS : LAUNCH_RETRY_MS;
  if (!force && g_launchState.lastFetchMs != 0 &&
      (now - g_launchState.lastFetchMs) < waitMs)
    return g_launchState.valid;
  g_launchState.lastAttemptMs = now;
  if (g_netTaskReady) {
    netEnqueue(NET_REQ_LAUNCH_POLL, 0);
    return g_launchState.valid;
  }
  netFetchLaunchData();
  return g_launchState.valid;
}

// ── End Transit net fetch ───────────────────────────────────────────────────

static bool updateWeatherFromApi(bool force) {
  if (WiFi.status() != WL_CONNECTED || !g_wifiSt.connected) return false;
  const uint32_t now = millis();
  const uint32_t waitMs = g_weather.valid ? weatherRefreshIntervalByEnergy() : weatherRetryIntervalByEnergy();
  if (!force && g_weather.lastFetchMs != 0 && (now - g_weather.lastFetchMs) < waitMs) return g_weather.valid;
  g_weather.lastFetchMs = now;  // mark attempt time

  if (g_netTaskReady) {
    netEnqueue(NET_REQ_WEATHER, 0);
    return g_weather.valid;  // return current state; result arrives async
  }
  // Fallback: inline fetch (during boot before netTask starts)
  WeatherState local = {};
  const bool ok = netFetchWeather(local);
  if (ok) {
    g_weather = local;
    g_weather.lastFetchMs = millis();
    g_weather.dirty = true;
  }
  return ok;
}
#else
static const char* weatherCodeLabelIt(int code) {
  (void)code;
  return "N/D";
}
static bool updateWeatherFromApi(bool force) {
  (void)force;
  return false;
}
#endif
#endif

#if TEST_DISPLAY && TEST_NTP
static inline int16_t canvasWidth() {
#if DISPLAY_BACKEND_ESP_LCD
  return dispWidth();
#elif DISPLAY_COORD_MODE == 1
  return LCD_HEIGHT;  // 640 logical landscape width
#else
  return g_gfx->width();
#endif
}

static inline int16_t canvasHeight() {
#if DISPLAY_BACKEND_ESP_LCD
  return dispHeight();
#elif DISPLAY_COORD_MODE == 1
  return LCD_WIDTH;
#else
  return g_gfx->height();
#endif
}

static void fillRectCanvas(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (w <= 0 || h <= 0) return;
#if DISPLAY_BACKEND_ESP_LCD
  dispFillRect(x, y, w, h, color);
#elif DISPLAY_COORD_MODE == 1
  // Match Waveshare LVGL software-rotated mapping (landscape 640x172 -> native 172x640).
  const int16_t logicalH = LCD_WIDTH;  // 172
  g_gfx->fillRect(logicalH - (y + h), x, h, w, color);
#else
  g_gfx->fillRect(x, y, w, h, color);
#endif
}

static void runStartupSplash() {
#if !TEST_DISPLAY
  return;
#else
  if (!initDisplay()) return;

#if DISPLAY_BACKEND_ESP_LCD
  if (!g_dispHw.canvasBuf) return;
  constexpr int16_t kLogoW = 283;
  constexpr int16_t kLogoH = 152;
  constexpr size_t kLogoBytes = (size_t)kLogoW * (size_t)kLogoH * 2U;
  if (_private_tmp_netmilk_logo_152h_rgb565_len < kLogoBytes) {
    Serial.printf("[SPLASH][WARN] logo bytes invalid: got=%u expected=%u\n",
                  _private_tmp_netmilk_logo_152h_rgb565_len, (unsigned)kLogoBytes);
    return;
  }

  setBacklightPercent(100);
  const int16_t canvasW = canvasWidth();
  const int16_t canvasH = canvasHeight();
  const int16_t startX = (canvasW - kLogoW) / 2;
  const int16_t startY = (canvasH - kLogoH) / 2;
  fillRectCanvas(0, 0, canvasW, canvasH, DB_COLOR_BLACK);

  const uint8_t *src = _private_tmp_netmilk_logo_152h_rgb565;
  for (int16_t y = 0; y < kLogoH; ++y) {
    const int16_t yy = startY + y;
    if (yy < 0 || yy >= canvasH) continue;
    const size_t srcOff = (size_t)y * (size_t)kLogoW * 2U;
    uint8_t *dst = reinterpret_cast<uint8_t *>(&g_dispHw.canvasBuf[(size_t)yy * (size_t)DB_CANVAS_W + (size_t)startX]);
    memcpy(dst, src + srcOff, (size_t)kLogoW * 2U);
  }
  dispFlush();
  delay(DISPLAY_SPLASH_MS);
#else
  Serial.println("[SPLASH][SKIP] Splash logo enabled only on esp_lcd backend.");
#endif
#endif
}

static inline void drawPixelCanvas(int16_t x, int16_t y, uint16_t color) {
  fillRectCanvas(x, y, 1, 1, color);
}

static void drawLineCanvas(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  int16_t dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int16_t dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int16_t err = dx + dy;
  for (;;) {
    drawPixelCanvas(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    int16_t e2 = err * 2;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

static void drawCircleCanvas(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  int16_t x = r;
  int16_t y = 0;
  int16_t err = 0;
  while (x >= y) {
    drawPixelCanvas(cx + x, cy + y, color);
    drawPixelCanvas(cx + y, cy + x, color);
    drawPixelCanvas(cx - y, cy + x, color);
    drawPixelCanvas(cx - x, cy + y, color);
    drawPixelCanvas(cx - x, cy - y, color);
    drawPixelCanvas(cx - y, cy - x, color);
    drawPixelCanvas(cx + y, cy - x, color);
    drawPixelCanvas(cx + x, cy - y, color);
    ++y;
    if (err <= 0) {
      err += 2 * y + 1;
    } else {
      --x;
      err -= 2 * x + 1;
    }
  }
}

static const char* uiClockModeName(UiClockMode mode) {
  (void)mode;
  return "WORDCLOCK";
}

#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI
static void lvglApplyPageVisibility(bool animate);
static bool lvglApplyPageDrag(int16_t dragDx);
static bool lvglAuxHeroContainsPoint(int16_t x, int16_t y);
static void lvglUpdateWiFiBars(bool force);
static void lvglCenterClockSentenceLabel();
static void lvglApplyClockSentenceAutoFit(const char *text);
static void lvglApplyThemeFonts();
static FeedDeckUi &activeFeedDeck();
static bool lvglDeckQrButtonContainsPoint(FeedDeckUi &d, int16_t x, int16_t y);
static bool lvglDeckRefreshButtonContainsPoint(FeedDeckUi &d, int16_t x, int16_t y);
static bool lvglDeckNextFeedButtonContainsPoint(FeedDeckUi &d, int16_t x, int16_t y);
static bool lvglDeckNewsContainsPoint(FeedDeckUi &d, int16_t x, int16_t y);
static void lvglSetDeckQrButtonPressed(FeedDeckUi &d, bool pressed);
static void lvglSetDeckRefreshButtonPressed(FeedDeckUi &d, bool pressed);
static void lvglSetDeckNextFeedButtonPressed(FeedDeckUi &d, bool pressed);
static void lvglSetDeckQrModalOpen(FeedDeckUi &d, bool open);
static bool lvglFeedQrButtonContainsPoint(int16_t x, int16_t y);
static bool lvglFeedRefreshButtonContainsPoint(int16_t x, int16_t y);
static bool lvglFeedNextFeedButtonContainsPoint(int16_t x, int16_t y);
static bool lvglFeedNewsContainsPoint(int16_t x, int16_t y);
static bool lvglFeedQrModalIsOpen();
static void lvglSetFeedQrModalOpen(bool open);
static void lvglSetFeedQrButtonPressed(bool pressed);
static void lvglSetFeedRefreshButtonPressed(bool pressed);
static void lvglSetFeedNextFeedButtonPressed(bool pressed);
static void lvglUpdateFeedDeck(FeedDeckUi &d, RssState &content, bool isWiki, bool force);
static void lvglInitFeedDeck(FeedDeckUi &d, lv_obj_t *root, bool isWiki);
static void lvglInitNowPlayingUi(NowPlayingUi &ui, lv_obj_t *root);
static void lvglUpdateNowPlayingUi(NowPlayingUi &ui, bool force);
#endif

// Returns true for pages that have a feed deck (AUX/RSS and WIKI).
// Used only for drag/swipe guards — not for interactive logic.
static bool uiPageIsFeedDeck(UiPageMode mode) {
  return (mode == UI_PAGE_AUX) || (mode == UI_PAGE_WIKI);
}

static const char* uiPageName(UiPageMode mode) {
  switch (mode) {
    case UI_PAGE_INFO:
      return "INFO";
    case UI_PAGE_AUX:
      return "AUX";
    case UI_PAGE_WIKI:
      return "WIKI";
    case UI_PAGE_NOW_PLAYING:
      return "NOW";
    case UI_PAGE_DOOM:
      return "DOOM";
    case UI_PAGE_TRANSIT:
      return "TRANSIT";
    case UI_PAGE_LAUNCH:
      return "LAUNCH";
    case UI_PAGE_HOME:
    default:
      return "HOME";
  }
}

static uint8_t uiViewFlagForPage(UiPageMode mode) {
  switch (mode) {
    case UI_PAGE_INFO: return UI_VIEW_FLAG_INFO;
    case UI_PAGE_AUX:  return UI_VIEW_FLAG_AUX;
    case UI_PAGE_WIKI: return UI_VIEW_FLAG_WIKI;
    case UI_PAGE_DOOM:        return UI_VIEW_FLAG_DOOM;
    case UI_PAGE_NOW_PLAYING: return UI_VIEW_FLAG_NOW_PLAYING;
    case UI_PAGE_TRANSIT:     return UI_VIEW_FLAG_TRANSIT;
    case UI_PAGE_LAUNCH:      return 0;  // no bitmask — always on with WiFi
    case UI_PAGE_HOME:
    default:
      return 0;
  }
}

static bool uiPageEnabledNoEnsure(UiPageMode mode) {
  if (mode == UI_PAGE_HOME) return true;
  if (mode == UI_PAGE_DOOM) {
#if TEST_DISPLAY && DOOM_SPIKE_ENABLED
    return (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_DOOM) != 0;
#else
    return false;
#endif
  }
  if (mode == UI_PAGE_TRANSIT) return g_transitConfig.configured;
  if (mode == UI_PAGE_LAUNCH) return g_wifiSt.connected;
  const uint8_t flag = uiViewFlagForPage(mode);
  return (flag != 0) && ((g_runtimeNetConfig.enabledViewsMask & flag) != 0);
}

static bool uiPageEnabled(UiPageMode mode) {
  ensureRuntimeNetConfig();
  return uiPageEnabledNoEnsure(mode);
}

static bool uiPageInSwipeCarousel(UiPageMode mode) {
  switch (mode) {
    case UI_PAGE_INFO:
    case UI_PAGE_HOME:
    case UI_PAGE_AUX:
    case UI_PAGE_WIKI:
    case UI_PAGE_NOW_PLAYING:
    case UI_PAGE_DOOM:
    case UI_PAGE_TRANSIT:
    case UI_PAGE_LAUNCH:
      return true;
    default:
      return false;
  }
}

static const UiPageMode kSwipePageOrder[] = {
    UI_PAGE_INFO,
    UI_PAGE_HOME,
    UI_PAGE_AUX,
    UI_PAGE_WIKI,
    UI_PAGE_NOW_PLAYING,
    UI_PAGE_DOOM,
    UI_PAGE_TRANSIT,
    UI_PAGE_LAUNCH,
};

static int8_t uiSwipePageCountNoEnsure() {
  int8_t count = 0;
  for (UiPageMode mode : kSwipePageOrder) {
    if (uiPageEnabledNoEnsure(mode)) ++count;
  }
  return count;
}

static UiPageMode uiFirstEnabledSwipePageNoEnsure() {
  for (UiPageMode mode : kSwipePageOrder) {
    if (uiPageEnabledNoEnsure(mode)) return mode;
  }
  return UI_PAGE_HOME;
}

static UiPageMode uiLastEnabledSwipePageNoEnsure() {
  for (int i = (int)(sizeof(kSwipePageOrder) / sizeof(kSwipePageOrder[0])) - 1; i >= 0; --i) {
    if (uiPageEnabledNoEnsure(kSwipePageOrder[i])) return kSwipePageOrder[i];
  }
  return UI_PAGE_HOME;
}

static UiPageMode uiLastEnabledMainViewNoEnsure() {
  for (int i = (int)(sizeof(kSwipePageOrder) / sizeof(kSwipePageOrder[0])) - 1; i >= 0; --i) {
    const UiPageMode mode = kSwipePageOrder[i];
    if (mode == UI_PAGE_INFO) continue;
    if (uiPageEnabledNoEnsure(mode)) return mode;
  }
  return UI_PAGE_HOME;
}

static int8_t uiPageOrdinal(UiPageMode mode) {
  ensureRuntimeNetConfig();
  if (!uiPageInSwipeCarousel(mode) || !uiPageEnabledNoEnsure(mode)) return -1;
  int8_t ord = 0;
  for (UiPageMode it : kSwipePageOrder) {
    if (!uiPageEnabledNoEnsure(it)) continue;
    if (it == mode) return ord;
    ++ord;
  }
  return -1;
}

static UiPageMode uiPageFromOrdinal(int8_t ord) {
  ensureRuntimeNetConfig();
  if (ord <= 0) return uiFirstEnabledSwipePageNoEnsure();
  int8_t idx = 0;
  UiPageMode lastEnabled = UI_PAGE_HOME;
  for (UiPageMode mode : kSwipePageOrder) {
    if (!uiPageEnabledNoEnsure(mode)) continue;
    lastEnabled = mode;
    if (idx == ord) return mode;
    ++idx;
  }
  return lastEnabled;
}

static void setUiPage(UiPageMode mode);

static bool stepUiPage(int8_t delta, bool wrap) {
  ensureRuntimeNetConfig();
  const int8_t pageCount = uiSwipePageCountNoEnsure();
  if (pageCount <= 0) return false;
  const int8_t cur = uiPageOrdinal(g_uiPageMode);
  if (cur < 0) {
    const UiPageMode fallback = (delta < 0) ? uiLastEnabledSwipePageNoEnsure() : uiFirstEnabledSwipePageNoEnsure();
    if (g_uiPageMode == fallback) return false;
    setUiPage(fallback);
    return true;
  }
  int8_t next = (int8_t)(cur + delta);
  const int8_t maxOrd = pageCount - 1;
  if (wrap) {
    if (next < 0) next = maxOrd;
    if (next > maxOrd) next = 0;
  } else {
    if (next < 0) next = 0;
    if (next > maxOrd) next = maxOrd;
  }
  if (next == cur) return false;
  setUiPage(uiPageFromOrdinal(next));
  return true;
}

#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI
static bool lvglContainsPoint(lv_obj_t *obj, int16_t x, int16_t y) {
  if (!obj) return false;
  lv_area_t a;
  lv_obj_get_coords(obj, &a);
  return (x >= a.x1 && x <= a.x2 && y >= a.y1 && y <= a.y2);
}

static bool lvglContainsPointExpanded(lv_obj_t *obj, int16_t x, int16_t y, int16_t pad) {
  if (!obj) return false;
  lv_area_t a;
  lv_obj_get_coords(obj, &a);
  return (x >= (a.x1 - pad) && x <= (a.x2 + pad) && y >= (a.y1 - pad) && y <= (a.y2 + pad));
}

// Returns the active feed deck based on current UI page.
static FeedDeckUi &activeFeedDeck() {
  return (g_uiPageMode == UI_PAGE_WIKI) ? g_wikiDeck : g_auxDeck;
}

// ── Unified deck helpers (operate on any FeedDeckUi instance) ─────────────
static bool lvglDeckQrButtonContainsPoint(FeedDeckUi &d, int16_t x, int16_t y) {
  return lvglContainsPointExpanded(d.qrBtn, x, y, 8);
}
static bool lvglDeckRefreshButtonContainsPoint(FeedDeckUi &d, int16_t x, int16_t y) {
  return lvglContainsPointExpanded(d.refreshBtn, x, y, 8);
}
static bool lvglDeckNextFeedButtonContainsPoint(FeedDeckUi &d, int16_t x, int16_t y) {
  return lvglContainsPointExpanded(d.nextFeedBtn, x, y, 8);
}
static bool lvglDeckNewsContainsPoint(FeedDeckUi &d, int16_t x, int16_t y) {
  return lvglContainsPoint(d.news, x, y);
}
static void lvglSetDeckQrButtonPressed(FeedDeckUi &d, bool pressed) {
  if (!d.qrBtn) return;
  const UiThemeLvglTokens &t = activeUiTheme().lvgl;
  const uint32_t bgHex = lvglResolvedAuxButtonBg(
      pressed ? t.auxQrBtnPressedBg : t.auxQrBtnBg,
      pressed ? 0xFFF19A : 0xFFD34D);
  lv_obj_set_style_bg_color(d.qrBtn, lv_color_hex(bgHex), LV_PART_MAIN);
  if (d.qrBtnText) {
    const uint32_t fgHex = lvglResolvedAuxButtonText(
        pressed ? t.auxQrBtnPressedText : t.auxQrBtnText, bgHex);
    lv_obj_set_style_text_color(d.qrBtnText, lv_color_hex(fgHex), 0);
  }
  lv_obj_invalidate(d.qrBtn);
}
static void lvglSetDeckRefreshButtonPressed(FeedDeckUi &d, bool pressed) {
  if (!d.refreshBtn) return;
  const UiThemeLvglTokens &t = activeUiTheme().lvgl;
  const uint32_t bgHex = lvglResolvedAuxButtonBg(
      pressed ? t.auxRefreshBtnPressedBg : t.auxRefreshBtnBg,
      pressed ? 0xB9ECFF : 0x6FD8FF);
  lv_obj_set_style_bg_color(d.refreshBtn, lv_color_hex(bgHex), LV_PART_MAIN);
  if (d.refreshBtnText) {
    const uint32_t fgHex = lvglResolvedAuxButtonText(t.auxRefreshBtnText, bgHex);
    lv_obj_set_style_text_color(d.refreshBtnText, lv_color_hex(fgHex), 0);
  }
  lv_obj_invalidate(d.refreshBtn);
}
static void lvglSetDeckNextFeedButtonPressed(FeedDeckUi &d, bool pressed) {
  if (!d.nextFeedBtn) return;
  const UiThemeLvglTokens &t = activeUiTheme().lvgl;
  const uint32_t bgHex = lvglResolvedAuxButtonBg(
      pressed ? t.auxNextBtnPressedBg : t.auxNextBtnBg,
      pressed ? 0x9E8EFF : 0x7B63FF);
  lv_obj_set_style_bg_color(d.nextFeedBtn, lv_color_hex(bgHex), LV_PART_MAIN);
  if (d.nextFeedBtnText) {
    const uint32_t fgHex = lvglResolvedAuxButtonText(t.auxNextBtnText, bgHex);
    lv_obj_set_style_text_color(d.nextFeedBtnText, lv_color_hex(fgHex), 0);
  }
  lv_obj_invalidate(d.nextFeedBtn);
}
static void lvglSetDeckQrModalOpen(FeedDeckUi &d, bool open) {
  if (d.qrModalOpen == open) return;
  d.qrModalOpen = open;
  if (d.qrBtnText) lv_label_set_text(d.qrBtnText, open ? "X" : "QR");
  if (open) {
    d.lastItemShown = -1;
    d.lastQrPayload[0] = '\0';
    markUserInteraction(millis());   // prevent screensaver while QR is visible
  }
  g_uiNeedsRedraw = true;
  if (d.qrOverlay) {
    if (open) lv_obj_clear_flag(d.qrOverlay, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(d.qrOverlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(d.qrOverlay);
  }
}

// Kept for touch handler call site
static bool lvglAuxHeroContainsPoint(int16_t x, int16_t y) {
  (void)x;
  (void)y;
  return false;
}

// Dispatch helpers — call the correct deck based on current page
static bool lvglFeedQrButtonContainsPoint(int16_t x, int16_t y)      { return lvglDeckQrButtonContainsPoint(activeFeedDeck(), x, y); }
static bool lvglFeedRefreshButtonContainsPoint(int16_t x, int16_t y)  { return lvglDeckRefreshButtonContainsPoint(activeFeedDeck(), x, y); }
static bool lvglFeedNextFeedButtonContainsPoint(int16_t x, int16_t y) { return lvglDeckNextFeedButtonContainsPoint(activeFeedDeck(), x, y); }
static bool lvglFeedNewsContainsPoint(int16_t x, int16_t y)           { return lvglDeckNewsContainsPoint(activeFeedDeck(), x, y); }
static bool lvglFeedQrModalIsOpen()                                    { return activeFeedDeck().qrModalOpen; }
static void lvglSetFeedQrModalOpen(bool open)                          { lvglSetDeckQrModalOpen(activeFeedDeck(), open); }
static void lvglSetFeedQrButtonPressed(bool pressed)                   { lvglSetDeckQrButtonPressed(activeFeedDeck(), pressed); }
static void lvglSetFeedRefreshButtonPressed(bool pressed)              { lvglSetDeckRefreshButtonPressed(activeFeedDeck(), pressed); }
static void lvglSetFeedNextFeedButtonPressed(bool pressed)             { lvglSetDeckNextFeedButtonPressed(activeFeedDeck(), pressed); }

static bool lvglThemeIsCyberpunk() {
  return strcmp(activeUiThemeId(), "cyberpunk-2077") == 0;
}

static bool lvglThemeIsToxicCandy() {
  return strcmp(activeUiThemeId(), "toxic-candy") == 0;
}

static bool lvglThemeIsTokyoTransit() {
  return strcmp(activeUiThemeId(), "tokyo-transit") == 0;
}

static bool lvglThemeIsMinimalBrutalistMono() {
  return strcmp(activeUiThemeId(), "minimal-brutalist-mono") == 0;
}

static bool lvglThemeIsCathodeRay() {
  return strcmp(activeUiThemeId(), "cathode-ray") == 0;
}

static uint16_t lvglColorLuma(uint32_t rgb) {
  const uint16_t r = (uint16_t)((rgb >> 16) & 0xFFu);
  const uint16_t g = (uint16_t)((rgb >> 8) & 0xFFu);
  const uint16_t b = (uint16_t)(rgb & 0xFFu);
  return (uint16_t)((299u * r + 587u * g + 114u * b) / 1000u);
}

/// Derive light/dark from screenBg luma — zero-config, works for any theme.
static bool lvglThemeIsLight() {
  return lvglColorLuma(activeUiTheme().lvgl.screenBg) >= 128u;
}

static uint16_t lvglColorContrastLuma(uint32_t fg, uint32_t bg) {
  const uint16_t lf = lvglColorLuma(fg);
  const uint16_t lb = lvglColorLuma(bg);
  return (lf >= lb) ? (uint16_t)(lf - lb) : (uint16_t)(lb - lf);
}

static uint32_t lvglBlendRgb(uint32_t a, uint32_t b, uint8_t mixB) {
  const uint16_t wa = (uint16_t)(255u - mixB);
  const uint16_t wb = (uint16_t)mixB;
  const uint16_t r = (uint16_t)(((((a >> 16) & 0xFFu) * wa) + (((b >> 16) & 0xFFu) * wb)) / 255u);
  const uint16_t g = (uint16_t)(((((a >> 8) & 0xFFu) * wa) + (((b >> 8) & 0xFFu) * wb)) / 255u);
  const uint16_t bl = (uint16_t)((((a & 0xFFu) * wa) + ((b & 0xFFu) * wb)) / 255u);
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;
}

static uint32_t lvglDarkenRgb(uint32_t rgb, uint8_t amount) {
  return lvglBlendRgb(rgb, 0x000000, amount);
}

static uint32_t lvglLightenRgb(uint32_t rgb, uint8_t amount) {
  return lvglBlendRgb(rgb, 0xFFFFFF, amount);
}

static uint8_t lvglRgbChroma(uint32_t rgb) {
  const uint8_t r = (uint8_t)((rgb >> 16) & 0xFFu);
  const uint8_t g = (uint8_t)((rgb >> 8) & 0xFFu);
  const uint8_t b = (uint8_t)(rgb & 0xFFu);
  uint8_t maxc = r;
  if (g > maxc) maxc = g;
  if (b > maxc) maxc = b;
  uint8_t minc = r;
  if (g < minc) minc = g;
  if (b < minc) minc = b;
  return (uint8_t)(maxc - minc);
}

static float lvglRelativeLuminance(uint32_t rgb) {
  auto linearize = [](uint8_t c) -> float {
    const float s = (float)c / 255.0f;
    if (s <= 0.04045f) return s / 12.92f;
    return powf((s + 0.055f) / 1.055f, 2.4f);
  };
  const float r = linearize((uint8_t)((rgb >> 16) & 0xFFu));
  const float g = linearize((uint8_t)((rgb >> 8) & 0xFFu));
  const float b = linearize((uint8_t)(rgb & 0xFFu));
  return (0.2126f * r) + (0.7152f * g) + (0.0722f * b);
}

static float lvglContrastRatio(uint32_t a, uint32_t b) {
  const float la = lvglRelativeLuminance(a);
  const float lb = lvglRelativeLuminance(b);
  const float hi = (la >= lb) ? la : lb;
  const float lo = (la >= lb) ? lb : la;
  return (hi + 0.05f) / (lo + 0.05f);
}

static uint32_t lvglResolvedOnColorText(uint32_t bg) {
  const float whiteContrast = lvglContrastRatio(0xFFFFFF, bg);
  const float blackContrast = lvglContrastRatio(0x000000, bg);
  return (whiteContrast >= blackContrast) ? 0xFFFFFF : 0x000000;
}

static uint32_t lvglRgb565ToRgb888(uint16_t rgb565) {
  const uint8_t r5 = (uint8_t)((rgb565 >> 11) & 0x1Fu);
  const uint8_t g6 = (uint8_t)((rgb565 >> 5) & 0x3Fu);
  const uint8_t b5 = (uint8_t)(rgb565 & 0x1Fu);
  const uint8_t r = (uint8_t)((r5 * 255u) / 31u);
  const uint8_t g = (uint8_t)((g6 * 255u) / 63u);
  const uint8_t b = (uint8_t)((b5 * 255u) / 31u);
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static uint32_t lvglWeightedAverageColorFromRgb565(const uint8_t *data, size_t dataSize, int16_t width, int16_t height) {
  if (!data || dataSize < 2u || width <= 0 || height <= 0) return 0x101418;
  uint64_t sumR = 0;
  uint64_t sumG = 0;
  uint64_t sumB = 0;
  uint64_t totalWeight = 0;

  for (size_t i = 0, pxIndex = 0; (i + 1u) < dataSize; i += 2u, ++pxIndex) {
    const uint16_t px = (uint16_t)(((uint16_t)data[i] << 8) | (uint16_t)data[i + 1u]);
    const uint32_t rgb = lvglRgb565ToRgb888(px);
    const uint8_t r = (uint8_t)((rgb >> 16) & 0xFFu);
    const uint8_t g = (uint8_t)((rgb >> 8) & 0xFFu);
    const uint8_t b = (uint8_t)(rgb & 0xFFu);
    const int16_t x = (int16_t)(pxIndex % (size_t)width);
    const int16_t y = (int16_t)(pxIndex / (size_t)width);

    uint8_t weight = 1;
    if (x < 18 || x >= (width - 18) || y < 18 || y >= (height - 18)) {
      weight = 6;
    } else if (x < 36 || x >= (width - 36) || y < 36 || y >= (height - 36)) {
      weight = 3;
    }

    sumR += (uint64_t)r * weight;
    sumG += (uint64_t)g * weight;
    sumB += (uint64_t)b * weight;
    totalWeight += weight;
  }

  if (totalWeight == 0) return 0x101418;

  uint32_t avgColor = (((uint32_t)(sumR / totalWeight)) << 16) |
                      (((uint32_t)(sumG / totalWeight)) << 8) |
                      ((uint32_t)(sumB / totalWeight));
  const uint16_t luma = lvglColorLuma(avgColor);
  if (luma < 14u) avgColor = lvglLightenRgb(avgColor, 6);
  if (luma > 242u) avgColor = lvglDarkenRgb(avgColor, 8);
  return avgColor;
}

static uint32_t lvglNowPlayingStaticCoverBackgroundColor() {
  static bool cached = false;
  static uint32_t cachedColor = 0x101418;
  if (cached) return cachedColor;

  cachedColor = lvglWeightedAverageColorFromRgb565(
      assets_img_test_cover_test_150_rgb565,
      sizeof(assets_img_test_cover_test_150_rgb565),
      150,
      150);
  cached = true;
  return cachedColor;
}

static uint32_t lvglNowPlayingCoverBackgroundColor(bool useLiveArtwork) {
  if (useLiveArtwork && g_liveNowPlayingArtwork.valid) {
    return g_liveNowPlayingArtwork.bgColor;
  }
  return lvglNowPlayingStaticCoverBackgroundColor();
}

static const lv_img_dsc_t* lvglNowPlayingCoverImageDsc(bool useLiveArtwork) {
  if (useLiveArtwork && g_liveNowPlayingArtwork.valid &&
      g_nowPlayingLiveCoverImage.data && g_nowPlayingLiveCoverImage.data_size > 0u) {
    return &g_nowPlayingLiveCoverImage;
  }
  return &kNowPlayingRealCover150;
}

static void clearLiveNowPlayingArtwork() {
  if (g_liveNowPlayingArtwork.data) {
    free(g_liveNowPlayingArtwork.data);
  }
  g_liveNowPlayingArtwork = {};
  g_nowPlayingLiveCoverImage.data_size = 0;
  g_nowPlayingLiveCoverImage.data = nullptr;
  g_nowPlayingLiveCoverImage.header.w = 150;
  g_nowPlayingLiveCoverImage.header.h = 150;
}

static void installLiveNowPlayingArtwork(uint8_t *data, size_t dataSize,
                                         uint16_t width, uint16_t height,
                                         const char *artworkId) {
  clearLiveNowPlayingArtwork();
  if (!data || dataSize == 0u || width == 0u || height == 0u) return;

  g_liveNowPlayingArtwork.valid = true;
  g_liveNowPlayingArtwork.width = width;
  g_liveNowPlayingArtwork.height = height;
  g_liveNowPlayingArtwork.dataSize = dataSize;
  g_liveNowPlayingArtwork.data = data;
  g_liveNowPlayingArtwork.bgColor = lvglWeightedAverageColorFromRgb565(data, dataSize, (int16_t)width, (int16_t)height);
  copyStringSafe(g_liveNowPlayingArtwork.artworkId, sizeof(g_liveNowPlayingArtwork.artworkId), artworkId ? artworkId : "");

  g_nowPlayingLiveCoverImage.header.w = width;
  g_nowPlayingLiveCoverImage.header.h = height;
  g_nowPlayingLiveCoverImage.data_size = dataSize;
  g_nowPlayingLiveCoverImage.data = data;
}

static bool decodeNowPlayingArtworkBase64(const String &artworkBase64,
                                          uint16_t width,
                                          uint16_t height,
                                          uint8_t **outData,
                                          size_t &outDataSize,
                                          String &err) {
  if (!outData) {
    err = "Artwork output buffer missing";
    return false;
  }
  *outData = nullptr;
  outDataSize = 0u;

#if !DB_HAS_MBEDTLS_BASE64
  (void)artworkBase64;
  (void)width;
  (void)height;
  err = "Firmware base64 decoder unavailable";
  return false;
#else
  if (artworkBase64.length() == 0) {
    err = "Empty artwork payload";
    return false;
  }
  if (width == 0u || height == 0u || width > 150u || height > 150u) {
    err = "Unsupported artwork dimensions";
    return false;
  }

  const size_t expectedSize = (size_t)width * (size_t)height * 2u;
  uint8_t *decoded = (uint8_t *)heap_caps_malloc(expectedSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!decoded) decoded = (uint8_t *)malloc(expectedSize);
  if (!decoded) {
    err = "Artwork allocation failed";
    return false;
  }

  size_t actualSize = 0u;
  const int rc = mbedtls_base64_decode(
      decoded,
      expectedSize,
      &actualSize,
      (const unsigned char *)artworkBase64.c_str(),
      artworkBase64.length());
  if (rc != 0 || actualSize != expectedSize) {
    free(decoded);
    err = "Artwork decode failed";
    return false;
  }

  *outData = decoded;
  outDataSize = actualSize;
  return true;
#endif
}

static uint32_t lvglResolvedNowPlayingVividBg(uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3, uint32_t c4) {
  const uint32_t candidates[] = {
      c0,
      c1,
      c2,
      c3,
      c4,
  };
  uint32_t best = candidates[0];
  uint16_t bestScore = 0;
  for (size_t i = 0; i < (sizeof(candidates) / sizeof(candidates[0])); ++i) {
    const uint32_t c = candidates[i];
    uint16_t score = (uint16_t)lvglRgbChroma(c) * 3u;
    const uint16_t luma = lvglColorLuma(c);
    if (luma < 38u || luma > 228u) score = (score > 32u) ? (uint16_t)(score - 32u) : 0u;
    if (score > bestScore) {
      best = c;
      bestScore = score;
    }
  }
  return best;
}

static uint32_t lvglBestContrastColor3(uint32_t c0, uint32_t c1, uint32_t c2, uint32_t bg) {
  uint32_t best = c0;
  uint16_t bestScore = lvglColorContrastLuma(c0, bg);
  const uint32_t candidates[] = {c1, c2};
  for (uint8_t i = 0; i < 2; ++i) {
    const uint16_t score = lvglColorContrastLuma(candidates[i], bg);
    if (score > bestScore) {
      best = candidates[i];
      bestScore = score;
    }
  }
  return best;
}

static uint32_t lvglResolvedNowPlayingPrimaryText(uint32_t accent, uint32_t bg) {
  const uint32_t lightAccent = lvglLightenRgb(accent, 132);
  const uint32_t darkAccent = lvglDarkenRgb(accent, 112);
  uint32_t best = lvglBestContrastColor3(lightAccent, 0xFFF8FB, 0x15181F, bg);
  if (lvglColorContrastLuma(best, bg) < 108u) {
    best = lvglBestContrastColor3(0xFFF8FB, 0xF2F5FF, 0x15181F, bg);
  }
  return best;
}

static uint32_t lvglResolvedNowPlayingSecondaryText(uint32_t primary, uint32_t bg) {
  uint32_t toned = lvglBlendRgb(primary, bg, 72);
  if (lvglColorContrastLuma(toned, bg) >= 74u) return toned;
  toned = (lvglColorLuma(primary) >= lvglColorLuma(bg))
              ? lvglLightenRgb(primary, 26)
              : lvglDarkenRgb(primary, 26);
  if (lvglColorContrastLuma(toned, bg) >= 74u) return toned;
  return (lvglColorLuma(bg) >= 118u) ? 0x28303A : 0xE8EDF7;
}

static uint32_t lvglActivePanelBgForContrast() {
  const UiThemeLvglTokens &t = activeUiTheme().lvgl;
  return lvglThemeIsCyberpunk() ? 0x091523 : t.panelBg;
}

static uint32_t lvglResolvedAuxButtonBg(uint32_t preferred, uint32_t fallback) {
  const uint32_t panelBg = lvglActivePanelBgForContrast();
  const uint16_t preferredContrast = lvglColorContrastLuma(preferred, panelBg);
  if (preferredContrast >= 42u) return preferred;
  const uint16_t fallbackContrast = lvglColorContrastLuma(fallback, panelBg);
  return (fallbackContrast > preferredContrast) ? fallback : preferred;
}

static uint32_t lvglResolvedAuxButtonText(uint32_t preferred, uint32_t bg) {
  if (lvglColorContrastLuma(preferred, bg) >= 96u) return preferred;
  return (lvglColorLuma(bg) >= 128u) ? 0x111111 : 0xF5F5F5;
}

static uint32_t lvglResolvedSaverReadableText(const UiThemeLvglTokens &t) {
  const bool light = lvglColorLuma(t.screenBg) >= 128u;
  const uint32_t fallback = light ? 0x111111 : 0xFFFFFF;
  const uint32_t bg = t.screenBg;
  const uint32_t candidates[] = {
      t.saverBalloon,
      t.saverFooter,
      t.infoText,
      t.auxSourceText,
      t.headerText,
      fallback,
  };
  uint32_t best = fallback;
  uint16_t bestScore = 0;
  for (size_t i = 0; i < (sizeof(candidates) / sizeof(candidates[0])); ++i) {
    const uint32_t c = candidates[i];
    if (c == t.saverCow) continue;
    const uint16_t score = lvglColorContrastLuma(c, bg);
    if (score > bestScore) {
      bestScore = score;
      best = c;
    }
  }
  if (bestScore < 95u) return fallback;
  return best;
}

static uint32_t lvglResolvedPanelBg(const UiThemeLvglTokens &t) {
  // Cyberpunk on ESP should stay deep navy/teal, not electric blue.
  if (lvglThemeIsCyberpunk()) return 0x091523;
  return t.panelBg;
}

static uint32_t lvglResolvedHeaderBg(const UiThemeLvglTokens &t) {
  if (lvglThemeIsCyberpunk()) return 0xFFE600;
  return t.headerBg;
}

static uint32_t lvglResolvedHeaderText(const UiThemeLvglTokens &t) {
  if (lvglThemeIsCyberpunk()) return 0x0F6272;
  return t.headerText;
}

static uint32_t lvglResolvedHeaderMeta(const UiThemeLvglTokens &t) {
  if (lvglThemeIsCyberpunk()) return 0x1A8296;
  return t.headerMeta;
}

static uint32_t lvglResolvedWeatherBg(const UiThemeLvglTokens &t) {
  // Weather icons are authored for transparent-on-light backgrounds.
  if (lvglColorLuma(t.weatherCardBg) >= 170u) return t.weatherCardBg;
  return 0xF3F7FF;
}

static uint32_t lvglResolvedWeatherPrimary(const UiThemeLvglTokens &t, uint32_t weatherBg) {
  if (lvglColorLuma(weatherBg) >= 170u && lvglColorLuma(t.weatherTextPrimary) <= 130u) {
    return t.weatherTextPrimary;
  }
  return 0x1B2D3A;
}

static uint32_t lvglResolvedWeatherSecondary(const UiThemeLvglTokens &t, uint32_t weatherBg, uint32_t weatherPrimary) {
  if (lvglColorLuma(weatherBg) >= 170u && lvglColorLuma(t.weatherTextSecondary) <= 170u) {
    return t.weatherTextSecondary;
  }
  const uint16_t p = lvglColorLuma(weatherPrimary);
  return (p <= 105u) ? 0x445D6D : 0x2E4655;
}

static uint32_t lvglResolvedForecastText(const UiThemeLvglTokens &t, uint32_t weatherBg, uint32_t weatherPrimary) {
  if (lvglColorLuma(weatherBg) >= 170u && lvglColorLuma(t.forecastText) <= 150u) {
    return t.forecastText;
  }
  return weatherPrimary;
}

static uint32_t lvglResolvedWeatherGlyphOnline(const UiThemeLvglTokens &t, uint32_t weatherBg, uint32_t weatherPrimary) {
  if (lvglColorLuma(weatherBg) >= 170u && lvglColorLuma(t.weatherGlyphOnline) <= 170u) {
    return t.weatherGlyphOnline;
  }
  return weatherPrimary;
}

static uint32_t lvglResolvedWeatherGlyphOffline(const UiThemeLvglTokens &t, uint32_t weatherBg, uint32_t weatherSecondary) {
  if (lvglColorLuma(weatherBg) >= 170u && lvglColorLuma(t.weatherGlyphOffline) <= 180u) {
    return t.weatherGlyphOffline;
  }
  return weatherSecondary;
}

// ── LVGL style helpers (M9) ─────────────────────────────────────────────────

// Set bg + grad to same hex (flat fill), with optional radius.
static inline void lvglSetBgFlat(lv_obj_t *o, uint32_t hex) {
  if (!o) return;
  lv_color_t c = lv_color_hex(hex);
  lv_obj_set_style_bg_color(o, c, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(o, c, LV_PART_MAIN);
}
static inline void lvglSetBgFlatR(lv_obj_t *o, uint32_t hex, lv_coord_t r) {
  lvglSetBgFlat(o, hex);
  if (o) lv_obj_set_style_radius(o, r, LV_PART_MAIN);
}

// Guarded text color from hex.
static inline void lvglSetTextHex(lv_obj_t *o, uint32_t hex) {
  if (o) lv_obj_set_style_text_color(o, lv_color_hex(hex), 0);
}

// Conditional header border (bordered themes: cyberpunk, minimal).
static inline void lvglSetHeaderBorder(lv_obj_t *o, bool show, uint32_t hex) {
  if (!o) return;
  lv_obj_set_style_border_width(o, show ? 1 : 0, LV_PART_MAIN);
  lv_obj_set_style_border_color(o, lv_color_hex(hex), LV_PART_MAIN);
  lv_obj_set_style_border_opa(o, show ? LV_OPA_80 : LV_OPA_0, LV_PART_MAIN);
}

// Button accent border (1px, 80% opacity).
static inline void lvglSetBtnBorder(lv_obj_t *o, uint32_t hex) {
  if (!o) return;
  lv_obj_set_style_border_width(o, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(o, lv_color_hex(hex), LV_PART_MAIN);
  lv_obj_set_style_border_opa(o, LV_OPA_80, LV_PART_MAIN);
}

// Create opaque panel: flat bg, no border/shadow/scroll.
static lv_obj_t *lvglCreatePanel(lv_obj_t *parent, lv_coord_t w, lv_coord_t h,
                                  lv_coord_t x, lv_coord_t y,
                                  lv_color_t bg, lv_coord_t radius) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_set_size(o, w, h);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_style_bg_color(o, bg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(o, bg, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(o, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(o, radius, LV_PART_MAIN);
  lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}

// Create a deck action button with centered label.
static lv_obj_t *lvglCreateDeckButton(lv_obj_t *parent, lv_coord_t w, lv_coord_t h,
                                       lv_coord_t x, lv_coord_t y,
                                       uint32_t bgHex, lv_coord_t radius,
                                       const char *label, uint32_t textHex,
                                       lv_obj_t *&outText) {
  lv_obj_t *btn = lvglCreatePanel(parent, w, h, x, y, lv_color_hex(bgHex), radius);
  outText = lv_label_create(btn);
  lv_obj_set_style_text_font(outText, lvglFontTiny(), 0);
  lv_obj_set_style_text_color(outText, lv_color_hex(textHex), 0);
  lv_label_set_text(outText, label);
  lv_obj_center(outText);
  lvglForceLabelVisible(outText);
  return btn;
}

// ── End LVGL style helpers ──────────────────────────────────────────────────

static void lvglApplyThemeStylesFeedDecks(const UiThemeLvglTokens &t,
                                           uint32_t panelBg, uint32_t headerBg,
                                           uint32_t headerText, uint32_t headerMeta,
                                           bool headerBordered, bool cyberpunk,
                                           lv_coord_t cardRadius, lv_coord_t buttonRadius,
                                           lv_coord_t badgeRadius, uint32_t btnBorderHex);

static void lvglApplyThemeStyles(bool forceInvalidate) {
  const UiThemeLvglTokens &t = activeUiTheme().lvgl;
  const bool cyberpunk = lvglThemeIsCyberpunk();
  const bool tokyo = lvglThemeIsTokyoTransit();
  const bool minimal = lvglThemeIsMinimalBrutalistMono();
  const bool cathode = lvglThemeIsCathodeRay();
  const bool sharpCorners = minimal || cathode;
  const lv_coord_t cardRadius = sharpCorners ? 0 : 10;
  const lv_coord_t infoRadius = sharpCorners ? 0 : 8;
  const lv_coord_t buttonRadius = sharpCorners ? 0 : 4;
  const lv_coord_t badgeRadius = sharpCorners ? 0 : 6;
  const lv_coord_t wifiBarRadius = sharpCorners ? 0 : 1;
  const bool headerBordered = cyberpunk || minimal || cathode;
  const uint32_t panelBg = lvglResolvedPanelBg(t);
  const uint32_t headerBg = lvglResolvedHeaderBg(t);
  const uint32_t headerText = lvglResolvedHeaderText(t);
  const uint32_t headerMeta = lvglResolvedHeaderMeta(t);
  const uint32_t infoHeaderBg = cyberpunk ? headerBg : t.infoHeaderBg;
  const uint32_t infoHeaderBorder = cyberpunk ? t.auxSourceText : t.infoHeaderBorder;
  const uint32_t infoHeaderText = (cyberpunk || minimal) ? headerText : t.infoText;
  const uint32_t weatherBg = lvglResolvedWeatherBg(t);
  const uint32_t weatherTextPrimary = lvglResolvedWeatherPrimary(t, weatherBg);
  const uint32_t weatherTextSecondary = lvglResolvedWeatherSecondary(t, weatherBg, weatherTextPrimary);
  const uint32_t forecastText = lvglResolvedForecastText(t, weatherBg, weatherTextPrimary);
  const uint32_t weatherGlyphOnline = lvglResolvedWeatherGlyphOnline(t, weatherBg, weatherTextPrimary);
  const uint32_t weatherGlyphOffline = lvglResolvedWeatherGlyphOffline(t, weatherBg, weatherTextSecondary);
  const uint32_t saverReadableText = lvglResolvedSaverReadableText(t);
  const bool light = lvglThemeIsLight();
  uint32_t clockLine1 = cyberpunk ? t.infoText : (light ? t.infoText : t.headerText);
  uint32_t clockLine2 = t.infoText;
  uint32_t clockLine3 = cyberpunk ? t.auxMeta : (light ? t.auxSourceText : headerMeta);
  uint32_t clockDivider = light ? t.auxSourceText : t.divider;
  if (tokyo) {
    clockLine1 = t.auxSourceText;
    clockLine2 = t.infoText;
    clockLine3 = t.auxWhenText;
    clockDivider = t.auxSourceText;
  } else if (minimal) {
    clockLine1 = t.infoText;
    clockLine2 = t.infoText;
    clockLine3 = t.auxMeta;
    clockDivider = t.auxSourceText;
  }

  lvglSetBgFlat(lv_scr_act(), t.screenBg);

  lvglSetBgFlatR(g_clockUi.block, panelBg, cardRadius);
  lvglSetBgFlatR(g_weatherUi.card, weatherBg, cardRadius);
  lvglSetBgFlatR(g_weatherUi.forecastBar, weatherBg, cardRadius);
  lvglSetBgFlat(g_weatherUi.forecastBarFill, weatherBg);

  const uint32_t headerBorderHex = cyberpunk ? t.auxSourceText : t.divider;
  lvglSetBgFlatR(g_clockUi.header, headerBg, cardRadius);
  lvglSetHeaderBorder(g_clockUi.header, headerBordered, headerBorderHex);
  lvglSetBgFlat(g_clockUi.headerFill, headerBg);

  lvglSetBgFlatR(g_weatherUi.header, headerBg, cardRadius);
  lvglSetHeaderBorder(g_weatherUi.header, headerBordered, headerBorderHex);
  lvglSetBgFlat(g_weatherUi.headerFill, headerBg);

  lvglSetBgFlatR(g_infoUi.card, t.infoBg, infoRadius);
  lvglSetBgFlatR(g_infoUi.header, infoHeaderBg, infoRadius);
  if (g_infoUi.header) lv_obj_set_style_border_color(g_infoUi.header, lv_color_hex(infoHeaderBorder), LV_PART_MAIN);
  lvglSetBgFlat(g_infoUi.headerFill, infoHeaderBg);

  lvglSetTextHex(g_clockUi.date, headerText);
  lvglSetTextHex(g_weatherUi.city, headerText);
  lvglSetTextHex(g_weatherUi.sun, headerText);
  lvglSetTextHex(g_clockUi.l1, clockLine1);
  lvglSetTextHex(g_clockUi.l2, clockLine2);
  lvglSetTextHex(g_clockUi.l3, clockLine3);
  if (g_clockUi.divider) lv_obj_set_style_bg_color(g_clockUi.divider, lv_color_hex(clockDivider), LV_PART_MAIN);

  lvglSetTextHex(g_weatherUi.temp, weatherTextPrimary);
  lvglSetTextHex(g_weatherUi.desc, weatherTextPrimary);
  lvglSetTextHex(g_weatherUi.humidity, weatherTextPrimary);
  lvglSetTextHex(g_weatherUi.wind, weatherTextSecondary);
  lvglSetTextHex(g_weatherUi.forecastNow, forecastText);
  lvglSetTextHex(g_weatherUi.forecastTomorrow, weatherTextSecondary);
  if (g_weatherUi.sep) lv_obj_set_style_bg_color(g_weatherUi.sep, lv_color_hex(weatherTextSecondary), LV_PART_MAIN);
  if (g_weatherUi.glyph) {
    lvglSetTextHex(g_weatherUi.glyph, g_weather.valid ? weatherGlyphOnline : weatherGlyphOffline);
  }

  lvglSetTextHex(g_infoUi.title, infoHeaderText);
  lvglSetTextHex(g_infoUi.endpoint, infoHeaderText);
  lvglSetTextHex(g_infoUi.bodyLeft, t.infoText);

#if defined(LV_USE_QRCODE) && LV_USE_QRCODE
  if (g_infoUi.webQr) {
    lv_obj_t *infoQrParent = lv_obj_get_parent(g_infoUi.webQr);
    lv_coord_t infoQrSize = lv_obj_get_width(g_infoUi.webQr);
    if (infoQrSize < 32 && infoQrParent) {
      const lv_coord_t pw = lv_obj_get_width(infoQrParent);
      const lv_coord_t ph = lv_obj_get_height(infoQrParent);
      infoQrSize = ((pw < ph) ? pw : ph) - 4;
    }
    if (infoQrSize < 64) infoQrSize = 64;
    char infoPayload[sizeof(g_infoUi.lastQrPayload)];
    char infoFallback[sizeof(g_infoUi.lastQrPayload)] = "http://--:8080";
    if (g_wifiSt.setupApActive) wifiBuildSetupPortalUrl(infoFallback, sizeof(infoFallback));
    copyStringSafe(
      infoPayload,
      sizeof(infoPayload),
      g_infoUi.lastQrPayload[0] ? g_infoUi.lastQrPayload : infoFallback
    );
    lv_obj_del(g_infoUi.webQr);
    const lv_color_t infoQrDark = lv_color_hex(t.infoQrDark);
    const lv_color_t infoQrLight = lv_color_hex(t.infoQrLight);
    g_infoUi.webQr = lv_qrcode_create(infoQrParent, infoQrSize, infoQrDark, infoQrLight);
    lv_obj_align(g_infoUi.webQr, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(g_infoUi.webQr, infoQrLight, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_infoUi.webQr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_infoUi.webQr, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_infoUi.webQr, lv_color_hex(t.infoHeaderBorder), LV_PART_MAIN);
    lv_obj_set_style_border_opa(g_infoUi.webQr, LV_OPA_80, LV_PART_MAIN);
    lv_qrcode_update(g_infoUi.webQr, infoPayload, strlen(infoPayload));
  }
#endif

  for (uint8_t i = 0; i < 4; ++i) {
    if (!g_clockUi.wifiBars[i]) continue;
    lv_obj_set_style_radius(g_clockUi.wifiBars[i], wifiBarRadius, LV_PART_MAIN);
  }
  const uint32_t btnBorderHex = (lvglColorContrastLuma(t.auxSourceText, panelBg) >= 36u) ? t.auxSourceText : 0xEAF0FF;
  lvglApplyThemeStylesFeedDecks(t, panelBg, headerBg, headerText, headerMeta,
                                 headerBordered, cyberpunk, cardRadius, buttonRadius,
                                 badgeRadius, btnBorderHex);

  lvglApplyThemeFonts();
  lvglCenterClockSentenceLabel();

#if SCREENSAVER_ENABLED
  if (g_saver.root) lv_obj_set_style_bg_color(g_saver.root, lv_color_hex(t.screenBg), LV_PART_MAIN);
  lvglSetTextHex(g_saver.sky, t.saverSky);
  lvglSetTextHex(g_saver.field, t.saverField);
  lvglSetTextHex(g_saver.cow, t.saverCow);
  lvglSetTextHex(g_saver.balloon, saverReadableText);
  lvglSetTextHex(g_saver.balloonTail, saverReadableText);
  lvglSetTextHex(g_saver.footer, saverReadableText);
  for (uint8_t r = 0; r < kSaverSkyRowsMax; ++r)
    for (uint8_t s = 0; s < kSaverStarsPerRow; ++s)
      lvglSetTextHex(g_saver.starObj[r][s], t.saverStarLow);
#endif

  g_clockUi.wifiMask = 0xFFFF;
  lvglUpdateWiFiBars(true);

  // Transit theme colors
  if (g_lvglTransitRoot) {
    lvglSetBgFlat(g_lvglTransitRoot, panelBg);
    if (g_transitUi.header) lvglSetBgFlat(g_transitUi.header, headerBg);
    if (g_transitUi.headerFill) lvglSetBgFlat(g_transitUi.headerFill, headerBg);
    lvglSetTextHex(g_transitUi.title,   headerText);
    lvglSetTextHex(g_transitUi.station, headerText);
    lvglSetTextHex(g_transitUi.status,  headerText);
    for (uint8_t i = 0; i < TRANSIT_MAX_DEPARTURES; ++i) {
      lvglSetTextHex(g_transitUi.dest[i],     t.infoText);
      lvglSetTextHex(g_transitUi.time_[i],    t.infoText);
      lvglSetTextHex(g_transitUi.platform[i], t.auxMeta);
      if (g_transitUi.rowSep[i])
        lv_obj_set_style_bg_color(g_transitUi.rowSep[i], lv_color_hex(t.divider), LV_PART_MAIN);
    }
    lvglSetTextHex(g_transitUi.noData, t.auxMeta);
  }

  // Launch theme colors
  if (g_lvglLaunchRoot) {
    lvglSetBgFlat(g_lvglLaunchRoot, panelBg);
    if (g_launchUi.header) lvglSetBgFlat(g_launchUi.header, headerBg);
    lvglSetTextHex(g_launchUi.title, headerText);
    lvglSetTextHex(g_launchUi.fetchTime, t.auxMeta);
    lvglSetBgFlat(g_launchUi.heroBg, panelBg);
    lvglSetTextHex(g_launchUi.heroName, t.infoText);
    lvglSetTextHex(g_launchUi.heroVehiclePad, t.auxMeta);
    lvglSetTextHex(g_launchUi.heroCountdown, t.infoText);
    lvglSetTextHex(g_launchUi.noData, t.auxMeta);
    for (int i = 0; i < 2; i++) {
      lvglSetTextHex(g_launchUi.compactName[i], t.infoText);
      lvglSetTextHex(g_launchUi.compactVehicle[i], t.auxMeta);
      lvglSetTextHex(g_launchUi.compactLocation[i], t.auxMeta);
      lvglSetTextHex(g_launchUi.compactDate[i], t.infoText);
    }
    lvglSetTextHex(g_launchUi.heroLocation, t.infoText);
    lvglSetTextHex(g_launchUi.heroCountry, t.auxMeta);
    lvglSetTextHex(g_launchUi.heroWeather, t.infoText);
    lvglSetTextHex(g_launchUi.heroWindow, t.auxMeta);
    if (g_launchUi.qrOverlay) lv_obj_set_style_bg_color(g_launchUi.qrOverlay, lv_color_hex(t.screenBg), LV_PART_MAIN);
  }

  if (!forceInvalidate) return;
  g_uiNeedsRedraw = true;
  if (g_infoUi.root) lv_obj_invalidate(g_infoUi.root);
  if (g_lvglHomeRoot) lv_obj_invalidate(g_lvglHomeRoot);
  if (g_lvglAuxRoot) lv_obj_invalidate(g_lvglAuxRoot);
  if (g_lvglTransitRoot) lv_obj_invalidate(g_lvglTransitRoot);
  if (g_lvglLaunchRoot) lv_obj_invalidate(g_lvglLaunchRoot);
#if SCREENSAVER_ENABLED
  if (g_saver.root) lv_obj_invalidate(g_saver.root);
#endif
}

static void lvglApplyThemeStylesFeedDecks(const UiThemeLvglTokens &t,
                                           uint32_t panelBg, uint32_t headerBg,
                                           uint32_t headerText, uint32_t headerMeta,
                                           bool headerBordered, bool cyberpunk,
                                           lv_coord_t cardRadius, lv_coord_t buttonRadius,
                                           lv_coord_t badgeRadius, uint32_t btnBorderHex) {
  const uint32_t headerBorderHex = cyberpunk ? t.auxSourceText : t.divider;
  FeedDeckUi *feedDecks[] = {&g_auxDeck, &g_wikiDeck};
  for (FeedDeckUi *d : feedDecks) {
    lvglSetBgFlatR(d->card, panelBg, cardRadius);
    lvglSetBgFlatR(d->header, headerBg, cardRadius);
    lvglSetHeaderBorder(d->header, headerBordered, headerBorderHex);
    lvglSetBgFlat(d->headerFill, headerBg);
    lvglSetTextHex(d->title, headerText);
    lvglSetTextHex(d->status, headerText);
    lvglSetTextHex(d->meta, headerMeta);
    if (d->sourceBadge) {
      lv_obj_set_style_bg_color(d->sourceBadge, lv_color_hex(t.auxBadgeBg), LV_PART_MAIN);
      lvglSetBtnBorder(d->sourceBadge, t.auxSourceText);
      if (d->sourceBadge) lv_obj_set_style_border_opa(d->sourceBadge, LV_OPA_70, LV_PART_MAIN);
      if (d->sourceBadge) lv_obj_set_style_radius(d->sourceBadge, badgeRadius, LV_PART_MAIN);
    }
    lvglSetTextHex(d->sourceBadgeText, t.auxBadgeText);
    lvglSetTextHex(d->sourceSite, t.auxSourceText);
    lvglSetTextHex(d->news, t.auxText);
    if (d->qrOverlay) lv_obj_set_style_bg_color(d->qrOverlay, lv_color_hex(t.screenBg), LV_PART_MAIN);
    lvglSetTextHex(d->qrHint, t.auxQrHint);
    if (d->nextFeedBtn) lv_obj_set_style_radius(d->nextFeedBtn, buttonRadius, LV_PART_MAIN);
    if (d->refreshBtn)  lv_obj_set_style_radius(d->refreshBtn,  buttonRadius, LV_PART_MAIN);
    if (d->qrBtn)       lv_obj_set_style_radius(d->qrBtn,       buttonRadius, LV_PART_MAIN);
    lvglSetBtnBorder(d->nextFeedBtn, btnBorderHex);
    lvglSetBtnBorder(d->refreshBtn, btnBorderHex);
    lvglSetBtnBorder(d->qrBtn, btnBorderHex);
#if defined(LV_USE_QRCODE) && LV_USE_QRCODE
    if (d->qr) {
      lv_obj_t *qrParent = lv_obj_get_parent(d->qr);
      lv_coord_t qrSize = canvasHeight();
      if (qrSize < 90) qrSize = 90;
      const bool qrHidden = lv_obj_has_flag(d->qr, LV_OBJ_FLAG_HIDDEN);
      const char *feedFallbackUrl = (d == &g_wikiDeck) ? "https://en.wikipedia.org" : "https://ansa.it";
      char qrPayload[sizeof(d->lastQrPayload)];
      copyStringSafe(qrPayload, sizeof(qrPayload),
        d->lastQrPayload[0] ? d->lastQrPayload : feedFallbackUrl);
      lv_obj_del(d->qr);
      const lv_color_t qrDark  = lv_color_hex(t.auxQrDark);
      const lv_color_t qrLight = lv_color_hex(t.auxQrLight);
      d->qr = lv_canvas_create(qrParent);
      uint32_t bufSz = LV_CANVAS_BUF_SIZE_INDEXED_1BIT(qrSize, qrSize);
      uint8_t *psBuf = (uint8_t *)ps_calloc(1, bufSz);
      if (!psBuf) psBuf = (uint8_t *)calloc(1, bufSz);
      if (psBuf) {
        lv_canvas_set_buffer(d->qr, psBuf, qrSize, qrSize, LV_IMG_CF_INDEXED_1BIT);
        lv_canvas_set_palette(d->qr, 0, qrDark);
        lv_canvas_set_palette(d->qr, 1, qrLight);
      }
      lv_obj_add_flag(d->qr, LV_OBJ_FLAG_FLOATING);
      lv_obj_set_pos(d->qr, 0, 0);
      lv_obj_set_style_border_width(d->qr, 0, LV_PART_MAIN);
      lv_qrcode_update(d->qr, qrPayload, strlen(qrPayload));
      if (qrHidden) lv_obj_add_flag(d->qr, LV_OBJ_FLAG_HIDDEN);
    }
#endif
    lvglSetDeckNextFeedButtonPressed(*d, false);
    lvglSetDeckRefreshButtonPressed(*d, false);
    lvglSetDeckQrButtonPressed(*d, false);
  }
}

#else
static bool lvglAuxHeroContainsPoint(int16_t x, int16_t y) { (void)x; (void)y; return false; }
static FeedDeckUi &activeFeedDeck() { return g_auxDeck; }
static bool lvglDeckQrButtonContainsPoint(FeedDeckUi &d, int16_t x, int16_t y) { (void)d; (void)x; (void)y; return false; }
static bool lvglDeckRefreshButtonContainsPoint(FeedDeckUi &d, int16_t x, int16_t y) { (void)d; (void)x; (void)y; return false; }
static bool lvglDeckNextFeedButtonContainsPoint(FeedDeckUi &d, int16_t x, int16_t y) { (void)d; (void)x; (void)y; return false; }
static bool lvglDeckNewsContainsPoint(FeedDeckUi &d, int16_t x, int16_t y) { (void)d; (void)x; (void)y; return false; }
static void lvglSetDeckQrButtonPressed(FeedDeckUi &d, bool pressed) { (void)d; (void)pressed; }
static void lvglSetDeckRefreshButtonPressed(FeedDeckUi &d, bool pressed) { (void)d; (void)pressed; }
static void lvglSetDeckNextFeedButtonPressed(FeedDeckUi &d, bool pressed) { (void)d; (void)pressed; }
static void lvglSetDeckQrModalOpen(FeedDeckUi &d, bool open) { (void)d; (void)open; }
static bool lvglFeedQrButtonContainsPoint(int16_t x, int16_t y) { (void)x; (void)y; return false; }
static bool lvglFeedRefreshButtonContainsPoint(int16_t x, int16_t y) { (void)x; (void)y; return false; }
static bool lvglFeedNextFeedButtonContainsPoint(int16_t x, int16_t y) { (void)x; (void)y; return false; }
static bool lvglFeedNewsContainsPoint(int16_t x, int16_t y) { (void)x; (void)y; return false; }
static bool lvglFeedQrModalIsOpen() { return false; }
static void lvglSetFeedQrModalOpen(bool open) { (void)open; }
static void lvglSetFeedQrButtonPressed(bool pressed) { (void)pressed; }
static void lvglSetFeedRefreshButtonPressed(bool pressed) { (void)pressed; }
static void lvglSetFeedNextFeedButtonPressed(bool pressed) { (void)pressed; }
#endif

static void setUiPage(UiPageMode mode) {
  ensureRuntimeNetConfig();
  if (!uiPageEnabledNoEnsure(mode)) {
    mode = uiFirstEnabledSwipePageNoEnsure();
  }
  if (g_uiPageMode == mode) return;
#if TEST_DISPLAY && DOOM_SPIKE_ENABLED
  if (g_uiPageMode == UI_PAGE_DOOM && mode != UI_PAGE_DOOM) {
    if (g_lvglDoomRoot) lv_obj_add_flag(g_lvglDoomRoot, LV_OBJ_FLAG_HIDDEN);
    g_doom.touchZone = DOOM_TOUCH_NONE;
    g_doom.neutralPending = false;
  }
  if (mode == UI_PAGE_DOOM && g_uiPageMode != UI_PAGE_DOOM) {
    if (g_lvglDoomRoot) lv_obj_clear_flag(g_lvglDoomRoot, LV_OBJ_FLAG_HIDDEN);
    g_doom.touchZone = DOOM_TOUCH_NONE;
#if DB_HAS_PRBOOM_DONOR
    g_doom.launchRequested = doomPrboomIsRunning();
#else
    g_doom.launchRequested = false;
#endif
    doomRequestNeutralCalibrate();
    g_doom.frameDirty = true;
  }
#endif
  if (!uiPageIsFeedDeck(mode)) {
    lvglSetDeckQrModalOpen(g_auxDeck, false);
    lvglSetDeckQrModalOpen(g_wikiDeck, false);
  }
  if (uiPageIsFeedDeck(g_uiPageMode) && uiPageIsFeedDeck(mode) && g_uiPageMode != mode) {
    lvglSetDeckQrModalOpen(g_auxDeck, false);
    lvglSetDeckQrModalOpen(g_wikiDeck, false);
    g_auxDeck.lastItemShown = -1;
    g_auxDeck.lastQrPayload[0] = '\0';
    g_wikiDeck.lastItemShown = -1;
    g_wikiDeck.lastQrPayload[0] = '\0';
  }
  g_uiPageMode = mode;
#if TEST_IMU
  syncImuActiveForUi();
#endif
#if TEST_DISPLAY && DOOM_SPIKE_ENABLED
  if (mode == UI_PAGE_DOOM) doomRenderSpike(true);
#endif
#if TEST_WIFI && RSS_ENABLED
  if (mode == UI_PAGE_WIKI) g_wikiVisiblePreloadLastMs = 0;
#endif
  g_uiNeedsRedraw = true;
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI
  if (g_lvglReady) {
    lvglApplyPageVisibility(true);
    // Content will be redrawn naturally by the next runLvglLoop() cycle.
    // No forced invalidate+flush here — let the slide animation complete first.
  }
#endif
}

static void toggleUiPage() {
  if (g_uiPageMode == UI_PAGE_HOME) {
    setUiPage(uiPageEnabled(UI_PAGE_AUX) ? UI_PAGE_AUX : uiLastEnabledMainViewNoEnsure());
    return;
  }
  setUiPage(UI_PAGE_HOME);
}

static void jumpToFirstMainView() {
  setUiPage(UI_PAGE_HOME);
}

static void jumpToLastMainView() {
  ensureRuntimeNetConfig();
  setUiPage(uiLastEnabledMainViewNoEnsure());
}

static void toggleClockMode() {
  g_clock.mode = UI_CLOCK_MODE_WORDCLOCK;
  g_uiNeedsRedraw = true;
}

#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI && SCREENSAVER_ENABLED
static void markUserInteraction(uint32_t nowMs) {
  g_saver.lastUserInteractionMs = nowMs;
}

static uint32_t lvglScreenSaverRandNext() {
  g_saver.rand = (g_saver.rand * 1664525UL) + 1013904223UL;
  return g_saver.rand;
}

static void lvglScreenSaverSetCowArt(int8_t dir) {
  if (!g_saver.cow) return;
  if (g_saver.cowState == COW_SLEEP) {
    static const char *kCowSleep =
        "      Z z z\n"
        "   __(o_o)__\n"
        "  /__|__|__\\~\n"
        "  ~~~~~~~~~~";
    lv_label_set_text(g_saver.cow, kCowSleep);
    return;
  }
  static const char *kCowRight =
      " _(__)_        V\n"
      "'-e e -'__,--.__)\n"
      "(o_o)        )\n"
      "   \\. /___.  |\n"
      "   ||| _)/_)/\n"
      "  //_(/_(/_(";
  static const char *kCowRightChew =
      " _(__)_        V\n"
      "'-e e -'__,--.__)\n"
      "(o-o)        )\n"
      "   \\. /___.  |\n"
      "   ||| _)/_)/\n"
      "  //_(/_(/_(";
  static const char *kCowLeft =
      "V        _(__)_ \n"
      "(__.--,__'-e e -'\n"
      "  (        (o_o) \n"
      "  |  .___\\ ./    \n"
      "   \\(_\\(_ |||    \n"
      "    )_\\)_\\)_\\\\";
  static const char *kCowLeftChew =
      "V        _(__)_ \n"
      "(__.--,__'-e e -'\n"
      "  (        (o-o) \n"
      "  |  .___\\ ./    \n"
      "   \\(_\\(_ |||    \n"
      "    )_\\)_\\)_\\\\";
  const bool chew = (g_saver.cowChewFrame == 1 && g_saver.cowState == COW_GRAZE);
  if (dir >= 0) {
    lv_label_set_text(g_saver.cow, chew ? kCowLeftChew : kCowLeft);
  } else {
    lv_label_set_text(g_saver.cow, chew ? kCowRightChew : kCowRight);
  }
}

static constexpr uint8_t kScreenSaverThoughtMaxLines = 4;

static uint8_t lvglScreenSaverWrapCols() {
  const int16_t cw = canvasWidth();
  if (cw <= 0) return 18;
  const lv_font_t *f = lvglFontScreenSaverBalloonText();
  uint16_t charPx = 12;
  if (f && f->line_height > 0) {
    charPx = (uint16_t)((f->line_height * 58u) / 100u);
    if (charPx < 9u) charPx = 9u;
  }
  uint16_t maxW = (uint16_t)((cw * 66) / 100);
  uint8_t cols = (charPx > 0u) ? (uint8_t)(maxW / charPx) : 18u;
  if (cols < 14u) cols = 14u;
  if (cols > 30u) cols = 30u;
  return cols;
}

static lv_coord_t lvglScreenSaverBalloonMaxWidthPx() {
  const int16_t cw = canvasWidth();
  lv_coord_t maxW = (cw > 40) ? (lv_coord_t)((cw * 66) / 100) : 320;
  if (maxW < 112) maxW = 112;
  return maxW;
}

static lv_coord_t lvglScreenSaverMeasureBalloonTextWidth(const char *text) {
  if (!text || !text[0]) return 0;
  const lv_font_t *f = lvglFontScreenSaverBalloonText();
  if (!f) return 0;
  lv_point_t size = {0, 0};
  lv_txt_get_size(&size, text, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  return size.x;
}

static uint16_t lvglScreenSaverResolvedBalloonWidth(const char *text) {
  const uint16_t kMinW = 112;
  const lv_font_t *f = lvglFontScreenSaverBalloonText();
  uint16_t padPx = 10;
  if (f && f->line_height > 24) padPx = 12;
  uint16_t w = kMinW;
  if (text && text[0]) {
    const lv_coord_t measured = lvglScreenSaverMeasureBalloonTextWidth(text);
    if (measured > 0) w = (uint16_t)(measured + padPx);
  }
  const uint16_t kMaxW = (uint16_t)lvglScreenSaverBalloonMaxWidthPx();
  if (w < kMinW) w = kMinW;
  if (w > kMaxW) w = kMaxW;
  return w;
}

static void lvglScreenSaverWrapQuote(const char *src, char *dst, size_t dstSize, uint8_t maxCols, uint8_t maxLines) {
  if (!dst || dstSize == 0) return;
  dst[0] = '\0';
  if (!src || !src[0] || maxCols < 8 || maxLines < 1) return;

  size_t di = 0;
  uint8_t lineLen = 0;
  uint8_t lines = 1;
  const char *p = src;

  while (*p && di + 1 < dstSize) {
    while (*p == ' ') ++p;
    if (!*p) break;

    char word[80];
    uint8_t wi = 0;
    while (*p && *p != ' ' && wi < (sizeof(word) - 1)) {
      word[wi++] = *p++;
    }
    word[wi] = '\0';
    if (wi == 0) continue;

    const uint8_t needed = (uint8_t)(wi + ((lineLen > 0) ? 1 : 0));
    if ((lineLen + needed) > maxCols) {
      if (lines >= maxLines) {
        const char *ell = "...";
        while (*ell && di + 1 < dstSize) dst[di++] = *ell++;
        dst[di] = '\0';
        return;
      }
      if (di + 1 >= dstSize) break;
      dst[di++] = '\n';
      lineLen = 0;
      ++lines;
    }

    if (lineLen > 0 && di + 1 < dstSize) {
      dst[di++] = ' ';
      ++lineLen;
    }

    for (uint8_t i = 0; i < wi && di + 1 < dstSize; ++i) {
      dst[di++] = word[i];
      ++lineLen;
    }
  }
  dst[di] = '\0';
}

static void lvglScreenSaverBuildFieldLine() {
  const uint8_t cols = g_saver.cols;
  if (cols < 24 || cols > kSaverSkyColsMax) return;
  static const char kPattern[] = "~`~~^~";
  const uint8_t patLen = (uint8_t)(sizeof(kPattern) - 1U);
  for (uint8_t i = 0; i < cols; ++i) {
    g_saver.fieldBuf[i] = kPattern[(uint8_t)((i + g_saver.fieldScroll) % patLen)];
  }
  // Tiny ASCII tree drifts with the field to avoid static burn-in lines.
  if (cols > 14) {
    uint8_t tx = (uint8_t)(3U + ((g_saver.fieldScroll / 2U) % (cols - 10U)));
    g_saver.fieldBuf[tx] = '/';
    g_saver.fieldBuf[tx + 1] = '^';
    g_saver.fieldBuf[tx + 2] = '\\';
    g_saver.fieldBuf[tx + 3] = '|';
  }
  g_saver.fieldBuf[cols] = '\0';
  if (g_saver.field) lv_label_set_text(g_saver.field, g_saver.fieldBuf);
}

static void lvglScreenSaverUpdateField(uint32_t nowMs) {
  if (!g_saver.field) return;
  if (nowMs < g_saver.fieldNextMs) return;
  g_saver.fieldNextMs = nowMs + 1400UL;
  g_saver.fieldScroll = (uint8_t)(g_saver.fieldScroll + 1U);
  lvglScreenSaverBuildFieldLine();
}

static void lvglScreenSaverUpdateFooter(uint32_t nowMs) {
  if (!g_saver.footer) return;
  if (nowMs >= g_saver.footerJitterNextMs) {
    g_saver.footerJitterNextMs = nowMs + (18000UL + (lvglScreenSaverRandNext() % 9000UL));
    g_saver.footerJitterIdx = (uint8_t)((g_saver.footerJitterIdx + 1U) % 4U);
  }
  if (nowMs < g_saver.footerNextMs) return;
  g_saver.footerNextMs = nowMs + 1000UL;
  char buf[32];
  if (g_clock.ntpSynced) {
    struct tm tmNow;
    if (getLocalTime(&tmNow, 20)) {
      snprintf(buf, sizeof(buf), "%02d:%02d  %02d/%02d",
               tmNow.tm_hour, tmNow.tm_min, tmNow.tm_mday, tmNow.tm_mon + 1);
    } else {
      snprintf(buf, sizeof(buf), "--:--  --/--");
    }
  } else {
    snprintf(buf, sizeof(buf), "--:--  --/--");
  }
  static const int8_t kJitterXY[4][2] = {
      {-10, -4},
      {-12, -4},
      {-10, -5},
      {-9, -3},
  };
  lv_label_set_text(g_saver.footer, buf);
  lv_obj_align(g_saver.footer, LV_ALIGN_BOTTOM_RIGHT,
               kJitterXY[g_saver.footerJitterIdx][0],
               kJitterXY[g_saver.footerJitterIdx][1]);
}

static uint32_t lvglScreenSaverIdleTargetMs(uint32_t nowMs) {
#if TEST_BATTERY
  if (g_batt.hasSample && !batteryExternalPowerLikelyNow(nowMs)) {
    return SCREENSAVER_IDLE_BATTERY_MS;
  }
  return SCREENSAVER_IDLE_USB_MS;
#else
  (void)nowMs;
  return SCREENSAVER_IDLE_USB_MS;
#endif
}

static void lvglScreenSaverInitStars() {
  const uint32_t nowMs = millis();
  for (uint8_t r = 0; r < kSaverSkyRowsMax; ++r) {
    for (uint8_t s = 0; s < kSaverStarsPerRow; ++s) {
      g_saver.starX[r][s] = 0;
      g_saver.starLevel[r][s] = 0;
      g_saver.starDir[r][s] = 1;
      g_saver.starNextMs[r][s] = nowMs;
      if (g_saver.starObj[r][s]) lv_obj_add_flag(g_saver.starObj[r][s], LV_OBJ_FLAG_HIDDEN);
    }
  }
  const int16_t rowPitch = 12;
  const int16_t topY = 8;
  for (uint8_t r = 0; r < g_saver.rows; ++r) {
    const uint8_t seg = (uint8_t)(g_saver.cols / kSaverStarsPerRow);
    for (uint8_t s = 0; s < kSaverStarsPerRow; ++s) {
      const uint8_t start = (uint8_t)(s * seg);
      const uint8_t span = (seg > 8) ? (seg - 4) : seg;
      g_saver.starX[r][s] = (uint8_t)(start + 2 + (lvglScreenSaverRandNext() % (span ? span : 1)));
      const bool startLit = (s == 0U) || ((lvglScreenSaverRandNext() % 100U) < 35U);
      if (startLit) {
        g_saver.starLevel[r][s] = (uint8_t)(1U + (lvglScreenSaverRandNext() % 2U));
        g_saver.starDir[r][s] = -1;
        g_saver.starNextMs[r][s] = nowMs + (420UL + (lvglScreenSaverRandNext() % 760UL));
      } else {
        g_saver.starLevel[r][s] = 0;
        g_saver.starDir[r][s] = 1;
        g_saver.starNextMs[r][s] = nowMs + (3500UL + (lvglScreenSaverRandNext() % 8500UL));
      }
      if (g_saver.starObj[r][s]) {
        lv_obj_set_pos(g_saver.starObj[r][s], 4 + ((int16_t)g_saver.starX[r][s] * 8), topY + ((int16_t)r * rowPitch));
      }
    }
  }
}

// --- Pasture Simulator: event end, sky phase, clouds ---

static void lvglScreenSaverEndEvent() {
  if (g_saver.eventActive == SAVER_EVENT_NONE) return;
  for (uint8_t r = 0; r < kSaverSkyRowsMax; ++r) {
    for (uint8_t s = 0; s < kSaverStarsPerRow; ++s) {
      const uint32_t bit = 1UL << (r * kSaverStarsPerRow + s);
      if (g_saver.starBorrowedMask & bit) {
        if (g_saver.starObj[r][s]) {
          lv_obj_add_flag(g_saver.starObj[r][s], LV_OBJ_FLAG_HIDDEN);
          lv_label_set_text(g_saver.starObj[r][s], ".");
        }
      }
    }
  }
  g_saver.starBorrowedMask = 0;
  g_saver.eventActive = SAVER_EVENT_NONE;
  g_saver.eventEndMs = 0;
  g_saver.eventCooldownMs = millis() + 180000UL;
  Serial.println("[SCRNSVR] event ended");
}

static void lvglScreenSaverUpdateSkyPhase(uint32_t nowMs) {
  if (nowMs < g_saver.skyNextMs) return;
  g_saver.skyNextMs = nowMs + 60000UL;

  uint8_t newPhase = SKY_NIGHT;
  if (g_clock.ntpSynced) {
    struct tm tmNow;
    if (getLocalTime(&tmNow, 20)) {
      const uint8_t h = (uint8_t)tmNow.tm_hour;
      if (h >= 5 && h < 7)       newPhase = SKY_DAWN;
      else if (h >= 7 && h < 19) newPhase = SKY_DAY;
      else if (h >= 19 && h < 21) newPhase = SKY_DUSK;
      else                        newPhase = SKY_NIGHT;
    }
  }

  if (newPhase == g_saver.skyPhase) return;
  g_saver.skyPhase = newPhase;
  Serial.printf("[SCRNSVR] skyPhase=%u\n", newPhase);

  // Phase transition safety: force-end incompatible events
  if (g_saver.eventActive != SAVER_EVENT_NONE) {
    const uint8_t ev = g_saver.eventActive;
    if (ev == SAVER_EVENT_RAIN && newPhase != SKY_DAY) lvglScreenSaverEndEvent();
    if ((ev == SAVER_EVENT_SHOOTING_STAR || ev == SAVER_EVENT_SATELLITE) &&
        (newPhase == SKY_DAY || newPhase == SKY_DAWN)) lvglScreenSaverEndEvent();
  }

  if (g_saver.sky) {
    if (newPhase == SKY_NIGHT) {
      lv_label_set_text(g_saver.sky, "");
      lv_obj_add_flag(g_saver.sky, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(g_saver.sky, LV_OBJ_FLAG_HIDDEN);
      if (newPhase == SKY_DAY) g_saver.cloudOffset = 0;
    }
  }
}

static void lvglScreenSaverUpdateClouds(uint32_t nowMs) {
  if (!g_saver.sky) return;
  if (nowMs < g_saver.cloudNextMs) return;
  g_saver.cloudNextMs = nowMs + 2000UL;

  char buf[128];
  memset(buf, 0, sizeof(buf));

  if (g_saver.skyPhase == SKY_DAY) {
    const char *cloud = "_.--\"\"--._";
    const uint8_t cloudLen = 10;
    uint8_t spaces = g_saver.cloudOffset;
    uint8_t i = 0;
    buf[i++] = '\n';
    for (uint8_t s = 0; s < spaces && i < 100; ++s) buf[i++] = ' ';
    for (uint8_t c = 0; c < cloudLen && i < 115; ++c) buf[i++] = cloud[c];
    buf[i] = '\0';
    g_saver.cloudOffset = (uint8_t)((g_saver.cloudOffset + 1) % 60);
    lv_label_set_text(g_saver.sky, buf);
  } else if (g_saver.skyPhase == SKY_DAWN) {
    snprintf(buf, sizeof(buf), "\n\n\n\n\n          ..:::::..::..");
    lv_label_set_text(g_saver.sky, buf);
  } else if (g_saver.skyPhase == SKY_DUSK) {
    snprintf(buf, sizeof(buf), "\n\n\n\n\n          ::..:::..:::..");
    lv_label_set_text(g_saver.sky, buf);
  }
}

// --- Pasture Simulator: star borrowing + event system ---

static void lvglScreenSaverBorrowStar(uint8_t r, uint8_t s, const char *text,
                                       int16_t x, int16_t y, uint32_t color) {
  if (r >= kSaverSkyRowsMax || s >= kSaverStarsPerRow) return;
  const uint32_t bit = 1UL << (r * kSaverStarsPerRow + s);
  g_saver.starBorrowedMask |= bit;
  lv_obj_t *obj = g_saver.starObj[r][s];
  if (!obj) return;
  lv_label_set_text(obj, text);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void lvglScreenSaverReleaseStar(uint8_t r, uint8_t s) {
  if (r >= kSaverSkyRowsMax || s >= kSaverStarsPerRow) return;
  const uint32_t bit = 1UL << (r * kSaverStarsPerRow + s);
  g_saver.starBorrowedMask &= ~bit;
  lv_obj_t *obj = g_saver.starObj[r][s];
  if (!obj) return;
  lv_label_set_text(obj, ".");
  lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void lvglScreenSaverUpdateEvent(uint32_t nowMs) {
  if (g_saver.eventActive != SAVER_EVENT_NONE) {
    if (nowMs >= g_saver.eventEndMs) {
      lvglScreenSaverEndEvent();
      return;
    }
    const int16_t cW = canvasWidth();
    const int16_t cH = canvasHeight();
    switch (g_saver.eventActive) {
      case SAVER_EVENT_SHOOTING_STAR: {
        g_saver.eventX += 40 * g_saver.eventDir;
        const uint32_t elapsed = nowMs - (g_saver.eventEndMs - 1500UL);
        int16_t y = (int16_t)(8 + (elapsed / 55) * 4);
        if (y > cH - 40) y = (int16_t)(cH - 40);
        if (g_saver.eventX > cW || g_saver.eventX < -40) {
          lvglScreenSaverEndEvent();
          return;
        }
        lvglScreenSaverBorrowStar(g_saver.rows - 1, 0, "--*",
            g_saver.eventX, y, activeUiTheme().lvgl.saverStarHigh);
        break;
      }
      case SAVER_EVENT_UFO: {
        g_saver.eventX += 3 * g_saver.eventDir;
        if (g_saver.eventX > cW) g_saver.eventX = -30;
        lvglScreenSaverBorrowStar(g_saver.rows - 1, 0, "<==>",
            g_saver.eventX, 6, activeUiTheme().lvgl.saverStarHigh);
        break;
      }
      case SAVER_EVENT_SATELLITE: {
        g_saver.eventX += 2 * g_saver.eventDir;
        if (g_saver.eventX > cW || g_saver.eventX < -10) {
          lvglScreenSaverEndEvent();
          return;
        }
        lvglScreenSaverBorrowStar(g_saver.rows - 1, 0, ".",
            g_saver.eventX, 10, activeUiTheme().lvgl.saverStarMid);
        break;
      }
      case SAVER_EVENT_RAIN: {
        for (uint8_t i = 0; i < 4; ++i) {
          uint8_t rr = (uint8_t)(g_saver.rows - 2 + (i / 2));
          uint8_t ss = (uint8_t)(i % 2);
          if (rr >= kSaverSkyRowsMax) rr = (uint8_t)(kSaverSkyRowsMax - 1);
          lv_obj_t *obj = g_saver.starObj[rr][ss];
          if (!obj) continue;
          lv_coord_t cy = lv_obj_get_y(obj);
          cy += 6;
          if (cy > cH - 30) {
            cy = 4;
            lv_obj_set_x(obj, (lv_coord_t)(8 + (lvglScreenSaverRandNext() % (uint32_t)(cW - 20))));
          }
          const char *drop = ((lvglScreenSaverRandNext() & 1) == 0) ? "|" : "'";
          lvglScreenSaverBorrowStar(rr, ss, drop,
              lv_obj_get_x(obj), cy, activeUiTheme().lvgl.saverStarMid);
        }
        break;
      }
      case SAVER_EVENT_MATRIX_GLITCH: {
        for (uint8_t i = 0; i < 2; ++i) {
          uint8_t rr = (uint8_t)(g_saver.rows - 1);
          uint8_t ss = (uint8_t)(i % kSaverStarsPerRow);
          char ch[2] = { (char)('0' + (lvglScreenSaverRandNext() % 16)), '\0' };
          if (ch[0] > '9') ch[0] = (char)('A' + (ch[0] - '9' - 1));
          uint32_t color = lvglThemeIsCathodeRay() ? 0xFFAA00u : 0x00FF00u;
          int16_t y = (int16_t)(20 + i * 14);
          lvglScreenSaverBorrowStar(rr, ss, ch,
              g_saver.eventX, y, color);
        }
        break;
      }
      default: break;
    }
    return;
  }

  // No event active — check cooldown and maybe start one
  if (nowMs < g_saver.eventCooldownMs) return;

  const uint32_t roll = lvglScreenSaverRandNext() % 1000;
  const bool canNight = (g_saver.skyPhase == SKY_NIGHT || g_saver.skyPhase == SKY_DUSK);
  const bool canDay = (g_saver.skyPhase == SKY_DAY);
  const uint32_t baseWait = 300000UL + (lvglScreenSaverRandNext() % 300000UL);

  if (canNight && roll < 300) {
    g_saver.eventActive = SAVER_EVENT_SHOOTING_STAR;
    g_saver.eventEndMs = nowMs + 1500UL;
    g_saver.eventDir = ((lvglScreenSaverRandNext() & 1) == 0) ? 1 : -1;
    g_saver.eventX = (g_saver.eventDir > 0) ? -30 : canvasWidth();
    if (g_saver.cowState != COW_SLEEP) {
      g_saver.cowPrevState = g_saver.cowState;
      g_saver.cowState = COW_STARE_UP;
      g_saver.cowStateNextMs = g_saver.eventEndMs;
    }
    Serial.println("[SCRNSVR] event=shooting_star");
  } else if (canNight && roll < 350) {
    g_saver.eventActive = SAVER_EVENT_SATELLITE;
    g_saver.eventEndMs = nowMs + 20000UL;
    g_saver.eventDir = ((lvglScreenSaverRandNext() & 1) == 0) ? 1 : -1;
    g_saver.eventX = (g_saver.eventDir > 0) ? -10 : canvasWidth();
    if ((lvglScreenSaverRandNext() % 100) < 30 && g_saver.cowState != COW_SLEEP) {
      g_saver.cowPrevState = g_saver.cowState;
      g_saver.cowState = COW_STARE_UP;
      g_saver.cowStateNextMs = g_saver.eventEndMs;
    }
    Serial.println("[SCRNSVR] event=satellite");
  } else if (roll < 380) {
    g_saver.eventActive = SAVER_EVENT_UFO;
    g_saver.eventEndMs = nowMs + 8000UL;
    g_saver.eventDir = ((lvglScreenSaverRandNext() & 1) == 0) ? 1 : -1;
    g_saver.eventX = (g_saver.eventDir > 0) ? -30 : canvasWidth();
    if (g_saver.cowState != COW_SLEEP) {
      g_saver.cowPrevState = g_saver.cowState;
      g_saver.cowState = COW_STARE_UP;
      g_saver.cowStateNextMs = g_saver.eventEndMs;
    }
    Serial.println("[SCRNSVR] event=ufo");
  } else if (canDay && roll < 420) {
    g_saver.eventActive = SAVER_EVENT_RAIN;
    g_saver.eventEndMs = nowMs + 120000UL + (lvglScreenSaverRandNext() % 60000UL);
    g_saver.eventX = 0;
    for (uint8_t i = 0; i < 4; ++i) {
      uint8_t rr = (uint8_t)(g_saver.rows - 2 + (i / 2));
      uint8_t ss = (uint8_t)(i % 2);
      if (rr >= kSaverSkyRowsMax) rr = (uint8_t)(kSaverSkyRowsMax - 1);
      int16_t rx = (int16_t)(8 + (lvglScreenSaverRandNext() % (uint32_t)(canvasWidth() - 20)));
      int16_t ry = (int16_t)(4 + (lvglScreenSaverRandNext() % 60));
      lvglScreenSaverBorrowStar(rr, ss, "|", rx, ry,
          activeUiTheme().lvgl.saverStarMid);
    }
    g_saver.cowState = COW_IDLE;
    g_saver.cowStateNextMs = g_saver.eventEndMs;
    Serial.println("[SCRNSVR] event=rain");
  } else if (roll < 435) {
    g_saver.eventActive = SAVER_EVENT_MATRIX_GLITCH;
    g_saver.eventEndMs = nowMs + 3000UL;
    g_saver.eventX = (int16_t)(40 + (lvglScreenSaverRandNext() % (uint32_t)(canvasWidth() - 80)));
    Serial.println("[SCRNSVR] event=matrix_glitch");
  } else {
    g_saver.eventCooldownMs = nowMs + baseWait;
  }
}

static const char *const kSaverQuotesIt[] = {
    "Mastico erba e penso a Nietzsche.",
    "Ho quattro stomaci e zero risposte.",
    "Il libero arbitrio finisce al recinto elettrico.",
    "Tutti cercano il senso della vita. Io cerco trifoglio.",
    "Non sono pigra. Sono in contemplazione.",
    "Il mondo gira, l'erba cresce, io mastico.",
    "Di notte le stelle promettono troppo.",
    "Produco latte e dubbi esistenziali.",
};

static const char *const kSaverQuotesEn[] = {
    "I chew grass and think about Nietzsche.",
    "I have four stomachs and zero answers.",
    "Free will ends at the electric fence.",
    "Everyone seeks meaning. I seek clover.",
    "I am not lazy. I am contemplating.",
    "The world spins, grass grows, I chew.",
    "At night, stars overpromise.",
    "I produce milk and existential doubt.",
};

static const char *const kSaverQuotesFr[] = {
    "Je rumine de l'herbe et des idees noires.",
    "J'ai quatre estomacs et zero reponse.",
    "La liberte s'arrete au fil electrique.",
    "Tout le monde cherche le sens, moi le trefle.",
    "Je ne suis pas paresseuse, je contemple.",
    "La nuit, les etoiles promettent trop.",
};

static const char *const kSaverQuotesDe[] = {
    "Ich kaue Gras und denke an Nietzsche.",
    "Ich habe vier Magen und null Antworten.",
    "Freier Wille endet am Elektrozaun.",
    "Alle suchen Sinn, ich suche Klee.",
    "Ich bin nicht faul, ich kontempliere.",
    "Nachts versprechen Sterne zu viel.",
};

static const char *const kSaverQuotesEs[] = {
    "Mastico hierba y pienso en Nietzsche.",
    "Tengo cuatro estomagos y cero respuestas.",
    "El libre albedrio termina en la cerca electrica.",
    "Todos buscan sentido, yo busco trebol.",
    "No soy perezosa, estoy contemplando.",
    "De noche, las estrellas prometen de mas.",
};

static const char *const kSaverQuotesPt[] = {
    "Mastigo erva e penso em Nietzsche.",
    "Tenho quatro estomagos e zero respostas.",
    "O livre arbitrio acaba na cerca eletrica.",
    "Todo mundo busca sentido, eu busco trevo.",
    "Nao sou preguicosa, estou contemplando.",
    "A noite, as estrelas prometem demais.",
};

static const char *const kSaverQuotesLa[] = {
    "Herbam rumino et de Nietzsche cogito.",
    "Quattuor ventriculos habeo, responsa nulla.",
    "Arbitrium liberum ad saeptum electricum finitur.",
    "Omnes sensum quaerunt, ego trifolium quaero.",
    "Pigra non sum; contemplor.",
    "Mundus volvitur, herba crescit, rumino.",
};

static const char *const kSaverQuotesEo[] = {
    "Mi machas herbon kaj pensas pri Nietzsche.",
    "Mi havas kvar stomakojn kaj nul respondojn.",
    "Chiuj serchas sencon; mi serchas trifolion.",
    "Mi ne estas pigra, mi kontemplas.",
};

static const char *const kSaverQuotesTlh[] = {
    "yotlh vISoptaH, Nietzsche vIqel.",
    "loS burgh vIghaj, pagh jangmey.",
    "Saeptum tIq law', qabDaq yIQub.",
    "Qapla? nope. vIneHbogh: clover.",
};

static const char *const kSaverQuotesL33t[] = {
    "1 ch3w gr455 4nd th1nk 4b0u7 N137z5ch3.",
    "1 h4v3 4 570m4ch5 x4 n0 4n5w3r5.",
    "fr33 w1ll 3nd5 47 3l3c7r1c f3nc3.",
    "n07 l4zy, ju57 c0n73mpl471n9.",
};

static const char *const kSaverQuotesSha[] = {
    "I chew the meadow and converse with dread.",
    "Four stomachs have I, yet answers none.",
    "Free will doth end where fences hum.",
    "I seek not glory, only clover.",
};

static const char *const kSaverQuotesVal[] = {
    "Like, I chew grass and overthink everything.",
    "I have four stomachs, still zero clarity.",
    "Free will? Not with that electric fence.",
    "I am not lazy, I am vibing in thought.",
};

static const char *const kSaverQuotesBellazio[] = {
    "Bro, mastico e overpenso pesante.",
    "Zio, quattro stomaci e zero lore.",
    "Dai, free will finisce al recinto.",
    "Una roba tipo filosofia, ma col trifoglio.",
    "Onesto: non pigra, solo chill contemplativo.",
    "Le stelle hypeano troppo, bro.",
};

// --- Hacker quotes ---
static const char *const kSaverQuotesHackerEn[] = {
    "sudo rm -rf /grass",
    "my udder has 256 bits of entropy",
    "is this the matrix or just good pasture",
    "segfault in rumination module",
    "404: meaning not found",
    "I run on grass, not JavaScript",
    "localhost:8080/pasture",
    "have you tried turning the fence off and on",
};
static const char *const kSaverQuotesHackerIt[] = {
    "sudo rm -rf /erba",
    "la mia mammella ha 256 bit di entropia",
    "segfault nel modulo ruminazione",
    "404: senso non trovato",
    "funziono a erba, non a JavaScript",
    "hai provato a spegnere e riaccendere il recinto",
};
static const char *const kSaverQuotesHackerFr[] = {
    "sudo rm -rf /herbe",
    "mon pis a 256 bits d'entropie",
    "segfault dans le module rumination",
    "404: sens introuvable",
    "je tourne a l'herbe, pas au JavaScript",
};
static const char *const kSaverQuotesHackerDe[] = {
    "sudo rm -rf /gras",
    "mein Euter hat 256 Bit Entropie",
    "Segfault im Wiederkaumodul",
    "404: Sinn nicht gefunden",
    "ich laufe auf Gras, nicht JavaScript",
};
static const char *const kSaverQuotesHackerEs[] = {
    "sudo rm -rf /hierba",
    "mi ubre tiene 256 bits de entropia",
    "segfault en modulo de ruminacion",
    "404: sentido no encontrado",
    "funciono con hierba, no con JavaScript",
};
static const char *const kSaverQuotesHackerPt[] = {
    "sudo rm -rf /erva",
    "meu ubre tem 256 bits de entropia",
    "segfault no modulo de ruminacao",
    "404: sentido nao encontrado",
    "funciono com erva, nao com JavaScript",
};
static const char *const kSaverQuotesHackerLa[] = {
    "sudo rm -rf /herba",
    "uber meum CCLVI bits entropiae habet",
    "segfault in modulo ruminationis",
    "CDIV: sensus non inventus",
};

// --- Meta quotes ---
static const char *const kSaverQuotesMetaEn[] = {
    "I've been walking back and forth for hours",
    "someone is watching me on a tiny screen",
    "am I screensaver or screensavee",
    "this field is exactly 640 pixels wide",
    "the tree never moves. suspicious.",
};
static const char *const kSaverQuotesMetaIt[] = {
    "cammino avanti e indietro da ore",
    "qualcuno mi guarda su uno schermino",
    "sono io lo screensaver o lo screensavato",
    "questo campo e' largo esattamente 640 pixel",
    "l'albero non si muove mai. sospetto.",
};

// --- Weather quotes (English, selected by skyPhase) ---
static const char *const kSaverQuotesWeatherNightEn[] = {
    "nice stars tonight",
    "is that a satellite or a pixel",
    "3 AM and still no answers",
};
static const char *const kSaverQuotesWeatherDawnEn[] = {
    "another sunrise. still a cow.",
    "the gradient is beautiful today",
};
static const char *const kSaverQuotesWeatherDayEn[] = {
    "that cloud looks like a TCP packet",
    "solar powered contemplation",
};
static const char *const kSaverQuotesWeatherDuskEn[] = {
    "golden hour. still chewing.",
    "sunset commits are the best",
};
static const char *const kSaverQuotesWeatherRainEn[] = {
    "rain again. at least I'm not a server.",
    "cloud computing, literally",
};
static const char *const kSaverQuotesWeatherUfoEn[] = {
    "I saw something. nobody will believe me.",
};

// --- Easter egg quotes (shared across all languages) ---
static const char *const kSaverQuotesEasterEn[] = {
    "01101101 01101111 01101111",
    "mooooo",
    "< this space intentionally left blank >",
};

static void lvglScreenSaverQuotePackForLang(const char *const **items, uint8_t *count,
                                             uint8_t category) {
  if (!items || !count) return;

  #define QPACK(arr) do { *items = (arr); *count = (uint8_t)(sizeof(arr) / sizeof((arr)[0])); } while(0)

  if (category == THOUGHT_EASTER_EGG) { QPACK(kSaverQuotesEasterEn); return; }

  if (category == THOUGHT_WEATHER) {
    if (g_saver.eventActive == SAVER_EVENT_RAIN)       { QPACK(kSaverQuotesWeatherRainEn); return; }
    if (g_saver.eventActive == SAVER_EVENT_UFO)        { QPACK(kSaverQuotesWeatherUfoEn); return; }
    if (g_saver.skyPhase == SKY_DAWN)                  { QPACK(kSaverQuotesWeatherDawnEn); return; }
    if (g_saver.skyPhase == SKY_DAY)                   { QPACK(kSaverQuotesWeatherDayEn); return; }
    if (g_saver.skyPhase == SKY_DUSK)                  { QPACK(kSaverQuotesWeatherDuskEn); return; }
    QPACK(kSaverQuotesWeatherNightEn); return;
  }

  if (category == THOUGHT_META) {
    if (strcmp(g_wordClockLang, "it") == 0) { QPACK(kSaverQuotesMetaIt); return; }
    QPACK(kSaverQuotesMetaEn); return;
  }

  if (category == THOUGHT_HACKER) {
    QPACK(kSaverQuotesHackerEn);
    if (strcmp(g_wordClockLang, "it") == 0) QPACK(kSaverQuotesHackerIt);
    else if (strcmp(g_wordClockLang, "fr") == 0) QPACK(kSaverQuotesHackerFr);
    else if (strcmp(g_wordClockLang, "de") == 0) QPACK(kSaverQuotesHackerDe);
    else if (strcmp(g_wordClockLang, "es") == 0) QPACK(kSaverQuotesHackerEs);
    else if (strcmp(g_wordClockLang, "pt") == 0) QPACK(kSaverQuotesHackerPt);
    else if (strcmp(g_wordClockLang, "la") == 0) QPACK(kSaverQuotesHackerLa);
    return;
  }

  // Philosophy (default) — existing arrays
  *items = kSaverQuotesIt;
  *count = (uint8_t)(sizeof(kSaverQuotesIt) / sizeof(kSaverQuotesIt[0]));
  if (strcmp(g_wordClockLang, "en") == 0) { QPACK(kSaverQuotesEn); return; }
  if (strcmp(g_wordClockLang, "fr") == 0) { QPACK(kSaverQuotesFr); return; }
  if (strcmp(g_wordClockLang, "de") == 0) { QPACK(kSaverQuotesDe); return; }
  if (strcmp(g_wordClockLang, "es") == 0) { QPACK(kSaverQuotesEs); return; }
  if (strcmp(g_wordClockLang, "pt") == 0) { QPACK(kSaverQuotesPt); return; }
  if (strcmp(g_wordClockLang, "la") == 0) { QPACK(kSaverQuotesLa); return; }
  if (strcmp(g_wordClockLang, "eo") == 0) { QPACK(kSaverQuotesEo); return; }
  if (strcmp(g_wordClockLang, "tlh") == 0) { QPACK(kSaverQuotesTlh); return; }
  if (strcmp(g_wordClockLang, "l33t") == 0) { QPACK(kSaverQuotesL33t); return; }
  if (strcmp(g_wordClockLang, "sha") == 0) { QPACK(kSaverQuotesSha); return; }
  if (strcmp(g_wordClockLang, "val") == 0) { QPACK(kSaverQuotesVal); return; }
  if (strcmp(g_wordClockLang, "bellazio") == 0) { QPACK(kSaverQuotesBellazio); return; }

  #undef QPACK
}

static void toUpperAsciiInPlace(char *s) {
  if (!s) return;
  for (size_t i = 0; s[i]; ++i) {
    const unsigned char c = (unsigned char)s[i];
    if (c >= 'a' && c <= 'z') s[i] = (char)(c - ('a' - 'A'));
  }
}

static void lvglScreenSaverSetBalloonText() {
  if (!g_saver.balloon) return;
  const char *const *quotes = nullptr;
  uint8_t n = 0;
  // Roll thought category with weighted probabilities
  const uint32_t catRoll = lvglScreenSaverRandNext() % 100;
  uint8_t cat;
  if (catRoll < 30)      cat = THOUGHT_PHILOSOPHY;
  else if (catRoll < 60) cat = THOUGHT_HACKER;
  else if (catRoll < 75) cat = THOUGHT_META;
  else if (catRoll < 90) cat = THOUGHT_WEATHER;
  else                    cat = THOUGHT_EASTER_EGG;
  g_saver.thoughtCategory = cat;
  lvglScreenSaverQuotePackForLang(&quotes, &n, cat);
  if (!quotes || n == 0) return;
  if (g_saver.balloonIdx >= n) g_saver.balloonIdx = 0;
  if (n > 1U) {
    g_saver.balloonIdx =
        (uint8_t)((g_saver.balloonIdx + 1U + (lvglScreenSaverRandNext() % (n - 1U))) % n);
  }
  const char *quote = quotes[g_saver.balloonIdx];
  static char wrapped[256];
  uint8_t cols = lvglScreenSaverWrapCols();
  const lv_coord_t maxBalloonW = lvglScreenSaverBalloonMaxWidthPx();
  while (true) {
    lvglScreenSaverWrapQuote(quote, wrapped, sizeof(wrapped), cols, kScreenSaverThoughtMaxLines);
    if (cols <= 8) break;
    if (lvglScreenSaverMeasureBalloonTextWidth(wrapped) <= maxBalloonW) break;
    --cols;
  }
  if (strcmp(g_wordClockLang, "l33t") == 0) toUpperAsciiInPlace(wrapped);
  lv_label_set_text(g_saver.balloon, wrapped);
  lv_label_set_long_mode(g_saver.balloon, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(g_saver.balloon, lvglScreenSaverResolvedBalloonWidth(wrapped), LV_SIZE_CONTENT);
  lv_obj_update_layout(g_saver.balloon);
  if (g_saver.balloonTail) {
    const int16_t bw = lv_obj_get_width(g_saver.balloon);
    int16_t dashes = bw / 6;
    if (dashes < 8) dashes = 8;
    if (dashes > 20) dashes = 20;
    static char ruleBuf[48];
    int16_t k = 0;
    for (int16_t i = 0; i < dashes && (k + 2) < (int16_t)sizeof(ruleBuf); ++i) {
      ruleBuf[k++] = '-';
      if ((i & 1) == 0 && (k + 1) < (int16_t)sizeof(ruleBuf)) ruleBuf[k++] = ' ';
    }
    ruleBuf[k] = '\0';
    lv_label_set_text(g_saver.balloonTail, ruleBuf);
  }
}

static void lvglScreenSaverUpdateBalloon(uint32_t nowMs) {
  if (!g_saver.balloon) return;
  const uint32_t kCycleMs = 25000UL;   // 10s OFF + 15s ON
  const uint32_t kShowFromMs = 10000UL;
  const bool shouldShow = ((nowMs % kCycleMs) >= kShowFromMs);
  if (!shouldShow && g_saver.balloonVisible) {
    g_saver.balloonVisible = false;
    lv_obj_add_flag(g_saver.balloon, LV_OBJ_FLAG_HIDDEN);
    if (g_saver.balloonTail) lv_obj_add_flag(g_saver.balloonTail, LV_OBJ_FLAG_HIDDEN);
  } else if (shouldShow && !g_saver.balloonVisible) {
    g_saver.balloonVisible = true;
    lvglScreenSaverSetBalloonText();
    lvglForceLabelVisible(g_saver.balloon);
    if (g_saver.balloonTail) lvglForceLabelVisible(g_saver.balloonTail);
  }
}

static void lvglScreenSaverUpdateStars(uint32_t nowMs) {
  if (!g_saver.root) return;
  const UiThemeLvglTokens &t = activeUiTheme().lvgl;
  for (uint8_t r = 0; r < g_saver.rows; ++r) {
    for (uint8_t s = 0; s < kSaverStarsPerRow; ++s) {
      const uint32_t starBit = 1UL << (r * kSaverStarsPerRow + s);
      if (g_saver.starBorrowedMask & starBit) continue;
      if (nowMs < g_saver.starNextMs[r][s]) continue;
      uint8_t &lvl = g_saver.starLevel[r][s];
      int8_t &dir = g_saver.starDir[r][s];
      if (dir > 0) {
        if (lvl < 3) ++lvl;
        if (lvl >= 3) dir = -1;
        g_saver.starNextMs[r][s] = nowMs + 520UL;
      } else {
        if (lvl > 0) --lvl;
        if (lvl == 0) {
          dir = 1;
          g_saver.starNextMs[r][s] = nowMs + (3500UL + (lvglScreenSaverRandNext() % 8500UL));
        } else {
          g_saver.starNextMs[r][s] = nowMs + 520UL;
        }
      }
    }
  }
  for (uint8_t r = 0; r < g_saver.rows; ++r) {
    for (uint8_t s = 0; s < kSaverStarsPerRow; ++s) {
      const uint32_t starBit = 1UL << (r * kSaverStarsPerRow + s);
      if (g_saver.starBorrowedMask & starBit) continue;
      lv_obj_t *star = g_saver.starObj[r][s];
      if (!star) continue;
      const uint8_t lvl = g_saver.starLevel[r][s];
      if (lvl == 0) {
        lv_obj_add_flag(star, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_clear_flag(star, LV_OBJ_FLAG_HIDDEN);
        lv_color_t col = lv_color_hex(t.saverStarLow);
        if (lvl == 2) col = lv_color_hex(t.saverStarMid);
        else if (lvl >= 3) col = lv_color_hex(t.saverStarHigh);
        lv_label_set_text(star, (lvl >= 3) ? "o" : (lvl == 2 ? ":" : "."));
        lv_obj_set_style_text_color(star, col, 0);
      }
    }
  }
  // During day, hide all non-borrowed stars
  if (g_saver.skyPhase == SKY_DAY) {
    for (uint8_t r = 0; r < g_saver.rows; ++r) {
      for (uint8_t s = 0; s < kSaverStarsPerRow; ++s) {
        const uint32_t bit = 1UL << (r * kSaverStarsPerRow + s);
        if (g_saver.starBorrowedMask & bit) continue;
        if (g_saver.starObj[r][s]) lv_obj_add_flag(g_saver.starObj[r][s], LV_OBJ_FLAG_HIDDEN);
      }
    }
  }
  // During dawn, cap max star brightness
  if (g_saver.skyPhase == SKY_DAWN) {
    for (uint8_t r = 0; r < g_saver.rows; ++r) {
      for (uint8_t s = 0; s < kSaverStarsPerRow; ++s) {
        const uint32_t bit = 1UL << (r * kSaverStarsPerRow + s);
        if (g_saver.starBorrowedMask & bit) continue;
        if (g_saver.starLevel[r][s] > 2) g_saver.starLevel[r][s] = 2;
      }
    }
  }
  // During dusk, cap brightness and show fewer stars
  if (g_saver.skyPhase == SKY_DUSK) {
    for (uint8_t r = 0; r < g_saver.rows; ++r) {
      for (uint8_t s = 0; s < kSaverStarsPerRow; ++s) {
        const uint32_t bit = 1UL << (r * kSaverStarsPerRow + s);
        if (g_saver.starBorrowedMask & bit) continue;
        if (g_saver.starLevel[r][s] > 2) g_saver.starLevel[r][s] = 2;
        if (s == 1 && (r % 3) != 0 && g_saver.starObj[r][s]) {
          lv_obj_add_flag(g_saver.starObj[r][s], LV_OBJ_FLAG_HIDDEN);
        }
      }
    }
  }
}

// --- Pasture Simulator: cow state machine ---

static void lvglScreenSaverTransitionCow(uint32_t nowMs) {
  // Exiting STARE_UP: restore previous state
  if (g_saver.cowState == COW_STARE_UP) {
    uint8_t prev = g_saver.cowPrevState;
    if (prev == COW_SLEEP && g_saver.skyPhase != SKY_NIGHT) prev = COW_GRAZE;
    g_saver.cowState = prev;
    g_saver.cowStateNextMs = nowMs + 3000UL + (lvglScreenSaverRandNext() % 5000UL);
    lvglScreenSaverSetCowArt(g_saver.cowDir);
    Serial.printf("[SCRNSVR] cowState=%u (restored)\n", prev);
    return;
  }

  const uint32_t roll = lvglScreenSaverRandNext() % 100;
  uint8_t newState;

  if (g_saver.skyPhase == SKY_NIGHT) {
    if (roll < 60)      newState = COW_SLEEP;
    else if (roll < 80) newState = COW_IDLE;
    else if (roll < 95) newState = COW_GRAZE;
    else                newState = COW_RUN;
  } else {
    if (roll < 50)      newState = COW_GRAZE;
    else if (roll < 80) newState = COW_IDLE;
    else                newState = COW_RUN;
  }

  g_saver.cowState = newState;
  lvglScreenSaverSetCowArt(g_saver.cowDir);

  switch (newState) {
    case COW_GRAZE:
      g_saver.cowStateNextMs = nowMs + 8000UL + (lvglScreenSaverRandNext() % 12000UL);
      g_saver.cowChewNextMs = nowMs + 400UL;
      break;
    case COW_IDLE:
      g_saver.cowStateNextMs = nowMs + 3000UL + (lvglScreenSaverRandNext() % 5000UL);
      g_saver.cowStepsLeft = 0;
      break;
    case COW_SLEEP:
      g_saver.cowStateNextMs = nowMs + 30000UL + (lvglScreenSaverRandNext() % 90000UL);
      g_saver.cowStepsLeft = 0;
      break;
    case COW_RUN:
      g_saver.cowStateNextMs = nowMs + 3000UL + (lvglScreenSaverRandNext() % 2000UL);
      g_saver.cowStepsLeft = 20;
      break;
    default: break;
  }
  Serial.printf("[SCRNSVR] cowState=%u\n", newState);
}

static void lvglScreenSaverRespawnCow() {
  const int16_t h = canvasHeight();
  g_saver.y = (h > 96) ? (h - 90) : 14;
  if (g_saver.x < 16 || g_saver.x > (canvasWidth() - 112)) {
    g_saver.x = (int16_t)(24 + (lvglScreenSaverRandNext() % 180U));
  }
  g_saver.colorIdx = (int8_t)((g_saver.colorIdx + 1) % 3);
  if (g_saver.cow) {
    const UiThemeLvglTokens &t = activeUiTheme().lvgl;
    (void)g_saver.colorIdx;
    lv_obj_set_style_text_color(g_saver.cow, lv_color_hex(t.saverCow), 0);
    lvglScreenSaverSetCowArt(g_saver.cowDir);
    lv_obj_set_pos(g_saver.cow, g_saver.x, g_saver.y);
  }
  g_saver.cowStepsLeft = 0;
  g_saver.cowDir = ((lvglScreenSaverRandNext() & 1U) == 0U) ? 1 : -1;
  lvglScreenSaverSetCowArt(g_saver.cowDir);
  g_saver.cowNextMoveMs = millis() + (1000UL + (lvglScreenSaverRandNext() % 5000UL));
  g_saver.balloonVisible = false;
  g_saver.balloonNextMs = millis() + 15000UL;
}

static void lvglSetScreenSaverActive(bool on) {
  if (!g_saver.root || !g_lvglReady) return;
  if (g_saver.active == on) return;
  g_saver.active = on;
  if (on) {
    lv_obj_clear_flag(g_saver.root, LV_OBJ_FLAG_HIDDEN);
    g_saver.cols = (uint8_t)((canvasWidth() / 8) - 2);
    if (g_saver.cols > kSaverSkyColsMax) g_saver.cols = kSaverSkyColsMax;
    if (g_saver.cols < 36) g_saver.cols = 36;
    g_saver.rows = (uint8_t)(((canvasHeight() - 68) / 12));
    if (g_saver.rows > kSaverSkyRowsMax) g_saver.rows = kSaverSkyRowsMax;
    if (g_saver.rows < 4) g_saver.rows = 4;
    lvglScreenSaverInitStars();
    g_saver.fieldScroll = (uint8_t)(lvglScreenSaverRandNext() & 0x0FU);
    g_saver.fieldNextMs = millis() + 1200UL;
    g_saver.footerJitterIdx = (uint8_t)(lvglScreenSaverRandNext() % 4U);
    g_saver.footerJitterNextMs = millis() + 10000UL;
    lvglScreenSaverBuildFieldLine();
    lvglScreenSaverUpdateStars(millis());
    lvglScreenSaverRespawnCow();
    // Pasture Simulator state init
    g_saver.eventActive = SAVER_EVENT_NONE;
    g_saver.eventEndMs = 0;
    g_saver.eventCooldownMs = millis() + 30000UL;
    g_saver.starBorrowedMask = 0;
    g_saver.cowState = COW_GRAZE;
    g_saver.cowChewFrame = 0;
    g_saver.cowChewNextMs = millis() + 400UL;
    g_saver.cowStateNextMs = millis() + 8000UL + (lvglScreenSaverRandNext() % 12000UL);
    g_saver.thoughtCategory = THOUGHT_PHILOSOPHY;
    g_saver.cloudOffset = 0;
    g_saver.cloudNextMs = 0;
    g_saver.cowPrevState = COW_GRAZE;
    g_saver.skyNextMs = 0;
    lvglScreenSaverUpdateSkyPhase(millis());
    lvglScreenSaverUpdateFooter(millis());
    g_saver.lastStepMs = millis();
    lv_obj_move_foreground(g_saver.root);
    if (g_saver.footer) lv_obj_clear_flag(g_saver.footer, LV_OBJ_FLAG_HIDDEN);
    if (g_saver.balloon) lv_obj_add_flag(g_saver.balloon, LV_OBJ_FLAG_HIDDEN);
    if (g_saver.balloonTail) lv_obj_add_flag(g_saver.balloonTail, LV_OBJ_FLAG_HIDDEN);
    Serial.println("[SCRNSVR] ON");
  } else {
    lv_obj_add_flag(g_saver.root, LV_OBJ_FLAG_HIDDEN);
    if (g_saver.footer) lv_obj_add_flag(g_saver.footer, LV_OBJ_FLAG_HIDDEN);
    if (g_saver.balloon) lv_obj_add_flag(g_saver.balloon, LV_OBJ_FLAG_HIDDEN);
    if (g_saver.balloonTail) lv_obj_add_flag(g_saver.balloonTail, LV_OBJ_FLAG_HIDDEN);
    g_saver.wakeGuardUntilMs = millis() + 900UL;
    g_saver.eventActive = SAVER_EVENT_NONE;
    g_saver.starBorrowedMask = 0;
    g_uiNeedsRedraw = true;
    Serial.println("[SCRNSVR] OFF");
  }
}

static void handleScreenSaverLoop(uint32_t nowMs) {
  if (!g_lvglReady || !g_saver.root) return;
#if TEST_TOUCH
  const bool rawTouch = isAnyTouchPresentRaw();
  if (rawTouch) {
    if (g_touch.rawPresenceCount < 6) ++g_touch.rawPresenceCount;
  } else {
    g_touch.rawPresenceCount = 0;
  }
  const bool touching = g_touch.down || (g_touch.rawPresenceCount >= 2);
#else
  const bool rawTouch = false;
  const bool touching = false;
#endif
  if (!g_saver.active && g_saver.lastUserInteractionMs == 0) g_saver.lastUserInteractionMs = nowMs;

  if (!g_saver.active) {
    if (nowMs < g_saver.wakeGuardUntilMs) return;
    // Never activate screensaver while a QR modal overlay is open
    if (g_auxDeck.qrModalOpen || g_wikiDeck.qrModalOpen) return;
    const uint32_t idleTargetMs = lvglScreenSaverIdleTargetMs(nowMs);
    if (!rawTouch && !touching && (nowMs - g_saver.lastUserInteractionMs) >= idleTargetMs) {
      lvglSetScreenSaverActive(true);
    }
    return;
  }

  if (touching) {
    lvglSetScreenSaverActive(false);
    markUserInteraction(nowMs);
    return;
  }
#if TEST_IMU
  if (g_imu.lastShakeMs != 0 && (nowMs - g_imu.lastShakeMs) < 1200UL) {
    lvglSetScreenSaverActive(false);
    markUserInteraction(nowMs);
    return;
  }
#endif

  if ((nowMs - g_saver.lastStepMs) < SCREENSAVER_STEP_MS) return;
  g_saver.lastStepMs = nowMs;
  lvglScreenSaverUpdateSkyPhase(nowMs);
  lvglScreenSaverUpdateClouds(nowMs);
  lvglScreenSaverUpdateEvent(nowMs);
  lvglScreenSaverUpdateStars(nowMs);
  lvglScreenSaverUpdateField(nowMs);
  lvglScreenSaverUpdateBalloon(nowMs);
  lvglScreenSaverUpdateFooter(nowMs);

  // Cow state machine transitions
  if (nowMs >= g_saver.cowStateNextMs && g_saver.cowState != COW_STARE_UP) {
    lvglScreenSaverTransitionCow(nowMs);
  }
  // STARE_UP exit when event ends
  if (g_saver.cowState == COW_STARE_UP && nowMs >= g_saver.cowStateNextMs) {
    lvglScreenSaverTransitionCow(nowMs);
  }

  // Chew animation (only during GRAZE)
  if (g_saver.cowState == COW_GRAZE && nowMs >= g_saver.cowChewNextMs) {
    g_saver.cowChewFrame = (uint8_t)(1 - g_saver.cowChewFrame);
    g_saver.cowChewNextMs = nowMs + 400UL;
    lvglScreenSaverSetCowArt(g_saver.cowDir);
  }

  // Movement (GRAZE and RUN move, others don't)
  if ((g_saver.cowState == COW_GRAZE || g_saver.cowState == COW_RUN) &&
      nowMs >= g_saver.cowNextMoveMs) {
    const uint8_t stepPx = (g_saver.cowState == COW_RUN) ? 18 : 6;
    const uint32_t stepMs = (g_saver.cowState == COW_RUN) ? 120UL : 180UL;
    bool dirChanged = false;
    if (g_saver.cowStepsLeft == 0) {
      g_saver.cowStepsLeft = (uint8_t)(2U + (lvglScreenSaverRandNext() % 5U));
      if ((lvglScreenSaverRandNext() % 5U) == 0U) {
        g_saver.cowDir = -g_saver.cowDir;
        dirChanged = true;
      }
    }
    const int16_t minX = 8;
    const int16_t maxX = canvasWidth() - 250;
    int16_t nx = (int16_t)(g_saver.x + (g_saver.cowDir * stepPx));
    if (nx < minX) {
      nx = minX;
      g_saver.cowDir = 1;
      dirChanged = true;
    } else if (nx > maxX) {
      nx = maxX;
      g_saver.cowDir = -1;
      dirChanged = true;
    }
    if (dirChanged) lvglScreenSaverSetCowArt(g_saver.cowDir);
    g_saver.x = nx;
    if (g_saver.cowStepsLeft > 0) --g_saver.cowStepsLeft;
    g_saver.cowNextMoveMs = nowMs + ((g_saver.cowStepsLeft > 0) ? stepMs :
        (1000UL + (lvglScreenSaverRandNext() % 5000UL)));
  }

  // IDLE: occasional head turn
  if (g_saver.cowState == COW_IDLE && (lvglScreenSaverRandNext() % 200) == 0) {
    g_saver.cowDir = -g_saver.cowDir;
    lvglScreenSaverSetCowArt(g_saver.cowDir);
  }

  if (g_saver.cow) {
    lv_obj_set_pos(g_saver.cow, g_saver.x, g_saver.y);
  }
  if (g_saver.balloon && g_saver.balloonVisible) {
    const int16_t bw = lv_obj_get_width(g_saver.balloon);
    const int16_t bh = lv_obj_get_height(g_saver.balloon);
    int16_t bx = g_saver.x + ((g_saver.cowDir >= 0) ? 120 : -bw + 96);
    const int16_t maxX = canvasWidth() - bw - 8;
    if (bx < 8) bx = 8;
    if (bx > maxX) bx = maxX;
    const int16_t by = g_saver.y - bh - 10;
    lv_obj_set_pos(g_saver.balloon, bx, (by < 4) ? 4 : by);
    if (g_saver.balloonTail) {
      lv_obj_set_pos(g_saver.balloonTail, bx, lv_obj_get_y(g_saver.balloon) + bh + 2);
    }
  }
}
#endif

#if TEST_TOUCH
static bool initTouchInput() {
  I2C_MAIN.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  I2C_MAIN.setClock(400000);
  I2C_MAIN.beginTransmission(TOUCH_I2C_ADDR);
  const uint8_t errMain = I2C_MAIN.endTransmission();

  I2C_ALT.begin(I2C_SDA_PIN_ALT, I2C_SCL_PIN_ALT);
  I2C_ALT.setClock(400000);
  I2C_ALT.beginTransmission(TOUCH_I2C_ADDR);
  const uint8_t errAlt = I2C_ALT.endTransmission();

  // On this board wiring the touch controller is on ALT bus; prefer ALT when both ACK.
  if (errAlt == 0) {
    g_touch.ready = true;
    g_touch.useAltBus = true;
  } else if (errMain == 0) {
    g_touch.ready = true;
    g_touch.useAltBus = false;
  } else {
    g_touch.ready = false;
  }

  Serial.printf("[TOUCH] probe addr=0x%02X main=%d alt=%d -> %s (%s)\n",
                TOUCH_I2C_ADDR, errMain, errAlt,
                g_touch.ready ? "OK" : "FAIL",
                g_touch.ready ? (g_touch.useAltBus ? "ALT" : "MAIN") : "-");
  return g_touch.ready;
}

static bool readTouchLogicalPoint(int16_t &lx, int16_t &ly) {
  if (!g_touch.ready) return false;
  static const uint8_t kReadCmd[11] = {0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00};
  uint8_t data[8] = {0};

  TwoWire &tb = g_touch.useAltBus ? I2C_ALT : I2C_MAIN;
  tb.beginTransmission(TOUCH_I2C_ADDR);
  tb.write(kReadCmd, sizeof(kReadCmd));
  if (tb.endTransmission() != 0) return false;

  const int n = tb.requestFrom((int)TOUCH_I2C_ADDR, 8);
  if (n != 8) return false;
  for (int i = 0; i < 8; ++i) data[i] = tb.read();
  const uint8_t points = data[1];
  if (points == 0) return false;
  const int16_t rawX = ((data[2] & 0x0F) << 8) | data[3];
  const int16_t rawY = ((data[4] & 0x0F) << 8) | data[5];
  // AXS touch returns 0xFFF coordinates when no finger is present.
  if (rawX >= 0x0FFF || rawY >= 0x0FFF) return false;
  // Reject out-of-panel raw values (observed on ghost frames while idle).
  if (rawX >= canvasWidth() || rawY >= canvasHeight()) return false;
  // Some panels intermittently report a phantom (0,0) point when idle.
  if (rawX == 0 && rawY == 0) return false;
  // Canonical desk orientation: USB-C on the left side.
  // If DISPLAY_FLIP_180 is enabled, panel output is already mirrored on both axes.
  int32_t tx = rawX;
  int32_t ty = rawY;
#if !DISPLAY_FLIP_180
  tx = (int32_t)canvasWidth() - 1 - (int32_t)rawX;
  ty = (int32_t)canvasHeight() - 1 - (int32_t)rawY;
#endif
  if (tx < 0) tx = 0;
  if (ty < 0) ty = 0;
  if (tx >= canvasWidth()) tx = canvasWidth() - 1;
  if (ty >= canvasHeight()) ty = canvasHeight() - 1;
  lx = (int16_t)tx;
  ly = (int16_t)ty;
  if (lx < 0) lx = 0;
  if (ly < 0) ly = 0;
  if (lx >= canvasWidth()) lx = canvasWidth() - 1;
  if (ly >= canvasHeight()) ly = canvasHeight() - 1;
  return true;
}

static bool isAnyTouchPresentRaw() {
  if (!g_touch.ready) return false;
  static const uint8_t kReadCmd[11] = {0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00};
  uint8_t data[8] = {0};
  TwoWire &tb = g_touch.useAltBus ? I2C_ALT : I2C_MAIN;
  tb.beginTransmission(TOUCH_I2C_ADDR);
  tb.write(kReadCmd, sizeof(kReadCmd));
  if (tb.endTransmission() != 0) return false;
  const int n = tb.requestFrom((int)TOUCH_I2C_ADDR, 8);
  if (n != 8) return false;
  for (int i = 0; i < 8; ++i) data[i] = tb.read();
  const uint8_t points = data[1];
  if (points == 0) return false;
  const int16_t rawX = ((data[2] & 0x0F) << 8) | data[3];
  const int16_t rawY = ((data[4] & 0x0F) << 8) | data[5];
  if (rawX >= 0x0FFF || rawY >= 0x0FFF) return false;
  if (rawX == 0 && rawY == 0) return false;
  if (rawX >= canvasWidth() || rawY >= canvasHeight()) return false;
  return true;
}

// ── M6: Release handlers for touch gestures ──
// (TouchReleaseInfo is defined in config.h for Arduino auto-prototype visibility.
//  auxBtnDown is uint8_t there; cast to TouchAuxButton in handlers.)

// ── M6: Release handler — feed deck buttons (QR/SKIP/NXT) ──

static void handleFeedDeckButtonRelease(const TouchReleaseInfo &r) {
  Serial.printf("[TOUCH] btn-up kind=%u tap=%d dx=%d dy=%d dur=%lums\n",
                (unsigned)(TouchAuxButton)r.auxBtnDown, r.isBtnTap ? 1 : 0, r.dx, r.dy, (unsigned long)r.durMs);
  if (r.isBtnTap && uiPageIsFeedDeck(g_uiPageMode)) {
    RssState &content = (g_uiPageMode == UI_PAGE_WIKI) ? g_wiki : g_rss;
    const char *tag = (g_uiPageMode == UI_PAGE_WIKI) ? "wiki" : "rss";
    if ((TouchAuxButton)r.auxBtnDown == TOUCH_AUX_BTN_QR) {
      if (lvglFeedQrModalIsOpen()) {
        lvglSetFeedQrModalOpen(false);
        Serial.println("[TOUCH] qr-close");
      } else {
        lv_obj_t *feedStatus = (g_uiPageMode == UI_PAGE_WIKI) ? g_wikiDeck.status : g_auxDeck.status;
        if (feedStatus) { lv_label_set_text(feedStatus, "QR..."); lvglForceLabelVisible(feedStatus); }
        lvglSetFeedQrModalOpen(true);
        Serial.println("[TOUCH] qr-open");
      }
    } else if ((TouchAuxButton)r.auxBtnDown == TOUCH_AUX_BTN_REFRESH) {
#if TEST_WIFI && RSS_ENABLED
      {
        lv_obj_t *feedStatus = (g_uiPageMode == UI_PAGE_WIKI) ? g_wikiDeck.status : g_auxDeck.status;
        if (feedStatus) { lv_label_set_text(feedStatus, "SKIP"); lvglForceLabelVisible(feedStatus); }
      }
      const bool moved = (g_uiPageMode == UI_PAGE_WIKI) ? wikiAdvanceToNextItem() : rssAdvanceToNextItem();
      if (moved) g_uiNeedsRedraw = true;
      Serial.printf("[TOUCH] %s-skip moved=%d idx=%u/%u\n", tag, moved ? 1 : 0,
                    (unsigned)(content.currentIndex + 1), (unsigned)content.itemCount);
#endif
    } else if ((TouchAuxButton)r.auxBtnDown == TOUCH_AUX_BTN_NEXT) {
#if TEST_WIFI && RSS_ENABLED
      {
        lv_obj_t *feedStatus = (g_uiPageMode == UI_PAGE_WIKI) ? g_wikiDeck.status : g_auxDeck.status;
        if (feedStatus) { lv_label_set_text(feedStatus, "FEED"); lvglForceLabelVisible(feedStatus); }
      }
      const bool moved = (g_uiPageMode == UI_PAGE_WIKI) ? wikiAdvanceToNextFeed() : rssAdvanceToNextFeed();
      if (moved) g_uiNeedsRedraw = true;
      Serial.printf("[TOUCH] %s-next-feed moved=%d idx=%u/%u\n", tag, moved ? 1 : 0,
                    (unsigned)(content.currentIndex + 1), (unsigned)content.itemCount);
#endif
    }
  }
  g_touch.awaitRelease = true;
  g_touch.releaseStartMs = 0;
}

// ── M6: Release handler — LVGL page drag commit/cancel ──

static void handlePageDragRelease(const TouchReleaseInfo &r) {
  g_touch.pageDragging = false;
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI
  g_pageAnim.dragActive = false;
  if (uiPageIsFeedDeck(g_uiPageMode) && lvglFeedQrModalIsOpen()) {
    lvglApplyPageVisibility(true);
    g_touch.awaitRelease = true;
    g_touch.releaseStartMs = 0;
    Serial.printf("[TOUCH] drag ignored (qr-open) dx=%d dy=%d dur=%lums\n", r.dx, r.dy, (unsigned long)r.durMs);
    return;
  }
  if (r.durMs <= 3000 && r.pageSwipe && ((millis() - g_touch.lastSwipeToggleMs) >= 140)) {
    const int8_t dir = (r.dx < 0) ? 1 : -1;
    bool moved = stepUiPage(dir, false);
    g_touch.lastSwipeToggleMs = millis();
    g_touch.awaitRelease = true;
    g_touch.releaseStartMs = 0;
    Serial.printf("[TOUCH] drag-swipe dx=%d dy=%d dur=%lums -> page=%s moved=%d\n",
                  r.dx, r.dy, (unsigned long)r.durMs, uiPageName(g_uiPageMode), moved ? 1 : 0);
  } else {
    lvglApplyPageVisibility(true);
    g_touch.awaitRelease = true;
    g_touch.releaseStartMs = 0;
    Serial.printf("[TOUCH] drag-cancel dx=%d dy=%d dur=%lums\n", r.dx, r.dy, (unsigned long)r.durMs);
  }
#endif
}

// ── M6: Release handler — DOOM touch (USE/FIRE/recenter/swipe-exit) ──

#if TEST_DISPLAY && DOOM_SPIKE_ENABLED
static void handleDoomTouchRelease(const TouchReleaseInfo &r) {
  if (r.pageSwipe) {
    UiPageMode doomExit = uiLastEnabledMainViewNoEnsure();
    const int8_t doomOrd = uiPageOrdinal(UI_PAGE_DOOM);
    if (doomOrd > 0) doomExit = uiPageFromOrdinal(doomOrd - 1);
    setUiPage(doomExit);
    g_touch.lastSwipeToggleMs = millis();
    g_touch.awaitRelease = true;
    g_touch.releaseStartMs = 0;
    Serial.printf("[DOOM][TOUCH] swipe-exit zone=%s dx=%d dy=%d dur=%lums -> %s\n",
                  doomTouchZoneName(r.doomTouchZone), r.dx, r.dy, (unsigned long)r.durMs, uiPageName(g_uiPageMode));
    return;
  }
  if (r.doomTouchZone == DOOM_TOUCH_LEFT || r.doomTouchZone == DOOM_TOUCH_RIGHT) {
    bool doomLaunching = g_doom.launchRequested;
#if DB_HAS_PRBOOM_DONOR
    doomLaunching = doomLaunching || doomPrboomIsRunning();
#endif
    if (!doomLaunching && r.doomTouchZone == DOOM_TOUCH_RIGHT && r.isTap) {
      g_doom.launchRequested = true;
      g_doom.frameDirty = true;
#if DB_HAS_PRBOOM_DONOR
      doomPrboomEnsureStarted();
#endif
      Serial.printf("[DOOM][TOUCH] zone=%s action=START tap=1 -> boot core\n",
                    doomTouchZoneName(r.doomTouchZone));
      g_touch.awaitRelease = true;
      g_touch.releaseStartMs = 0;
      return;
    }
    const char *action = (r.doomTouchZone == DOOM_TOUCH_LEFT) ? "USE" : "FIRE";
    Serial.printf("[DOOM][TOUCH] zone=%s action=%s tap=%d dx=%d dy=%d dur=%lums\n",
                  doomTouchZoneName(r.doomTouchZone), action, r.isTap ? 1 : 0, r.dx, r.dy, (unsigned long)r.durMs);
    g_touch.awaitRelease = true;
    g_touch.releaseStartMs = 0;
    return;
  }
  if (r.isTap) {
    doomRequestNeutralCalibrate();
    g_touch.awaitRelease = true;
    g_touch.releaseStartMs = 0;
    Serial.printf("[DOOM][TOUCH] center-tap recalibrate x=%d y=%d\n", g_touch.startX, g_touch.startY);
  }
}
#endif

// ── M6: Release handler — generic carousel swipe ──

static void handleCarouselSwipe(const TouchReleaseInfo &r) {
#if TEST_DISPLAY
  if (g_uiPageMode == UI_PAGE_DOOM && r.pageSwipe) {
    UiPageMode doomExit = uiLastEnabledMainViewNoEnsure();
    const int8_t doomOrd = uiPageOrdinal(UI_PAGE_DOOM);
    if (doomOrd > 0) doomExit = uiPageFromOrdinal(doomOrd - 1);
    setUiPage(doomExit);
    g_touch.lastSwipeToggleMs = millis();
    g_touch.awaitRelease = true;
    g_touch.releaseStartMs = 0;
    Serial.printf("[TOUCH] doom-exit dx=%d dy=%d tap=%d -> %s\n", r.dx, r.dy, r.isTap ? 1 : 0, uiPageName(g_uiPageMode));
    return;
  }
#endif
  if (r.pageSwipe) {
    const int8_t dir = (r.dx < 0) ? 1 : -1;
    bool moved = stepUiPage(dir, false);
    g_touch.lastSwipeToggleMs = millis();
    g_touch.awaitRelease = true;
    g_touch.releaseStartMs = 0;
    const char *dirLabel = (r.dx < 0) ? "LEFT" : "RIGHT";
    Serial.printf("[TOUCH] swipe %s dx=%d dy=%d dur=%lums -> page=%s moved=%d\n",
                  dirLabel, r.dx, r.dy, (unsigned long)r.durMs, uiPageName(g_uiPageMode), moved ? 1 : 0);
  }
}

// ── M6: Release handler — feed deck tap (news area + QR overlay close) ──

static void handleFeedDeckTapRelease(const TouchReleaseInfo &r) {
  if (r.isTap && uiPageIsFeedDeck(g_uiPageMode) &&
      (lvglFeedNewsContainsPoint(g_touch.startX, g_touch.startY) ||
       lvglAuxHeroContainsPoint(g_touch.startX, g_touch.startY))) {
#if TEST_WIFI && RSS_ENABLED
    const bool moved = (g_uiPageMode == UI_PAGE_WIKI) ? wikiAdvanceToNextItem() : rssAdvanceToNextItem();
    RssState &content = (g_uiPageMode == UI_PAGE_WIKI) ? g_wiki : g_rss;
    const char *tag = (g_uiPageMode == UI_PAGE_WIKI) ? "wiki" : "rss";
    if (moved) {
      g_uiNeedsRedraw = true;
      g_touch.awaitRelease = true;
      g_touch.releaseStartMs = 0;
      Serial.printf("[TOUCH] aux-news-tap -> %s %u/%u\n", tag,
                    (unsigned)(content.currentIndex + 1), (unsigned)content.itemCount);
      return;
    }
#endif
  }
  // In AUX/WIKI, ignore neutral taps not on actionable regions.
  if (r.isTap && uiPageIsFeedDeck(g_uiPageMode)) return;
  // LAUNCH: tap to open/close QR overlay.
  if (r.isTap && g_uiPageMode == UI_PAGE_LAUNCH) {
    if (g_launchUi.qrModalOpen) {
      lvglCloseLaunchQr();
      g_uiNeedsRedraw = true;
      Serial.println("[TOUCH] launch-qr-close");
      return;
    }
    const int16_t ty = g_touch.startY;
    if (ty < 33) return;  // header tap, ignore
    if (g_launchUi.viewIndex == 0) {
      // View 0: tap anywhere below header → QR for mission 0
      lvglOpenLaunchQr(0);
      g_uiNeedsRedraw = true;
    } else {
      // View 1: top half → mission 1, bottom half → mission 2
      const int16_t midY = 33 + (canvasHeight() - 33) / 2;
      if (ty < midY && g_launchState.count > 1) { lvglOpenLaunchQr(1); g_uiNeedsRedraw = true; }
      else if (ty >= midY && g_launchState.count > 2) { lvglOpenLaunchQr(2); g_uiNeedsRedraw = true; }
    }
    return;
  }
  // TRANSIT: tap anywhere toggles origin/terminus display (auto-reverts after 8 s).
  if (r.isTap && g_uiPageMode == UI_PAGE_TRANSIT) {
    g_transitOrgMode   = !g_transitOrgMode;
    g_transitOrgModeMs = millis();
    g_uiNeedsRedraw    = true;
    Serial.printf("[TOUCH] transit-tap -> orgMode=%d\n", (int)g_transitOrgMode);
    return;
  }
  // HOME: tap left panel toggles clock mode.
  if (r.isTap && g_uiPageMode == UI_PAGE_HOME &&
      g_touch.startX < (canvasWidth() - DISPLAY_WEATHER_PANEL_W)) {
    toggleClockMode();
    Serial.printf("[TOUCH] tap x=%d y=%d -> mode=%s\n",
                  g_touch.startX, g_touch.startY, uiClockModeName(g_clock.mode));
  }
}

// ── M6: Orchestrator — touch state machine ──

static void handleTouchSwipeInput() {
  int16_t x = 0, y = 0;
  const bool touched = readTouchLogicalPoint(x, y);
  const uint32_t now = millis();

  // ── Phase 1: Screensaver wake + release gate ──
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI && SCREENSAVER_ENABLED
  if (touched) {
    markUserInteraction(now);
    if (g_saver.active) {
      lvglSetScreenSaverActive(false);
      g_touch.down = false; g_touch.pageDragging = false;
      g_touch.auxBtnDown = TOUCH_AUX_BTN_NONE;
      g_touch.awaitRelease = true; g_touch.releaseStartMs = 0;
      return;
    }
  }
#endif
  if (g_touch.awaitRelease) {
    if (touched) { g_touch.releaseStartMs = 0; return; }
    if (g_touch.releaseStartMs == 0) { g_touch.releaseStartMs = now; return; }
    if ((now - g_touch.releaseStartMs) < 70) return;
    g_touch.awaitRelease = false; g_touch.releaseStartMs = 0;
    return;
  }

  // ── Phase 2: Touch-down registration + drag tracking ──
  if (touched) {
    g_touch.missCount = 0;
    if (!g_touch.down) {
      g_touch.down = true; g_touch.startX = x; g_touch.startY = y;
      g_touch.startMs = now; g_touch.pageDragging = false;
      g_touch.auxBtnDown = TOUCH_AUX_BTN_NONE;
      if (uiPageIsFeedDeck(g_uiPageMode)) {
        if (lvglFeedQrButtonContainsPoint(x, y)) g_touch.auxBtnDown = TOUCH_AUX_BTN_QR;
        else if (lvglFeedRefreshButtonContainsPoint(x, y)) g_touch.auxBtnDown = TOUCH_AUX_BTN_REFRESH;
        else if (lvglFeedNextFeedButtonContainsPoint(x, y)) g_touch.auxBtnDown = TOUCH_AUX_BTN_NEXT;
      }
      if (g_touch.auxBtnDown == TOUCH_AUX_BTN_QR) { lvglSetFeedQrButtonPressed(true); Serial.printf("[TOUCH] btn-down QR x=%d y=%d\n", x, y); }
      else if (g_touch.auxBtnDown == TOUCH_AUX_BTN_REFRESH) { lvglSetFeedRefreshButtonPressed(true); Serial.printf("[TOUCH] btn-down SKIP x=%d y=%d\n", x, y); }
      else if (g_touch.auxBtnDown == TOUCH_AUX_BTN_NEXT) { lvglSetFeedNextFeedButtonPressed(true); Serial.printf("[TOUCH] btn-down NXT x=%d y=%d\n", x, y); }
#if TEST_DISPLAY && DOOM_SPIKE_ENABLED
      if (g_uiPageMode == UI_PAGE_DOOM) {
        g_doom.touchZone = doomTouchZoneFromX(x); g_doom.frameDirty = true;
        Serial.printf("[DOOM][TOUCH] down zone=%s x=%d y=%d\n", doomTouchZoneName(g_doom.touchZone), x, y);
      }
#endif
    }
    g_touch.lastX = x; g_touch.lastY = y;
#if TEST_DISPLAY
    if (g_uiPageMode == UI_PAGE_DOOM) return;
#endif
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI
    if (g_lvglReady) {
      if (g_touch.auxBtnDown != TOUCH_AUX_BTN_NONE) return;
      const int16_t liveDx = g_touch.lastX - g_touch.startX;
      const int16_t liveDy = g_touch.lastY - g_touch.startY;
      constexpr int16_t kDragStartPx = 5;
      if (g_touch.pageDragging) { lvglApplyPageDrag(liveDx); return; }
      if (abs(liveDx) >= kDragStartPx && abs(liveDx) >= abs(liveDy)) {
        g_touch.pageDragging = true; lvglApplyPageDrag(liveDx); return;
      }
    }
#endif
    return;
  }

  // ── Miss counter (touch controller scan rate bridge) ──
  if (g_touch.down) { if (++g_touch.missCount < 12) return; g_touch.missCount = 0; }
  if (!g_touch.down) return;
  g_touch.down = false;

  // ── Phase 3: Release — compute gesture + dispatch ──
  const TouchReleaseInfo r = {
    .dx = (int16_t)(g_touch.lastX - g_touch.startX),
    .dy = (int16_t)(g_touch.lastY - g_touch.startY),
    .durMs = millis() - g_touch.startMs,
    .horizontalIntent = (abs(g_touch.lastX - g_touch.startX) >= abs(g_touch.lastY - g_touch.startY)),
    .pageSwipe = [&]() {
      const int16_t adx = abs(g_touch.lastX - g_touch.startX);
      const int16_t ady = abs(g_touch.lastY - g_touch.startY);
      const bool horiz = (adx >= ady);
      const uint32_t dur = millis() - g_touch.startMs;
      const bool fast = (dur <= 220) && (adx >= ((DISPLAY_TOUCH_SWIPE_MIN_PX / 2) + 2));
      return horiz && ((adx >= DISPLAY_TOUCH_SWIPE_MIN_PX) || fast);
    }(),
    .isTap = ((millis() - g_touch.startMs) <= DISPLAY_TOUCH_TAP_MAX_MS &&
              abs(g_touch.lastX - g_touch.startX) <= DISPLAY_TOUCH_TAP_MAX_PX &&
              abs(g_touch.lastY - g_touch.startY) <= DISPLAY_TOUCH_TAP_MAX_PX),
    .isBtnTap = ((millis() - g_touch.startMs) <= 1400 &&
                 abs(g_touch.lastX - g_touch.startX) <= 72 &&
                 abs(g_touch.lastY - g_touch.startY) <= 72),
    .auxBtnDown = (uint8_t)g_touch.auxBtnDown,
#if TEST_DISPLAY && DOOM_SPIKE_ENABLED
    .doomTouchZone = g_doom.touchZone,
#else
    .doomTouchZone = 0,
#endif
  };
  g_touch.auxBtnDown = TOUCH_AUX_BTN_NONE;
#if TEST_DISPLAY && DOOM_SPIKE_ENABLED
  g_doom.touchZone = DOOM_TOUCH_NONE;
  if (r.doomTouchZone != DOOM_TOUCH_NONE) g_doom.frameDirty = true;
#endif
  if ((TouchAuxButton)r.auxBtnDown == TOUCH_AUX_BTN_QR) lvglSetFeedQrButtonPressed(false);
  else if ((TouchAuxButton)r.auxBtnDown == TOUCH_AUX_BTN_REFRESH) lvglSetFeedRefreshButtonPressed(false);
  else if ((TouchAuxButton)r.auxBtnDown == TOUCH_AUX_BTN_NEXT) lvglSetFeedNextFeedButtonPressed(false);

  // Dispatch by priority
  if ((TouchAuxButton)r.auxBtnDown != TOUCH_AUX_BTN_NONE) { handleFeedDeckButtonRelease(r); return; }
  if (g_touch.pageDragging) { handlePageDragRelease(r); return; }
#if TEST_DISPLAY && DOOM_SPIKE_ENABLED
  if (g_uiPageMode == UI_PAGE_DOOM) { handleDoomTouchRelease(r); return; }
#endif
  if (r.durMs > 3000) return;
  if (uiPageIsFeedDeck(g_uiPageMode) && lvglFeedQrModalIsOpen()) {
    if (r.durMs <= 2500) { lvglSetFeedQrModalOpen(false); Serial.println("[TOUCH] qr-close-overlay"); }
    g_touch.awaitRelease = true; g_touch.releaseStartMs = 0;
    return;
  }
  if ((millis() - g_touch.lastSwipeToggleMs) < 140) return;
  if (r.pageSwipe) { handleCarouselSwipe(r); return; }
  handleFeedDeckTapRelease(r);
}
#else
static bool initTouchInput() { return false; }
static void handleTouchSwipeInput() {}
#endif

static void draw7SegDigit(int16_t x, int16_t y, int16_t w, int16_t h, int16_t t, uint8_t digit, uint16_t onColor, uint16_t offColor) {
  static const uint8_t segMask[10] = {
      0b00111111,  // 0
      0b00000110,  // 1
      0b01011011,  // 2
      0b01001111,  // 3
      0b01100110,  // 4
      0b01101101,  // 5
      0b01111101,  // 6
      0b00000111,  // 7
      0b01111111,  // 8
      0b01101111   // 9
  };
  if (digit > 9) digit = 0;

  const uint8_t m = segMask[digit];
  const int16_t hh = h / 2;
  const int16_t vw = (w - (2 * t));
  const int16_t vh = (hh - t);

  fillRectCanvas(x + t, y, vw, t, (m & 0b00000001) ? onColor : offColor);                 // a
  fillRectCanvas(x + w - t, y + t, t, vh, (m & 0b00000010) ? onColor : offColor);         // b
  fillRectCanvas(x + w - t, y + hh, t, vh, (m & 0b00000100) ? onColor : offColor);        // c
  fillRectCanvas(x + t, y + h - t, vw, t, (m & 0b00001000) ? onColor : offColor);         // d
  fillRectCanvas(x, y + hh, t, vh, (m & 0b00010000) ? onColor : offColor);                 // e
  fillRectCanvas(x, y + t, t, vh, (m & 0b00100000) ? onColor : offColor);                  // f
  fillRectCanvas(x + t, y + (hh - (t / 2)), vw, t, (m & 0b01000000) ? onColor : offColor); // g
}

static void draw7SegColon(int16_t x, int16_t y, int16_t h, int16_t dot, bool on, uint16_t onColor, uint16_t offColor) {
  const uint16_t c = on ? onColor : offColor;
  const int16_t topY = y + (h / 3) - (dot / 2);
  const int16_t botY = y + ((2 * h) / 3) - (dot / 2);
  fillRectCanvas(x, topY, dot, dot, c);
  fillRectCanvas(x, botY, dot, dot, c);
}

static void drawSevenSegClockInRect(int16_t ox, int16_t oy, int16_t ow, int16_t oh, const tm &timeinfo) {
  const int gap = (ow >= 420) ? 8 : 5;
  const int colonW = (oh >= 120) ? 10 : 6;
  int digitW = (ow - (2 * colonW) - (7 * gap) - 12) / 6;
  if (digitW < 10) digitW = 10;
  int digitH = digitW * 2;
  int maxDigitH = oh - 16;
  if (digitH > maxDigitH) {
    digitH = maxDigitH;
    digitW = digitH / 2;
  }
  int thick = digitW / 5;
  if (thick < 2) thick = 2;

  const int totalW = (6 * digitW) + (2 * colonW) + (7 * gap);
  int startX = ox + ((ow - totalW) / 2);
  int startY = oy + ((oh - digitH) / 2);
  if (startX < ox + 2) startX = ox + 2;
  if (startY < oy + 2) startY = oy + 2;

  fillRectCanvas(ox, oy, ow, oh, DB_COLOR_BLACK);

  const int d0 = timeinfo.tm_hour / 10;
  const int d1 = timeinfo.tm_hour % 10;
  const int d2 = timeinfo.tm_min / 10;
  const int d3 = timeinfo.tm_min % 10;
  const int d4 = timeinfo.tm_sec / 10;
  const int d5 = timeinfo.tm_sec % 10;

  int x = startX;
  draw7SegDigit(x, startY, digitW, digitH, thick, d0, DB_COLOR_WHITE, DB_COLOR_BLACK);
  x += digitW + gap;
  draw7SegDigit(x, startY, digitW, digitH, thick, d1, DB_COLOR_WHITE, DB_COLOR_BLACK);
  x += digitW + gap;
  draw7SegColon(x, startY, digitH, colonW, (timeinfo.tm_sec % 2) == 0, DB_COLOR_WHITE, DB_COLOR_BLACK);
  x += colonW + gap;
  draw7SegDigit(x, startY, digitW, digitH, thick, d2, DB_COLOR_WHITE, DB_COLOR_BLACK);
  x += digitW + gap;
  draw7SegDigit(x, startY, digitW, digitH, thick, d3, DB_COLOR_WHITE, DB_COLOR_BLACK);
  x += digitW + gap;
  draw7SegColon(x, startY, digitH, colonW, true, DB_COLOR_WHITE, DB_COLOR_BLACK);
  x += colonW + gap;
  draw7SegDigit(x, startY, digitW, digitH, thick, d4, DB_COLOR_WHITE, DB_COLOR_BLACK);
  x += digitW + gap;
  draw7SegDigit(x, startY, digitW, digitH, thick, d5, DB_COLOR_WHITE, DB_COLOR_BLACK);
}

static void drawClockClockGlyph(int16_t cx, int16_t cy, int16_t r, float aMul, float bMul) {
  drawCircleCanvas(cx, cy, r, DB_COLOR_GRAY);
  const float a0 = aMul * (PI / 2.0f);
  const float b0 = bMul * (PI / 2.0f);
  const int16_t ra = r - 2;
  const int16_t ax = cx + (int16_t)lroundf(cosf(a0) * ra);
  const int16_t ay = cy + (int16_t)lroundf(sinf(a0) * ra);
  const int16_t bx = cx + (int16_t)lroundf(cosf(b0) * ra);
  const int16_t by = cy + (int16_t)lroundf(sinf(b0) * ra);
  drawLineCanvas(cx, cy, ax, ay, DB_COLOR_WHITE);
  drawLineCanvas(cx, cy, bx, by, DB_COLOR_WHITE);
  fillRectCanvas(cx - 1, cy - 1, 3, 3, DB_COLOR_WHITE);
}

static void drawClockClockDigit(int16_t x, int16_t y, int16_t cell, int16_t gap, uint8_t digit) {
  static const float kDigitMap[10][12] = {
      {2, 1, 2, 3, 0, 2, 0, 2, 0, 1, 0, 3},
      {2.6f, 2.6f, 2, 2, 2.6f, 2.6f, 0, 2, 2.6f, 2.6f, 0, 0},
      {1, 1, 2, 3, 1, 2, 0, 3, 0, 1, 3, 3},
      {1, 1, 2, 3, 1, 1, 0, 3, 1, 1, 0, 3},
      {2, 2, 2, 2, 0, 1, 0, 2, 2.6f, 2.6f, 0, 0},
      {1, 2, 3, 3, 0, 1, 3, 2, 1, 1, 0, 3},
      {1, 2, 3, 3, 0, 2, 3, 2, 0, 1, 0, 3},
      {1, 1, 3, 2, 2.6f, 2.6f, 0, 2, 2.6f, 2.6f, 0, 0},
      {2, 1, 2, 3, 2, 1, 2, 3, 0, 1, 0, 3},
      {2, 1, 2, 3, 0, 1, 0, 2, 1, 1, 0, 3},
  };
  if (digit > 9) digit = 0;
  const int16_t r = (cell / 2) - 1;
  for (int i = 0; i < 6; ++i) {
    const int16_t row = i / 2;
    const int16_t col = i % 2;
    const int16_t cx = x + col * (cell + gap) + (cell / 2);
    const int16_t cy = y + row * (cell + gap) + (cell / 2);
    drawClockClockGlyph(cx, cy, r, kDigitMap[digit][i * 2], kDigitMap[digit][(i * 2) + 1]);
  }
}

static void drawClockClockInRect(int16_t ox, int16_t oy, int16_t ow, int16_t oh, const tm &timeinfo) {
  fillRectCanvas(ox, oy, ow, oh, DB_COLOR_BLACK);
  int16_t cell = (oh - 20) / 3;
  int16_t gap = 4;
  if (cell > 28) cell = 28;
  if (cell < 12) cell = 12;
  int16_t digitW = (2 * cell) + gap;
  int16_t digitH = (3 * cell) + (2 * gap);
  int16_t digitGap = (ow > 400) ? 12 : 8;
  int16_t colonGap = cell / 2;
  int16_t totalW = (4 * digitW) + (3 * digitGap) + colonGap;
  int16_t startX = ox + ((ow - totalW) / 2);
  int16_t startY = oy + ((oh - digitH) / 2);

  const uint8_t d0 = timeinfo.tm_hour / 10;
  const uint8_t d1 = timeinfo.tm_hour % 10;
  const uint8_t d2 = timeinfo.tm_min / 10;
  const uint8_t d3 = timeinfo.tm_min % 10;

  int16_t x = startX;
  drawClockClockDigit(x, startY, cell, gap, d0);
  x += digitW + digitGap;
  drawClockClockDigit(x, startY, cell, gap, d1);
  x += digitW + (digitGap / 2);
  const int16_t cDot = (cell / 3) < 2 ? 2 : (cell / 3);
  fillRectCanvas(x, startY + (digitH / 3), cDot, cDot, DB_COLOR_WHITE);
  fillRectCanvas(x, startY + ((2 * digitH) / 3), cDot, cDot, DB_COLOR_WHITE);
  x += colonGap + (digitGap / 2);
  drawClockClockDigit(x, startY, cell, gap, d2);
  x += digitW + digitGap;
  drawClockClockDigit(x, startY, cell, gap, d3);
}

static const uint8_t* tinyGlyph5x7(char c) {
  switch (c) {
    case '0': { static const uint8_t g[7] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}; return g; }
    case '1': { static const uint8_t g[7] = {0x04, 0x0C, 0x14, 0x04, 0x04, 0x04, 0x1F}; return g; }
    case '2': { static const uint8_t g[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}; return g; }
    case '3': { static const uint8_t g[7] = {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}; return g; }
    case '4': { static const uint8_t g[7] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}; return g; }
    case '5': { static const uint8_t g[7] = {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}; return g; }
    case '6': { static const uint8_t g[7] = {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}; return g; }
    case '7': { static const uint8_t g[7] = {0x1F, 0x11, 0x01, 0x02, 0x04, 0x04, 0x04}; return g; }
    case '8': { static const uint8_t g[7] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}; return g; }
    case '9': { static const uint8_t g[7] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}; return g; }
    case 'A': { static const uint8_t g[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}; return g; }
    case 'C': { static const uint8_t g[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}; return g; }
    case 'D': { static const uint8_t g[7] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}; return g; }
    case 'E': { static const uint8_t g[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}; return g; }
    case 'G': { static const uint8_t g[7] = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E}; return g; }
    case 'H': { static const uint8_t g[7] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}; return g; }
    case 'I': { static const uint8_t g[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F}; return g; }
    case 'L': { static const uint8_t g[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}; return g; }
    case 'M': { static const uint8_t g[7] = {0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11}; return g; }
    case 'N': { static const uint8_t g[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}; return g; }
    case 'O': { static const uint8_t g[7] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}; return g; }
    case 'P': { static const uint8_t g[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}; return g; }
    case 'Q': { static const uint8_t g[7] = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}; return g; }
    case 'R': { static const uint8_t g[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}; return g; }
    case 'S': { static const uint8_t g[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}; return g; }
    case 'T': { static const uint8_t g[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}; return g; }
    case 'U': { static const uint8_t g[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}; return g; }
    case 'V': { static const uint8_t g[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}; return g; }
    case 'Z': { static const uint8_t g[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}; return g; }
    case '%': { static const uint8_t g[7] = {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03}; return g; }
    case '\'': { static const uint8_t g[7] = {0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00}; return g; }
    default: { static const uint8_t g[7] = {0, 0, 0, 0, 0, 0, 0}; return g; }
  }
}

static void drawTinyText5x7(int16_t x, int16_t y, const char *txt, int scale, uint16_t color) {
  if (!txt || scale < 1) return;
  int16_t cx = x;
  const int16_t adv = (5 * scale) + scale;
  for (const char *p = txt; *p; ++p) {
    const uint8_t *rows = tinyGlyph5x7(*p);
    for (int ry = 0; ry < 7; ++ry) {
      for (int rx = 0; rx < 5; ++rx) {
        if (rows[ry] & (1 << (4 - rx))) {
          fillRectCanvas(cx + (rx * scale), y + (ry * scale), scale, scale, color);
        }
      }
    }
    cx += adv;
  }
}

static int16_t tinyTextWidth5x7(const char *txt, int scale) {
  if (!txt || !*txt || scale < 1) return 0;
  const int len = (int)strlen(txt);
  return (len * ((5 * scale) + scale)) - scale;
}

static int drawTinyTextWrapped5x7(int16_t x, int16_t y, int16_t maxW, int maxLines, const char *txt, int scale, uint16_t color) {
  if (!txt || !*txt || maxW <= 0 || maxLines <= 0 || scale < 1) return 0;
  const int charAdv = (5 * scale) + scale;
  const int maxChars = maxW / charAdv;
  if (maxChars <= 1) return 0;

  const int len = (int)strlen(txt);
  int pos = 0;
  int lines = 0;
  const int lineH = (7 * scale) + scale;

  while (pos < len && lines < maxLines) {
    while (pos < len && txt[pos] == ' ') ++pos;
    if (pos >= len) break;

    int remaining = len - pos;
    int take = (remaining <= maxChars) ? remaining : maxChars;
    if (take < remaining) {
      int breakAt = -1;
      for (int i = take - 1; i >= 0; --i) {
        if (txt[pos + i] == ' ') {
          breakAt = i;
          break;
        }
      }
      if (breakAt > 0) take = breakAt;
    }

    if (take <= 0) break;
    char line[48];
    int copyLen = take;
    if (copyLen > (int)sizeof(line) - 1) copyLen = (int)sizeof(line) - 1;
    memcpy(line, txt + pos, copyLen);
    line[copyLen] = '\0';

    drawTinyText5x7(x, y + (lines * lineH), line, scale, color);
    pos += take;
    while (pos < len && txt[pos] == ' ') ++pos;
    ++lines;
  }

  return lines;
}

static const char* wordHourIt(int h12) {
  switch (h12) {
    case 1: return "UNA";
    case 2: return "DUE";
    case 3: return "TRE";
    case 4: return "QUATTRO";
    case 5: return "CINQUE";
    case 6: return "SEI";
    case 7: return "SETTE";
    case 8: return "OTTO";
    case 9: return "NOVE";
    case 10: return "DIECI";
    case 11: return "UNDICI";
    default: return "DODICI";
  }
}

static const char* wordMinuteIt(int m5) {
  switch (m5) {
    case 0: return "IN PUNTO";
    case 5: return "E CINQUE";
    case 10: return "E DIECI";
    case 15: return "E UN QUARTO";
    case 20: return "E VENTI";
    case 25: return "E VENTICINQUE";
    case 30: return "E MEZZA";
    case 35: return "MENO VENTICINQUE";
    case 40: return "MENO VENTI";
    case 45: return "MENO UN QUARTO";
    case 50: return "MENO DIECI";
    default: return "MENO CINQUE";
  }
}

static void composeWordClockIt(const tm &timeinfo, char *l1, size_t l1n, char *l2, size_t l2n, char *l3, size_t l3n) {
  int h12 = timeinfo.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  int m5 = ((timeinfo.tm_min + 2) / 5) * 5;
  if (m5 >= 60) {
    m5 = 0;
    h12 = (h12 % 12) + 1;
  } else if (m5 >= 35) {
    h12 = (h12 % 12) + 1;
  }

  snprintf(l1, l1n, "SONO LE");
  snprintf(l2, l2n, "%s", wordHourIt(h12));
  snprintf(l3, l3n, "%s", wordMinuteIt(m5));
}

static const char* wordHourItSentence(int h12) {
  switch (h12) {
    case 1: return "l'una";
    case 2: return "due";
    case 3: return "tre";
    case 4: return "quattro";
    case 5: return "cinque";
    case 6: return "sei";
    case 7: return "sette";
    case 8: return "otto";
    case 9: return "nove";
    case 10: return "dieci";
    case 11: return "undici";
    default: return "dodici";
  }
}

static const char* wordMinuteItSentence(int m5) {
  switch (m5) {
    case 0: return "in punto";
    case 5: return "e cinque";
    case 10: return "e dieci";
    case 15: return "e un quarto";
    case 20: return "e venti";
    case 25: return "e venticinque";
    case 30: return "e mezza";
    case 35: return "meno venticinque";
    case 40: return "meno venti";
    case 45: return "meno un quarto";
    case 50: return "meno dieci";
    default: return "meno cinque";
  }
}

static void composeWordClockSentenceIt(const tm &timeinfo, char *out, size_t outLen) {
  int h12 = timeinfo.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  int m5 = ((timeinfo.tm_min + 2) / 5) * 5;
  if (m5 >= 60) {
    m5 = 0;
    h12 = (h12 % 12) + 1;
  } else if (m5 >= 35) {
    h12 = (h12 % 12) + 1;
  }

  if (h12 == 1) {
    // Current LVGL Montserrat subset misses uppercase accented glyphs (e.g. "È").
    // Use ASCII fallback to avoid tofu squares on device.
    snprintf(out, outLen, "E' %s %s", wordHourItSentence(h12), wordMinuteItSentence(m5));
  } else {
    snprintf(out, outLen, "Sono le %s %s", wordHourItSentence(h12), wordMinuteItSentence(m5));
  }
}

// --- Klingon (tlhIngan Hol) word clock ---
// Uses ASCII transliteration; pIqaD has no coverage in Montserrat 38.
// Format: "DaH [hour] rep [minutes] tup"  e.g. "DaH wej rep wa'maH vagh tup" = 3:15

static const char* wordHourTlh(int h12) {
  switch (h12) {
    case 1:  return "wa'";
    case 2:  return "cha'";
    case 3:  return "wej";
    case 4:  return "loS";
    case 5:  return "vagh";
    case 6:  return "jav";
    case 7:  return "Soch";
    case 8:  return "chorgh";
    case 9:  return "Hut";
    case 10: return "wa'maH";
    case 11: return "wa'maH wa'";
    default: return "cha'maH";
  }
}

static const char* wordMinuteTlh(int m5) {
  switch (m5) {
    case 0:  return "";
    case 5:  return "vagh";
    case 10: return "wa'maH";
    case 15: return "wa'maH vagh";
    case 20: return "cha'maH";
    case 25: return "cha'maH vagh";
    case 30: return "wejmaH";
    case 35: return "wejmaH vagh";
    case 40: return "loSmaH";
    case 45: return "loSmaH vagh";
    case 50: return "vaghmaH";
    default: return "vaghmaH vagh";
  }
}

static void composeWordClockSentenceTlh(const tm &timeinfo, char *out, size_t outLen) {
  int h12 = timeinfo.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  int m5 = ((timeinfo.tm_min + 2) / 5) * 5;
  if (m5 >= 60) { m5 = 0; h12 = (h12 % 12) + 1; }
  const char* minStr = wordMinuteTlh(m5);
  if (m5 == 0) {
    snprintf(out, outLen, "DaH %s rep", wordHourTlh(h12));
  } else {
    snprintf(out, outLen, "DaH %s rep %s tup", wordHourTlh(h12), minStr);
  }
}

// --- English (EN) ---

static const char* wordHourEn(int h12) {
  switch (h12) {
    case 1:  return "one";
    case 2:  return "two";
    case 3:  return "three";
    case 4:  return "four";
    case 5:  return "five";
    case 6:  return "six";
    case 7:  return "seven";
    case 8:  return "eight";
    case 9:  return "nine";
    case 10: return "ten";
    case 11: return "eleven";
    default: return "twelve";
  }
}

static void composeWordClockSentenceEn(const tm &timeinfo, char *out, size_t outLen) {
  int h12 = timeinfo.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  int m5 = ((timeinfo.tm_min + 2) / 5) * 5;
  if (m5 >= 60) { m5 = 0; h12 = (h12 % 12) + 1; }
  if (m5 == 0)       snprintf(out, outLen, "It's %s o'clock", wordHourEn(h12));
  else if (m5 == 15) snprintf(out, outLen, "It's quarter past %s", wordHourEn(h12));
  else if (m5 == 30) snprintf(out, outLen, "It's half past %s", wordHourEn(h12));
  else if (m5 == 45) { int nh = (h12 % 12) + 1; snprintf(out, outLen, "It's quarter to %s", wordHourEn(nh)); }
  else if (m5 < 30)  snprintf(out, outLen, "It's %d past %s", m5, wordHourEn(h12));
  else               { int nh = (h12 % 12) + 1; snprintf(out, outLen, "It's %d to %s", 60 - m5, wordHourEn(nh)); }
}

// --- Français (FR) ---

static const char* wordHourFr(int h12) {
  switch (h12) {
    case 1:  return "une";
    case 2:  return "deux";
    case 3:  return "trois";
    case 4:  return "quatre";
    case 5:  return "cinq";
    case 6:  return "six";
    case 7:  return "sept";
    case 8:  return "huit";
    case 9:  return "neuf";
    case 10: return "dix";
    case 11: return "onze";
    default: return "douze";
  }
}

static void composeWordClockSentenceFr(const tm &timeinfo, char *out, size_t outLen) {
  int h12 = timeinfo.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  int m5 = ((timeinfo.tm_min + 2) / 5) * 5;
  if (m5 >= 60) { m5 = 0; h12 = (h12 % 12) + 1; }
  if (m5 == 0)       snprintf(out, outLen, "Il est %s heure%s", wordHourFr(h12), h12 == 1 ? "" : "s");
  else if (m5 == 15) snprintf(out, outLen, "Il est %s heure%s et quart", wordHourFr(h12), h12 == 1 ? "" : "s");
  else if (m5 == 30) snprintf(out, outLen, "Il est %s heure%s et demie", wordHourFr(h12), h12 == 1 ? "" : "s");
  else if (m5 == 45) { int nh = (h12 % 12) + 1; snprintf(out, outLen, "Il est %s heure%s moins le quart", wordHourFr(nh), nh == 1 ? "" : "s"); }
  else if (m5 < 30)  snprintf(out, outLen, "Il est %s heure%s %d", wordHourFr(h12), h12 == 1 ? "" : "s", m5);
  else               { int nh = (h12 % 12) + 1; snprintf(out, outLen, "Il est %s heure%s moins %d", wordHourFr(nh), nh == 1 ? "" : "s", 60 - m5); }
}

// --- Deutsch (DE) — native halb style ---

static const char* wordHourDe(int h12) {
  switch (h12) {
    case 1:  return "ein";
    case 2:  return "zwei";
    case 3:  return "drei";
    case 4:  return "vier";
    case 5:  return "f\xC3\xBC" "nf";
    case 6:  return "sechs";
    case 7:  return "sieben";
    case 8:  return "acht";
    case 9:  return "neun";
    case 10: return "zehn";
    case 11: return "elf";
    default: return "zw\xC3\xB6" "lf";
  }
}

static void composeWordClockSentenceDe(const tm &timeinfo, char *out, size_t outLen) {
  int h12 = timeinfo.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  int m5 = ((timeinfo.tm_min + 2) / 5) * 5;
  if (m5 >= 60) { m5 = 0; h12 = (h12 % 12) + 1; }
  int nh = (h12 % 12) + 1;  // next hour for "vor" and "halb"
  if (m5 == 0)       snprintf(out, outLen, "Es ist %s Uhr", wordHourDe(h12));
  else if (m5 == 15) snprintf(out, outLen, "Es ist Viertel nach %s", wordHourDe(h12));
  else if (m5 == 30) snprintf(out, outLen, "Es ist halb %s", wordHourDe(nh));
  else if (m5 == 45) snprintf(out, outLen, "Es ist Viertel vor %s", wordHourDe(nh));
  else if (m5 == 20) snprintf(out, outLen, "Es ist zwanzig nach %s", wordHourDe(h12));
  else if (m5 == 40) snprintf(out, outLen, "Es ist zwanzig vor %s", wordHourDe(nh));
  else if (m5 < 30)  snprintf(out, outLen, "Es ist %d nach %s", m5, wordHourDe(h12));
  else               snprintf(out, outLen, "Es ist %d vor %s", 60 - m5, wordHourDe(nh));
}

// --- Español (ES) ---

static const char* wordHourEs(int h12) {
  switch (h12) {
    case 1:  return "la una";
    case 2:  return "las dos";
    case 3:  return "las tres";
    case 4:  return "las cuatro";
    case 5:  return "las cinco";
    case 6:  return "las seis";
    case 7:  return "las siete";
    case 8:  return "las ocho";
    case 9:  return "las nueve";
    case 10: return "las diez";
    case 11: return "las once";
    default: return "las doce";
  }
}

static void composeWordClockSentenceEs(const tm &timeinfo, char *out, size_t outLen) {
  int h12 = timeinfo.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  int m5 = ((timeinfo.tm_min + 2) / 5) * 5;
  if (m5 >= 60) { m5 = 0; h12 = (h12 % 12) + 1; }
  if (m5 == 0)       snprintf(out, outLen, "Son %s en punto", wordHourEs(h12));
  else if (m5 == 15) snprintf(out, outLen, "Son %s y cuarto", wordHourEs(h12));
  else if (m5 == 30) snprintf(out, outLen, "Son %s y media", wordHourEs(h12));
  else if (m5 == 45) { int nh = (h12 % 12) + 1; snprintf(out, outLen, "Son %s menos cuarto", wordHourEs(nh)); }
  else if (m5 < 30)  snprintf(out, outLen, "Son %s y %d", wordHourEs(h12), m5);
  else               { int nh = (h12 % 12) + 1; snprintf(out, outLen, "Son %s menos %d", wordHourEs(nh), 60 - m5); }
}

// --- Português (PT) ---

static const char* wordHourPt(int h12) {
  switch (h12) {
    case 1:  return "uma";
    case 2:  return "duas";
    case 3:  return "tr\xC3\xAA" "s";
    case 4:  return "quatro";
    case 5:  return "cinco";
    case 6:  return "seis";
    case 7:  return "sete";
    case 8:  return "oito";
    case 9:  return "nove";
    case 10: return "dez";
    case 11: return "onze";
    default: return "doze";
  }
}

static void composeWordClockSentencePt(const tm &timeinfo, char *out, size_t outLen) {
  int h12 = timeinfo.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  int m5 = ((timeinfo.tm_min + 2) / 5) * 5;
  if (m5 >= 60) { m5 = 0; h12 = (h12 % 12) + 1; }
  const char* verb = (h12 == 1) ? "\xC3\x89" : "S\xC3\xA3o";
  if (m5 == 0)       snprintf(out, outLen, "%s %s", verb, wordHourPt(h12));
  else if (m5 == 15) snprintf(out, outLen, "%s %s e um quarto", verb, wordHourPt(h12));
  else if (m5 == 30) snprintf(out, outLen, "%s %s e meia", verb, wordHourPt(h12));
  else if (m5 == 45) { int nh = (h12 % 12) + 1; const char* v2 = (nh == 1) ? "\xC3\x89" : "S\xC3\xA3o"; snprintf(out, outLen, "%s %s menos um quarto", v2, wordHourPt(nh)); }
  else if (m5 < 30)  snprintf(out, outLen, "%s %s e %d", verb, wordHourPt(h12), m5);
  else               { int nh = (h12 % 12) + 1; const char* v2 = (nh == 1) ? "\xC3\x89" : "S\xC3\xA3o"; snprintf(out, outLen, "%s %s menos %d", v2, wordHourPt(nh), 60 - m5); }
}

// --- Latina (LA) — hora romana classica ---
// Frazioni: quadrans = 1/4, semis = 1/2, dodrante = 3/4 (= minus quadrans)

static const char* wordHourLa(int h12) {
  switch (h12) {
    case 1:  return "prima";
    case 2:  return "secunda";
    case 3:  return "tertia";
    case 4:  return "quarta";
    case 5:  return "quinta";
    case 6:  return "sexta";
    case 7:  return "septima";
    case 8:  return "octava";
    case 9:  return "nona";
    case 10: return "decima";
    case 11: return "undecima";
    default: return "duodecima";
  }
}

static void composeWordClockSentenceLa(const tm &timeinfo, char *out, size_t outLen) {
  int h12 = timeinfo.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  int m5 = ((timeinfo.tm_min + 2) / 5) * 5;
  if (m5 >= 60) { m5 = 0; h12 = (h12 % 12) + 1; }
  int nh = (h12 % 12) + 1;
  if (m5 == 0)       snprintf(out, outLen, "hora %s est", wordHourLa(h12));
  else if (m5 == 15) snprintf(out, outLen, "hora %s et quadrans", wordHourLa(h12));
  else if (m5 == 30) snprintf(out, outLen, "hora %s et semis", wordHourLa(h12));
  else if (m5 == 45) snprintf(out, outLen, "hora %s minus quadrans", wordHourLa(nh));
  else if (m5 < 30)  snprintf(out, outLen, "hora %s et %d minuta", wordHourLa(h12), m5);
  else               snprintf(out, outLen, "hora %s minus %d minuta", wordHourLa(nh), 60 - m5);
}

// --- Esperanto (EO) ---

static const char* wordHourEo(int h12) {
  switch (h12) {
    case 1:  return "unu";
    case 2:  return "du";
    case 3:  return "tri";
    case 4:  return "kvar";
    case 5:  return "kvin";
    case 6:  return "ses";
    case 7:  return "sep";
    case 8:  return "ok";
    case 9:  return "na\xC5\xAD";
    case 10: return "dek";
    case 11: return "dek unu";
    default: return "dek du";
  }
}

static void composeWordClockSentenceEo(const tm &timeinfo, char *out, size_t outLen) {
  int h12 = timeinfo.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  int m5 = ((timeinfo.tm_min + 2) / 5) * 5;
  if (m5 >= 60) { m5 = 0; h12 = (h12 % 12) + 1; }
  if (m5 == 0)       snprintf(out, outLen, "Estas la %s horo", wordHourEo(h12));
  else if (m5 == 15) snprintf(out, outLen, "Estas kvarono post la %s", wordHourEo(h12));
  else if (m5 == 30) snprintf(out, outLen, "Estas duono post la %s", wordHourEo(h12));
  else if (m5 == 45) { int nh = (h12 % 12) + 1; snprintf(out, outLen, "Estas kvarono al la %s", wordHourEo(nh)); }
  else if (m5 < 30)  snprintf(out, outLen, "Estas %d minutoj post la %s", m5, wordHourEo(h12));
  else               { int nh = (h12 % 12) + 1; snprintf(out, outLen, "Estas %d minutoj al la %s", 60 - m5, wordHourEo(nh)); }
}

// --- 1337 Speak word clock ---

static const char* wordHourL33t(int h12) {
  switch (h12) {
    case 1:  return "0N3";
    case 2:  return "7W0";
    case 3:  return "7HR33";
    case 4:  return "F0UR";
    case 5:  return "F1V3";
    case 6:  return "51X";
    case 7:  return "53V3N";
    case 8:  return "31GH7";
    case 9:  return "N1N3";
    case 10: return "73N";
    case 11: return "3L3V3N";
    default: return "7W3LV3";
  }
}

static void composeWordClockSentenceL33t(const tm &timeinfo, char *out, size_t outLen) {
  int h12 = timeinfo.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  int m5 = ((timeinfo.tm_min + 2) / 5) * 5;
  if (m5 >= 60) { m5 = 0; h12 = (h12 % 12) + 1; }
  if (m5 == 0)       snprintf(out, outLen, "1T'5 %s 0'CL0CK", wordHourL33t(h12));
  else if (m5 == 15) snprintf(out, outLen, "1T'5 QU4R73R P457 %s", wordHourL33t(h12));
  else if (m5 == 30) snprintf(out, outLen, "1T'5 H4LF P457 %s", wordHourL33t(h12));
  else if (m5 == 45) { int nh = (h12 % 12) + 1; snprintf(out, outLen, "1T'5 QU4R73R 70 %s", wordHourL33t(nh)); }
  else if (m5 < 30)  snprintf(out, outLen, "1T'5 %d P457 %s", m5, wordHourL33t(h12));
  else               { int nh = (h12 % 12) + 1; snprintf(out, outLen, "1T'5 %d 70 %s", 60 - m5, wordHourL33t(nh)); }
}

// --- Shakespearean English word clock ---

static const char* wordHourSha(int h12) {
  switch (h12) {
    case 1:  return "one";
    case 2:  return "two";
    case 3:  return "three";
    case 4:  return "four";
    case 5:  return "five";
    case 6:  return "six";
    case 7:  return "seven";
    case 8:  return "eight";
    case 9:  return "nine";
    case 10: return "ten";
    case 11: return "eleven";
    default: return "twelve";
  }
}

static const char* shaExclaim(int h12) {
  static const char* e[] = {"Marry", "Verily", "Hark", "Prithee", "Forsooth", "Zounds"};
  return e[h12 % 6];
}

static void composeWordClockSentenceSha(const tm &timeinfo, char *out, size_t outLen) {
  int h12 = timeinfo.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  int m5 = ((timeinfo.tm_min + 2) / 5) * 5;
  if (m5 >= 60) { m5 = 0; h12 = (h12 % 12) + 1; }
  const char* ex = shaExclaim(h12);
  if (m5 == 0)       snprintf(out, outLen, "%s, 'tis %s of the clock", ex, wordHourSha(h12));
  else if (m5 == 15) snprintf(out, outLen, "%s, 'tis quarter past %s", ex, wordHourSha(h12));
  else if (m5 == 30) snprintf(out, outLen, "%s, 'tis half past %s", ex, wordHourSha(h12));
  else if (m5 == 45) { int nh = (h12 % 12) + 1; snprintf(out, outLen, "%s, 'tis quarter to %s", ex, wordHourSha(nh)); }
  else if (m5 < 30)  snprintf(out, outLen, "%s, 'tis %d minutes past %s", ex, m5, wordHourSha(h12));
  else               { int nh = (h12 % 12) + 1; snprintf(out, outLen, "%s, 'tis %d minutes to %s", ex, 60 - m5, wordHourSha(nh)); }
}

// --- Valley Girl word clock ---

static const char* wordHourVal(int h12) {
  switch (h12) {
    case 1:  return "one";
    case 2:  return "two";
    case 3:  return "three";
    case 4:  return "four";
    case 5:  return "five";
    case 6:  return "six";
    case 7:  return "seven";
    case 8:  return "eight";
    case 9:  return "nine";
    case 10: return "ten";
    case 11: return "eleven";
    default: return "twelve";
  }
}

static void composeWordClockSentenceVal(const tm &timeinfo, char *out, size_t outLen) {
  int h12 = timeinfo.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  int m5 = ((timeinfo.tm_min + 2) / 5) * 5;
  if (m5 >= 60) { m5 = 0; h12 = (h12 % 12) + 1; }
  if (m5 == 0)       snprintf(out, outLen, "Oh em gee, it's %s o'clock", wordHourVal(h12));
  else if (m5 == 15) snprintf(out, outLen, "It's like quarter past %s, totally", wordHourVal(h12));
  else if (m5 == 30) snprintf(out, outLen, "It's like half past %s, you know", wordHourVal(h12));
  else if (m5 == 45) { int nh = (h12 % 12) + 1; snprintf(out, outLen, "It's like almost %s, literally", wordHourVal(nh)); }
  else if (m5 < 30)  snprintf(out, outLen, "It's like %d past %s, whatever", m5, wordHourVal(h12));
  else               { int nh = (h12 % 12) + 1; snprintf(out, outLen, "Only %d to %s, so ugh", 60 - m5, wordHourVal(nh)); }
}

// --- Italian Bellazio scazzata word clock ---

static const char* wordHourBellazio(int h12) {
  switch (h12) {
    case 1:  return "una";
    case 2:  return "due";
    case 3:  return "tre";
    case 4:  return "quattro";
    case 5:  return "cinque";
    case 6:  return "sei";
    case 7:  return "sette";
    case 8:  return "otto";
    case 9:  return "nove";
    case 10: return "dieci";
    case 11: return "undici";
    default: return "dodici";
  }
}

static const char* wordMinuteBellazio(int m) {
  switch (m) {
    case 5:  return "cinque";
    case 10: return "dieci";
    case 15: return "un quarto";
    case 20: return "venti";
    case 25: return "venticinque";
    case 30: return "mezza";
    default: return "";
  }
}

static const char* const kBellazioLeads[] = {
  "Dai,",
  "Bro,",
  "Zio,",
  "Raga,",
  "Fra,",
  "Amo,",
  "Senti,",
  "Aspetta,",
  "Guarda,",
  "Ok raga,",
  "Minchia spettacolo,",
  "Minchia oh,",
  "No vabbe',",
  "Bomber:",
  "Cavallo!",
  "Stai sereno..."
};

struct BellazioCloser {
  const char* text;
  const char* type;
};

static const BellazioCloser kBellazioClosers[] = {
  {"Onesto.", "punto"},
  {"Davvero.", "punto"},
  {"Ci sta.", "virgola"},
  {"Serio.", "virgola"},
  {"Ti giuro.", "punto"},
  {"For real.", "punto"},
  {"Assurdo.", "punto"},
  {"Pazzesco.", "punto"},
  {"Bestiale.", "punto"},
  {"E una roba.", "punto"},
  {"Boh.", "punto"},
  {"Tipo.", "punto"},
  {"Adorooo!", "punto"},
  {"Giuro...", "ellissi"},
  {"Sul serio...", "ellissi"},
  {"Tipo...", "ellissi"},
  {"Cioe...", "ellissi"},
  {"Mah...", "ellissi"},
  {"Ma anche no...", "ellissi"},
  {"Bene, ma non benissimo.", "punto"},
  {"Escile.", "punto"},
  {"Apericena?", "punto"},
  {"Buongiornissimo!", "punto"},
  {"Ciaone.", "punto"},
  {"Da paura.", "punto"},
  {"Il disagio proprio.", "punto"},
  {"Spacca", "virgola"},
  {"Sta senza p'nzier.", "punto"},
  {"Ti lovvo.", "punto"},
};

static uint32_t bellazioMixSeed(uint32_t v) {
  v ^= v >> 16;
  v *= 0x7feb352dUL;
  v ^= v >> 15;
  v *= 0x846ca68bUL;
  v ^= v >> 16;
  return v;
}

static const char* bellazioCloserSeparator(const char* type) {
  if (!type) return ". ";
  if (strcmp(type, "virgola") == 0) return ", ";
  if (strcmp(type, "ellissi") == 0) return ". ";
  return ". ";
}

static void composeWordClockSentenceBellazio(const tm &timeinfo, char *out, size_t outLen) {
  int h12 = timeinfo.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  int m5 = ((timeinfo.tm_min + 2) / 5) * 5;
  if (m5 >= 60) { m5 = 0; h12 = (h12 % 12) + 1; }
  int nh = (h12 % 12) + 1;
  const char* hr  = wordHourBellazio(h12);
  const char* nhr = wordHourBellazio(nh);
  bool sing   = (h12 == 1);   // l'una vs le X
  bool nhSing = (nh  == 1);
  const size_t leadCount = sizeof(kBellazioLeads) / sizeof(kBellazioLeads[0]);
  const size_t closerCount = sizeof(kBellazioClosers) / sizeof(kBellazioClosers[0]);
  const uint32_t minuteKey = (uint32_t)((timeinfo.tm_yday * 1440) + (timeinfo.tm_hour * 60) + timeinfo.tm_min);
  if (minuteKey != g_clock.bellazioLastMinuteKey) {
    size_t nextLeadIdx = (size_t)(bellazioMixSeed(minuteKey ^ 0xA53C9E5Du) % (uint32_t)leadCount);
    size_t nextCloserIdx = (size_t)(bellazioMixSeed(minuteKey ^ 0x61C88647u) % (uint32_t)closerCount);
    if (leadCount > 1 && nextLeadIdx == g_clock.bellazioLastLeadIdx) {
      nextLeadIdx = (nextLeadIdx + 1 + (minuteKey % (leadCount - 1))) % leadCount;
    }
    if (closerCount > 1 && nextCloserIdx == g_clock.bellazioLastCloserIdx) {
      nextCloserIdx = (nextCloserIdx + 1 + (minuteKey % (closerCount - 1))) % closerCount;
    }
    g_clock.bellazioLastLeadIdx = (uint8_t)nextLeadIdx;
    g_clock.bellazioLastCloserIdx = (uint8_t)nextCloserIdx;
    g_clock.bellazioLastMinuteKey = minuteKey;
  }
  const char* lead = kBellazioLeads[g_clock.bellazioLastLeadIdx % leadCount];
  const BellazioCloser* closer = &kBellazioClosers[g_clock.bellazioLastCloserIdx % closerCount];
  const char* closerSep = bellazioCloserSeparator(closer->type);

  char timePhrase[72];
  timePhrase[0] = '\0';
  if (m5 == 0) {
    if (sing) snprintf(timePhrase, sizeof(timePhrase), "e' l'%s", hr);
    else      snprintf(timePhrase, sizeof(timePhrase), "sono le %s", hr);
  } else if (m5 == 15) {
    if (sing) snprintf(timePhrase, sizeof(timePhrase), "l'%s e un quarto", hr);
    else      snprintf(timePhrase, sizeof(timePhrase), "le %s e un quarto", hr);
  } else if (m5 == 30) {
    if (sing) snprintf(timePhrase, sizeof(timePhrase), "l'%s e mezza", hr);
    else      snprintf(timePhrase, sizeof(timePhrase), "le %s e mezza", hr);
  } else if (m5 >= 45) {
    if (nhSing) snprintf(timePhrase, sizeof(timePhrase), "sara' l'%s tra poco", nhr);
    else        snprintf(timePhrase, sizeof(timePhrase), "saranno le %s tra poco", nhr);
  } else if (m5 < 30) {
    const char *mm = wordMinuteBellazio(m5);
    if (sing) snprintf(timePhrase, sizeof(timePhrase), "l'%s e %s", hr, mm);
    else      snprintf(timePhrase, sizeof(timePhrase), "le %s e %s", hr, mm);
  } else {
    const char *mm = wordMinuteBellazio(60 - m5);
    if (nhSing) snprintf(timePhrase, sizeof(timePhrase), "mancano %s all'%s", mm, nhr);
    else        snprintf(timePhrase, sizeof(timePhrase), "mancano %s alle %s", mm, nhr);
  }

  snprintf(out, outLen, "%s %s%s%s", lead, timePhrase, closerSep, closer->text);
}

// --- Language dispatch — vtable wrappers (M2) ---

static void composeWordClockSentenceActive(const tm &timeinfo, char *out, size_t outLen) {
  findLangVtable()->wordClock(timeinfo, out, outLen);
}

static const UiStrings* activeUiStrings() {
  return findLangVtable()->uiStrings;
}

static void drawWordClockInRect(int16_t ox, int16_t oy, int16_t ow, int16_t oh, const tm &timeinfo) {
  fillRectCanvas(ox, oy, ow, oh, DB_COLOR_BLACK);
  char l1[20], l2[20], l3[28];
  composeWordClockIt(timeinfo, l1, sizeof(l1), l2, sizeof(l2), l3, sizeof(l3));

  int scale = (ow >= 400) ? 3 : 2;
  const int lh = 7 * scale;
  const int gap = scale * 3;
  int y = oy + ((oh - ((3 * lh) + (2 * gap))) / 2);
  if (y < oy + 6) y = oy + 6;

  int16_t w1 = tinyTextWidth5x7(l1, scale);
  int16_t w2 = tinyTextWidth5x7(l2, scale);
  int16_t w3 = tinyTextWidth5x7(l3, scale);
  drawTinyText5x7(ox + ((ow - w1) / 2), y, l1, scale, DB_COLOR_WHITE);
  drawTinyText5x7(ox + ((ow - w2) / 2), y + lh + gap, l2, scale, DB_COLOR_WHITE);
  drawTinyText5x7(ox + ((ow - w3) / 2), y + (2 * (lh + gap)), l3, scale, DB_COLOR_WHITE);
}

static uint16_t weatherAccentColor(int code, bool isDay) {
  if (code >= 95) return DB_COLOR_YELLOW;
  if (code >= 71 && code <= 77) return 0xC69F;  // icy white-blue
  if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return 0x64DF;  // sky blue
  if (code == 0) return isDay ? 0xFDB8 : 0xB59F;  // warm sun / violet night
  return 0x9CF3;
}

#if !TEST_LVGL_UI
static const WeatherIconBitmap* weatherIconFromCode(int code, bool isDay) {
  if (code == 0 || code == 1) return isDay ? &WEATHER_ICON_SUN : &WEATHER_ICON_NIGHT;
  if (code == 2 || code == 3 || code == 45 || code == 48) return &WEATHER_ICON_CLOUD;
  if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return &WEATHER_ICON_RAIN;
  if (code >= 71 && code <= 77) return &WEATHER_ICON_SNOW;
  if (code >= 95) return &WEATHER_ICON_THUNDER;
  return &WEATHER_ICON_CLOUD;
}

static void drawWeatherIcon(int16_t centerX, int16_t centerY, int code, bool isDay) {
  const WeatherIconBitmap *icon = weatherIconFromCode(code, isDay);
  if (!icon) return;

  const int16_t x0 = centerX - (int16_t)(icon->w / 2);
  const int16_t y0 = centerY - (int16_t)(icon->h / 2);
  const int16_t cW = canvasWidth();
  const int16_t cH = canvasHeight();

  for (uint16_t y = 0; y < icon->h; ++y) {
    const int16_t py = y0 + (int16_t)y;
    if (py < 0 || py >= cH) continue;
    const uint32_t row = (uint32_t)y * (uint32_t)icon->w;
    for (uint16_t x = 0; x < icon->w; ++x) {
      const int16_t px = x0 + (int16_t)x;
      if (px < 0 || px >= cW) continue;
      const uint32_t idx = row + (uint32_t)x;
      if (!icon->alphaMask[idx]) continue;
      drawPixelCanvas(px, py, icon->rgb565[idx]);
    }
  }
}
#else
static void drawWeatherIcon(int16_t x, int16_t y, int code, bool isDay) {
  // Lightweight fallback icon set when LVGL UI is enabled (to save flash).
  if (code == 0) {
    if (isDay) {
      drawCircleCanvas(x, y, 10, DB_COLOR_YELLOW);
      fillRectCanvas(x - 2, y - 2, 4, 4, DB_COLOR_YELLOW);
    } else {
      drawCircleCanvas(x, y, 9, DB_COLOR_WHITE);
      fillRectCanvas(x + 2, y - 10, 8, 20, DB_COLOR_BLACK);
    }
    return;
  }
  drawCircleCanvas(x - 8, y + 2, 8, DB_COLOR_WHITE);
  drawCircleCanvas(x + 2, y - 2, 10, DB_COLOR_WHITE);
  drawCircleCanvas(x + 12, y + 2, 7, DB_COLOR_WHITE);
  fillRectCanvas(x - 16, y + 4, 34, 8, DB_COLOR_WHITE);
}
#endif

static void drawWeatherPanel(int16_t ox, int16_t oy, int16_t ow, int16_t oh, const tm &timeinfo) {
  (void)timeinfo;
  const uint16_t panelBg = 0x0841;
  const uint16_t textPrimary = 0xEF7D;
  const uint16_t textSecondary = 0xA534;
  fillRectCanvas(ox, oy, ow, oh, panelBg);
  const uint16_t accent =
#if TEST_WIFI
      g_weather.valid ? weatherAccentColor(g_weather.weatherCode, g_weather.isDay) : 0xA534;
#else
      0xA534;
#endif
#if TEST_WIFI
  char cityPretty[32];
  formatCityLabel(runtimeWeatherCityLabel(), cityPretty, sizeof(cityPretty));
  drawTinyText5x7(ox + 14, oy + 14, cityPretty, 2, textPrimary);
#else
  char cityPretty[32];
  formatCityLabel(WEATHER_CITY_LABEL, cityPretty, sizeof(cityPretty));
  drawTinyText5x7(ox + 14, oy + 14, cityPretty, 2, textPrimary);
#endif

#if TEST_WIFI
  if (!g_weather.valid) {
    drawTinyText5x7(ox + 14, oy + 56, "METEO OFFLINE", 2, textPrimary);
    drawTinyText5x7(ox + 14, oy + 82, "SYNC IN CORSO...", 2, textSecondary);
    drawWeatherIcon(ox + ow - 40, oy + 44, 2, true);
    return;
  }

  drawWeatherIcon(ox + ow - 40, oy + 44, g_weather.weatherCode, g_weather.isDay);
  const int temp = (int)lroundf(g_weather.tempC);
  const int absTemp = abs(temp);
  const int dW = 14;
  const int dH = 28;
  const int thick = 3;
  int tx = ox + 14;
  const int ty = oy + 42;

  if (temp < 0) {
    fillRectCanvas(tx, ty + (dH / 2), 8, 2, textPrimary);
    tx += 10;
  }
  if (absTemp >= 10) {
    draw7SegDigit(tx, ty, dW, dH, thick, (uint8_t)(absTemp / 10), textPrimary, panelBg);
    tx += dW + 4;
  }
  draw7SegDigit(tx, ty, dW, dH, thick, (uint8_t)(absTemp % 10), textPrimary, panelBg);
  fillRectCanvas(tx + dW + 4, ty + 4, 4, 4, accent);
  drawTinyText5x7(tx + dW + 12, ty + 2, "C", 2, textPrimary);

  const int condLines = drawTinyTextWrapped5x7(ox + 14, oy + 84, ow - 28, 2, weatherCodeLabelIt(g_weather.weatherCode), 2, textPrimary);
  const int humidityY = oy + 84 + ((condLines > 1 ? condLines : 1) * ((7 * 2) + 2)) + 8;
  drawTinyText5x7(ox + 14, humidityY, "UMIDITA", 2, textSecondary);
  char humBuf[8];
  snprintf(humBuf, sizeof(humBuf), "%d", g_weather.humidity);
  drawTinyText5x7(ox + ow - 52, humidityY, humBuf, 2, textPrimary);
  drawTinyText5x7(ox + ow - 28, humidityY, "%", 2, textPrimary);
#else
  drawTinyText5x7(ox + 14, oy + 56, "WIFI OFF", 2, textPrimary);
  drawWeatherIcon(ox + ow - 42, oy + 44, 2, true);
#endif
}

#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI && DISPLAY_BACKEND_ESP_LCD
// ---------- Unified Funnel Display font accessors ----------
// All themes share the same typeface. Only size varies by semantic role.

static const lv_font_t* lvglFontTitle()    { return &scry_font_funnel_display_30; }
static const lv_font_t* lvglFontBody()     { return &scry_font_funnel_display_24; }
static const lv_font_t* lvglFontSmall()    { return &scry_font_funnel_display_18; }
static const lv_font_t* lvglFontTiny()     { return &scry_font_funnel_display_14; }
static const lv_font_t* lvglFontMini()     { return &scry_font_funnel_display_16; }
static const lv_font_t* lvglFontMono()     { return &scry_font_funnel_display_16; }
static const lv_font_t* lvglFontMonoTiny() { return &lv_font_unscii_8; }
static const lv_font_t* lvglFontMeta()     { return &scry_font_funnel_display_20; }
static const lv_font_t* lvglFontInfoBody() { return &scry_font_funnel_display_16; }
static const lv_font_t* lvglFontRssNews()  { return &scry_font_funnel_display_22; }
static const lv_font_t* lvglFontClock()    { return &scry_font_funnel_display_32; }
static const lv_font_t* lvglFontBig()      { return &scry_font_funnel_display_32; }
static const lv_font_t* lvglFontCountdown(){ return &scry_font_funnel_display_countdown_60; }
static const lv_font_t* lvglFontTemp()     { return &scry_font_funnel_display_24; }

static const lv_font_t* lvglFontScreenSaverBalloonText() { return &scry_font_funnel_display_18; }
static const lv_font_t* lvglFontScreenSaverFooterText()  { return &scry_font_funnel_display_24; }
static const lv_font_t* lvglFontScreenSaverTail()        { return &scry_font_funnel_display_16; }

static const lv_font_t* lvglNowPlayingTitleFont()  { return &scry_font_funnel_display_25; }
static const lv_font_t* lvglNowPlayingBodyFont()   { return &scry_font_funnel_display_18; }
static const lv_font_t* lvglNowPlayingArtistFont() { return &scry_font_funnel_display_23; }
static const lv_font_t* lvglNowPlayingMetaFont()   { return &scry_font_funnel_display_16; }

static lv_coord_t lvglClockLineSpaceForFont(const lv_font_t *font) {
  if (!font) return 2;
  const lv_coord_t h = (lv_coord_t)font->line_height;
  if (h >= 34) return 4;
  if (h >= 24) return 3;
  if (h >= 18) return 2;
  return 1;
}

static uint8_t lvglCollectClockFonts(const lv_font_t **out, uint8_t cap) {
  if (!out || cap == 0) return 0;
  uint8_t n = 0;
  out[n++] = &scry_font_funnel_display_32;
  if (n < cap) out[n++] = &scry_font_funnel_display_30;
  if (n < cap) out[n++] = &scry_font_funnel_display_24;
  if (n < cap) out[n++] = &scry_font_funnel_display_22;
  if (n < cap) out[n++] = &scry_font_funnel_display_20;
  if (n < cap) out[n++] = &scry_font_funnel_display_18;
  return n;
}

static void lvglApplyClockSentenceAutoFit(const char *text) {
  if (!g_clockUi.l1 || !g_clockUi.block || !g_clockUi.header || !text) return;
  const int16_t blockH = lv_obj_get_height(g_clockUi.block);
  const int16_t headerH = lv_obj_get_height(g_clockUi.header);
  const int16_t maxTextH = (blockH - headerH) - 6;
  const lv_font_t *fonts[10];
  const uint8_t count = lvglCollectClockFonts(fonts, (uint8_t)(sizeof(fonts) / sizeof(fonts[0])));
  if (count == 0) return;

  const lv_font_t *chosen = fonts[count - 1];
  lv_coord_t chosenLineSpace = lvglClockLineSpaceForFont(chosen);

  for (uint8_t i = 0; i < count; ++i) {
    const lv_font_t *f = fonts[i];
    const lv_coord_t ls = lvglClockLineSpaceForFont(f);
    lv_obj_set_style_text_font(g_clockUi.l1, f, 0);
    lv_obj_set_style_text_line_space(g_clockUi.l1, ls, 0);
    lv_label_set_text(g_clockUi.l1, text);
    lvglCenterClockSentenceLabel();
    lv_obj_update_layout(g_clockUi.l1);
    const int16_t textH = lv_obj_get_height(g_clockUi.l1);
    if (textH <= maxTextH) {
      chosen = f;
      chosenLineSpace = ls;
      break;
    }
  }

  lv_obj_set_style_text_font(g_clockUi.l1, chosen, 0);
  lv_obj_set_style_text_line_space(g_clockUi.l1, chosenLineSpace, 0);
  lv_label_set_text(g_clockUi.l1, text);
  lvglCenterClockSentenceLabel();
}

static void formatTrackDuration(uint16_t seconds, char *out, size_t outLen) {
  const uint16_t mins = seconds / 60U;
  const uint16_t secs = seconds % 60U;
  snprintf(out, outLen, "%u:%02u", (unsigned)mins, (unsigned)secs);
}

static void resolveFakeNowPlayingTrack(uint32_t nowMs, uint16_t *elapsedSecOut, uint8_t *indexOut) {
  const size_t trackCount = sizeof(kFakeNowPlayingTracks) / sizeof(kFakeNowPlayingTracks[0]);
  if (trackCount == 0) {
    if (elapsedSecOut) *elapsedSecOut = 0;
    if (indexOut) *indexOut = 0;
    return;
  }
  const uint8_t idx = (uint8_t)((nowMs / NOW_PLAYING_FAKE_ROTATE_MS) % trackCount);
  const FakeNowPlayingTrack &track = kFakeNowPlayingTracks[idx];
  const uint32_t trackMs = nowMs % NOW_PLAYING_FAKE_ROTATE_MS;
  uint16_t elapsed = (uint16_t)(track.baseElapsedSec + (trackMs / 1000UL));
  if (elapsed >= track.durationSec) elapsed = (uint16_t)(track.durationSec - 1U);
  if (elapsedSecOut) *elapsedSecOut = elapsed;
  if (indexOut) *indexOut = idx;
}

static void lvglApplyAdaptiveWrapFont(lv_obj_t *label, const char *text, lv_coord_t maxHeight,
                                      const lv_font_t *f0, const lv_font_t *f1,
                                      const lv_font_t *f2, const lv_font_t *f3) {
  if (!label || !text) return;
  const lv_font_t *fonts[] = {f0, f1, f2, f3};
  const lv_font_t *chosenFallback = fonts[3] ? fonts[3] : lvglFontTiny();
  const lv_font_t *chosen = chosenFallback;
  for (const lv_font_t *font : fonts) {
    if (!font) continue;
    lv_obj_set_style_text_font(label, font, 0);
    lv_label_set_text(label, text);
    lv_obj_update_layout(label);
    if (lv_obj_get_height(label) <= maxHeight) {
      chosen = font;
      break;
    }
  }
  lv_obj_set_style_text_font(label, chosen, 0);
  lv_label_set_text(label, text);
}

static int16_t lvglMeasureFontTextWidth(const lv_font_t *font, const char *text) {
  if (!font || !text) return 0;
  int16_t width = 0;
  while (*text) {
    const uint32_t letter = (uint8_t)*text;
    const uint32_t next = (uint8_t)*(text + 1);
    width += (int16_t)lv_font_get_glyph_width(font, letter, next);
    ++text;
  }
  return width;
}

static void lvglBuildWrappedTitle(char *out, size_t outSize, const char *text,
                                  const lv_font_t *font, lv_coord_t maxWidth, uint8_t maxLines) {
  if (!out || outSize == 0) return;
  out[0] = '\0';
  if (!text || !*text || !font || maxWidth <= 0 || maxLines == 0) return;

  size_t outLen = 0;
  uint8_t line = 1;
  const char *p = text;
  char currentLine[160] = {0};

  auto appendToOut = [&](const char *chunk) {
    if (!chunk || !*chunk || outLen >= (outSize - 1)) return;
    const size_t remain = outSize - outLen - 1;
    strncat(out, chunk, remain);
    outLen = strlen(out);
  };

  while (*p && line <= maxLines) {
    while (*p == ' ') ++p;
    if (!*p) break;

    char word[80] = {0};
    size_t wordLen = 0;
    while (*p && *p != ' ' && wordLen < (sizeof(word) - 1)) word[wordLen++] = *p++;
    word[wordLen] = '\0';
    if (!word[0]) continue;

    char candidate[160] = {0};
    if (currentLine[0]) {
      snprintf(candidate, sizeof(candidate), "%s %s", currentLine, word);
    } else {
      snprintf(candidate, sizeof(candidate), "%s", word);
    }

    if (lvglMeasureFontTextWidth(font, candidate) <= maxWidth) {
      strlcpy(currentLine, candidate, sizeof(currentLine));
      continue;
    }

    if (line < maxLines) {
      if (currentLine[0]) {
        if (outLen > 0) appendToOut("\n");
        appendToOut(currentLine);
        strlcpy(currentLine, word, sizeof(currentLine));
      } else {
        char dotted[96] = {0};
        strlcpy(currentLine, word, sizeof(currentLine));
        while (currentLine[0]) {
          snprintf(dotted, sizeof(dotted), "%s...", currentLine);
          if (lvglMeasureFontTextWidth(font, dotted) <= maxWidth) break;
          currentLine[strlen(currentLine) - 1] = '\0';
        }
        if (!currentLine[0]) strlcpy(currentLine, "...", sizeof(currentLine));
      }
      ++line;
      continue;
    }

    char finalLine[160] = {0};
    strlcpy(finalLine, currentLine[0] ? currentLine : word, sizeof(finalLine));
    while (finalLine[0]) {
      char dotted[168] = {0};
      snprintf(dotted, sizeof(dotted), "%s...", finalLine);
      if (lvglMeasureFontTextWidth(font, dotted) <= maxWidth) {
        if (outLen > 0) appendToOut("\n");
        appendToOut(dotted);
        return;
      }
      char *lastSpace = strrchr(finalLine, ' ');
      if (lastSpace) {
        *lastSpace = '\0';
      } else {
        finalLine[strlen(finalLine) - 1] = '\0';
      }
    }

    if (outLen > 0) appendToOut("\n");
    appendToOut("...");
    return;
  }

  if (currentLine[0]) {
    if (outLen > 0) appendToOut("\n");
    appendToOut(currentLine);
  }
}

static void lvglApplyThemeFonts() {
  if (g_infoUi.title) lv_obj_set_style_text_font(g_infoUi.title, lvglFontSmall(), 0);
  if (g_infoUi.endpoint) lv_obj_set_style_text_font(g_infoUi.endpoint, lvglFontSmall(), 0);
  if (g_infoUi.bodyLeft) lv_obj_set_style_text_font(g_infoUi.bodyLeft, lvglFontInfoBody(), 0);

  if (g_clockUi.date) lv_obj_set_style_text_font(g_clockUi.date, lvglFontSmall(), 0);
  if (g_clockUi.l1) lv_obj_set_style_text_font(g_clockUi.l1, lvglFontClock(), 0);
  if (g_clockUi.l2) lv_obj_set_style_text_font(g_clockUi.l2, lvglFontTitle(), 0);
  if (g_clockUi.l3) lv_obj_set_style_text_font(g_clockUi.l3, lvglFontTitle(), 0);
  if (g_weatherUi.city) lv_obj_set_style_text_font(g_weatherUi.city, lvglFontSmall(), 0);
  if (g_weatherUi.sun) lv_obj_set_style_text_font(g_weatherUi.sun, lvglFontSmall(), 0);
  if (g_weatherUi.temp) lv_obj_set_style_text_font(g_weatherUi.temp, lvglFontTemp(), 0);
  if (g_weatherUi.glyph) lv_obj_set_style_text_font(g_weatherUi.glyph, lvglFontBig(), 0);
  if (g_weatherUi.desc) lv_obj_set_style_text_font(g_weatherUi.desc, lvglFontMeta(), 0);
  if (g_weatherUi.humidity) lv_obj_set_style_text_font(g_weatherUi.humidity, lvglFontMini(), 0);
  if (g_weatherUi.wind) lv_obj_set_style_text_font(g_weatherUi.wind, lvglFontTiny(), 0);
  if (g_weatherUi.forecastNow) lv_obj_set_style_text_font(g_weatherUi.forecastNow, lvglFontSmall(), 0);
  if (g_weatherUi.forecastTomorrow) lv_obj_set_style_text_font(g_weatherUi.forecastTomorrow, lvglFontTiny(), 0);
  if (g_nowPlayingUi.title) lv_obj_set_style_text_font(g_nowPlayingUi.title, lvglNowPlayingMetaFont(), 0);
  if (g_nowPlayingUi.status) lv_obj_set_style_text_font(g_nowPlayingUi.status, lvglNowPlayingMetaFont(), 0);
  if (g_nowPlayingUi.coverTop) lv_obj_set_style_text_font(g_nowPlayingUi.coverTop, lvglFontTiny(), 0);
  if (g_nowPlayingUi.coverBottom) lv_obj_set_style_text_font(g_nowPlayingUi.coverBottom, lvglFontSmall(), 0);
  if (g_nowPlayingUi.track) lv_obj_set_style_text_font(g_nowPlayingUi.track, lvglNowPlayingTitleFont(), 0);
  if (g_nowPlayingUi.artist) lv_obj_set_style_text_font(g_nowPlayingUi.artist, lvglNowPlayingArtistFont(), 0);
  if (g_nowPlayingUi.album) lv_obj_set_style_text_font(g_nowPlayingUi.album, lvglFontTiny(), 0);
  if (g_nowPlayingUi.source) lv_obj_set_style_text_font(g_nowPlayingUi.source, lvglFontTiny(), 0);
  if (g_nowPlayingUi.progressElapsed) lv_obj_set_style_text_font(g_nowPlayingUi.progressElapsed, lvglFontTiny(), 0);
  if (g_nowPlayingUi.progressRemaining) lv_obj_set_style_text_font(g_nowPlayingUi.progressRemaining, lvglFontTiny(), 0);

  {
    FeedDeckUi *feedDecks[] = {&g_auxDeck, &g_wikiDeck};
    for (FeedDeckUi *d : feedDecks) {
      if (d->nextFeedBtnText) lv_obj_set_style_text_font(d->nextFeedBtnText, lvglFontTiny(),    0);
      if (d->refreshBtnText)  lv_obj_set_style_text_font(d->refreshBtnText,  lvglFontTiny(),    0);
      if (d->qrBtnText)       lv_obj_set_style_text_font(d->qrBtnText,       lvglFontTiny(),    0);
      if (d->title)           lv_obj_set_style_text_font(d->title,           lvglFontSmall(),   0);
      if (d->status)          lv_obj_set_style_text_font(d->status,          lvglFontTiny(),    0);
      if (d->sourceBadgeText) lv_obj_set_style_text_font(d->sourceBadgeText, lvglFontTiny(),    0);
      if (d->sourceSite)      lv_obj_set_style_text_font(d->sourceSite,      lvglFontMeta(),    0);
      if (d->news)            lv_obj_set_style_text_font(d->news,            lvglFontRssNews(), 0);
      if (d->meta)            lv_obj_set_style_text_font(d->meta,            lvglFontSmall(),   0);
      if (d->qrHint)          lv_obj_set_style_text_font(d->qrHint,          lvglNowPlayingArtistFont(), 0);
    }
  }

#if SCREENSAVER_ENABLED
  if (g_saver.sky) lv_obj_set_style_text_font(g_saver.sky, lvglFontMono(), 0);
  for (uint8_t r = 0; r < kSaverSkyRowsMax; ++r) {
    for (uint8_t s = 0; s < kSaverStarsPerRow; ++s) {
      if (!g_saver.starObj[r][s]) continue;
      lv_obj_set_style_text_font(g_saver.starObj[r][s], lvglFontMonoTiny(), 0);
    }
  }
  if (g_saver.field) lv_obj_set_style_text_font(g_saver.field, lvglFontMonoTiny(), 0);
  if (g_saver.cow) lv_obj_set_style_text_font(g_saver.cow, lvglFontMonoTiny(), 0);
  if (g_saver.balloon) lv_obj_set_style_text_font(g_saver.balloon, lvglFontScreenSaverBalloonText(), 0);
  if (g_saver.balloonTail) lv_obj_set_style_text_font(g_saver.balloonTail, lvglFontScreenSaverTail(), 0);
  if (g_saver.footer) lv_obj_set_style_text_font(g_saver.footer, lvglFontScreenSaverFooterText(), 0);
#endif
  if (g_clockUi.l1) lvglApplyClockSentenceAutoFit(lv_label_get_text(g_clockUi.l1));

  // Transit fonts
  if (g_transitUi.title)   lv_obj_set_style_text_font(g_transitUi.title,   lvglFontSmall(), 0);
  if (g_transitUi.station) lv_obj_set_style_text_font(g_transitUi.station, lvglFontSmall(), 0);
  if (g_transitUi.status)  lv_obj_set_style_text_font(g_transitUi.status,  lvglFontTiny(),  0);
  if (g_transitUi.noData)  lv_obj_set_style_text_font(g_transitUi.noData,  lvglFontSmall(), 0);
  for (uint8_t i = 0; i < TRANSIT_MAX_DEPARTURES; ++i) {
    if (g_transitUi.line_[i])    lv_obj_set_style_text_font(g_transitUi.line_[i],    lvglFontTiny(),  0);
    if (g_transitUi.dest[i])     lv_obj_set_style_text_font(g_transitUi.dest[i],     lvglFontMeta(),  0);
    if (g_transitUi.time_[i])    lv_obj_set_style_text_font(g_transitUi.time_[i],    lvglFontMeta(),  0);
    if (g_transitUi.delay[i])    lv_obj_set_style_text_font(g_transitUi.delay[i],    lvglFontTiny(),  0);
    if (g_transitUi.platform[i]) lv_obj_set_style_text_font(g_transitUi.platform[i], lvglFontMeta(),  0);
  }
}

static const char* weatherGlyphText(int code, bool isDay) {
  if (code >= 95) return LV_SYMBOL_WARNING;
  if (code >= 71 && code <= 77) return LV_SYMBOL_DRIVE;
  if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return LV_SYMBOL_REFRESH;
  if (code == 0 || code == 1) return isDay ? LV_SYMBOL_EYE_OPEN : LV_SYMBOL_EYE_CLOSE;
  return LV_SYMBOL_UPLOAD;
}

static const lv_img_dsc_t* weatherImageFromCode(int code, bool isDay) {
#if DB_HAS_LVGL_WEATHER_IMAGES
  if (code >= 95) return &image_weather_thunder;
  if (code >= 71 && code <= 77) return &image_weather_snow;
  if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return &image_weather_rain;
  if (code == 0 || code == 1) return isDay ? &image_weather_sun : &image_weather_night;
  return &image_weather_cloud;
#else
  (void)code;
  (void)isDay;
  return nullptr;
#endif
}

static const lv_img_dsc_t* weatherForecastImageFromCode(int code, bool isDay) {
#if DB_HAS_LVGL_WEATHER_MIN_IMAGES
  if (code >= 95) return &image_weather_min_thunder;
  if (code >= 71 && code <= 77) return &image_weather_min_snow;
  if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return &image_weather_min_rain;
  if (code == 0 || code == 1) return isDay ? &image_weather_min_sun : &image_weather_min_night;
  return &image_weather_min_cloud;
#else
  return weatherImageFromCode(code, isDay);
#endif
}

static const char* utf8Degree() {
  return "\xC2\xB0";
}

static void lvglIconFloatAnimCb(void *obj, int32_t v) {
  if (!obj) return;
  lv_obj_set_style_translate_y((lv_obj_t*)obj, v, LV_PART_MAIN);
}

static void lvglCenterClockSentenceLabel() {
  if (!g_clockUi.l1 || !g_clockUi.block || !g_clockUi.header) return;
  const int16_t blockW = lv_obj_get_width(g_clockUi.block);
  const int16_t blockH = lv_obj_get_height(g_clockUi.block);
  const int16_t headerH = lv_obj_get_height(g_clockUi.header);
  constexpr int16_t kSidePad = 8;
  const int16_t labelW = blockW - (kSidePad * 2);
  if (labelW < 24) return;
  lv_obj_set_width(g_clockUi.l1, labelW);
  lv_obj_set_x(g_clockUi.l1, kSidePad);
  lv_obj_update_layout(g_clockUi.l1);
  const int16_t textH = lv_obj_get_height(g_clockUi.l1);
  const int16_t bodyH = blockH - headerH;
  int16_t y = headerH + ((bodyH - textH) / 2);
  if (y < (headerH + 2)) y = headerH + 2;
  lv_obj_set_y(g_clockUi.l1, y);
}

static void lvglForceLabelVisible(lv_obj_t *obj) {
  if (!obj) return;
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_text_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_text_decor(obj, LV_TEXT_DECOR_NONE, LV_PART_MAIN);
  lv_obj_move_foreground(obj);
}

static uint8_t wifiSignalBarsFromRssi(int rssiDbm) {
  if (rssiDbm >= -55) return 4;
  if (rssiDbm >= -67) return 3;
  if (rssiDbm >= -75) return 2;
  if (rssiDbm >= -85) return 1;
  return 0;
}

static bool wifiIsReconnectingUiState() {
#if TEST_WIFI || TEST_NTP
  const wl_status_t st = WiFi.status();
  if (st == WL_CONNECTED && g_wifiSt.connected) return false;
  if (st == WL_DISCONNECTED || st == WL_CONNECT_FAILED || st == WL_CONNECTION_LOST) return true;
  if (st == WL_IDLE_STATUS || st == WL_SCAN_COMPLETED || st == WL_NO_SSID_AVAIL) return true;
  if (g_wifiSt.reconnectAttemptActive) return true;
  if (g_wifiSt.everConnected) return true;
  const uint32_t now = millis();
  if (g_wifiSt.lastDisconnectMs > 0 && (now - g_wifiSt.lastDisconnectMs) < 20000UL) return true;
#endif
  return false;
}

static void lvglUpdateWiFiBars(bool force) {
  if (!g_clockUi.wifiBars[0]) return;

  const UiThemeLvglTokens &t = activeUiTheme().lvgl;
  const lv_color_t kBarOff = lv_color_hex(t.wifiBarOff);
  const lv_color_t kBarOn = lv_color_hex(t.wifiBarOn);
  const lv_color_t kBarWave = lv_color_hex(t.wifiBarWave);
  uint8_t mask = 0;
  uint8_t waveMask = 0;

#if TEST_WIFI || TEST_NTP
  const wl_status_t st = WiFi.status();
  const bool connected = (st == WL_CONNECTED) && g_wifiSt.connected;
  if (connected) {
    uint8_t bars = wifiSignalBarsFromRssi(WiFi.RSSI());
    if (bars == 0) bars = 1;
    mask = (uint8_t)((1U << bars) - 1U);
  } else if (wifiIsReconnectingUiState()) {
    const uint8_t phase = (uint8_t)((millis() / 240UL) % 4UL);
    const uint8_t prev = (uint8_t)((phase + 3U) % 4U);
    mask = (uint8_t)(1U << phase);
    waveMask = (uint8_t)(1U << prev);
  } else {
    mask = 0;
  }
#else
  mask = 0;
#endif

  const uint16_t styleKey = (uint16_t)mask | ((uint16_t)waveMask << 8);
  if (!force && styleKey == g_clockUi.wifiMask) return;
  g_clockUi.wifiMask = styleKey;

  for (uint8_t i = 0; i < 4; ++i) {
    lv_obj_t *bar = g_clockUi.wifiBars[i];
    if (!bar) continue;
    if (mask & (1U << i)) {
      lv_obj_set_style_bg_color(bar, kBarOn, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    } else if (waveMask & (1U << i)) {
      lv_obj_set_style_bg_color(bar, kBarWave, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(bar, LV_OPA_80, LV_PART_MAIN);
    } else {
      lv_obj_set_style_bg_color(bar, kBarOff, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(bar, (lv_opa_t)190, LV_PART_MAIN);
    }
    lv_obj_invalidate(bar);
  }
}

static void formatRssTextToLabelBounds(const char *text, lv_obj_t *label, char *out, size_t outLen) {
  if (!out || outLen == 0) return;
  out[0] = '\0';
  if (!text || !text[0]) return;

  String src(text);
  src.trim();
  if (src.length() == 0) return;
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI
  if (label) {
    lv_obj_update_layout(lv_scr_act());
    const lv_font_t *font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    const lv_coord_t lineSpace = lv_obj_get_style_text_line_space(label, LV_PART_MAIN);
    const lv_coord_t lineH = font ? lv_font_get_line_height(font) : 22;
    lv_coord_t maxH = lv_obj_get_height(label);
    if (maxH < lineH) maxH = (lineH * 3) + (lineSpace * 2);

    String candidate = src;
    auto fitsInBounds = [&](const String &s) -> bool {
      lv_label_set_text(label, s.c_str());
      lv_obj_update_layout(lv_scr_act());
      return lv_obj_get_content_height(label) <= maxH;
    };

    if (fitsInBounds(candidate)) {
      strncpy(out, candidate.c_str(), outLen - 1);
      out[outLen - 1] = '\0';
      return;
    }

    while (candidate.length() > 0) {
      const int cut = candidate.lastIndexOf(' ');
      if (cut <= 0) break;
      candidate.remove(cut);
      String trial = candidate;
      trial.trim();
      trial += "...";
      if (fitsInBounds(trial)) {
        strncpy(out, trial.c_str(), outLen - 1);
        out[outLen - 1] = '\0';
        return;
      }
    }
  }
#endif
  // Fallback without pixel metrics.
  if (src.length() > (int)(outLen - 1)) {
    src = src.substring(0, outLen - 4);
    src += "...";
  }
  strncpy(out, src.c_str(), outLen - 1);
  out[outLen - 1] = '\0';
}

static void buildRssStoryBlock(const RssItem &item, lv_obj_t *label, char *out, size_t outLen, bool includeTitle) {
  if (!out || outLen == 0) return;
  out[0] = '\0';

  String story;
  if (includeTitle && item.title[0]) {
    story = item.title;
  }
  String summary(item.summary);
  summary.trim();
  if (summary.length() > 0) {
    if (story.length() > 0) story += "\n";
    story += summary;
  }
  story.trim();
  if (story.length() == 0) {
    if (item.title[0]) {
      formatRssTextToLabelBounds(item.title, label, out, outLen);
      return;
    }
    strncpy(out, activeUiStrings()->rssSyncing, outLen - 1);
    out[outLen - 1] = '\0';
    return;
  }
  formatRssTextToLabelBounds(story.c_str(), label, out, outLen);
}

static const char *wikiFeedUiName(uint8_t slot) {
  switch (slot) {
    case 0: return "Featured";
    case 1: return "On this day";
    case 2: return "Random";
    default: return "Wikipedia";
  }
}

static const char *wikiFeedUiBadge(uint8_t slot) {
  switch (slot) {
    case 0: return "WF";
    case 1: return "WD";
    case 2: return "WR";
    default: return "W";
  }
}

static uint32_t wikiFeedUiColorHex(uint8_t slot) {
  switch (slot) {
    case 0: return 0x295FA8;
    case 1: return 0x2D7D64;
    case 2: return 0xD4A017;
    default: return 0x2B468E;
  }
}

static void lvglUpdateFeedDeck(FeedDeckUi &d, RssState &content, bool isWiki, bool force) {
  if (!d.news) return;
  const char *contentTag = isWiki ? "WIKI-DECK" : "RSS";
#if TEST_WIFI && RSS_ENABLED
  const uint32_t now = millis();
  if (g_wifiSt.connected && content.valid && content.itemCount > 1 && content.lastRotateMs != 0 &&
      !d.qrModalOpen && (now - content.lastRotateMs) >= RSS_ROTATE_MS) {
    content.currentIndex = (uint8_t)((content.currentIndex + 1) % content.itemCount);
    content.lastRotateMs = now;
    force = true;
  }
#endif

  char title3[256];
  char whenLine[64];  // AUX only; unused for Wiki
  char status[32];
  char meta[96];
  char siteShort[16];
  char siteBadge[4];
  char sourceLine[96];
  char siteHost[96];
  uint32_t siteColorHex = 0x2B468E;
  uint32_t siteTextHex  = 0xFFFFFF;
  bool sourceLineSet    = false;
  title3[0] = whenLine[0] = status[0] = meta[0] = sourceLine[0] = '\0';
  strncpy(siteShort, isWiki ? "WIKI" : "RSS", sizeof(siteShort) - 1); siteShort[sizeof(siteShort)-1] = '\0';
  strncpy(siteBadge, isWiki ? "W"    : "R",   sizeof(siteBadge) - 1); siteBadge[sizeof(siteBadge)-1] = '\0';
  strncpy(siteHost,  isWiki ? "wiki" : "rss", sizeof(siteHost)  - 1); siteHost[sizeof(siteHost)-1]   = '\0';
  int16_t showIndex = -1;
  if (d.title) lv_label_set_text(d.title, isWiki ? "ScryBar Wiki" : "ScryBar RSS");

#if TEST_WIFI && RSS_ENABLED
  if (!g_wifiSt.connected) {
    strncpy(title3, activeUiStrings()->rssOffline, sizeof(title3) - 1); title3[sizeof(title3)-1] = '\0';
    if (!isWiki) { strncpy(whenLine, "--/-- --:--", sizeof(whenLine)-1); whenLine[sizeof(whenLine)-1] = '\0'; }
    snprintf(status, sizeof(status), "OFF");
    snprintf(meta, sizeof(meta), "Ultimo fetch: %s", content.lastFetchMs ? content.fetchedAt : "--/-- --:--");
  } else if (content.valid && content.itemCount > 0) {
    showIndex = (int16_t)(content.currentIndex % content.itemCount);
    buildRssStoryBlock(content.items[showIndex], d.news, title3, sizeof(title3), !isWiki);
    if (!isWiki) buildRssWhenLabel(content.items[showIndex].pubDate, whenLine, sizeof(whenLine));
    snprintf(status, sizeof(status), "%u/%u", (unsigned)(showIndex + 1), (unsigned)content.itemCount);
    uint32_t rotateLeftSec = 0;
    if (content.lastRotateMs != 0) {
      const uint32_t elapsed = now - content.lastRotateMs;
      rotateLeftSec = (elapsed >= RSS_ROTATE_MS) ? 0 : (uint32_t)(((RSS_ROTATE_MS - elapsed) + 999UL) / 1000UL);
    }
    if (isWiki) {
      const uint8_t wikiSlot = content.items[showIndex].feedSlot;
      extractRssHost(content.items[showIndex].link, siteHost, sizeof(siteHost));
      copyStringSafe(siteBadge, sizeof(siteBadge), wikiFeedUiBadge(wikiSlot));
      siteColorHex = wikiFeedUiColorHex(wikiSlot);
      siteTextHex  = 0xFFFFFF;
      const char *feedName = wikiFeedUiName(wikiSlot);
      snprintf(sourceLine, sizeof(sourceLine), "Wikipedia | %s", feedName);
      if (d.qrModalOpen) {
        snprintf(meta, sizeof(meta), "%s | QR", feedName);
      } else {
        char titleHead[72];
        copyStringSafe(titleHead, sizeof(titleHead), content.items[showIndex].title[0] ? content.items[showIndex].title : "-");
        sanitizeAsciiBuffer(titleHead, sizeof(titleHead));
        snprintf(meta, sizeof(meta), "%s | %s", feedName, titleHead);
      }
      sourceLineSet = true;
    } else {
      rssResolveSourceHost(content.items[showIndex], siteHost, sizeof(siteHost));
      buildRssSiteShortName(siteHost, siteShort, sizeof(siteShort));
      buildRssSiteBadge(siteShort, siteBadge, sizeof(siteBadge));
      siteColorHex = rssSiteColorHexFromHost(siteHost);
      if (strcmp(siteShort, "ANSA") == 0) { siteColorHex = 0xFFFFFF; siteTextHex = 0x1B3C86; }
      if (d.qrModalOpen) snprintf(meta, sizeof(meta), "Fetch %s | QR", content.fetchedAt);
      else               snprintf(meta, sizeof(meta), "Fetch %s | %lus", content.fetchedAt, (unsigned long)rotateLeftSec);
    }
  } else if (content.lastHttpCode != 0) {
    strncpy(title3, activeUiStrings()->rssFeedError, sizeof(title3) - 1); title3[sizeof(title3)-1] = '\0';
    if (!isWiki) { strncpy(whenLine, "--/-- --:--", sizeof(whenLine)-1); whenLine[sizeof(whenLine)-1] = '\0'; }
    snprintf(status, sizeof(status), "ERR %d", content.lastHttpCode);
    snprintf(meta, sizeof(meta), "Fetch %s", content.lastFetchMs ? content.fetchedAt : "--/-- --:--");
  } else {
    strncpy(title3, activeUiStrings()->rssSyncing, sizeof(title3) - 1); title3[sizeof(title3)-1] = '\0';
    if (!isWiki) { strncpy(whenLine, "--/-- --:--", sizeof(whenLine)-1); whenLine[sizeof(whenLine)-1] = '\0'; }
    snprintf(status, sizeof(status), "SYNC");
    snprintf(meta, sizeof(meta), "Fetch --/-- --:--");
  }
#elif TEST_WIFI
  strncpy(title3, activeUiStrings()->rssDisabled, sizeof(title3)-1); title3[sizeof(title3)-1] = '\0';
  if (!isWiki) { strncpy(whenLine, "--/-- --:--", sizeof(whenLine)-1); whenLine[sizeof(whenLine)-1] = '\0'; }
  snprintf(status, sizeof(status), g_wifiSt.connected ? "WiFi" : "OFF");
  snprintf(meta, sizeof(meta), "Fetch --/-- --:--");
#else
  {
    const char *naMsg = isWiki ? "Wiki non disponibile\n(TEST_WIFI=0)." : "RSS non disponibile\n(TEST_WIFI=0).";
    strncpy(title3, naMsg, sizeof(title3)-1); title3[sizeof(title3)-1] = '\0';
  }
  if (!isWiki) { strncpy(whenLine, "--/-- --:--", sizeof(whenLine)-1); whenLine[sizeof(whenLine)-1] = '\0'; }
  snprintf(status, sizeof(status), "N/A");
  snprintf(meta, sizeof(meta), "Fetch --/-- --:--");
#endif

  if (!sourceLineSet) snprintf(sourceLine, sizeof(sourceLine), "%s", siteShort);

  sanitizeAsciiBuffer(title3, sizeof(title3));
  sanitizeAsciiBuffer(meta, sizeof(meta));
  sanitizeAsciiBuffer(sourceLine, sizeof(sourceLine));

  char sourceWithWhen[140];
  copyStringSafe(sourceWithWhen, sizeof(sourceWithWhen), sourceLine);
  if (!isWiki) {
    sanitizeAsciiBuffer(whenLine, sizeof(whenLine));
    if (whenLine[0] && strcmp(whenLine, "--/-- --:--") != 0) {
      strncat(sourceWithWhen, " - ", sizeof(sourceWithWhen) - strlen(sourceWithWhen) - 1);
      strncat(sourceWithWhen, whenLine, sizeof(sourceWithWhen) - strlen(sourceWithWhen) - 1);
    }
  }

  lv_label_set_text(d.news, title3);
  lvglForceLabelVisible(d.news);
  if (d.sourceBadge) lv_obj_clear_flag(d.sourceBadge, LV_OBJ_FLAG_HIDDEN);
  if (d.sourceSite) { lv_label_set_text(d.sourceSite, sourceWithWhen); lvglForceLabelVisible(d.sourceSite); }
  if (d.sourceBadgeText) {
    lv_label_set_text(d.sourceBadgeText, siteBadge);
    lv_obj_set_style_text_color(d.sourceBadgeText, lv_color_hex(siteTextHex), 0);
    lvglForceLabelVisible(d.sourceBadgeText);
  }
  if (d.sourceBadge) lv_obj_set_style_bg_color(d.sourceBadge, lv_color_hex(siteColorHex), LV_PART_MAIN);
  // Show cached favicon image over text badge (if available) — touch LRU on hit
  if (d.sourceBadgeImg) {
    const lv_img_dsc_t *fav = nullptr;
    for (uint8_t i = 0; i < kFaviconCacheSlots; ++i) {
      if (g_faviconCache[i].valid && strcmp(g_faviconCache[i].host, siteHost) == 0) {
        g_faviconCache[i].lastAccessMs = millis();
        fav = &g_faviconCache[i].imgDsc;
        break;
      }
    }
    if (fav) {
      lv_img_set_src(d.sourceBadgeImg, fav);
      lv_obj_clear_flag(d.sourceBadgeImg, LV_OBJ_FLAG_HIDDEN);
      if (d.sourceBadgeText) lv_obj_add_flag(d.sourceBadgeText, LV_OBJ_FLAG_HIDDEN);
      // Hide badge background — favicon fills the space
      if (d.sourceBadge) lv_obj_set_style_bg_opa(d.sourceBadge, LV_OPA_TRANSP, 0);
    } else {
      lv_obj_add_flag(d.sourceBadgeImg, LV_OBJ_FLAG_HIDDEN);
      if (d.sourceBadgeText) lv_obj_clear_flag(d.sourceBadgeText, LV_OBJ_FLAG_HIDDEN);
      if (d.sourceBadge) lv_obj_set_style_bg_opa(d.sourceBadge, LV_OPA_COVER, 0);
    }
  }
  if (d.status) { lv_label_set_text(d.status, status); lvglForceLabelVisible(d.status); }
  if (d.meta)   { lv_label_set_text(d.meta, meta);     lvglForceLabelVisible(d.meta); }

#if defined(LV_USE_QRCODE) && LV_USE_QRCODE
#if TEST_WIFI && RSS_ENABLED
  if (d.qrOverlay) {
    if (d.qrModalOpen) {
      lv_obj_clear_flag(d.qrOverlay, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(d.qrOverlay);
      if (d.qrHint) lv_obj_clear_flag(d.qrHint, LV_OBJ_FLAG_HIDDEN);
      if (d.qr) {
        const char *url = isWiki ? "https://en.wikipedia.org" : "https://ansa.it";
        bool qrReady = true;
        int16_t qrIndex = -1;
        if (showIndex >= 0 && showIndex < (int16_t)content.itemCount) {
          RssItem &item = content.items[showIndex];
          if (item.link[0]) url = item.link;
          qrIndex = showIndex;
        }
        if (qrReady) {
          if (force || d.lastItemShown != qrIndex ||
              strncmp(d.lastQrPayload, url, sizeof(d.lastQrPayload) - 1) != 0) {
            lv_qrcode_update(d.qr, url, strlen(url));
            strncpy(d.lastQrPayload, url, sizeof(d.lastQrPayload) - 1);
            d.lastQrPayload[sizeof(d.lastQrPayload) - 1] = '\0';
            d.lastItemShown = qrIndex;
            if (qrIndex >= 0) Serial.printf("[%s] qr %u -> %s\n", contentTag, (unsigned)(qrIndex + 1), url);
            else               Serial.printf("[%s] qr fallback -> %s\n", contentTag, url);
          }
          lv_obj_clear_flag(d.qr, LV_OBJ_FLAG_HIDDEN);
          if (d.qrHint) {
            lv_label_set_text(d.qrHint, "Tap anywhere\nto close");
            lv_obj_clear_flag(d.qrHint, LV_OBJ_FLAG_HIDDEN);
          }
          if (d.status) lv_label_set_text(d.status, status);
        } else {
          lv_obj_add_flag(d.qr, LV_OBJ_FLAG_HIDDEN);
          if (d.qrHint) {
            lv_label_set_text(d.qrHint, "Generating QR...");
            lv_obj_clear_flag(d.qrHint, LV_OBJ_FLAG_HIDDEN);
          }
          if (d.status) lv_label_set_text(d.status, "QR...");
        }
      }
    } else {
      lv_obj_add_flag(d.qrOverlay, LV_OBJ_FLAG_HIDDEN);
      if (d.qrHint) lv_obj_add_flag(d.qrHint, LV_OBJ_FLAG_HIDDEN);
    }
  }
#else
  if (d.qrOverlay) lv_obj_add_flag(d.qrOverlay, LV_OBJ_FLAG_HIDDEN);
  if (d.qrHint)    lv_obj_add_flag(d.qrHint, LV_OBJ_FLAG_HIDDEN);
#endif
#endif
}

static void lvglSetObjXAnim(void *obj, int32_t x) {
  lv_obj_set_x((lv_obj_t *)obj, (lv_coord_t)x);
}

// Helper: compute dynamic X offset for a page in the carousel.
// Returns the screen-widths offset relative to the current page, or hides
// the root object and returns a sentinel when the page is disabled.
static int32_t lvglCarouselPageX(UiPageMode mode, int8_t curOrd, int16_t w, lv_obj_t *root) {
  const int8_t ord = uiPageOrdinal(mode);
  if (ord < 0) {
    // Disabled page — park far off-screen and hide.
    if (root) lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
    return (int32_t)w * 10;  // sentinel, never rendered
  }
  return (int32_t)(ord - curOrd) * w;
}

// ── Transit LVGL init / update ──────────────────────────────────────────────

static void lvglInitTransitUi() {
  if (!g_lvglTransitRoot) return;
  lv_obj_t *root = g_lvglTransitRoot;
  const lv_coord_t cW = canvasWidth();
  const lv_coord_t cH = canvasHeight();
  const UiThemeLvglTokens &t = activeUiTheme().lvgl;
  const uint32_t panelBg   = lvglResolvedPanelBg(t);
  const uint32_t headerBg  = lvglResolvedHeaderBg(t);
  const uint32_t headerTxt = lvglResolvedHeaderText(t);
  const lv_coord_t hdrH = 30;

  lvglSetBgFlat(root, panelBg);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);

  // Header bar
  g_transitUi.header = lv_obj_create(root);
  lv_obj_set_size(g_transitUi.header, cW, hdrH);
  lv_obj_set_pos(g_transitUi.header, 0, 0);
  lvglSetBgFlat(g_transitUi.header, headerBg);
  lv_obj_set_style_bg_opa(g_transitUi.header, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(g_transitUi.header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(g_transitUi.header, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_transitUi.header, 0, LV_PART_MAIN);

  g_transitUi.headerFill = lv_obj_create(g_transitUi.header);
  lv_obj_set_size(g_transitUi.headerFill, cW, hdrH);
  lv_obj_set_pos(g_transitUi.headerFill, 0, 0);
  lvglSetBgFlat(g_transitUi.headerFill, headerBg);
  lv_obj_set_style_bg_opa(g_transitUi.headerFill, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(g_transitUi.headerFill, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(g_transitUi.headerFill, 0, LV_PART_MAIN);

  g_transitUi.title = lv_label_create(g_transitUi.header);
  lv_obj_set_style_text_color(g_transitUi.title, lv_color_hex(headerTxt), LV_PART_MAIN);
  lv_obj_set_style_text_font(g_transitUi.title, lvglFontSmall(), 0);
  lv_label_set_text(g_transitUi.title, "DEPARTURES");
  lv_obj_align(g_transitUi.title, LV_ALIGN_LEFT_MID, 8, 2);

  g_transitUi.station = lv_label_create(g_transitUi.header);
  lv_obj_set_style_text_color(g_transitUi.station, lv_color_hex(headerTxt), LV_PART_MAIN);
  lv_obj_set_style_text_font(g_transitUi.station, lvglFontSmall(), 0);
  lv_label_set_text(g_transitUi.station, g_transitConfig.station[0] ? g_transitConfig.station : "--");
  lv_obj_align(g_transitUi.station, LV_ALIGN_CENTER, 0, 2);

  g_transitUi.status = lv_label_create(g_transitUi.header);
  lv_obj_set_style_text_color(g_transitUi.status, lv_color_hex(headerTxt), LV_PART_MAIN);
  lv_obj_set_style_text_font(g_transitUi.status, lvglFontTiny(), 0);
  lv_label_set_text(g_transitUi.status, "--:--");
  lv_obj_align(g_transitUi.status, LV_ALIGN_RIGHT_MID, -8, 2);

  // Departure rows — r247 layout (640px wide, 35px per row):
  //  [ 4.. 85]  badge "S30" / "Elizabeth" (82×30) — pill BUS/TRAM/COACH, rect rail
  //             label scrolls (LV_LABEL_LONG_SCROLL) when name > badge width
  //  [90..337]  destination   (248px, 22px Funnel Display, LV_LABEL_LONG_DOT)
  //  [344..413] dep time      (70px, 20px, right-aligned "HH:MM")
  //  [418..499] arr time      (82px, 18px, ">HH:MM" or ">---")
  //  [504..553] delay         (50px, 14px, semaphore color)
  //  [558..633] platform/LIVE (76px, 14px, right-aligned "LIVE" or "Bin.X")
  const lv_coord_t rowH = 35;
  const lv_coord_t badgeW = 82, badgeH = 30;
  // Alternate row tint: white on dark themes, black on light themes
  const bool darkPanel = (lvglColorLuma(panelBg) < 128u);
  const uint32_t altTintColor = darkPanel ? 0xFFFFFF : 0x000000;
  for (uint8_t i = 0; i < TRANSIT_MAX_DEPARTURES; ++i) {
    const lv_coord_t ry = hdrH + (lv_coord_t)i * rowH;

    // Alternate row background (odd rows get a subtle tint)
    g_transitUi.rowBg[i] = lv_obj_create(root);
    lv_obj_set_size(g_transitUi.rowBg[i], cW, rowH);
    lv_obj_set_pos(g_transitUi.rowBg[i], 0, ry);
    lv_obj_set_style_border_width(g_transitUi.rowBg[i], 0, LV_PART_MAIN);
    lv_obj_set_style_radius(g_transitUi.rowBg[i], 0, LV_PART_MAIN);
    lv_obj_clear_flag(g_transitUi.rowBg[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(g_transitUi.rowBg[i], 0, LV_PART_MAIN);
    if (i & 1) {
      lv_obj_set_style_bg_color(g_transitUi.rowBg[i], lv_color_hex(altTintColor), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(g_transitUi.rowBg[i], 22, LV_PART_MAIN);  // ~9% tint
    } else {
      lv_obj_set_style_bg_opa(g_transitUi.rowBg[i], LV_OPA_TRANSP, LV_PART_MAIN);
    }

    // Separator line (below each row except last)
    if (i < TRANSIT_MAX_DEPARTURES - 1) {
      g_transitUi.rowSep[i] = lv_obj_create(root);
      lv_obj_set_size(g_transitUi.rowSep[i], cW - 16, 1);
      lv_obj_set_pos(g_transitUi.rowSep[i], 8, ry + rowH - 1);
      lv_obj_set_style_bg_color(g_transitUi.rowSep[i], lv_color_hex(t.divider), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(g_transitUi.rowSep[i], LV_OPA_30, LV_PART_MAIN);
      lv_obj_set_style_border_width(g_transitUi.rowSep[i], 0, LV_PART_MAIN);
      lv_obj_set_style_radius(g_transitUi.rowSep[i], 0, LV_PART_MAIN);
    }

    // Colored line badge (category + number)
    const lv_coord_t badgeX = 4, badgeY = ry + (rowH - badgeH) / 2;
    g_transitUi.lineBg[i] = lv_obj_create(root);
    lv_obj_set_size(g_transitUi.lineBg[i], badgeW, badgeH);
    lv_obj_set_pos(g_transitUi.lineBg[i], badgeX, badgeY);
    lv_obj_set_style_bg_color(g_transitUi.lineBg[i], lv_color_hex(0x555577), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_transitUi.lineBg[i], LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_transitUi.lineBg[i], 0, LV_PART_MAIN);
    lv_obj_set_style_radius(g_transitUi.lineBg[i], 6, LV_PART_MAIN);
    lv_obj_clear_flag(g_transitUi.lineBg[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(g_transitUi.lineBg[i], 0, LV_PART_MAIN);

    // Badge label: fixed size so LV_LABEL_LONG_SCROLL clips & scrolls long names
    g_transitUi.line_[i] = lv_label_create(g_transitUi.lineBg[i]);
    lv_obj_set_size(g_transitUi.line_[i], badgeW - 6, badgeH - 4);   // 76×26 clip area
    lv_obj_align(g_transitUi.line_[i], LV_ALIGN_CENTER, 0, 3);  // +3 Funnel Display ascender
    lv_obj_set_style_text_color(g_transitUi.line_[i], lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_transitUi.line_[i], lvglFontMeta(), 0);  // 20px default
    lv_obj_set_style_text_align(g_transitUi.line_[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(g_transitUi.line_[i], LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_anim_speed(g_transitUi.line_[i], 15, LV_PART_MAIN);  // ~5-8 s cycle
    lv_label_set_text(g_transitUi.line_[i], "--");

    // Destination (big, truncates with "..." on overflow)
    g_transitUi.dest[i] = lv_label_create(root);
    lv_obj_set_pos(g_transitUi.dest[i], 90, ry);
    lv_obj_set_size(g_transitUi.dest[i], 248, rowH);
    lv_obj_set_style_text_color(g_transitUi.dest[i], lv_color_hex(t.infoText), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_transitUi.dest[i], lvglFontMeta(), 0);  // 20px
    lv_obj_set_style_text_align(g_transitUi.dest[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_long_mode(g_transitUi.dest[i], LV_LABEL_LONG_DOT);
    lv_obj_set_style_pad_top(g_transitUi.dest[i], (rowH - 20) / 2 + 2, LV_PART_MAIN);  // +2 ascender
    lv_label_set_text(g_transitUi.dest[i], "--");

    // Departure time ("HH:MM", right-aligned)
    g_transitUi.time_[i] = lv_label_create(root);
    lv_obj_set_pos(g_transitUi.time_[i], 344, ry);
    lv_obj_set_size(g_transitUi.time_[i], 70, rowH);
    lv_obj_set_style_text_color(g_transitUi.time_[i], lv_color_hex(t.infoText), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_transitUi.time_[i], lvglFontMeta(), 0);  // 20px
    lv_obj_set_style_text_align(g_transitUi.time_[i], LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_pad_top(g_transitUi.time_[i], (rowH - 20) / 2 + 2, LV_PART_MAIN);  // +2 ascender
    lv_label_set_text(g_transitUi.time_[i], "--:--");

    // Arrival time at destination (">HH:MM" or ">---")
    g_transitUi.arr[i] = lv_label_create(root);
    lv_obj_set_pos(g_transitUi.arr[i], 418, ry);
    lv_obj_set_size(g_transitUi.arr[i], 82, rowH);
    lv_obj_set_style_text_color(g_transitUi.arr[i], lv_color_hex(t.auxMeta), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_transitUi.arr[i], lvglFontMeta(), 0);  // 20px
    lv_obj_set_style_text_align(g_transitUi.arr[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_style_pad_top(g_transitUi.arr[i], (rowH - 20) / 2 + 2, LV_PART_MAIN);  // +2 ascender
    lv_label_set_text(g_transitUi.arr[i], ">---");

    // Delay (+Xm / -Xm)
    g_transitUi.delay[i] = lv_label_create(root);
    lv_obj_set_pos(g_transitUi.delay[i], 504, ry + (rowH - 16) / 2 + 2);  // +2 ascender
    lv_obj_set_size(g_transitUi.delay[i], 50, 16);
    lv_obj_set_style_text_color(g_transitUi.delay[i], lv_color_hex(0x22AA33), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_transitUi.delay[i], lvglFontMini(), 0);  // 16px
    lv_obj_set_style_text_align(g_transitUi.delay[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(g_transitUi.delay[i], "");

    // Platform / LIVE indicator (right edge)
    g_transitUi.platform[i] = lv_label_create(root);
    lv_obj_set_pos(g_transitUi.platform[i], 558, ry);
    lv_obj_set_size(g_transitUi.platform[i], 76, rowH);
    lv_obj_set_style_text_color(g_transitUi.platform[i], lv_color_hex(t.auxMeta), LV_PART_MAIN);
    lv_obj_set_style_text_font(g_transitUi.platform[i], lvglFontMeta(), 0);  // 20px
    lv_obj_set_style_text_align(g_transitUi.platform[i], LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_pad_top(g_transitUi.platform[i], (rowH - 20) / 2 + 2, LV_PART_MAIN);  // +2 ascender
    lv_label_set_text(g_transitUi.platform[i], "");
  }

  // No-data label (shown when no departures)
  g_transitUi.noData = lv_label_create(root);
  lv_obj_set_style_text_color(g_transitUi.noData, lv_color_hex(t.auxMeta), LV_PART_MAIN);
  lv_obj_set_style_text_font(g_transitUi.noData, lvglFontSmall(), 0);
  lv_label_set_text(g_transitUi.noData,
      g_transitConfig.configured ? "Loading..." : "Set station in web UI \xF0\x9F\x9A\x89");
  lv_obj_align(g_transitUi.noData, LV_ALIGN_CENTER, 0, 10);
}

// ── Launch Page LVGL ────────────────────────────────────────────────────────

static uint32_t launchProviderColor(const char *slug) {
  // Bright, saturated colors visible on dark backgrounds (like Transit badges)
  if (strstr(slug, "spacex"))       return 0x1E88E5;  // bright blue
  if (strstr(slug, "rocket-lab"))   return 0x5C6BC0;  // indigo
  if (strstr(slug, "ula"))          return 0x2979FF;  // vivid blue
  if (strstr(slug, "isro"))         return 0xF57C00;  // bright orange
  if (strstr(slug, "arianespace"))  return 0x1976D2;  // blue
  if (strstr(slug, "casc"))         return 0xE53935;  // bright red
  if (strstr(slug, "roscosmos"))    return 0x42A5F5;  // sky blue
  return 0xAB47BC;  // purple fallback (visible on any bg)
}

static void lvglInitLaunchUi() {
  if (!g_lvglLaunchRoot) return;
  const int16_t cW = canvasWidth();
  const int16_t cH = canvasHeight();
  const UiThemeLvglTokens &t = activeUiTheme().lvgl;

  // ---- Header (30px, full width) ----
  const uint32_t headerBg  = lvglResolvedHeaderBg(t);
  const uint32_t headerTxt = lvglResolvedHeaderText(t);

  g_launchUi.header = lv_obj_create(g_lvglLaunchRoot);
  lv_obj_set_size(g_launchUi.header, cW, 30);
  lv_obj_set_pos(g_launchUi.header, 0, 0);
  lvglSetBgFlat(g_launchUi.header, headerBg);
  lv_obj_set_style_bg_opa(g_launchUi.header, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(g_launchUi.header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(g_launchUi.header, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_launchUi.header, 0, LV_PART_MAIN);

  g_launchUi.title = lv_label_create(g_launchUi.header);
  lv_label_set_text(g_launchUi.title, "LAUNCHES");
  lv_obj_set_style_text_font(g_launchUi.title, lvglFontSmall(), 0);
  lvglSetTextHex(g_launchUi.title, headerTxt);
  lv_obj_align(g_launchUi.title, LV_ALIGN_LEFT_MID, 8, 2);

  g_launchUi.headerCenter = lv_label_create(g_launchUi.header);
  lv_label_set_text(g_launchUi.headerCenter, "");
  lv_obj_set_style_text_font(g_launchUi.headerCenter, lvglFontSmall(), 0);
  lvglSetTextHex(g_launchUi.headerCenter, headerTxt);
  lv_obj_align(g_launchUi.headerCenter, LV_ALIGN_CENTER, 0, 2);

  g_launchUi.fetchTime = lv_label_create(g_launchUi.header);
  lv_label_set_text(g_launchUi.fetchTime, "--:--");
  lv_obj_set_style_text_font(g_launchUi.fetchTime, lvglFontSmall(), 0);
  lvglSetTextHex(g_launchUi.fetchTime, headerTxt);
  lv_obj_align(g_launchUi.fetchTime, LV_ALIGN_RIGHT_MID, -8, 2);

  const int16_t bodyY = 30;
  const int16_t bodyH = cH - bodyY;  // 139px
  const bool darkPanel = (lvglColorLuma(t.panelBg) < 128u);
  const uint32_t altTint = darkPanel ? 0xFFFFFF : 0x000000;

  // ==== VIEW 0: Hero — "Mission Control" layout ====
  // Top: badge + mission name + weather/vehicle info
  // Center: BIG countdown (dominant element)
  // Bottom: location + window details
  g_launchUi.heroBg = lv_obj_create(g_lvglLaunchRoot);
  lv_obj_set_size(g_launchUi.heroBg, cW, bodyH);
  lv_obj_set_pos(g_launchUi.heroBg, 0, bodyY);
  lv_obj_set_style_border_width(g_launchUi.heroBg, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_launchUi.heroBg, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_launchUi.heroBg, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_launchUi.heroBg, lv_color_hex(altTint), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_launchUi.heroBg, 12, LV_PART_MAIN);
  lv_obj_clear_flag(g_launchUi.heroBg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(g_launchUi.heroBg, LV_OBJ_FLAG_CLICKABLE);

  // ── Top zone (y=0..50): badge + mission name + right-side info ──
  const int16_t heroBadgeW = 110, heroBadgeH = 28;
  g_launchUi.heroBadge = lv_obj_create(g_launchUi.heroBg);
  lv_obj_set_size(g_launchUi.heroBadge, heroBadgeW, heroBadgeH);
  lv_obj_set_pos(g_launchUi.heroBadge, 6, 4);
  lv_obj_set_style_radius(g_launchUi.heroBadge, 6, 0);
  lv_obj_set_style_border_width(g_launchUi.heroBadge, 0, LV_PART_MAIN);
  lvglSetBgFlat(g_launchUi.heroBadge, t.headerBg);
  lv_obj_set_style_bg_opa(g_launchUi.heroBadge, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(g_launchUi.heroBadge, LV_OBJ_FLAG_SCROLLABLE);

  g_launchUi.heroBadgeLabel = lv_label_create(g_launchUi.heroBadge);
  lv_label_set_text(g_launchUi.heroBadgeLabel, "");
  lv_obj_set_style_text_font(g_launchUi.heroBadgeLabel, lvglFontMeta(), 0);
  lvglSetTextHex(g_launchUi.heroBadgeLabel, 0xFFFFFF);
  lv_obj_set_size(g_launchUi.heroBadgeLabel, heroBadgeW - 6, heroBadgeH - 4);
  lv_obj_align(g_launchUi.heroBadgeLabel, LV_ALIGN_CENTER, 0, 3);
  lv_label_set_long_mode(g_launchUi.heroBadgeLabel, LV_LABEL_LONG_SCROLL);
  lv_obj_set_style_text_align(g_launchUi.heroBadgeLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_anim_speed(g_launchUi.heroBadgeLabel, 15, 0);

  // Mission name — 22px, right of badge
  const int16_t nameX = heroBadgeW + 14;
  g_launchUi.heroName = lv_label_create(g_launchUi.heroBg);
  lv_label_set_text(g_launchUi.heroName, "");
  lv_obj_set_style_text_font(g_launchUi.heroName, lvglFontRssNews(), 0);  // 22px
  lvglSetTextHex(g_launchUi.heroName, t.infoText);
  lv_obj_set_pos(g_launchUi.heroName, nameX, 3);
  lv_obj_set_width(g_launchUi.heroName, cW - nameX - 8);
  lv_label_set_long_mode(g_launchUi.heroName, LV_LABEL_LONG_SCROLL_CIRCULAR);

  // Vehicle | Pad — 16px, below mission name
  g_launchUi.heroVehiclePad = lv_label_create(g_launchUi.heroBg);
  lv_label_set_text(g_launchUi.heroVehiclePad, "");
  lv_obj_set_style_text_font(g_launchUi.heroVehiclePad, lvglFontMini(), 0);
  lvglSetTextHex(g_launchUi.heroVehiclePad, t.auxMeta);
  lv_obj_set_pos(g_launchUi.heroVehiclePad, nameX, 28);
  lv_obj_set_width(g_launchUi.heroVehiclePad, cW - nameX - 180);
  lv_label_set_long_mode(g_launchUi.heroVehiclePad, LV_LABEL_LONG_DOT);

  // Weather — 18px, top-right
  g_launchUi.heroWeather = lv_label_create(g_launchUi.heroBg);
  lv_label_set_text(g_launchUi.heroWeather, "");
  lv_obj_set_style_text_font(g_launchUi.heroWeather, lvglFontSmall(), 0);
  lvglSetTextHex(g_launchUi.heroWeather, t.infoText);
  lv_obj_set_style_text_align(g_launchUi.heroWeather, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(g_launchUi.heroWeather, cW - 190, 28);
  lv_obj_set_width(g_launchUi.heroWeather, 182);

  // ── Separator (1px at y=48) ──
  lv_obj_t *heroSep = lv_obj_create(g_launchUi.heroBg);
  lv_obj_set_size(heroSep, cW - 16, 1);
  lv_obj_set_pos(heroSep, 8, 48);
  lv_obj_set_style_bg_color(heroSep, lv_color_hex(t.divider), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(heroSep, LV_OPA_30, LV_PART_MAIN);
  lv_obj_set_style_border_width(heroSep, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(heroSep, 0, LV_PART_MAIN);

  // ── Center zone: BIG COUNTDOWN (dominant) ──
  g_launchUi.heroCountdown = lv_label_create(g_launchUi.heroBg);
  lv_label_set_text(g_launchUi.heroCountdown, "T-00:00:00");
  lv_obj_set_style_text_font(g_launchUi.heroCountdown, lvglFontTitle(), 0);  // 30px
  lvglSetTextHex(g_launchUi.heroCountdown, t.infoText);
  lv_obj_set_style_text_align(g_launchUi.heroCountdown, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(g_launchUi.heroCountdown, 0, 58);
  lv_obj_set_width(g_launchUi.heroCountdown, cW);

  // ── Bottom zone (y=100..142): location + window ──
  // Location + Country — left
  g_launchUi.heroLocation = lv_label_create(g_launchUi.heroBg);
  lv_label_set_text(g_launchUi.heroLocation, "");
  lv_obj_set_style_text_font(g_launchUi.heroLocation, lvglFontMini(), 0);  // 16px
  lvglSetTextHex(g_launchUi.heroLocation, t.auxMeta);
  lv_obj_set_pos(g_launchUi.heroLocation, 8, bodyH - 40);
  lv_obj_set_width(g_launchUi.heroLocation, cW / 2 - 12);
  lv_label_set_long_mode(g_launchUi.heroLocation, LV_LABEL_LONG_DOT);

  g_launchUi.heroCountry = lv_label_create(g_launchUi.heroBg);
  lv_label_set_text(g_launchUi.heroCountry, "");
  lv_obj_set_style_text_font(g_launchUi.heroCountry, lvglFontMini(), 0);
  lvglSetTextHex(g_launchUi.heroCountry, t.auxMeta);
  lv_obj_set_pos(g_launchUi.heroCountry, 8, bodyH - 22);
  lv_obj_set_width(g_launchUi.heroCountry, cW / 2 - 12);

  // Window — right-aligned, bottom
  g_launchUi.heroWindow = lv_label_create(g_launchUi.heroBg);
  lv_label_set_text(g_launchUi.heroWindow, "");
  lv_obj_set_style_text_font(g_launchUi.heroWindow, lvglFontMini(), 0);
  lvglSetTextHex(g_launchUi.heroWindow, t.auxMeta);
  lv_obj_set_style_text_align(g_launchUi.heroWindow, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(g_launchUi.heroWindow, cW / 2, bodyH - 40);
  lv_obj_set_width(g_launchUi.heroWindow, cW / 2 - 8);

  // ==== VIEW 1: Compact (2 rows, missions 2-3) ====
  const int16_t compactRowH = bodyH / 2;  // ~69px each
  const int16_t badgeW = 100, badgeH = 28;
  for (int i = 0; i < 2; i++) {
    const int16_t ry = bodyY + i * compactRowH;

    g_launchUi.compactBg[i] = lv_obj_create(g_lvglLaunchRoot);
    lv_obj_set_size(g_launchUi.compactBg[i], cW, compactRowH);
    lv_obj_set_pos(g_launchUi.compactBg[i], 0, ry);
    lv_obj_set_style_border_width(g_launchUi.compactBg[i], 0, LV_PART_MAIN);
    lv_obj_set_style_radius(g_launchUi.compactBg[i], 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_launchUi.compactBg[i], 0, LV_PART_MAIN);
    lv_obj_clear_flag(g_launchUi.compactBg[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_launchUi.compactBg[i], LV_OBJ_FLAG_CLICKABLE);
    if (i & 1) {
      lv_obj_set_style_bg_color(g_launchUi.compactBg[i], lv_color_hex(altTint), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(g_launchUi.compactBg[i], 22, LV_PART_MAIN);
    } else {
      lv_obj_set_style_bg_opa(g_launchUi.compactBg[i], LV_OPA_TRANSP, LV_PART_MAIN);
    }
    // Start hidden (View 0 is default)
    lv_obj_add_flag(g_launchUi.compactBg[i], LV_OBJ_FLAG_HIDDEN);

    // Badge
    g_launchUi.compactBadge[i] = lv_obj_create(g_launchUi.compactBg[i]);
    lv_obj_set_size(g_launchUi.compactBadge[i], badgeW, badgeH);
    lv_obj_set_pos(g_launchUi.compactBadge[i], 4, 4);
    lv_obj_set_style_radius(g_launchUi.compactBadge[i], 6, 0);
    lv_obj_set_style_border_width(g_launchUi.compactBadge[i], 0, LV_PART_MAIN);
    lvglSetBgFlat(g_launchUi.compactBadge[i], t.headerBg);
    lv_obj_set_style_bg_opa(g_launchUi.compactBadge[i], LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(g_launchUi.compactBadge[i], LV_OBJ_FLAG_SCROLLABLE);

    g_launchUi.compactBadgeLabel[i] = lv_label_create(g_launchUi.compactBadge[i]);
    lv_label_set_text(g_launchUi.compactBadgeLabel[i], "");
    lv_obj_set_style_text_font(g_launchUi.compactBadgeLabel[i], lvglFontMeta(), 0);
    lvglSetTextHex(g_launchUi.compactBadgeLabel[i], 0xFFFFFF);
    lv_obj_set_size(g_launchUi.compactBadgeLabel[i], badgeW - 6, badgeH - 4);
    lv_obj_align(g_launchUi.compactBadgeLabel[i], LV_ALIGN_CENTER, 0, 3);
    lv_label_set_long_mode(g_launchUi.compactBadgeLabel[i], LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_text_align(g_launchUi.compactBadgeLabel[i], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_anim_speed(g_launchUi.compactBadgeLabel[i], 15, 0);

    // Mission name — 20px, line 1
    const int16_t cTextX = badgeW + 10;
    g_launchUi.compactName[i] = lv_label_create(g_launchUi.compactBg[i]);
    lv_label_set_text(g_launchUi.compactName[i], "");
    lv_obj_set_style_text_font(g_launchUi.compactName[i], lvglFontMeta(), 0);
    lvglSetTextHex(g_launchUi.compactName[i], t.infoText);
    lv_obj_set_pos(g_launchUi.compactName[i], cTextX, 4);
    lv_obj_set_width(g_launchUi.compactName[i], cW - cTextX - 140);
    lv_label_set_long_mode(g_launchUi.compactName[i], LV_LABEL_LONG_DOT);

    // Vehicle | Pad — 16px, line 2
    g_launchUi.compactVehicle[i] = lv_label_create(g_launchUi.compactBg[i]);
    lv_label_set_text(g_launchUi.compactVehicle[i], "");
    lv_obj_set_style_text_font(g_launchUi.compactVehicle[i], lvglFontMini(), 0);
    lvglSetTextHex(g_launchUi.compactVehicle[i], t.auxMeta);
    lv_obj_set_pos(g_launchUi.compactVehicle[i], cTextX, 26);
    lv_obj_set_width(g_launchUi.compactVehicle[i], cW - cTextX - 140);
    lv_label_set_long_mode(g_launchUi.compactVehicle[i], LV_LABEL_LONG_DOT);

    // Location — 16px, line 3
    g_launchUi.compactLocation[i] = lv_label_create(g_launchUi.compactBg[i]);
    lv_label_set_text(g_launchUi.compactLocation[i], "");
    lv_obj_set_style_text_font(g_launchUi.compactLocation[i], lvglFontMini(), 0);
    lvglSetTextHex(g_launchUi.compactLocation[i], t.auxMeta);
    lv_obj_set_pos(g_launchUi.compactLocation[i], cTextX, 46);
    lv_obj_set_width(g_launchUi.compactLocation[i], cW - cTextX - 140);
    lv_label_set_long_mode(g_launchUi.compactLocation[i], LV_LABEL_LONG_DOT);

    // Date/time — 20px, right-aligned
    g_launchUi.compactDate[i] = lv_label_create(g_launchUi.compactBg[i]);
    lv_label_set_text(g_launchUi.compactDate[i], "");
    lv_obj_set_style_text_font(g_launchUi.compactDate[i], lvglFontMeta(), 0);
    lvglSetTextHex(g_launchUi.compactDate[i], t.infoText);
    lv_obj_set_style_text_align(g_launchUi.compactDate[i], LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(g_launchUi.compactDate[i], cW - 134, 4);
    lv_obj_set_width(g_launchUi.compactDate[i], 128);
  }

  // Compact separator (between the two rows)
  g_launchUi.compactSep = lv_obj_create(g_lvglLaunchRoot);
  lv_obj_set_size(g_launchUi.compactSep, cW - 16, 1);
  lv_obj_set_pos(g_launchUi.compactSep, 8, bodyY + compactRowH - 1);
  lv_obj_set_style_bg_color(g_launchUi.compactSep, lv_color_hex(t.divider), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_launchUi.compactSep, LV_OPA_30, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_launchUi.compactSep, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_launchUi.compactSep, 0, LV_PART_MAIN);
  lv_obj_add_flag(g_launchUi.compactSep, LV_OBJ_FLAG_HIDDEN);

  // ---- No-data label ----
  g_launchUi.noData = lv_label_create(g_lvglLaunchRoot);
  lv_label_set_text(g_launchUi.noData, "No upcoming launches");
  lv_obj_set_style_text_font(g_launchUi.noData, lvglFontTiny(), 0);
  lvglSetTextHex(g_launchUi.noData, t.auxMeta);
  lv_obj_set_pos(g_launchUi.noData, 0, bodyY);
  lv_obj_set_size(g_launchUi.noData, cW, bodyH);
  lv_obj_set_style_text_align(g_launchUi.noData, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_pad_top(g_launchUi.noData, 50, 0);
  lv_obj_add_flag(g_launchUi.noData, LV_OBJ_FLAG_HIDDEN);

  // ==== QR overlay (RSS-style: full-screen, big QR left, hint right) ====
#if defined(LV_USE_QRCODE) && LV_USE_QRCODE
  const UiThemeLvglTokens &theme = t;
  g_launchUi.qrOverlay = lvglCreatePanel(g_lvglLaunchRoot, cW, cH, 0, 0, lv_color_hex(theme.screenBg), 0);
  lv_obj_set_style_layout(g_launchUi.qrOverlay, 0, 0);
  lv_obj_add_flag(g_launchUi.qrOverlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(g_launchUi.qrOverlay, LV_OBJ_FLAG_CLICKABLE);

  int16_t qrSize = cH;
  if (qrSize > cW) qrSize = cW;
  if (qrSize < 90) qrSize = 90;
  const lv_color_t qrDark  = lv_color_hex(theme.auxQrDark);
  const lv_color_t qrLight = lv_color_hex(theme.auxQrLight);
  {
    uint32_t bufSz = LV_CANVAS_BUF_SIZE_INDEXED_1BIT(qrSize, qrSize);
    uint8_t *psBuf = (uint8_t *)ps_calloc(1, bufSz);
    if (!psBuf) psBuf = (uint8_t *)calloc(1, bufSz);
    g_launchUi.qr = lv_canvas_create(g_launchUi.qrOverlay);
    if (psBuf) {
      lv_canvas_set_buffer(g_launchUi.qr, psBuf, qrSize, qrSize, LV_IMG_CF_INDEXED_1BIT);
      lv_canvas_set_palette(g_launchUi.qr, 0, qrDark);
      lv_canvas_set_palette(g_launchUi.qr, 1, qrLight);
    }
    if (!psBuf) Serial.println("[LAUNCH][QR][ERR] canvas buffer alloc failed");
  }
  lv_obj_add_flag(g_launchUi.qr, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_pos(g_launchUi.qr, 0, 0);
  lv_obj_set_style_border_width(g_launchUi.qr, 0, LV_PART_MAIN);
  const char *qrFallback = "https://rocketlaunch.live";
  lv_qrcode_update(g_launchUi.qr, qrFallback, strlen(qrFallback));

  const int16_t hintX = qrSize + 16;
  const int16_t hintW = cW - hintX - 12;
  g_launchUi.qrHint = lv_label_create(g_launchUi.qrOverlay);
  lv_obj_set_style_text_font(g_launchUi.qrHint, lvglNowPlayingArtistFont(), 0);
  lv_obj_set_style_text_color(g_launchUi.qrHint, lv_color_hex(theme.auxQrHint), 0);
  lv_obj_set_size(g_launchUi.qrHint, hintW, LV_SIZE_CONTENT);
  lv_label_set_long_mode(g_launchUi.qrHint, LV_LABEL_LONG_WRAP);
  lv_label_set_text(g_launchUi.qrHint, "Tap anywhere\nto close");
  lv_obj_add_flag(g_launchUi.qrHint, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_pos(g_launchUi.qrHint, hintX, (cH / 2) - 28);
  lv_obj_add_flag(g_launchUi.qrHint, LV_OBJ_FLAG_HIDDEN);
#endif

  g_launchUi.viewIndex = 0;
  g_launchUi.lastViewRotateMs = millis();
}

static void lvglOpenLaunchQr(int8_t index) {
#if defined(LV_USE_QRCODE) && LV_USE_QRCODE
  if (index < 0 || index >= g_launchState.count) return;
  if (g_launchUi.qrModalOpen) return;
  g_launchUi.qrModalOpen = true;
  g_launchUi.qrItemIndex = index;
  const LaunchItem &item = g_launchState.items[index];

  char qrUrl[128];
  snprintf(qrUrl, sizeof(qrUrl), "https://rocketlaunch.live/launch/%s",
           item.providerSlug[0] ? item.providerSlug : "upcoming");
  if (strncmp(g_launchUi.lastQrPayload, qrUrl, sizeof(g_launchUi.lastQrPayload) - 1) != 0) {
    lv_qrcode_update(g_launchUi.qr, qrUrl, strlen(qrUrl));
    strncpy(g_launchUi.lastQrPayload, qrUrl, sizeof(g_launchUi.lastQrPayload) - 1);
    g_launchUi.lastQrPayload[sizeof(g_launchUi.lastQrPayload) - 1] = '\0';
    Serial.printf("[LAUNCH] qr %d -> %s\n", (int)index, qrUrl);
  }

  if (g_launchUi.qrOverlay) {
    lv_obj_clear_flag(g_launchUi.qrOverlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_launchUi.qrOverlay);
  }
  if (g_launchUi.qr) lv_obj_clear_flag(g_launchUi.qr, LV_OBJ_FLAG_HIDDEN);
  if (g_launchUi.qrHint) lv_obj_clear_flag(g_launchUi.qrHint, LV_OBJ_FLAG_HIDDEN);
  markUserInteraction(millis());
#endif
}

static void lvglCloseLaunchQr() {
  if (!g_launchUi.qrModalOpen) return;
  g_launchUi.qrModalOpen = false;
  g_launchUi.qrItemIndex = -1;
  if (g_launchUi.qrOverlay) lv_obj_add_flag(g_launchUi.qrOverlay, LV_OBJ_FLAG_HIDDEN);
  if (g_launchUi.qrHint) lv_obj_add_flag(g_launchUi.qrHint, LV_OBJ_FLAG_HIDDEN);
}

static void lvglSetLaunchView(uint8_t view) {
  g_launchUi.viewIndex = view;
  if (view == 0) {
    // Show hero, hide compact
    lv_obj_clear_flag(g_launchUi.heroBg, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 2; i++) lv_obj_add_flag(g_launchUi.compactBg[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_launchUi.compactSep, LV_OBJ_FLAG_HIDDEN);
  } else {
    // Show compact, hide hero
    lv_obj_add_flag(g_launchUi.heroBg, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 2; i++) {
      if (i + 1 < g_launchState.count)
        lv_obj_clear_flag(g_launchUi.compactBg[i], LV_OBJ_FLAG_HIDDEN);
      else
        lv_obj_add_flag(g_launchUi.compactBg[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (g_launchState.count > 2)
      lv_obj_clear_flag(g_launchUi.compactSep, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_add_flag(g_launchUi.compactSep, LV_OBJ_FLAG_HIDDEN);
  }
}

static void lvglUpdateLaunchUi(bool force) {
  if (!g_lvglLaunchRoot) return;
  const UiThemeLvglTokens &t = activeUiTheme().lvgl;

  lv_label_set_text(g_launchUi.fetchTime, g_launchState.fetchedAt);

  if (!g_launchState.valid || g_launchState.count == 0) {
    lv_obj_clear_flag(g_launchUi.noData, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_launchUi.heroBg, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 2; i++) lv_obj_add_flag(g_launchUi.compactBg[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_launchUi.compactSep, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  lv_obj_add_flag(g_launchUi.noData, LV_OBJ_FLAG_HIDDEN);

  // ---- View 0: Hero (mission 0, all details) ----
  const LaunchItem &hero = g_launchState.items[0];
  lv_label_set_text(g_launchUi.headerCenter, hero.name);
  lv_label_set_text(g_launchUi.heroBadgeLabel, hero.provider);
  lv_label_set_text(g_launchUi.heroName, hero.name);
  lvglSetBgFlat(g_launchUi.heroBadge, launchProviderColor(hero.providerSlug));

  char vpBuf[96];
  snprintf(vpBuf, sizeof(vpBuf), "%s | %s", hero.vehicle, hero.pad);
  lv_label_set_text(g_launchUi.heroVehiclePad, vpBuf);

  // Bottom zone: location + country, window
  lv_label_set_text(g_launchUi.heroLocation, hero.location[0] ? hero.location : "");
  lv_label_set_text(g_launchUi.heroCountry, hero.country[0] ? hero.country : "");

  char weatherBuf[48] = {};
  if (hero.weatherCondition[0]) {
    if (hero.weatherTemp[0])
      snprintf(weatherBuf, sizeof(weatherBuf), "%s %sF", hero.weatherCondition, hero.weatherTemp);
    else
      snprintf(weatherBuf, sizeof(weatherBuf), "%s", hero.weatherCondition);
  }
  lv_label_set_text(g_launchUi.heroWeather, weatherBuf);

  char winBuf[64] = {};
  if (hero.winOpen || hero.winClose) {
    struct tm wo = {}, wc = {};
    if (hero.winOpen) gmtime_r(&hero.winOpen, &wo);
    if (hero.winClose) gmtime_r(&hero.winClose, &wc);
    if (hero.winOpen && hero.winClose)
      snprintf(winBuf, sizeof(winBuf), "Window %02d:%02d-%02d:%02d UTC",
               wo.tm_hour, wo.tm_min, wc.tm_hour, wc.tm_min);
    else if (hero.winOpen)
      snprintf(winBuf, sizeof(winBuf), "Window %02d:%02d UTC", wo.tm_hour, wo.tm_min);
  }
  lv_label_set_text(g_launchUi.heroWindow, winBuf);

  // ---- View 1: Compact rows (missions 1-2) ----
  static const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};
  for (int i = 0; i < 2; i++) {
    const int idx = i + 1;
    if (idx >= g_launchState.count) {
      lv_obj_add_flag(g_launchUi.compactBg[i], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    const LaunchItem &item = g_launchState.items[idx];

    lv_label_set_text(g_launchUi.compactBadgeLabel[i], item.provider);
    lvglSetBgFlat(g_launchUi.compactBadge[i], launchProviderColor(item.providerSlug));
    lv_label_set_text(g_launchUi.compactName[i], item.name);

    char cvpBuf[96];
    snprintf(cvpBuf, sizeof(cvpBuf), "%s | %s", item.vehicle, item.pad);
    lv_label_set_text(g_launchUi.compactVehicle[i], cvpBuf);

    char clocBuf[96];
    snprintf(clocBuf, sizeof(clocBuf), "%s, %s", item.location, item.country);
    lv_label_set_text(g_launchUi.compactLocation[i], clocBuf);

    if (item.hasT0) {
      struct tm ti;
      time_t epoch = item.t0Epoch;
      gmtime_r(&epoch, &ti);
      char dateBuf[20];
      snprintf(dateBuf, sizeof(dateBuf), "%s %02d %02d:%02d",
               months[ti.tm_mon], ti.tm_mday, ti.tm_hour, ti.tm_min);
      lv_label_set_text(g_launchUi.compactDate[i], dateBuf);
    } else {
      lv_label_set_text(g_launchUi.compactDate[i], "TBD");
    }
  }

  // Apply current view visibility
  lvglSetLaunchView(g_launchUi.viewIndex);

  g_launchState.dirty = false;
}

static void lvglTickLaunchCountdown() {
  if (!g_lvglLaunchRoot || g_uiPageMode != UI_PAGE_LAUNCH) return;
  if (g_launchState.count == 0 || !g_launchState.items[0].hasT0) {
    lv_label_set_text(g_launchUi.heroCountdown, "TBD");
    return;
  }

  const UiThemeLvglTokens &t = activeUiTheme().lvgl;
  const time_t now = time(nullptr);
  const uint32_t now32 = millis();
  const time_t t0 = g_launchState.items[0].t0Epoch;
  const int32_t delta = (int32_t)(t0 - now);

  char buf[24];
  uint32_t textColor = t.infoText;

  if (delta <= 0) {
    snprintf(buf, sizeof(buf), "LIFTOFF!");
    textColor = 0x00E676;
  } else if (delta < 600) {
    int m = delta / 60, s = delta % 60;
    snprintf(buf, sizeof(buf), "T-%02d:%02d", m, s);
    textColor = 0xFF1744;
  } else if (delta < 3600) {
    int m = delta / 60, s = delta % 60;
    snprintf(buf, sizeof(buf), "T-%02d:%02d", m, s);
    textColor = 0xFFD600;
  } else if (delta < 86400) {
    int h = delta / 3600, m = (delta % 3600) / 60, s = delta % 60;
    snprintf(buf, sizeof(buf), "T-%02d:%02d:%02d", h, m, s);
  } else {
    int d = delta / 86400, h = (delta % 86400) / 3600, m = (delta % 3600) / 60;
    if (d > 99) snprintf(buf, sizeof(buf), "T-%dd", d);
    else snprintf(buf, sizeof(buf), "T-%dd %02d:%02d", d, h, m);
  }

  lv_label_set_text(g_launchUi.heroCountdown, buf);
  lvglSetTextHex(g_launchUi.heroCountdown, textColor);

  // Auto-rotate views every 10 seconds (pause while QR is open)
  if (!g_launchUi.qrModalOpen && g_launchState.count > 1 &&
      (now32 - g_launchUi.lastViewRotateMs) >= 10000UL) {
    g_launchUi.lastViewRotateMs = now32;
    uint8_t next = (g_launchUi.viewIndex + 1) % 2;
    Serial.printf("[LAUNCH] view rotate %d -> %d (count=%d)\n",
                  (int)g_launchUi.viewIndex, (int)next, (int)g_launchState.count);
    lvglSetLaunchView(next);
  }
}

// ── End Launch Page LVGL ───────────────────────────────────────────────────

static void lvglUpdateTransitUi(bool force) {
  (void)force;
  if (!g_lvglTransitRoot) return;
  const UiThemeLvglTokens &t = activeUiTheme().lvgl;
  // r242: auto-revert origin/terminus display after 8 seconds
  if (g_transitOrgMode && g_transitOrgModeMs &&
      (millis() - g_transitOrgModeMs) > 8000UL) {
    g_transitOrgMode   = false;
    g_transitOrgModeMs = 0;
  }

  // Station name in header: prefer official API name, fallback to user-typed
  if (g_transitUi.station) {
    const char *stn = g_transitState.stationName[0] ? g_transitState.stationName
                    : (g_transitConfig.station[0]   ? g_transitConfig.station : "--");
    lv_label_set_text(g_transitUi.station, stn);
  }

  // Status (last fetch time)
  if (g_transitUi.status) {
    char buf[24];
    if (g_transitState.valid) {
      snprintf(buf, sizeof(buf), "%s", g_transitState.fetchedAt);
    } else if (g_transitState.lastHttpCode != 0 && g_transitState.lastHttpCode != 200) {
      snprintf(buf, sizeof(buf), "err %d", g_transitState.lastHttpCode);
    } else {
      copyStringSafe(buf, sizeof(buf), g_transitConfig.configured ? "..." : "");
    }
    lv_label_set_text(g_transitUi.status, buf);
  }

  // Show/hide rows
  const uint8_t cnt = g_transitState.valid ? g_transitState.count : 0;
  if (g_transitUi.noData) {
    if (cnt == 0) lv_obj_clear_flag(g_transitUi.noData, LV_OBJ_FLAG_HIDDEN);
    else          lv_obj_add_flag(g_transitUi.noData,   LV_OBJ_FLAG_HIDDEN);
  }

  for (uint8_t i = 0; i < TRANSIT_MAX_DEPARTURES; ++i) {
    const bool hasRow = (i < cnt);
    // Toggle visibility of row elements
    auto rowShow = [&](lv_obj_t *obj, bool show) {
      if (!obj) return;
      if (show) lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
      else      lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    };
    rowShow(g_transitUi.lineBg[i],   hasRow);
    rowShow(g_transitUi.line_[i],    hasRow);
    rowShow(g_transitUi.dest[i],     hasRow);
    rowShow(g_transitUi.time_[i],    hasRow);
    rowShow(g_transitUi.arr[i],      hasRow);
    rowShow(g_transitUi.delay[i],    hasRow);
    rowShow(g_transitUi.platform[i], hasRow);

    if (!hasRow) continue;
    const TransitDeparture &d = g_transitState.departures[i];

    // r242: badge shape — pill for BUS/TRAM, rect for rail
    const bool isBus = transitIsBus(d.category);
    if (g_transitUi.lineBg[i]) {
      lv_obj_set_style_radius(g_transitUi.lineBg[i], isBus ? 14 : 6, LV_PART_MAIN);
    }
    // Badge color: API routeColor > mode semantic (bus→blue, rail→green/red); toxic-candy override.
    if (g_transitUi.lineBg[i]) {
      uint32_t badgeColor;
      if (lvglThemeIsToxicCandy()) {
        badgeColor = 0xCC00AA;  // toxic-candy: magenta for all transit badges
      } else if (d.cancelled) {
        badgeColor = 0x888888;  // grey out cancelled services
      } else if (d.routeColor != 0) {
        // Renfe feeds return near-white #F2F5F5 — invisible on light themes.
        // Skip if luma > 230 (near-white) and let the mode fallback handle it.
        uint8_t cr = (d.routeColor >> 16) & 0xFF, cg = (d.routeColor >> 8) & 0xFF, cb = d.routeColor & 0xFF;
        uint16_t luma = (cr * 299u + cg * 587u + cb * 114u) / 1000u;
        badgeColor = (luma > 230) ? transitCategoryColor(d.category) : d.routeColor;
      } else if (isBus) {
        badgeColor = 0x1565C0;  // blue shade for bus/tram (vs green trains)
      } else {
        badgeColor = transitCategoryColor(d.category);
      }
      lv_obj_set_style_bg_color(g_transitUi.lineBg[i], lv_color_hex(badgeColor), LV_PART_MAIN);
    }
    // Badge text color + adaptive font (shorter names → larger font; long → shrink to fit)
    if (g_transitUi.line_[i]) {
      uint32_t textColor = 0xFFFFFF;
      if (!lvglThemeIsToxicCandy() && d.routeTextColor != 0) {
        textColor = d.routeTextColor;
      } else if (!lvglThemeIsToxicCandy() && d.routeColor != 0) {
        textColor = (lvglColorLuma(d.routeColor) >= 128) ? 0x111111 : 0xFFFFFF;
      }
      lv_obj_set_style_text_color(g_transitUi.line_[i], lv_color_hex(textColor), LV_PART_MAIN);
      // Adaptive font: shrink so short names still look bold, long ones legible
      const size_t ll = strlen(d.line);
      const lv_font_t *bFont = (ll <= 4) ? lvglFontMeta()  :  // 20px
                                (ll <= 6) ? lvglFontSmall() :  // 18px
                                (ll <= 8) ? lvglFontMini()  :  // 16px
                                            lvglFontTiny();    // 14px — scroll reveals rest
      lv_obj_set_style_text_font(g_transitUi.line_[i], bFont, 0);
      lv_label_set_text(g_transitUi.line_[i], d.line);
    }

    // Destination: cancelled → grey "X dest"; org mode → "From > Dest"; normal → headsign
    if (g_transitUi.dest[i]) {
      if (d.cancelled) {
        char cbuf[52];
        snprintf(cbuf, sizeof(cbuf), "X %s", d.destination);
        lv_label_set_text(g_transitUi.dest[i], cbuf);
        lv_obj_set_style_text_color(g_transitUi.dest[i], lv_color_hex(0x888888), LV_PART_MAIN);
      } else if (g_transitOrgMode && d.tripFromName[0]) {
        char cbuf[84];
        snprintf(cbuf, sizeof(cbuf), "%s > %s", d.tripFromName, d.destination);
        lv_label_set_text(g_transitUi.dest[i], cbuf);
        lv_obj_set_style_text_color(g_transitUi.dest[i], lv_color_hex(t.auxMeta), LV_PART_MAIN);
      } else {
        lv_label_set_text(g_transitUi.dest[i], d.destination);
        lv_obj_set_style_text_color(g_transitUi.dest[i], lv_color_hex(t.infoText), LV_PART_MAIN);
      }
    }

    // Departure time ("HH:MM")
    if (g_transitUi.time_[i]) {
      char tbuf[8];
      snprintf(tbuf, sizeof(tbuf), "%02u:%02u", d.depHour, d.depMinute);
      lv_label_set_text(g_transitUi.time_[i], tbuf);
    }

    // Arrival time at destination (">HH:MM" from trip endpoint, or ">---")
    if (g_transitUi.arr[i]) {
      char abuf[10];
      if (d.hasArr) {
        snprintf(abuf, sizeof(abuf), ">%02u:%02u", d.arrHour, d.arrMinute);
        lv_obj_set_style_text_color(g_transitUi.arr[i], lv_color_hex(t.auxMeta), LV_PART_MAIN);
      } else {
        copyStringSafe(abuf, sizeof(abuf), ">---");
        lv_obj_set_style_text_color(g_transitUi.arr[i], lv_color_hex(t.divider), LV_PART_MAIN);
      }
      lv_label_set_text(g_transitUi.arr[i], abuf);
    }

    // Delay indicator (+Xm = late, -Xm = early, blank = on time / no data)
    if (g_transitUi.delay[i]) {
      if (d.hasDelay && d.delayMin != 0) {
        char dbuf[8];
        snprintf(dbuf, sizeof(dbuf), d.delayMin > 0 ? "+%dm" : "%dm", (int)d.delayMin);
        lv_label_set_text(g_transitUi.delay[i], dbuf);
        lv_obj_set_style_text_color(g_transitUi.delay[i],
            lv_color_hex(d.delayMin > 0 ? 0xCC3322 : 0x22AA33), LV_PART_MAIN);
      } else {
        lv_label_set_text(g_transitUi.delay[i], "");
      }
    }

    // Platform / LIVE indicator: track number when available, else "LIVE" badge for real-time data
    if (g_transitUi.platform[i]) {
      char pbuf[16];
      if (d.platform[0]) {
        snprintf(pbuf, sizeof(pbuf), "Bin.%s", d.platform);
      } else if (d.realTime) {
        copyStringSafe(pbuf, sizeof(pbuf), "LIVE");
      } else {
        pbuf[0] = '\0';
      }
      lv_label_set_text(g_transitUi.platform[i], pbuf);
      lv_obj_set_style_text_color(g_transitUi.platform[i],
          lv_color_hex(d.realTime ? 0x22AA33 : t.auxMeta), LV_PART_MAIN);
    }
  }
}

// ── End Transit LVGL ────────────────────────────────────────────────────────

static bool lvglApplyPageDrag(int16_t dragDx) {
  if (!g_infoUi.root || !g_lvglHomeRoot || !g_lvglAuxRoot || !g_lvglWikiRoot || !g_lvglNowPlayingRoot) return false;
  if (g_uiPageMode == UI_PAGE_DOOM) return false;

  int32_t dx = dragDx;
  const int16_t w = canvasWidth();
  if (w <= 0) return false;
  if (dx > w) dx = w;
  if (dx < -w) dx = -w;
  const int8_t cur = uiPageOrdinal(g_uiPageMode);
  const int8_t maxOrd = uiSwipePageCountNoEnsure() - 1;
  if (cur < 0 || maxOrd < 0) return false;
  // Edge damping when dragging past first/last page.
  if ((cur == 0 && dx > 0) || (cur == maxOrd && dx < 0)) dx /= 3;

  lv_anim_del(g_infoUi.root, lvglSetObjXAnim);
  lv_anim_del(g_lvglHomeRoot, lvglSetObjXAnim);
  lv_anim_del(g_lvglAuxRoot, lvglSetObjXAnim);
  lv_anim_del(g_lvglWikiRoot, lvglSetObjXAnim);
  lv_anim_del(g_lvglNowPlayingRoot, lvglSetObjXAnim);
  if (g_lvglTransitRoot) lv_anim_del(g_lvglTransitRoot, lvglSetObjXAnim);
  g_pageAnim.untilMs = 0;
  g_pageAnim.dragActive = true;

  struct { UiPageMode mode; lv_obj_t *root; } pages[] = {
    {UI_PAGE_INFO,        g_infoUi.root},
    {UI_PAGE_HOME,        g_lvglHomeRoot},
    {UI_PAGE_AUX,         g_lvglAuxRoot},
    {UI_PAGE_WIKI,        g_lvglWikiRoot},
    {UI_PAGE_NOW_PLAYING, g_lvglNowPlayingRoot},
    {UI_PAGE_TRANSIT,     g_lvglTransitRoot},
    {UI_PAGE_LAUNCH,     g_lvglLaunchRoot},
  };
  for (auto &p : pages) {
    if (!p.root) continue;
    const int32_t tx = lvglCarouselPageX(p.mode, cur, w, p.root);
    if (tx >= (int32_t)w * 10) continue;  // disabled, already hidden by helper
    lv_obj_clear_flag(p.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(p.root, (lv_coord_t)(tx + dx), 0);
  }
  return true;
}

static void lvglStartSlideAnim(lv_obj_t *obj, int32_t fromX, int32_t toX, uint16_t durMs) {
  if (!obj || fromX == toX) return;
  lv_anim_del(obj, lvglSetObjXAnim);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_exec_cb(&a, lvglSetObjXAnim);
  lv_anim_set_values(&a, fromX, toX);
  lv_anim_set_time(&a, durMs);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
}

static void lvglApplyPageVisibility(bool animate) {
  if (!g_infoUi.root || !g_lvglHomeRoot || !g_lvglAuxRoot || !g_lvglWikiRoot || !g_lvglNowPlayingRoot) return;

  if (g_lvglDoomRoot) {
    if (g_uiPageMode == UI_PAGE_DOOM) lv_obj_clear_flag(g_lvglDoomRoot, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(g_lvglDoomRoot, LV_OBJ_FLAG_HIDDEN);
  }

  if (g_uiPageMode == UI_PAGE_DOOM) {
    lv_obj_add_flag(g_infoUi.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_lvglHomeRoot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_lvglAuxRoot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_lvglWikiRoot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_lvglNowPlayingRoot, LV_OBJ_FLAG_HIDDEN);
    if (g_lvglLaunchRoot) lv_obj_add_flag(g_lvglLaunchRoot, LV_OBJ_FLAG_HIDDEN);
    g_pageAnim.dragActive = false;
    g_pageAnim.untilMs = 0;
    return;
  }

  const int16_t w = canvasWidth();
  const int8_t cur = uiPageOrdinal(g_uiPageMode);
  if (cur < 0) return;
  const uint32_t now = millis();

  // Build dynamic target positions using live ordinals (skip disabled pages).
  struct PageSlot { lv_obj_t *root; UiPageMode mode; int32_t targetX; };
  PageSlot slots[] = {
    {g_infoUi.root,        UI_PAGE_INFO,        0},
    {g_lvglHomeRoot,       UI_PAGE_HOME,        0},
    {g_lvglAuxRoot,        UI_PAGE_AUX,         0},
    {g_lvglWikiRoot,       UI_PAGE_WIKI,        0},
    {g_lvglNowPlayingRoot, UI_PAGE_NOW_PLAYING, 0},
    {g_lvglTransitRoot,    UI_PAGE_TRANSIT,     0},
    {g_lvglLaunchRoot,     UI_PAGE_LAUNCH,      0},
  };
  constexpr size_t kSlotCount = sizeof(slots) / sizeof(slots[0]);
  for (size_t i = 0; i < kSlotCount; ++i) {
    if (!slots[i].root) { slots[i].targetX = (int32_t)w * 10 + 1; continue; }
    slots[i].targetX = lvglCarouselPageX(slots[i].mode, cur, w, slots[i].root);
  }

  if (!animate) {
    if (g_pageAnim.dragActive) return;
    if (now < g_pageAnim.untilMs) return;
    // Only update positions if they actually changed — avoids constant LVGL invalidation.
    bool allOk = true;
    for (size_t i = 0; i < kSlotCount; ++i) {
      if (slots[i].targetX >= (int32_t)w * 10) continue;  // disabled, already hidden
      if (lv_obj_get_x(slots[i].root) != (lv_coord_t)slots[i].targetX) { allOk = false; break; }
    }
    if (allOk) return;
    for (size_t i = 0; i < kSlotCount; ++i) if (slots[i].root) lv_anim_del(slots[i].root, lvglSetObjXAnim);
    for (size_t i = 0; i < kSlotCount; ++i) {
      if (slots[i].targetX >= (int32_t)w * 10) continue;
      lv_obj_clear_flag(slots[i].root, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(slots[i].root, (lv_coord_t)slots[i].targetX, 0);
      if (abs(slots[i].targetX) >= w) lv_obj_add_flag(slots[i].root, LV_OBJ_FLAG_HIDDEN);
    }
    g_pageAnim.dragActive = false;
    return;
  }

  constexpr uint16_t kSlideMs = 250;
  for (size_t i = 0; i < kSlotCount; ++i) {
    if (slots[i].targetX >= (int32_t)w * 10) continue;  // disabled
    lv_obj_clear_flag(slots[i].root, LV_OBJ_FLAG_HIDDEN);
    lvglStartSlideAnim(slots[i].root, lv_obj_get_x(slots[i].root), slots[i].targetX, kSlideMs);
  }
  g_pageAnim.dragActive = false;
  g_pageAnim.untilMs = now + kSlideMs + 30;
}

static void formatDateIt(const tm &timeinfo, char *out, size_t outLen) {
  static const char* kWeekday[] = {"Domenica", "Lunedi", "Martedi", "Mercoledi", "Giovedi", "Venerdi", "Sabato"};
  static const char* kMonth[] = {"Gennaio", "Febbraio", "Marzo", "Aprile", "Maggio", "Giugno",
                                 "Luglio", "Agosto", "Settembre", "Ottobre", "Novembre", "Dicembre"};
  const char* wd = (timeinfo.tm_wday >= 0 && timeinfo.tm_wday < 7) ? kWeekday[timeinfo.tm_wday] : "";
  const char* mo = (timeinfo.tm_mon >= 0 && timeinfo.tm_mon < 12) ? kMonth[timeinfo.tm_mon] : "";
  snprintf(out, outLen, "%s %d %s %d", wd, timeinfo.tm_mday, mo, timeinfo.tm_year + 1900);
}

static void formatDateEn(const tm &timeinfo, char *out, size_t outLen) {
  static const char* kWeekday[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
  static const char* kMonth[] = {"January","February","March","April","May","June","July","August","September","October","November","December"};
  snprintf(out, outLen, "%s %d %s %d",
    (timeinfo.tm_wday>=0&&timeinfo.tm_wday<7)?kWeekday[timeinfo.tm_wday]:"",
    timeinfo.tm_mday,
    (timeinfo.tm_mon>=0&&timeinfo.tm_mon<12)?kMonth[timeinfo.tm_mon]:"",
    timeinfo.tm_year+1900);
}

static void formatDateFr(const tm &timeinfo, char *out, size_t outLen) {
  static const char* kWeekday[] = {"Dimanche","Lundi","Mardi","Mercredi","Jeudi","Vendredi","Samedi"};
  static const char* kMonth[] = {"Janvier","Fevrier","Mars","Avril","Mai","Juin","Juillet","Aout","Septembre","Octobre","Novembre","Decembre"};
  snprintf(out, outLen, "%s %d %s %d",
    (timeinfo.tm_wday>=0&&timeinfo.tm_wday<7)?kWeekday[timeinfo.tm_wday]:"",
    timeinfo.tm_mday,
    (timeinfo.tm_mon>=0&&timeinfo.tm_mon<12)?kMonth[timeinfo.tm_mon]:"",
    timeinfo.tm_year+1900);
}

static void formatDateDe(const tm &timeinfo, char *out, size_t outLen) {
  static const char* kWeekday[] = {"Sonntag","Montag","Dienstag","Mittwoch","Donnerstag","Freitag","Samstag"};
  static const char* kMonth[] = {"Januar","Februar","Maerz","April","Mai","Juni","Juli","August","September","Oktober","November","Dezember"};
  snprintf(out, outLen, "%s, %d. %s %d",
    (timeinfo.tm_wday>=0&&timeinfo.tm_wday<7)?kWeekday[timeinfo.tm_wday]:"",
    timeinfo.tm_mday,
    (timeinfo.tm_mon>=0&&timeinfo.tm_mon<12)?kMonth[timeinfo.tm_mon]:"",
    timeinfo.tm_year+1900);
}

static void formatDateEs(const tm &timeinfo, char *out, size_t outLen) {
  static const char* kWeekday[] = {"Domingo","Lunes","Martes","Miercoles","Jueves","Viernes","Sabado"};
  static const char* kMonth[] = {"Enero","Febrero","Marzo","Abril","Mayo","Junio","Julio","Agosto","Septiembre","Octubre","Noviembre","Diciembre"};
  snprintf(out, outLen, "%s %d %s %d",
    (timeinfo.tm_wday>=0&&timeinfo.tm_wday<7)?kWeekday[timeinfo.tm_wday]:"",
    timeinfo.tm_mday,
    (timeinfo.tm_mon>=0&&timeinfo.tm_mon<12)?kMonth[timeinfo.tm_mon]:"",
    timeinfo.tm_year+1900);
}

static void formatDatePt(const tm &timeinfo, char *out, size_t outLen) {
  static const char* kWeekday[] = {"Domingo","Segunda","Terca","Quarta","Quinta","Sexta","Sabado"};
  static const char* kMonth[] = {"Janeiro","Fevereiro","Marco","Abril","Maio","Junho","Julho","Agosto","Setembro","Outubro","Novembro","Dezembro"};
  snprintf(out, outLen, "%s %d %s %d",
    (timeinfo.tm_wday>=0&&timeinfo.tm_wday<7)?kWeekday[timeinfo.tm_wday]:"",
    timeinfo.tm_mday,
    (timeinfo.tm_mon>=0&&timeinfo.tm_mon<12)?kMonth[timeinfo.tm_mon]:"",
    timeinfo.tm_year+1900);
}

static void formatDateLa(const tm &timeinfo, char *out, size_t outLen) {
  static const char* kWeekday[] = {"Dies Solis","Dies Lunae","Dies Martis","Dies Mercurii","Dies Iovis","Dies Veneris","Dies Saturni"};
  static const char* kMonth[] = {"Ianuarius","Februarius","Martius","Aprilis","Maius","Iunius","Iulius","Augustus","September","October","November","December"};
  snprintf(out, outLen, "%s %d %s %d",
    (timeinfo.tm_wday>=0&&timeinfo.tm_wday<7)?kWeekday[timeinfo.tm_wday]:"",
    timeinfo.tm_mday,
    (timeinfo.tm_mon>=0&&timeinfo.tm_mon<12)?kMonth[timeinfo.tm_mon]:"",
    timeinfo.tm_year+1900);
}

static void formatDateEo(const tm &timeinfo, char *out, size_t outLen) {
  static const char* kWeekday[] = {"Dimanco","Lundo","Mardo","Merkredo","Jaudo","Vendredo","Sabato"};
  static const char* kMonth[] = {"Januaro","Februaro","Marto","Aprilo","Majo","Junio","Julio","Auxgusto","Septembro","Oktobro","Novembro","Decembro"};
  snprintf(out, outLen, "%s %d %s %d",
    (timeinfo.tm_wday>=0&&timeinfo.tm_wday<7)?kWeekday[timeinfo.tm_wday]:"",
    timeinfo.tm_mday,
    (timeinfo.tm_mon>=0&&timeinfo.tm_mon<12)?kMonth[timeinfo.tm_mon]:"",
    timeinfo.tm_year+1900);
}

static void formatDateTlh(const tm &timeinfo, char *out, size_t outLen) {
  // Klingon: "jaj N jar M, DIS Y" (day N month M, year Y)
  // Fan-made weekday names in ASCII transliteration
  static const char* kWeekday[] = {"jaj wa'","jaj cha'","jaj wej","jaj loS","jaj vagh","jaj jav","jaj Soch"};
  snprintf(out, outLen, "%s, jaj %d jar %d, DIS %d",
    (timeinfo.tm_wday>=0&&timeinfo.tm_wday<7)?kWeekday[timeinfo.tm_wday]:"",
    timeinfo.tm_mday,
    timeinfo.tm_mon+1,
    timeinfo.tm_year+1900);
}

static void formatDateL33t(const tm &timeinfo, char *out, size_t outLen) {
  static const char* kWeekday[] = {"5uNd4y","M0Nd4y","7u35d4y","W3dN35d4y","7Hur5d4y","Fr1d4y","54TuRd4y"};
  static const char* kMonth[] = {"J4Nu4rY","F3bRu4rY","M4rCH","4pr1L","M4Y","JuN3","JuLY","4ugu57","53p73mb3r","0c70b3r","N0v3mb3r","d3c3mb3r"};
  snprintf(out, outLen, "%s %d %s %d",
    (timeinfo.tm_wday>=0&&timeinfo.tm_wday<7)?kWeekday[timeinfo.tm_wday]:"",
    timeinfo.tm_mday,
    (timeinfo.tm_mon>=0&&timeinfo.tm_mon<12)?kMonth[timeinfo.tm_mon]:"",
    timeinfo.tm_year+1900);
}

static void formatDateSha(const tm &timeinfo, char *out, size_t outLen) {
  static const char* kWeekday[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
  static const char* kMonth[] = {"January","February","March","April","May","June","July","August","September","October","November","December"};
  snprintf(out, outLen, "%s %d %s AD %d",
    (timeinfo.tm_wday>=0&&timeinfo.tm_wday<7)?kWeekday[timeinfo.tm_wday]:"",
    timeinfo.tm_mday,
    (timeinfo.tm_mon>=0&&timeinfo.tm_mon<12)?kMonth[timeinfo.tm_mon]:"",
    timeinfo.tm_year+1900);
}

static void formatDateVal(const tm &timeinfo, char *out, size_t outLen) {
  static const char* kWeekday[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
  static const char* kMonth[] = {"January","February","March","April","May","June","July","August","September","October","November","December"};
  snprintf(out, outLen, "%s %d %s %d",
    (timeinfo.tm_wday>=0&&timeinfo.tm_wday<7)?kWeekday[timeinfo.tm_wday]:"",
    timeinfo.tm_mday,
    (timeinfo.tm_mon>=0&&timeinfo.tm_mon<12)?kMonth[timeinfo.tm_mon]:"",
    timeinfo.tm_year+1900);
}

static void formatDateBellazio(const tm &timeinfo, char *out, size_t outLen) {
  static const char* kWeekday[] = {"Domenica","Lunedi","Martedi","Mercoledi","Giovedi","Venerdi","Sabato"};
  static const char* kMonth[] = {"Gennaio","Febbraio","Marzo","Aprile","Maggio","Giugno","Luglio","Agosto","Settembre","Ottobre","Novembre","Dicembre"};
  snprintf(out, outLen, "%s %d %s %d",
    (timeinfo.tm_wday>=0&&timeinfo.tm_wday<7)?kWeekday[timeinfo.tm_wday]:"",
    timeinfo.tm_mday,
    (timeinfo.tm_mon>=0&&timeinfo.tm_mon<12)?kMonth[timeinfo.tm_mon]:"",
    timeinfo.tm_year+1900);
}

// --- Pirate word clock (over-the-top) ---

static const char* wordHourPir(int h12) {
  switch (h12) {
    case 1:  return "one";
    case 2:  return "two";
    case 3:  return "three";
    case 4:  return "four";
    case 5:  return "five";
    case 6:  return "six";
    case 7:  return "seven";
    case 8:  return "eight";
    case 9:  return "nine";
    case 10: return "ten";
    case 11: return "eleven";
    default: return "twelve";
  }
}

static const char* pirateInsult(int m) {
  switch (m % 4) {
    case 0:  return "ye scurvy dog";
    case 1:  return "ye landlubber";
    case 2:  return "ye bilge rat";
    default: return "ye barnacle brain";
  }
}

static const char* pirateExclaim(int m) {
  switch (m % 4) {
    case 0:  return "Yarr";
    case 1:  return "Ahoy";
    case 2:  return "Avast";
    default: return "Arrr";
  }
}

static void composeWordClockSentencePir(const tm &timeinfo, char *out, size_t outLen) {
  int h12 = timeinfo.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  int m5 = ((timeinfo.tm_min + 2) / 5) * 5;
  if (m5 >= 60) { m5 = 0; h12 = (h12 % 12) + 1; }
  const char* ins = pirateInsult(timeinfo.tm_min);
  const char* exc = pirateExclaim(timeinfo.tm_min);
  if (m5 == 0)       snprintf(out, outLen, "Blimey! It be %s o'clock, %s!", wordHourPir(h12), ins);
  else if (m5 == 15) snprintf(out, outLen, "Shiver me timbers! Quarter past %s!", wordHourPir(h12));
  else if (m5 == 30) snprintf(out, outLen, "Arrr! Half past %s, %s!", wordHourPir(h12), ins);
  else if (m5 == 45) { int nh = (h12 % 12) + 1; snprintf(out, outLen, "Avast! Quarter to %s, %s!", wordHourPir(nh), ins); }
  else if (m5 < 30)  snprintf(out, outLen, "%s! It be %d past %s, %s!", exc, m5, wordHourPir(h12), ins);
  else               { int nh = (h12 % 12) + 1; snprintf(out, outLen, "%s! It be %d to %s, %s!", exc, 60 - m5, wordHourPir(nh), ins); }
}

static const char* pirateDaySuffix(int day) {
  if (day >= 11 && day <= 13) return "th";
  switch (day % 10) {
    case 1: return "st";
    case 2: return "nd";
    case 3: return "rd";
    default: return "th";
  }
}

static void formatDatePir(const tm &timeinfo, char *out, size_t outLen) {
  static const char* kWeekday[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
  static const char* kMonth[] = {"January","February","March","April","May","June","July","August","September","October","November","December"};
  snprintf(out, outLen, "%s the %d%s of %s, Year of Our Plunder %d",
    (timeinfo.tm_wday>=0&&timeinfo.tm_wday<7)?kWeekday[timeinfo.tm_wday]:"",
    timeinfo.tm_mday, pirateDaySuffix(timeinfo.tm_mday),
    (timeinfo.tm_mon>=0&&timeinfo.tm_mon<12)?kMonth[timeinfo.tm_mon]:"",
    timeinfo.tm_year+1900);
}

static void formatDateActive(const tm &timeinfo, char *out, size_t outLen) {
  findLangVtable()->formatDate(timeinfo, out, outLen);
}

static void formatCityLabelCore(const char *src, char *out, size_t outLen, size_t maxCodepoints, bool withEllipsis) {
  if (!out || outLen == 0) return;
  if (!src || !*src) {
    out[0] = '\0';
    return;
  }

  size_t j = 0;
  size_t codepoints = 0;
  bool clipped = false;
  bool cap = true;
  for (size_t i = 0; src[i] != '\0' && j + 1 < outLen; ++i) {
    unsigned char c = (unsigned char)src[i];
    const bool leadByte = (c & 0xC0u) != 0x80u;
    if (leadByte) {
      if (maxCodepoints > 0 && codepoints >= maxCodepoints) {
        clipped = true;
        break;
      }
      ++codepoints;
    }
    if (c >= 'a' && c <= 'z') {
      out[j++] = cap ? (char)toupper(c) : (char)c;
      cap = false;
    } else if (c >= 'A' && c <= 'Z') {
      out[j++] = cap ? (char)c : (char)tolower(c);
      cap = false;
    } else {
      out[j++] = (char)c;
      cap = (c == ' ' || c == '-' || c == '/');
    }
  }
  if (withEllipsis && clipped && outLen > 4) {
    while (j > 0 && out[j - 1] == ' ') {
      --j;
    }
    while (j + 4 > outLen) {
      if (j == 0) break;
      --j;
    }
    if (j + 4 <= outLen) {
      out[j++] = '.';
      out[j++] = '.';
      out[j++] = '.';
    }
  }
  out[j] = '\0';
}

static void formatCityLabel(const char *src, char *out, size_t outLen) {
  formatCityLabelCore(src, out, outLen, 10, true);
}

static void formatCityLabelFull(const char *src, char *out, size_t outLen) {
  formatCityLabelCore(src, out, outLen, 0, false);
}

static void lvglUpdateCityTicker(const char *rawCity, bool force) {
  if (!g_weatherUi.city) return;

  char shortCity[32];
  char fullCity[48];
  formatCityLabel(rawCity, shortCity, sizeof(shortCity));
  formatCityLabelFull(rawCity, fullCity, sizeof(fullCity));

  const uint32_t now = millis();
  const bool cityChanged = strncmp(g_weatherUi.cityRawLast, rawCity ? rawCity : "", sizeof(g_weatherUi.cityRawLast) - 1) != 0;
  if (cityChanged) {
    copyStringSafe(g_weatherUi.cityRawLast, sizeof(g_weatherUi.cityRawLast), rawCity ? rawCity : "");
    g_weatherUi.cityTickerScroll = false;
    g_weatherUi.cityTickerEndMs = 0;
    g_weatherUi.cityTickerNextMs = now + 10000UL;
    force = true;
  }

  if (g_weatherUi.cityTickerScroll) {
    if (now >= g_weatherUi.cityTickerEndMs) {
      lv_label_set_long_mode(g_weatherUi.city, LV_LABEL_LONG_DOT);
      lv_label_set_text(g_weatherUi.city, shortCity);
      lvglForceLabelVisible(g_weatherUi.city);
      g_weatherUi.cityTickerScroll = false;
      g_weatherUi.cityTickerNextMs = now + 10000UL;
      return;
    }
    if (force) {
      lv_label_set_long_mode(g_weatherUi.city, LV_LABEL_LONG_SCROLL_CIRCULAR);
      lv_label_set_text(g_weatherUi.city, fullCity);
      lvglForceLabelVisible(g_weatherUi.city);
    }
    return;
  }

  if (force) {
    lv_label_set_long_mode(g_weatherUi.city, LV_LABEL_LONG_DOT);
    lv_label_set_text(g_weatherUi.city, shortCity);
    lvglForceLabelVisible(g_weatherUi.city);
  }

  if (now < g_weatherUi.cityTickerNextMs) return;
  if (strcmp(fullCity, shortCity) == 0) {
    g_weatherUi.cityTickerNextMs = now + 10000UL;
    return;
  }

  lv_label_set_long_mode(g_weatherUi.city, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_label_set_text(g_weatherUi.city, fullCity);
  lvglForceLabelVisible(g_weatherUi.city);

  uint32_t showMs = 2600UL + (uint32_t)strlen(fullCity) * 220UL;
  if (showMs > 6500UL) showMs = 6500UL;
  g_weatherUi.cityTickerScroll = true;
  g_weatherUi.cityTickerEndMs = now + showMs;
}

static void lvglUpdateInfoPanel(bool force) {
  (void)force;
  if (!g_infoUi.title || !g_infoUi.endpoint || !g_infoUi.bodyLeft || !g_infoUi.bodyRight) return;

  char ipBuf[32] = "--";
  char macBuf[20] = "--";
  char wifiBuf[48] = "OFFLINE";
  char ssidBuf[40] = "--";
  char pwrBuf[40] = "--";
  char pwrSourceBuf[24] = "--";
  char battVizBuf[96] = "N/A";
  char webUrlBuf[72] = "http://--:8080";
  char endpointBuf[48] = "--:8080";

#if TEST_WIFI
  const wl_status_t st = WiFi.status();
  const bool wifiOk = (st == WL_CONNECTED) && g_wifiSt.connected;
  const bool setupApOk = g_wifiSt.setupApActive;
  snprintf(wifiBuf, sizeof(wifiBuf), "%s", wlStatusToStr(st));
  if (wifiOk) {
    snprintf(ipBuf, sizeof(ipBuf), "%s", WiFi.localIP().toString().c_str());
    snprintf(macBuf, sizeof(macBuf), "%s", WiFi.macAddress().c_str());
    snprintf(wifiBuf, sizeof(wifiBuf), "OK %ddBm", WiFi.RSSI());
    copyStringSafe(ssidBuf, sizeof(ssidBuf), WiFi.SSID().c_str());
  } else if (g_wifiSt.lastDiscReason >= 0) {
    snprintf(wifiBuf, sizeof(wifiBuf), "DISC %d", g_wifiSt.lastDiscReason);
  }
#if WEB_CONFIG_ENABLED
  if (wifiOk && g_webCfg.serverStarted) {
    snprintf(webUrlBuf, sizeof(webUrlBuf), "http://%s:%u", ipBuf, (unsigned)WEB_CONFIG_PORT);
    snprintf(endpointBuf, sizeof(endpointBuf), "%s:%u", ipBuf, (unsigned)WEB_CONFIG_PORT);
  } else if (setupApOk) {
    wifiBuildSetupPortalUrl(webUrlBuf, sizeof(webUrlBuf));
    IPAddress apIp = WiFi.softAPIP();
    if ((uint32_t)apIp == 0U) apIp = IPAddress(192, 168, 4, 1);
    snprintf(ipBuf, sizeof(ipBuf), "%s", apIp.toString().c_str());
    snprintf(endpointBuf, sizeof(endpointBuf), "%s:%u", ipBuf, (unsigned)WEB_CONFIG_PORT);
    snprintf(wifiBuf, sizeof(wifiBuf), "AP SETUP");
    if (g_wifiSt.setupApSsid[0]) copyStringSafe(ssidBuf, sizeof(ssidBuf), g_wifiSt.setupApSsid);
  }
#endif
#endif

#if TEST_BATTERY
  if (g_batt.hasSample) {
    char barBuf[16];
    batteryBarsForPercent(g_batt.percent, barBuf, sizeof(barBuf));
    const char *levelColor = batteryLevelColorHex(g_batt.percent);
    snprintf(pwrBuf, sizeof(pwrBuf), "%s %d%%", batteryPowerModeText(), g_batt.percent);
    snprintf(pwrSourceBuf, sizeof(pwrSourceBuf), "%s", batteryPowerSourceText(millis()));
    if (g_batt.chargingLikely) {
      snprintf(battVizBuf, sizeof(battVizBuf), "#%s %s +CHG#", levelColor, barBuf);
    } else {
      snprintf(battVizBuf, sizeof(battVizBuf), "#%s %s#", levelColor, barBuf);
    }
  } else {
    snprintf(pwrBuf, sizeof(pwrBuf), "BATT N/A");
    snprintf(pwrSourceBuf, sizeof(pwrSourceBuf), "UNKNOWN");
    snprintf(battVizBuf, sizeof(battVizBuf), "N/A");
  }
#else
  snprintf(pwrBuf, sizeof(pwrBuf), "BATT OFF");
  snprintf(pwrSourceBuf, sizeof(pwrSourceBuf), "OFF");
  snprintf(battVizBuf, sizeof(battVizBuf), "OFF");
#endif

  char ntpBuf[16] = "OFF";
#if TEST_NTP
  snprintf(ntpBuf, sizeof(ntpBuf), "%s", g_clock.ntpSynced ? "SYNCED" : "WAIT");
#endif

  if (strcmp(endpointBuf, "--:8080") == 0) {
    snprintf(endpointBuf, sizeof(endpointBuf), "%s:%u", ipBuf, (unsigned)WEB_CONFIG_PORT);
  }

  const size_t kLeftColSz = 512;
  char *leftCol = (char *)heap_caps_malloc(kLeftColSz, MALLOC_CAP_SPIRAM);
  if (!leftCol) return;
  snprintf(leftCol, kLeftColSz,
           "wifi: %s\n"
           "ssid: %s\n"
           "bat: %s\n"
           "pwr: %s\n"
           "src: %s\n"
           "mac: %s\n"
           "fw: %s\n"
           "ntp: %s",
           wifiBuf,
           ssidBuf,
           battVizBuf,
           pwrBuf,
           pwrSourceBuf,
           macBuf,
           FW_BUILD_TAG,
           ntpBuf);

  char infoTitleBuf[48];
  snprintf(infoTitleBuf, sizeof(infoTitleBuf), "ScryBar Stats  %s", FW_BUILD_TAG);
  lv_label_set_text(g_infoUi.title, infoTitleBuf);
  lv_label_set_text(g_infoUi.endpoint, endpointBuf);
  lv_label_set_text(g_infoUi.bodyLeft, leftCol);
  heap_caps_free(leftCol);
#if defined(LV_USE_QRCODE) && LV_USE_QRCODE
  if (g_infoUi.webQr) {
    if (strncmp(g_infoUi.lastQrPayload, webUrlBuf, sizeof(g_infoUi.lastQrPayload) - 1) != 0) {
      copyStringSafe(g_infoUi.lastQrPayload, sizeof(g_infoUi.lastQrPayload), webUrlBuf);
      lv_qrcode_update(g_infoUi.webQr, g_infoUi.lastQrPayload, strlen(g_infoUi.lastQrPayload));
    }
    lv_obj_invalidate(g_infoUi.webQr);
  }
#endif
  lvglForceLabelVisible(g_infoUi.title);
  lvglForceLabelVisible(g_infoUi.endpoint);
  lvglForceLabelVisible(g_infoUi.bodyLeft);
}

static void lvglDisplayFlushCb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
  (void)drv;
  if (!g_dispHw.canvasBuf) { lv_disp_flush_ready(drv); return; }

  const int32_t srcW = area->x2 - area->x1 + 1;
  const int16_t cW   = canvasWidth();
  const int16_t cH   = canvasHeight();

  for (int32_t y = area->y1; y <= area->y2; ++y, color_p += srcW) {
    if (y < 0 || y >= cH) continue;
    const int32_t x1 = (area->x1 < 0)    ? 0      : area->x1;
    const int32_t x2 = (area->x2 >= cW)  ? cW - 1 : area->x2;
    if (x1 > x2) continue;
    uint16_t *dst = &g_dispHw.canvasBuf[(size_t)y * DB_CANVAS_W + (size_t)x1];
#if LV_COLOR_DEPTH == 16
    memcpy(dst, color_p + (x1 - area->x1), (size_t)(x2 - x1 + 1) * sizeof(uint16_t));
#else
    for (int32_t x = x1; x <= x2; ++x) dst[x - x1] = lv_color_to16(color_p[x - area->x1]);
#endif
  }

  g_dispHw.canvasDirty = true;
  lv_disp_flush_ready(drv);
}

// Initialises all LVGL widgets for one feed deck (AUX or WIKI).
// root:   the lv_obj_t page created by the nav system for this deck
// d:      the FeedDeckUi struct to populate
// isWiki: false=AUX/RSS, true=WIKI
static void lvglInitFeedDeck(FeedDeckUi &d, lv_obj_t *root, bool isWiki) {
  const UiThemeLvglTokens &theme = activeUiTheme().lvgl;
  const int16_t cW = canvasWidth();
  const int16_t cH = canvasHeight();
  const bool minimalTheme = lvglThemeIsMinimalBrutalistMono();
  const lv_coord_t kCardRadius   = minimalTheme ? 0 : 10;
  const lv_coord_t kButtonRadius = minimalTheme ? 0 : 4;
  const lv_coord_t kBadgeRadius  = minimalTheme ? 0 : 6;
  const lv_color_t kPanelBg    = lv_color_hex(lvglResolvedPanelBg(theme));
  const lv_color_t kHeaderBlue = lv_color_hex(lvglResolvedHeaderBg(theme));

  d.card = lvglCreatePanel(root, cW, cH, 0, 0, kPanelBg, kCardRadius);
  constexpr int16_t kDeckHeaderH = 30;
  const int16_t cardW = cW;
  const int16_t cardH = cH;
  d.header = lvglCreatePanel(d.card, cardW, kDeckHeaderH, 0, 0, kHeaderBlue, kCardRadius);
  d.headerFill = lvglCreatePanel(d.header, cardW, 10, 0, kDeckHeaderH - 10, kHeaderBlue, 0);

  // Right sidebar: favicon (top) + 3 buttons stacked vertically
  const int16_t btnW = 44;
  const int16_t btnH = 26;
  const int16_t sidebarX   = cardW - btnW - 5;
  const int16_t sidebarTop = kDeckHeaderH + 4;
  const int16_t sidebarGap = 4;

  d.title = lv_label_create(d.header);
  lv_obj_set_style_text_font(d.title, lvglFontSmall(), 0);
  {
    const uint32_t hdrTxt = activeUiTheme().lvgl.headerText;
    lv_obj_set_style_text_color(d.title, lv_color_hex(hdrTxt), 0);
  }
  lv_obj_align(d.title, LV_ALIGN_LEFT_MID, 12, 2);
  lv_label_set_text(d.title, isWiki ? "ScryBar Wiki" : "ScryBar RSS");
  lvglForceLabelVisible(d.title);

  if (!isWiki) {
    d.feedIcon = lv_label_create(d.header);
    lv_obj_add_flag(d.feedIcon, LV_OBJ_FLAG_HIDDEN);
  }

  d.status = lv_label_create(d.header);
  lv_obj_set_style_text_font(d.status, lvglFontTiny(), 0);
  lv_obj_set_style_text_color(d.status, lv_color_hex(activeUiTheme().lvgl.headerText), 0);
  lv_obj_align(d.status, LV_ALIGN_RIGHT_MID, -5, 2);
  lv_label_set_text(d.status, "SYNC");
  lvglForceLabelVisible(d.status);

  // ── Right sidebar: favicon badge at top, then 3 buttons ──
  const int16_t sourceBadgeSize = 35;
  int16_t sideY = sidebarTop;

  d.sourceBadge = lvglCreatePanel(d.card, btnW, sourceBadgeSize,
                                   sidebarX, sideY, lv_color_hex(activeUiTheme().lvgl.auxBadgeBg), kBadgeRadius);
  d.sourceBadgeText = lv_label_create(d.sourceBadge);
  lv_obj_set_style_text_font(d.sourceBadgeText, lvglFontTiny(), 0);
  lv_obj_set_style_text_color(d.sourceBadgeText, lv_color_hex(activeUiTheme().lvgl.auxBadgeText), 0);
  lv_label_set_text(d.sourceBadgeText, isWiki ? "W" : "WEB");
  lv_obj_center(d.sourceBadgeText);
  lvglForceLabelVisible(d.sourceBadgeText);
  d.sourceBadgeImg = lv_img_create(d.sourceBadge);
  lv_obj_set_size(d.sourceBadgeImg, kFaviconSize, kFaviconSize);
  lv_obj_center(d.sourceBadgeImg);
  lv_obj_add_flag(d.sourceBadgeImg, LV_OBJ_FLAG_HIDDEN);
  if (isWiki) lv_obj_add_flag(d.sourceBadge, LV_OBJ_FLAG_HIDDEN);
  sideY += sourceBadgeSize + sidebarGap;

  d.nextFeedBtn = lvglCreateDeckButton(d.card, btnW, btnH, sidebarX, sideY,
                                        0x7B63FF, kButtonRadius, "NXT", 0xF7F2FF, d.nextFeedBtnText);
  sideY += btnH + sidebarGap;
  d.refreshBtn  = lvglCreateDeckButton(d.card, btnW, btnH, sidebarX, sideY,
                                        0x6FD8FF, kButtonRadius, "SKIP", 0x113063, d.refreshBtnText);
  sideY += btnH + sidebarGap;
  d.qrBtn       = lvglCreateDeckButton(d.card, btnW, btnH, sidebarX, sideY,
                                        0xFFD34D, kButtonRadius, "QR", 0x1E2F63, d.qrBtnText);

  // ── Left pane: source label + news text (full height) ──
  const int16_t leftPaneX = 8;
  const int16_t leftPaneW = sidebarX - leftPaneX - 6;  // gap before sidebar
  const int16_t sourceTextY = kDeckHeaderH + 6;

  d.sourceSite = lv_label_create(d.card);
  lv_obj_set_style_text_font(d.sourceSite, lvglFontMeta(), 0);
  lv_obj_set_style_text_color(d.sourceSite, lv_color_hex(0xEAF0FF), 0);
  lv_label_set_long_mode(d.sourceSite, LV_LABEL_LONG_DOT);
  lv_obj_set_size(d.sourceSite, leftPaneW, 26);
  lv_obj_set_pos(d.sourceSite, leftPaneX, sourceTextY);
  lv_obj_set_style_text_align(d.sourceSite, LV_TEXT_ALIGN_LEFT, 0);
  lv_label_set_text(d.sourceSite, isWiki ? "Wiki  --/-- --:--" : "RSS  --/-- --:--");
  lvglForceLabelVisible(d.sourceSite);

  const int16_t newsY = sourceTextY + 26;
  int16_t newsH = cardH - newsY - 4;
  if (newsH < 44) newsH = 44;

  d.news = lv_label_create(d.card);
  lv_obj_set_style_text_font(d.news, lvglFontRssNews(), 0);
  lv_obj_set_style_text_color(d.news, lv_color_hex(0xEAF0FF), 0);
  lv_obj_set_style_text_line_space(d.news, 3, 0);
  lv_label_set_long_mode(d.news, LV_LABEL_LONG_DOT);
  lv_obj_set_size(d.news, leftPaneW, newsH);
  lv_obj_set_pos(d.news, leftPaneX, newsY);
  lv_obj_set_style_text_align(d.news, LV_TEXT_ALIGN_LEFT, 0);
  lv_label_set_text(d.news, activeUiStrings()->rssSyncing);
  lvglForceLabelVisible(d.news);

  d.meta = lv_label_create(d.header);
  lv_obj_set_style_text_font(d.meta, lvglFontSmall(), 0);
  lv_obj_set_style_text_color(d.meta, lv_color_hex(0xAFC2F5), 0);
  lv_label_set_long_mode(d.meta, LV_LABEL_LONG_DOT);
  lv_obj_set_size(d.meta, cardW - 316, 22);
  lv_obj_align(d.meta, LV_ALIGN_CENTER, 0, 4);
  lv_label_set_text(d.meta, "Fetch --/-- --:--");
  lvglForceLabelVisible(d.meta);

#if defined(LV_USE_QRCODE) && LV_USE_QRCODE
  // ── QR overlay: full-height QR flush left, hint right ──
  const int16_t qrOverlayH = cardH;
  d.qrOverlay = lvglCreatePanel(d.card, cardW, qrOverlayH, 0, 0, lv_color_hex(activeUiTheme().lvgl.screenBg), 0);
  lv_obj_set_style_layout(d.qrOverlay, 0, 0);
  lv_obj_add_flag(d.qrOverlay, LV_OBJ_FLAG_HIDDEN);

  // QR code: full viewport height, flush left.
  // Use a raw canvas with PSRAM buffer (lv_qrcode_create uses lv_mem_alloc
  // which can't handle the full 172×172 buffer in LVGL's constrained heap).
  int16_t qrSize = qrOverlayH;
  if (qrSize > cardW) qrSize = cardW;
  if (qrSize < 90) qrSize = 90;
  const lv_color_t qrDark  = lv_color_hex(theme.auxQrDark);
  const lv_color_t qrLight = lv_color_hex(theme.auxQrLight);
  const char *qrFallback = isWiki ? "https://en.wikipedia.org" : "https://ansa.it";
  {
    uint32_t bufSz = LV_CANVAS_BUF_SIZE_INDEXED_1BIT(qrSize, qrSize);
    uint8_t *psBuf = (uint8_t *)ps_calloc(1, bufSz);
    if (!psBuf) psBuf = (uint8_t *)calloc(1, bufSz);
    d.qr = lv_canvas_create(d.qrOverlay);
    if (psBuf) {
      lv_canvas_set_buffer(d.qr, psBuf, qrSize, qrSize, LV_IMG_CF_INDEXED_1BIT);
      lv_canvas_set_palette(d.qr, 0, qrDark);
      lv_canvas_set_palette(d.qr, 1, qrLight);
    }
    if (!psBuf) Serial.println("[QR][ERR] canvas buffer alloc failed");
  }
  lv_obj_add_flag(d.qr, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_pos(d.qr, 0, 0);
  lv_obj_set_style_border_width(d.qr, 0, LV_PART_MAIN);
  lv_qrcode_update(d.qr, qrFallback, strlen(qrFallback));

  // Hint label: right of QR, 16px Montserrat, vertically centered
  const int16_t hintX = qrSize + 16;
  const int16_t hintW = cardW - hintX - 12;
  d.qrHint = lv_label_create(d.qrOverlay);
  lv_obj_set_style_text_font(d.qrHint, lvglNowPlayingArtistFont(), 0);  // 23px medium, same as Now Playing
  lv_obj_set_style_text_color(d.qrHint, lv_color_hex(theme.auxQrHint), 0);
  lv_obj_set_size(d.qrHint, hintW, LV_SIZE_CONTENT);
  lv_label_set_long_mode(d.qrHint, LV_LABEL_LONG_WRAP);
  lv_label_set_text(d.qrHint, "Tap anywhere\nto close");
  lv_obj_add_flag(d.qrHint, LV_OBJ_FLAG_FLOATING);  // bypass parent layout
  lv_obj_set_pos(d.qrHint, hintX, (qrOverlayH / 2) - 28);
  lv_obj_add_flag(d.qrHint, LV_OBJ_FLAG_HIDDEN);
#endif
}

static void lvglInitNowPlayingUi(NowPlayingUi &ui, lv_obj_t *root) {
  const UiThemeLvglTokens &theme = activeUiTheme().lvgl;
  const int16_t cW = canvasWidth();
  const int16_t cH = canvasHeight();
  const int16_t headerH = 22;
  const int16_t coverGapRight = 18;
  const int16_t rightPad = 10;
  const int16_t bodyTop = headerH;
  const int16_t coverSize = cH - bodyTop;
  const int16_t contentX = coverSize + coverGapRight;
  const int16_t contentW = cW - contentX - rightPad;
  const int16_t textW = contentW;

  ui.card = lvglCreatePanel(root, cW, cH, 0, 0, lv_color_hex(theme.screenBg), 0);
  lv_obj_set_style_bg_grad_color(ui.card, lv_color_hex(theme.panelBg), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(ui.card, LV_GRAD_DIR_HOR, LV_PART_MAIN);

  ui.header = lvglCreatePanel(ui.card, cW, headerH, 0, 0, lv_color_hex(0x140C23), 0);
  lv_obj_set_style_bg_opa(ui.header, LV_OPA_40, LV_PART_MAIN);

  ui.headerFill = lvglCreatePanel(ui.header, cW, 1, 0, headerH - 1, lv_color_hex(activeUiTheme().lvgl.divider), 0);
  lv_obj_set_style_bg_opa(ui.headerFill, LV_OPA_20, LV_PART_MAIN);

  ui.title = lv_label_create(ui.header);
  lv_obj_set_style_text_font(ui.title, lvglNowPlayingMetaFont(), 0);
  lv_obj_set_style_text_color(ui.title, lv_color_hex(activeUiTheme().lvgl.auxText), 0);
  lv_obj_align(ui.title, LV_ALIGN_LEFT_MID, 8, 2);
  lv_label_set_text(ui.title, "Now Playing");
  lvglForceLabelVisible(ui.title);

  ui.statusDot = lvglCreatePanel(ui.header, 8, 8, 0, 0, lv_color_hex(0x7CFF9D), LV_RADIUS_CIRCLE);
  lv_obj_align(ui.statusDot, LV_ALIGN_RIGHT_MID, -10, 2);

  ui.status = lv_label_create(ui.header);
  lv_obj_set_style_text_font(ui.status, lvglNowPlayingMetaFont(), 0);
  lv_obj_set_style_text_color(ui.status, lv_color_hex(0xB8F7D4), 0);
  lv_obj_align(ui.status, LV_ALIGN_RIGHT_MID, -26, 2);
  lv_label_set_text(ui.status, "IN SYNC");
  lvglForceLabelVisible(ui.status);

  ui.headerTime = lv_label_create(ui.header);
  lv_obj_set_style_text_font(ui.headerTime, lvglNowPlayingMetaFont(), 0);
  lv_obj_set_style_text_color(ui.headerTime, lv_color_hex(activeUiTheme().lvgl.auxText), 0);
  lv_obj_set_style_text_opa(ui.headerTime, LV_OPA_70, 0);
  lv_obj_align(ui.headerTime, LV_ALIGN_RIGHT_MID, -100, 2);
  lv_label_set_text(ui.headerTime, "");
  lvglForceLabelVisible(ui.headerTime);

  ui.coverShell = lvglCreatePanel(ui.card, coverSize, coverSize, 0, bodyTop, lv_color_hex(0x09111B), 0);
  lv_obj_set_style_bg_opa(ui.coverShell, LV_OPA_TRANSP, LV_PART_MAIN);

  ui.cover = lvglCreatePanel(ui.coverShell, coverSize, coverSize, 0, 0, lv_color_hex(0x2E145C), 0);
  lv_obj_set_style_bg_grad_color(ui.cover, lv_color_hex(0xD34B70), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(ui.cover, LV_GRAD_DIR_VER, LV_PART_MAIN);

  ui.coverImage = lv_img_create(ui.cover);
  lv_img_set_src(ui.coverImage, &kNowPlayingRealCover150);
  lv_obj_center(ui.coverImage);
  lv_obj_clear_flag(ui.coverImage, LV_OBJ_FLAG_SCROLLABLE);

  ui.coverStripe = lvglCreatePanel(ui.cover, coverSize, 22, 0, 16, lv_color_hex(0xFFB847), 0);
  lv_obj_set_style_bg_opa(ui.coverStripe, LV_OPA_70, LV_PART_MAIN);

  ui.coverOrb = lvglCreatePanel(ui.cover, 52, 52, 0, 0, lv_color_hex(0xFFE17F), LV_RADIUS_CIRCLE);
  lv_obj_align(ui.coverOrb, LV_ALIGN_CENTER, 10, 3);
  lv_obj_set_style_bg_grad_color(ui.coverOrb, lv_color_hex(0xFFF1B4), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(ui.coverOrb, LV_GRAD_DIR_VER, LV_PART_MAIN);

  ui.coverTop = lv_label_create(ui.cover);
  lv_obj_set_style_text_font(ui.coverTop, lvglFontTiny(), 0);
  lv_obj_set_style_text_color(ui.coverTop, lv_color_hex(0xFDF7FF), 0);
  lv_obj_set_pos(ui.coverTop, 8, 5);
  lv_label_set_text(ui.coverTop, "CITY");
  lvglForceLabelVisible(ui.coverTop);

  ui.coverBottom = lv_label_create(ui.cover);
  lv_obj_set_style_text_font(ui.coverBottom, lvglFontSmall(), 0);
  lv_obj_set_style_text_color(ui.coverBottom, lv_color_hex(0xFDF7FF), 0);
  lv_obj_align(ui.coverBottom, LV_ALIGN_BOTTOM_LEFT, 8, -8);
  lv_label_set_text(ui.coverBottom, "RAIN");
  lvglForceLabelVisible(ui.coverBottom);

  ui.track = lv_label_create(ui.card);
  lv_obj_set_style_text_font(ui.track, lvglNowPlayingTitleFont(), 0);
  lv_obj_set_style_text_color(ui.track, lv_color_hex(activeUiTheme().lvgl.auxText), 0);
  lv_obj_set_style_text_line_space(ui.track, 0, 0);
  lv_label_set_long_mode(ui.track, LV_LABEL_LONG_WRAP);
  lv_obj_set_size(ui.track, textW, 60);
  lv_obj_set_pos(ui.track, contentX, bodyTop + 10);
  lv_label_set_text(ui.track, "");
  lvglForceLabelVisible(ui.track);

  ui.artist = lv_label_create(ui.card);
  lv_obj_set_style_text_font(ui.artist, lvglNowPlayingArtistFont(), 0);
  lv_obj_set_style_text_color(ui.artist, lv_color_hex(0xFFF3F8), 0);
  lv_obj_set_style_text_line_space(ui.artist, 0, 0);
  lv_label_set_long_mode(ui.artist, LV_LABEL_LONG_DOT);
  lv_obj_set_size(ui.artist, textW, 28);
  lv_obj_set_pos(ui.artist, contentX, cH - 44);
  lv_label_set_text(ui.artist, "");
  lvglForceLabelVisible(ui.artist);

  ui.album = lv_label_create(ui.card);
  lv_obj_set_style_text_font(ui.album, lvglFontTiny(), 0);
  lv_obj_set_style_text_color(ui.album, lv_color_hex(0xFFE5A8), 0);
  lv_label_set_long_mode(ui.album, LV_LABEL_LONG_DOT);
  lv_obj_set_size(ui.album, textW, 12);
  lv_obj_set_pos(ui.album, contentX, cH - 18);
  lv_label_set_text(ui.album, "");
  lv_obj_add_flag(ui.album, LV_OBJ_FLAG_HIDDEN);

  ui.source = lv_label_create(ui.card);
  lv_obj_set_style_text_font(ui.source, lvglFontTiny(), 0);
  lv_obj_set_style_text_color(ui.source, lv_color_hex(0xF7D9FF), 0);
  lv_label_set_long_mode(ui.source, LV_LABEL_LONG_DOT);
  lv_obj_set_size(ui.source, textW, 12);
  lv_obj_set_pos(ui.source, contentX, cH - 18);
  lv_label_set_text(ui.source, "");
  lv_obj_add_flag(ui.source, LV_OBJ_FLAG_HIDDEN);

  ui.progressElapsed = lv_label_create(ui.card);
  lv_obj_set_style_text_font(ui.progressElapsed, lvglFontTiny(), 0);
  lv_obj_set_style_text_color(ui.progressElapsed, lv_color_hex(activeUiTheme().lvgl.auxText), 0);
  lv_obj_set_pos(ui.progressElapsed, contentX, cH - 18);
  lv_label_set_text(ui.progressElapsed, "0:00 / 0:00");
  lv_obj_add_flag(ui.progressElapsed, LV_OBJ_FLAG_HIDDEN);

  ui.progressRemaining = lv_label_create(ui.card);
  lv_obj_set_style_text_font(ui.progressRemaining, lvglFontTiny(), 0);
  lv_obj_set_style_text_color(ui.progressRemaining, lv_color_hex(activeUiTheme().lvgl.auxText), 0);
  lv_obj_align(ui.progressRemaining, LV_ALIGN_TOP_RIGHT, -18, cH - 18);
  lv_label_set_text(ui.progressRemaining, "0% left");
  lv_obj_add_flag(ui.progressRemaining, LV_OBJ_FLAG_HIDDEN);

  {
    const auto &th = activeUiTheme().lvgl;
    ui.progressRail = lvglCreatePanel(ui.card, textW, 5, contentX, cH - 16,
                                       lv_color_hex(th.panelBg), LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(ui.progressRail, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui.progressRail, lv_color_hex(th.divider), LV_PART_MAIN);
    lv_obj_set_style_border_opa(ui.progressRail, LV_OPA_30, LV_PART_MAIN);
    lv_obj_add_flag(ui.progressRail, LV_OBJ_FLAG_HIDDEN);

    ui.progressFill = lvglCreatePanel(ui.progressRail, 1, 5, 0, 0,
                                       lv_color_hex(th.auxSourceText), LV_RADIUS_CIRCLE);
  }
  lv_obj_add_flag(ui.progressFill, LV_OBJ_FLAG_HIDDEN);

  // Control buttons (prev/pause/next) removed — decorative only, no event
  // handlers, and their touch area interfered with swipe navigation.
}

static void lvglUpdateNowPlayingUi(NowPlayingUi &ui, bool force) {
  if (!ui.card) return;
  const uint32_t nowMs = millis();
  const bool useLive = liveNowPlayingAvailable();
  const bool useLiveArtwork = useLive && g_liveNowPlayingArtwork.valid;
  const bool displaySync = useLive ? liveNowPlayingDisplayInSync(nowMs) : true;
  uint16_t elapsedSec = 0;
  uint8_t trackIndex = 0;
  const char *trackTitle = "";
  const char *trackArtist = "";
  const char *trackSource = "";
  const FakeNowPlayingTrack *fakeTrack = nullptr;
  char placeholderTitle[128] = {0};
  char placeholderArtist[128] = {0};
  if (useLive) {
    elapsedSec = g_liveNowPlaying.elapsedSec;
    trackTitle = g_liveNowPlaying.title;
    trackArtist = g_liveNowPlaying.artist;
    trackSource = liveNowPlayingSourceLabel();
  } else {
    resolveFakeNowPlayingTrack(nowMs, &elapsedSec, &trackIndex);
    fakeTrack = &kFakeNowPlayingTracks[trackIndex];
    // Show companion setup instructions instead of fake track data
    snprintf(placeholderTitle, sizeof(placeholderTitle), "Launch ScryBar Companion");
    const String ip = WiFi.localIP().toString();
    if (g_webCfg.mdnsHost[0] && ip.length() > 1) {
      snprintf(placeholderArtist, sizeof(placeholderArtist), "%s / %s.local", ip.c_str(), g_webCfg.mdnsHost);
    } else if (ip.length() > 1) {
      snprintf(placeholderArtist, sizeof(placeholderArtist), "%s:8080", ip.c_str());
    } else {
      snprintf(placeholderArtist, sizeof(placeholderArtist), "Waiting for WiFi...");
    }
    trackTitle = placeholderTitle;
    trackArtist = placeholderArtist;
    trackSource = "Companion";
  }

  // Elapsed interpolation is done companion-side for accuracy.
  // Anti-jitter: never let displayed elapsed jump backward within the same track.
  const uint32_t currentToken = useLive ? g_liveNowPlaying.contentToken : 0U;
  if (currentToken != 0 && currentToken == ui.lastLiveToken) {
    if (elapsedSec < ui.lastDisplayedElapsed && (ui.lastDisplayedElapsed - elapsedSec) < 3) {
      elapsedSec = ui.lastDisplayedElapsed;
    }
  }
  ui.lastDisplayedElapsed = elapsedSec;

  const FakeNowPlayingTrack &coverTrack = fakeTrack ? *fakeTrack : kFakeNowPlayingTracks[0];
  const uint16_t durationRaw = useLive ? g_liveNowPlaying.durationSec : coverTrack.durationSec;
  const uint16_t durationSec = durationRaw ? durationRaw : 1U;
  const uint32_t bgSurface = lvglNowPlayingCoverBackgroundColor(useLiveArtwork);
  const uint16_t bgLuma = lvglColorLuma(bgSurface);
  const bool bgIsDark = bgLuma < 116u;
  const uint32_t headerBg = bgIsDark ? lvglLightenRgb(bgSurface, 10) : lvglDarkenRgb(bgSurface, 10);
  const uint32_t primaryText = lvglResolvedOnColorText(bgSurface);
  const uint32_t railBg = bgIsDark ? lvglLightenRgb(bgSurface, 50) : lvglDarkenRgb(bgSurface, 40);
  const uint32_t coverBgA = useLiveArtwork ? bgSurface : coverTrack.coverBgA;
  const uint32_t coverBgB = useLiveArtwork ? bgSurface : coverTrack.coverBgB;
  char headerTitle[64];
  char wrappedTitle[192];
  snprintf(headerTitle, sizeof(headerTitle), "Now Playing / %s",
           (trackSource && trackSource[0]) ? trackSource : "Companion");

  const uint32_t liveToken = useLive ? g_liveNowPlaying.contentToken : 0U;
  const bool needsFullRefresh =
      force ||
      ui.lastUsingLive != useLive ||
      ui.lastInSync != displaySync ||
      (useLive ? (ui.lastLiveToken != liveToken) : (ui.lastTrackIndex != (int8_t)trackIndex));

  if (needsFullRefresh) {
    ui.lastUsingLive = useLive;
    ui.lastInSync = displaySync;
    ui.lastLiveToken = liveToken;
    ui.lastTrackIndex = useLive ? -1 : (int8_t)trackIndex;
    lv_obj_set_style_bg_color(ui.card, lv_color_hex(bgSurface), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(ui.card, lv_color_hex(bgSurface), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(ui.card, LV_GRAD_DIR_NONE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.header, lv_color_hex(headerBg), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(ui.header, lv_color_hex(headerBg), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.headerFill, lv_color_hex(primaryText), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.statusDot, lv_color_hex(displaySync ? 0x7CFF9D : 0xFFC857), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.cover, lv_color_hex(coverBgA), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(ui.cover, lv_color_hex(coverBgB), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(ui.cover, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.coverStripe, lv_color_hex(coverTrack.coverStripe), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.coverOrb, lv_color_hex(coverTrack.coverOrb), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(ui.coverOrb, lv_color_hex(coverTrack.coverOrb), LV_PART_MAIN);
    lv_obj_set_style_text_color(ui.coverTop, lv_color_hex(primaryText), 0);
    lv_obj_set_style_text_color(ui.coverBottom, lv_color_hex(primaryText), 0);
    lv_obj_set_style_text_color(ui.title, lv_color_hex(primaryText), 0);
    lv_obj_set_style_text_color(ui.status, lv_color_hex(primaryText), 0);
    lv_obj_set_style_text_color(ui.track, lv_color_hex(primaryText), 0);
    lv_obj_set_style_text_color(ui.artist, lv_color_hex(primaryText), 0);
    lv_obj_set_style_bg_color(ui.progressRail, lv_color_hex(railBg), LV_PART_MAIN);
    lv_obj_set_style_border_color(ui.progressRail, lv_color_hex(bgIsDark ? lvglDarkenRgb(railBg, 20) : lvglLightenRgb(railBg, 20)), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.progressFill, lv_color_hex(primaryText), LV_PART_MAIN);
    lv_img_set_src(ui.coverImage, lvglNowPlayingCoverImageDsc(useLiveArtwork));
    lv_obj_center(ui.coverImage);
    lv_label_set_text(ui.coverTop, coverTrack.coverTop);
    lv_label_set_text(ui.coverBottom, coverTrack.coverBottom);
    lv_label_set_text(ui.title, headerTitle);
    lv_label_set_text(ui.status, displaySync ? "IN SYNC" : "OUT OF SYNC");
    lv_obj_set_style_text_color(ui.headerTime, lv_color_hex(primaryText), 0);
    lv_obj_add_flag(ui.coverStripe, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui.coverOrb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui.coverTop, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui.coverBottom, LV_OBJ_FLAG_HIDDEN);
    if (durationRaw > 0) {
      lv_obj_clear_flag(ui.progressRail, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui.progressFill, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(ui.progressRail, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui.progressFill, LV_OBJ_FLAG_HIDDEN);
    }
  }

  lv_obj_set_style_text_font(ui.track, lvglNowPlayingTitleFont(), 0);
  lv_coord_t titleMeasureW = lv_obj_get_width(ui.track) - 24;
  if (titleMeasureW < 80) titleMeasureW = lv_obj_get_width(ui.track);
  lvglBuildWrappedTitle(wrappedTitle, sizeof(wrappedTitle), trackTitle, lvglNowPlayingTitleFont(),
                        titleMeasureW, 2);
  lv_label_set_text(ui.track, wrappedTitle);
  lv_obj_update_layout(ui.track);
  lv_obj_set_style_text_font(ui.artist, lvglNowPlayingArtistFont(), 0);
  lv_label_set_text(ui.artist, trackArtist);
  lv_obj_update_layout(ui.artist);

  const lv_coord_t trackY = lv_obj_get_y(ui.track);
  const lv_coord_t trackBottom = (lv_coord_t)(trackY + lv_obj_get_height(ui.track));
  const lv_coord_t desiredArtistY = (lv_coord_t)(trackBottom + 14);
  const lv_coord_t maxArtistY = (lv_coord_t)(lv_obj_get_height(ui.card) - lv_obj_get_height(ui.artist) - 10);
  const lv_coord_t artistY = (desiredArtistY <= maxArtistY) ? desiredArtistY : maxArtistY;
  lv_obj_set_y(ui.artist, artistY);

  // Show total duration only in header — progress bar handles position visually
  if (durationRaw > 0) {
    char timeBuf[24];
    snprintf(timeBuf, sizeof(timeBuf), "%u:%02u",
             (unsigned)(durationRaw / 60), (unsigned)(durationRaw % 60));
    lv_label_set_text(ui.headerTime, timeBuf);
  } else {
    lv_label_set_text(ui.headerTime, "");
  }

  const lv_coord_t railW = lv_obj_get_width(ui.progressRail);
  lv_coord_t fillW = (lv_coord_t)((railW * (int32_t)elapsedSec) / durationSec);
  if (fillW < 2) fillW = 2;
  if (fillW > railW) fillW = railW;
  lv_obj_set_width(ui.progressFill, fillW);
}

// ---------------------------------------------------------------------------
// M1: Sub-functions extracted from initLvglUi() for decomposition.
// Each sub-function creates one logical panel/section of the LVGL UI.
// ---------------------------------------------------------------------------

// Creates a transparent, non-scrollable LVGL page root container at (0,0).
static lv_obj_t* lvglCreatePageRoot(lv_obj_t* parent, int16_t w, int16_t h) {
  lv_obj_t* root = lv_obj_create(parent);
  lv_obj_set_size(root, w, h);
  lv_obj_set_pos(root, 0, 0);
  lv_obj_set_style_radius(root, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(root, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(root, 0, LV_PART_MAIN);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_OFF);
  return root;
}

// INFO page: stats panel with header, body text, and QR code.
static void initLvglInfoPanel(lv_obj_t* scr) {
  const int16_t cW = canvasWidth();
  const int16_t cH = canvasHeight();
  const UiThemeLvglTokens &theme = activeUiTheme().lvgl;
  const bool minimalTheme = lvglThemeIsMinimalBrutalistMono();
  const lv_coord_t kInfoRadius = minimalTheme ? 0 : 8;
  const lv_color_t kInfoBg = lv_color_hex(theme.infoBg);
  const lv_color_t kInfoHeaderBg = lv_color_hex(theme.infoHeaderBg);

  g_infoUi.root = lvglCreatePageRoot(scr, cW, cH);

  g_infoUi.card = lv_obj_create(g_infoUi.root);
  lv_obj_set_size(g_infoUi.card, cW, cH);
  lv_obj_set_pos(g_infoUi.card, 0, 0);
  lv_obj_set_style_radius(g_infoUi.card, kInfoRadius, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_infoUi.card, kInfoBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(g_infoUi.card, kInfoBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(g_infoUi.card, LV_GRAD_DIR_NONE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_infoUi.card, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_infoUi.card, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_infoUi.card, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_infoUi.card, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_infoUi.card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(g_infoUi.card, LV_SCROLLBAR_MODE_OFF);

  constexpr int16_t infoHeaderH = 30;
  g_infoUi.header = lv_obj_create(g_infoUi.card);
  lv_obj_set_size(g_infoUi.header, cW, infoHeaderH);
  lv_obj_set_pos(g_infoUi.header, 0, 0);
  lv_obj_set_style_bg_color(g_infoUi.header, kInfoHeaderBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(g_infoUi.header, kInfoHeaderBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(g_infoUi.header, LV_GRAD_DIR_NONE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_infoUi.header, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(g_infoUi.header, kInfoRadius, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_infoUi.header, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_infoUi.header, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_infoUi.header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(g_infoUi.header, LV_SCROLLBAR_MODE_OFF);
  g_infoUi.headerFill = lv_obj_create(g_infoUi.header);
  lv_obj_set_size(g_infoUi.headerFill, cW, 10);
  lv_obj_set_pos(g_infoUi.headerFill, 0, infoHeaderH - 10);
  lv_obj_set_style_bg_color(g_infoUi.headerFill, kInfoHeaderBg, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_infoUi.headerFill, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_infoUi.headerFill, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_infoUi.headerFill, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_infoUi.headerFill, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_infoUi.headerFill, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(g_infoUi.headerFill, LV_SCROLLBAR_MODE_OFF);

  g_infoUi.title = lv_label_create(g_infoUi.header);
  lv_obj_set_style_text_font(g_infoUi.title, lvglFontSmall(), 0);
  lv_obj_set_style_text_color(g_infoUi.title, lv_color_hex(theme.infoText), 0);
  lv_label_set_long_mode(g_infoUi.title, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(g_infoUi.title, cW * 3 / 5);
  lv_obj_align(g_infoUi.title, LV_ALIGN_LEFT_MID, 12, 2);
  lv_label_set_text(g_infoUi.title, "ScryBar Stats  " FW_BUILD_TAG);
  lvglForceLabelVisible(g_infoUi.title);

  g_infoUi.endpoint = lv_label_create(g_infoUi.header);
  lv_obj_set_style_text_font(g_infoUi.endpoint, lvglFontSmall(), 0);
  lv_obj_set_style_text_color(g_infoUi.endpoint, lv_color_hex(theme.infoText), 0);
  lv_label_set_long_mode(g_infoUi.endpoint, LV_LABEL_LONG_DOT);
  lv_obj_set_size(g_infoUi.endpoint, (cW / 2) - 16, 20);
  lv_obj_align(g_infoUi.endpoint, LV_ALIGN_RIGHT_MID, -10, 2);
  lv_obj_set_style_text_align(g_infoUi.endpoint, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_text(g_infoUi.endpoint, "--:8080");
  lvglForceLabelVisible(g_infoUi.endpoint);

  const int16_t infoColsY = infoHeaderH + 4;
  const int16_t infoColsH = cH - infoColsY - 4;
  const int16_t infoQrPad = 5;
  const int16_t infoQrSize = infoColsH - infoQrPad * 2;
  const int16_t infoQrAreaW = infoQrSize + infoQrPad * 2;
  const int16_t infoTextColW = cW - infoQrAreaW - 16;

  lv_obj_t *infoColLeft = lv_obj_create(g_infoUi.card);
  lv_obj_set_size(infoColLeft, infoTextColW, infoColsH);
  lv_obj_set_pos(infoColLeft, 8, infoColsY);
  lv_obj_set_style_bg_color(infoColLeft, lv_color_hex(theme.infoBg), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(infoColLeft, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(infoColLeft, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(infoColLeft, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(infoColLeft, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(infoColLeft, 0, LV_PART_MAIN);
  lv_obj_clear_flag(infoColLeft, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(infoColLeft, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *infoColRight = lv_obj_create(g_infoUi.card);
  lv_obj_set_size(infoColRight, infoQrAreaW, infoColsH);
  lv_obj_set_pos(infoColRight, cW - infoQrAreaW - 8, infoColsY);
  lv_obj_set_style_bg_color(infoColRight, lv_color_hex(theme.infoBg), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(infoColRight, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(infoColRight, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(infoColRight, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(infoColRight, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(infoColRight, 0, LV_PART_MAIN);
  lv_obj_clear_flag(infoColRight, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(infoColRight, LV_SCROLLBAR_MODE_OFF);

  g_infoUi.bodyLeft = lv_label_create(infoColLeft);
  lv_obj_set_style_text_font(g_infoUi.bodyLeft, lvglFontInfoBody(), 0);
  lv_obj_set_style_text_color(g_infoUi.bodyLeft, lv_color_hex(theme.infoText), 0);
  lv_obj_set_style_text_line_space(g_infoUi.bodyLeft, 1, 0);
  lv_label_set_recolor(g_infoUi.bodyLeft, true);
  lv_label_set_long_mode(g_infoUi.bodyLeft, LV_LABEL_LONG_WRAP);
  lv_obj_set_size(g_infoUi.bodyLeft, infoTextColW - 8, infoColsH - 8);
  lv_obj_set_pos(g_infoUi.bodyLeft, 4, 4);
  lv_obj_set_style_text_align(g_infoUi.bodyLeft, LV_TEXT_ALIGN_LEFT, 0);
  lv_label_set_text(g_infoUi.bodyLeft, "...");
  lvglForceLabelVisible(g_infoUi.bodyLeft);

  // Right column is QR-only; keep the body label as a hidden placeholder
  g_infoUi.bodyRight = lv_label_create(infoColRight);
  lv_obj_add_flag(g_infoUi.bodyRight, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(g_infoUi.bodyRight, "");

#if defined(LV_USE_QRCODE) && LV_USE_QRCODE
  // QR: centred in the right column
  const lv_color_t infoQrDark = lv_color_hex(theme.infoQrDark);
  const lv_color_t infoQrLight = lv_color_hex(theme.infoQrLight);
  g_infoUi.webQr = lv_qrcode_create(infoColRight, infoQrSize, infoQrDark, infoQrLight);
  lv_obj_align(g_infoUi.webQr, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(g_infoUi.webQr, infoQrLight, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_infoUi.webQr, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_infoUi.webQr, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_infoUi.webQr, lv_color_hex(theme.infoHeaderBorder), LV_PART_MAIN);
  lv_obj_set_style_border_opa(g_infoUi.webQr, LV_OPA_80, LV_PART_MAIN);
  lv_qrcode_update(g_infoUi.webQr, "http://--:8080", strlen("http://--:8080"));
#endif
}

// HOME page — clock block: header, date, WiFi bars, word clock labels, divider.
static void initLvglClockPanel(lv_obj_t* homeRoot) {
  const int16_t cW = canvasWidth();
  const int16_t cH = canvasHeight();
  const UiThemeLvglTokens &theme = activeUiTheme().lvgl;
  const bool minimalTheme = lvglThemeIsMinimalBrutalistMono();
  const lv_coord_t kCardRadius = minimalTheme ? 0 : 10;
  const lv_coord_t kWifiBarRadius = minimalTheme ? 0 : 1;
  const int16_t weatherW = (DISPLAY_WEATHER_PANEL_W > (cW / 2)) ? (cW / 3) : DISPLAY_WEATHER_PANEL_W;
  const int16_t leftW = cW - weatherW - 10;
  const int16_t clockBlockW = leftW;
  const int16_t clockBlockH = cH;
  const int16_t clockHeaderH = 30;
  const int16_t innerPad = 18;
  const lv_color_t kPanelBg = lv_color_hex(lvglResolvedPanelBg(theme));
  const lv_color_t kHeaderBlue = lv_color_hex(lvglResolvedHeaderBg(theme));

  lv_obj_t *left = lv_obj_create(homeRoot);
  lv_obj_set_size(left, leftW, cH);
  lv_obj_set_pos(left, 0, 0);
  lv_obj_set_style_radius(left, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(left, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(left, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(left, 0, LV_PART_MAIN);
  lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);

  g_clockUi.block = lv_obj_create(left);
  lv_obj_set_size(g_clockUi.block, clockBlockW, clockBlockH);
  lv_obj_set_pos(g_clockUi.block, 0, 0);
  lv_obj_set_style_bg_color(g_clockUi.block, kPanelBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(g_clockUi.block, kPanelBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(g_clockUi.block, LV_GRAD_DIR_NONE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_clockUi.block, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_clockUi.block, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_clockUi.block, kCardRadius, LV_PART_MAIN);
  lv_obj_set_style_clip_corner(g_clockUi.block, false, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_clockUi.block, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_clockUi.block, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_clockUi.block, LV_OBJ_FLAG_SCROLLABLE);

  g_clockUi.header = lv_obj_create(g_clockUi.block);
  lv_obj_set_size(g_clockUi.header, clockBlockW, clockHeaderH);
  lv_obj_set_pos(g_clockUi.header, 0, 0);
  lv_obj_set_style_bg_color(g_clockUi.header, kHeaderBlue, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(g_clockUi.header, kHeaderBlue, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(g_clockUi.header, LV_GRAD_DIR_NONE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_clockUi.header, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(g_clockUi.header, kCardRadius, LV_PART_MAIN);
  lv_obj_set_style_clip_corner(g_clockUi.header, false, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_clockUi.header, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_clockUi.header, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_clockUi.header, LV_OBJ_FLAG_SCROLLABLE);

  g_clockUi.headerFill = lv_obj_create(g_clockUi.header);
  lv_obj_set_size(g_clockUi.headerFill, clockBlockW, 10);
  lv_obj_set_pos(g_clockUi.headerFill, 0, clockHeaderH - 10);
  lv_obj_set_style_bg_color(g_clockUi.headerFill, kHeaderBlue, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_clockUi.headerFill, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_clockUi.headerFill, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_clockUi.headerFill, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_clockUi.headerFill, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_clockUi.headerFill, LV_OBJ_FLAG_SCROLLABLE);

  constexpr int16_t kWifiBarW = 4;
  constexpr int16_t kWifiBarGap = 3;
  constexpr int16_t kWifiBarCount = 4;
  const int16_t clockWiFiTotalW = (kWifiBarW * kWifiBarCount) + (kWifiBarGap * (kWifiBarCount - 1));
  const int16_t clockWiFiStartX = clockBlockW - 12 - clockWiFiTotalW;
  const int16_t clockWiFiBaseY = clockHeaderH - 6;
  const int16_t clockWiFiHeights[4] = {5, 8, 11, 14};
  for (uint8_t i = 0; i < 4; ++i) {
    g_clockUi.wifiBars[i] = lv_obj_create(g_clockUi.header);
    lv_obj_set_size(g_clockUi.wifiBars[i], kWifiBarW, clockWiFiHeights[i]);
    lv_obj_set_pos(g_clockUi.wifiBars[i],
                   clockWiFiStartX + (int16_t)i * (kWifiBarW + kWifiBarGap),
                   clockWiFiBaseY - clockWiFiHeights[i]);
    lv_obj_set_style_bg_color(g_clockUi.wifiBars[i], lv_color_hex(theme.wifiBarOff), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_clockUi.wifiBars[i], (lv_opa_t)190, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_clockUi.wifiBars[i], 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(g_clockUi.wifiBars[i], 0, LV_PART_MAIN);
    lv_obj_set_style_radius(g_clockUi.wifiBars[i], kWifiBarRadius, LV_PART_MAIN);
    lv_obj_clear_flag(g_clockUi.wifiBars[i], LV_OBJ_FLAG_SCROLLABLE);
  }

  g_clockUi.date = lv_label_create(g_clockUi.header);
  lv_obj_set_style_text_font(g_clockUi.date, lvglFontSmall(), 0);
  lv_obj_set_style_text_color(g_clockUi.date, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(g_clockUi.date, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_label_set_long_mode(g_clockUi.date, LV_LABEL_LONG_DOT);
  const int16_t clockDateW = (clockWiFiStartX > 28) ? (clockWiFiStartX - 20) : (clockBlockW - 24);
  lv_obj_set_width(g_clockUi.date, clockDateW);
  lv_obj_align(g_clockUi.date, LV_ALIGN_LEFT_MID, 12, 2);
  lv_label_set_text(g_clockUi.date, "...");
  lvglForceLabelVisible(g_clockUi.date);

  g_clockUi.l1 = lv_label_create(g_clockUi.block);
  g_clockUi.l2 = lv_label_create(g_clockUi.block);
  g_clockUi.l3 = lv_label_create(g_clockUi.block);
  lv_obj_set_style_text_font(g_clockUi.l1, lvglFontClock(), 0);
  lv_obj_set_style_text_font(g_clockUi.l2, lvglFontTitle(), 0);
  lv_obj_set_style_text_font(g_clockUi.l3, lvglFontTitle(), 0);
  {
    const auto &th = activeUiTheme().lvgl;
    lv_obj_set_style_text_color(g_clockUi.l1, lv_color_hex(th.auxText), 0);
    lv_obj_set_style_text_color(g_clockUi.l2, lv_color_hex(th.auxMeta), 0);
    lv_obj_set_style_text_color(g_clockUi.l3, lv_color_hex(th.auxMeta), 0);
  }
  lv_obj_set_style_text_line_space(g_clockUi.l1, 4, 0);
  lv_obj_set_style_text_line_space(g_clockUi.l2, 2, 0);
  lv_obj_set_style_text_line_space(g_clockUi.l3, 2, 0);
  lv_obj_set_style_text_align(g_clockUi.l1, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_bg_opa(g_clockUi.l1, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_clockUi.l1, 0, LV_PART_MAIN);
  lv_label_set_long_mode(g_clockUi.l1, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(g_clockUi.l1, clockBlockW - 16);
  lv_obj_set_pos(g_clockUi.l1, 8, 38);
  lvglApplyClockSentenceAutoFit("Clock...");
  lvglForceLabelVisible(g_clockUi.l1);
  lv_obj_add_flag(g_clockUi.l2, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(g_clockUi.l3, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(g_clockUi.l2, "");
  lv_label_set_text(g_clockUi.l3, "");

  g_clockUi.divider = lv_obj_create(g_clockUi.block);
  lv_obj_set_size(g_clockUi.divider, clockBlockW - (innerPad * 2), 1);
  lv_obj_set_pos(g_clockUi.divider, innerPad, cH - 58);
  lv_obj_set_style_bg_color(g_clockUi.divider, lv_color_hex(0x9FB5EE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_clockUi.divider, LV_OPA_0, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_clockUi.divider, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_clockUi.divider, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_clockUi.divider, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_clockUi.divider, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(g_clockUi.divider, LV_OBJ_FLAG_HIDDEN);
}

// HOME page — weather body: temp, icon, glyph, separator, description,
// humidity, wind, forecast bar, forecast labels.
static void initLvglWeatherBodyWidgets() {
  const int16_t cW = canvasWidth();
  const int16_t cH = canvasHeight();
  const UiThemeLvglTokens &theme = activeUiTheme().lvgl;
  const bool minimalTheme = lvglThemeIsMinimalBrutalistMono();
  const lv_coord_t kCardRadius = minimalTheme ? 0 : 10;
  const int16_t weatherW = (DISPLAY_WEATHER_PANEL_W > (cW / 2)) ? (cW / 3) : DISPLAY_WEATHER_PANEL_W;
  const int16_t weatherHeaderH = 30;
  const int16_t weatherCardW = weatherW;
  const int16_t weatherCardH = cH;
  const int16_t weatherBodyH = weatherCardH - weatherHeaderH;
  const int16_t weatherIconW = 60;
  const int16_t weatherTextW = weatherCardW - 24;
  const int16_t weatherTopTextW = weatherCardW - (weatherIconW + 48);
  const uint32_t weatherBgHex = lvglResolvedWeatherBg(theme);
  const uint32_t weatherTextPrimaryHex = lvglResolvedWeatherPrimary(theme, weatherBgHex);
  const uint32_t weatherTextSecondaryHex = lvglResolvedWeatherSecondary(theme, weatherBgHex, weatherTextPrimaryHex);
  const uint32_t weatherForecastTextHex = lvglResolvedForecastText(theme, weatherBgHex, weatherTextPrimaryHex);
  const uint32_t weatherGlyphOnlineHex = lvglResolvedWeatherGlyphOnline(theme, weatherBgHex, weatherTextPrimaryHex);
  const lv_color_t kWeatherCardBg = lv_color_hex(weatherBgHex);
  const lv_color_t kWeatherTextDark = lv_color_hex(weatherTextPrimaryHex);
  const lv_color_t kWeatherTextMid = lv_color_hex(weatherTextSecondaryHex);
  const lv_color_t kWeatherForecastText = lv_color_hex(weatherForecastTextHex);
  const lv_color_t kWeatherGlyphOnline = lv_color_hex(weatherGlyphOnlineHex);

  g_weatherUi.temp = lv_label_create(g_weatherUi.body);
  lv_obj_set_style_text_font(g_weatherUi.temp, lvglFontTemp(), 0);
  lv_obj_set_style_text_color(g_weatherUi.temp, kWeatherTextDark, 0);
  lv_obj_set_width(g_weatherUi.temp, weatherTopTextW);
  lv_obj_align(g_weatherUi.temp, LV_ALIGN_TOP_LEFT, 12, 8);
  lv_label_set_text(g_weatherUi.temp, "--\xC2\xB0, --%");
  lvglForceLabelVisible(g_weatherUi.temp);

  g_weatherUi.icon = lv_img_create(g_weatherUi.body);
  constexpr uint16_t kMainIconZoom = 336;  // ~63px rendered from 48px source
  const int16_t mainIconPx = (int16_t)(((int32_t)weatherIconW * kMainIconZoom + 128) / 256);
  const int16_t mainIconY = ((weatherBodyH - mainIconPx) / 2) + 2;  // lower icon a bit for vertical centering
  lv_obj_align(g_weatherUi.icon, LV_ALIGN_TOP_RIGHT, -21, mainIconY);     // move icon 7px right
  const lv_img_dsc_t *bootIcon = weatherImageFromCode(2, true);
  if (bootIcon) {
    lv_img_set_src(g_weatherUi.icon, bootIcon);
    lv_obj_clear_flag(g_weatherUi.icon, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(g_weatherUi.icon, LV_OBJ_FLAG_HIDDEN);
  }
  lv_img_set_zoom(g_weatherUi.icon, kMainIconZoom);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, g_weatherUi.icon);
  lv_anim_set_values(&a, -2, 2);
  lv_anim_set_time(&a, 2600);
  lv_anim_set_playback_time(&a, 2600);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_exec_cb(&a, lvglIconFloatAnimCb);
  lv_anim_start(&a);

  g_weatherUi.glyph = lv_label_create(g_weatherUi.body);
  lv_obj_set_style_text_font(g_weatherUi.glyph, lvglFontBig(), 0);
  lv_obj_set_style_text_color(g_weatherUi.glyph, kWeatherGlyphOnline, 0);
  lv_obj_align(g_weatherUi.glyph, LV_ALIGN_TOP_LEFT, 12, 4);
  lv_label_set_text(g_weatherUi.glyph, "*");
  lv_obj_add_flag(g_weatherUi.glyph, LV_OBJ_FLAG_HIDDEN);

  g_weatherUi.sep = lv_obj_create(g_weatherUi.body);
  const int16_t weatherSepW = weatherTopTextW - 26;  // leave extra space near large icon
  lv_obj_set_size(g_weatherUi.sep, weatherSepW, 1);
  lv_obj_set_pos(g_weatherUi.sep, 12, 44);
  lv_obj_set_style_bg_color(g_weatherUi.sep, kWeatherTextMid, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_weatherUi.sep, LV_OPA_70, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_weatherUi.sep, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_weatherUi.sep, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_weatherUi.sep, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_weatherUi.sep, LV_OBJ_FLAG_SCROLLABLE);

  g_weatherUi.desc = lv_label_create(g_weatherUi.body);
  lv_obj_set_style_text_font(g_weatherUi.desc, lvglFontMeta(), 0);
  lv_obj_set_style_text_color(g_weatherUi.desc, kWeatherTextDark, 0);
  lv_obj_set_style_anim_speed(g_weatherUi.desc, 28, 0);
  lv_label_set_long_mode(g_weatherUi.desc, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(g_weatherUi.desc, weatherTopTextW - 10);
  lv_obj_align(g_weatherUi.desc, LV_ALIGN_TOP_LEFT, 12, 50);
  lv_label_set_text(g_weatherUi.desc, "Meteo in arrivo");
  lvglForceLabelVisible(g_weatherUi.desc);

  g_weatherUi.humidity = lv_label_create(g_weatherUi.body);
  lv_obj_set_style_text_font(g_weatherUi.humidity, lvglFontMini(), 0);
  lv_obj_set_style_text_color(g_weatherUi.humidity, kWeatherTextDark, 0);
  lv_obj_set_width(g_weatherUi.humidity, weatherTopTextW);
  lv_obj_align(g_weatherUi.humidity, LV_ALIGN_TOP_LEFT, 12, 77);
  lv_label_set_text(g_weatherUi.humidity, activeUiStrings()->windNa);
  lvglForceLabelVisible(g_weatherUi.humidity);

  g_weatherUi.wind = lv_label_create(g_weatherUi.body);
  lv_obj_set_style_text_font(g_weatherUi.wind, lvglFontTiny(), 0);
  lv_obj_set_style_text_color(g_weatherUi.wind, kWeatherTextMid, 0);
  lv_obj_set_width(g_weatherUi.wind, weatherTextW);
  lv_obj_align(g_weatherUi.wind, LV_ALIGN_TOP_LEFT, 12, 90);
  lv_label_set_text(g_weatherUi.wind, "");
  lv_obj_add_flag(g_weatherUi.wind, LV_OBJ_FLAG_HIDDEN);

  constexpr int16_t forecastBarH = 34;
  const int16_t forecastBarY = weatherCardH - forecastBarH;
  g_weatherUi.forecastBar = lv_obj_create(g_weatherUi.card);
  lv_obj_set_size(g_weatherUi.forecastBar, weatherCardW, forecastBarH);
  lv_obj_set_pos(g_weatherUi.forecastBar, 0, forecastBarY);
  lv_obj_set_style_bg_color(g_weatherUi.forecastBar, kWeatherCardBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(g_weatherUi.forecastBar, kWeatherCardBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(g_weatherUi.forecastBar, LV_GRAD_DIR_NONE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_weatherUi.forecastBar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(g_weatherUi.forecastBar, kCardRadius, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_weatherUi.forecastBar, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_weatherUi.forecastBar, 0, LV_PART_MAIN);
  lv_obj_set_style_clip_corner(g_weatherUi.forecastBar, false, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_weatherUi.forecastBar, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_weatherUi.forecastBar, LV_OBJ_FLAG_SCROLLABLE);
  g_weatherUi.forecastBarFill = lv_obj_create(g_weatherUi.forecastBar);
  lv_obj_set_size(g_weatherUi.forecastBarFill, weatherCardW, 10);
  lv_obj_set_pos(g_weatherUi.forecastBarFill, 0, 0);
  lv_obj_set_style_bg_color(g_weatherUi.forecastBarFill, kWeatherCardBg, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_weatherUi.forecastBarFill, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_weatherUi.forecastBarFill, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_weatherUi.forecastBarFill, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_weatherUi.forecastBarFill, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_weatherUi.forecastBarFill, LV_OBJ_FLAG_SCROLLABLE);

  g_weatherUi.forecastIcon = lv_img_create(g_weatherUi.forecastBar);
  lv_obj_set_pos(g_weatherUi.forecastIcon, 5, -11);  // keep center while growing icon (+6px)
  if (bootIcon) {
    lv_img_set_src(g_weatherUi.forecastIcon, bootIcon);
    lv_obj_clear_flag(g_weatherUi.forecastIcon, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(g_weatherUi.forecastIcon, LV_OBJ_FLAG_HIDDEN);
  }
  lv_img_set_zoom(g_weatherUi.forecastIcon, 123);  // ~23px rendered from 48px source

  g_weatherUi.forecastNow = lv_label_create(g_weatherUi.forecastBar);
  lv_obj_set_style_text_font(g_weatherUi.forecastNow, lvglFontSmall(), 0);
  lv_obj_set_style_text_color(g_weatherUi.forecastNow, kWeatherForecastText, 0);
  lv_obj_set_width(g_weatherUi.forecastNow, weatherCardW - 65);
  lv_obj_set_pos(g_weatherUi.forecastNow, 52, 6);
  lv_label_set_long_mode(g_weatherUi.forecastNow, LV_LABEL_LONG_DOT);
  lv_label_set_text(g_weatherUi.forecastNow, activeUiStrings()->forecastNa);
  lvglForceLabelVisible(g_weatherUi.forecastNow);

  g_weatherUi.forecastTomorrow = lv_label_create(g_weatherUi.body);
  lv_obj_set_style_text_font(g_weatherUi.forecastTomorrow, lvglFontTiny(), 0);
  lv_obj_set_style_text_color(g_weatherUi.forecastTomorrow, kWeatherTextMid, 0);
  lv_label_set_long_mode(g_weatherUi.forecastTomorrow, LV_LABEL_LONG_DOT);
  lv_obj_set_width(g_weatherUi.forecastTomorrow, weatherTextW);
  lv_obj_align(g_weatherUi.forecastTomorrow, LV_ALIGN_TOP_LEFT, 12, 102);
  lv_label_set_text(g_weatherUi.forecastTomorrow, "");
  lv_obj_add_flag(g_weatherUi.forecastTomorrow, LV_OBJ_FLAG_HIDDEN);
}

// HOME page — weather card: header, current conditions, icon, forecast bar.
static void initLvglWeatherPanel(lv_obj_t* homeRoot) {
  const int16_t cW = canvasWidth();
  const int16_t cH = canvasHeight();
  const UiThemeLvglTokens &theme = activeUiTheme().lvgl;
  const bool minimalTheme = lvglThemeIsMinimalBrutalistMono();
  const lv_coord_t kCardRadius = minimalTheme ? 0 : 10;
  const int16_t weatherW = (DISPLAY_WEATHER_PANEL_W > (cW / 2)) ? (cW / 3) : DISPLAY_WEATHER_PANEL_W;
  const int16_t cardsGapX = 10;
  const int16_t leftW = cW - weatherW - cardsGapX;
  const int16_t outerPadX = 0;
  const int16_t outerPadY = 0;
  const int16_t weatherHeaderH = 30;
  const int16_t weatherCardW = weatherW - (outerPadX * 2);
  const int16_t weatherCardH = cH - (outerPadY * 2);
  const int16_t weatherBodyH = weatherCardH - weatherHeaderH;
  const int16_t weatherIconW = 60;
  const int16_t weatherTextW = weatherCardW - 24;
  const int16_t weatherTopTextW = weatherCardW - (weatherIconW + 48);
  const uint32_t weatherBgHex = lvglResolvedWeatherBg(theme);
  const uint32_t weatherTextPrimaryHex = lvglResolvedWeatherPrimary(theme, weatherBgHex);
  const uint32_t weatherTextSecondaryHex = lvglResolvedWeatherSecondary(theme, weatherBgHex, weatherTextPrimaryHex);
  const uint32_t weatherForecastTextHex = lvglResolvedForecastText(theme, weatherBgHex, weatherTextPrimaryHex);
  const uint32_t weatherGlyphOnlineHex = lvglResolvedWeatherGlyphOnline(theme, weatherBgHex, weatherTextPrimaryHex);
  const lv_color_t kHeaderBlue = lv_color_hex(lvglResolvedHeaderBg(theme));
  const lv_color_t kWeatherCardBg = lv_color_hex(weatherBgHex);
  const lv_color_t kWeatherTextDark = lv_color_hex(weatherTextPrimaryHex);
  const lv_color_t kWeatherTextMid = lv_color_hex(weatherTextSecondaryHex);
  const lv_color_t kWeatherForecastText = lv_color_hex(weatherForecastTextHex);
  const lv_color_t kWeatherGlyphOnline = lv_color_hex(weatherGlyphOnlineHex);

  lv_obj_t *right = lv_obj_create(homeRoot);
  lv_obj_set_size(right, weatherW, cH);
  lv_obj_set_pos(right, leftW + cardsGapX, 0);
  lv_obj_set_style_radius(right, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(right, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(right, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(right, 0, LV_PART_MAIN);
  lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);

  g_weatherUi.card = lv_obj_create(right);
  lv_obj_set_size(g_weatherUi.card, weatherCardW, weatherCardH);
  lv_obj_set_pos(g_weatherUi.card, outerPadX, outerPadY);
  lv_obj_set_style_radius(g_weatherUi.card, kCardRadius, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_weatherUi.card, kWeatherCardBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(g_weatherUi.card, kWeatherCardBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(g_weatherUi.card, LV_GRAD_DIR_NONE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_weatherUi.card, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_weatherUi.card, 0, LV_PART_MAIN);
  lv_obj_set_style_clip_corner(g_weatherUi.card, false, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_weatherUi.card, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_weatherUi.card, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_weatherUi.card, LV_OBJ_FLAG_SCROLLABLE);

  g_weatherUi.header = lv_obj_create(g_weatherUi.card);
  lv_obj_set_size(g_weatherUi.header, weatherCardW, weatherHeaderH);
  lv_obj_set_pos(g_weatherUi.header, 0, 0);
  lv_obj_set_style_bg_color(g_weatherUi.header, kHeaderBlue, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(g_weatherUi.header, kHeaderBlue, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(g_weatherUi.header, LV_GRAD_DIR_NONE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_weatherUi.header, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(g_weatherUi.header, kCardRadius, LV_PART_MAIN);
  lv_obj_set_style_clip_corner(g_weatherUi.header, false, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_weatherUi.header, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_weatherUi.header, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_weatherUi.header, LV_OBJ_FLAG_SCROLLABLE);
  g_weatherUi.headerFill = lv_obj_create(g_weatherUi.header);
  lv_obj_set_size(g_weatherUi.headerFill, weatherCardW, 10);
  lv_obj_set_pos(g_weatherUi.headerFill, 0, weatherHeaderH - 10);
  lv_obj_set_style_bg_color(g_weatherUi.headerFill, kHeaderBlue, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_weatherUi.headerFill, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_weatherUi.headerFill, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_weatherUi.headerFill, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_weatherUi.headerFill, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_weatherUi.headerFill, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *headerDivider = lv_obj_create(g_weatherUi.card);
  lv_obj_set_size(headerDivider, weatherCardW - 16, 2);
  lv_obj_set_pos(headerDivider, 8, weatherHeaderH - 1);
  lv_obj_set_style_bg_color(headerDivider, lv_color_hex(0x90A3DE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(headerDivider, LV_OPA_0, LV_PART_MAIN);
  lv_obj_set_style_border_width(headerDivider, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(headerDivider, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(headerDivider, 0, LV_PART_MAIN);
  lv_obj_clear_flag(headerDivider, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(headerDivider, LV_OBJ_FLAG_HIDDEN);

  g_weatherUi.body = lv_obj_create(g_weatherUi.card);
  lv_obj_set_size(g_weatherUi.body, weatherCardW, weatherCardH - weatherHeaderH);
  lv_obj_set_pos(g_weatherUi.body, 0, weatherHeaderH);
  lv_obj_set_style_bg_opa(g_weatherUi.body, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_radius(g_weatherUi.body, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_weatherUi.body, 0, LV_PART_MAIN);
  lv_obj_set_style_clip_corner(g_weatherUi.body, false, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_weatherUi.body, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_left(g_weatherUi.body, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_right(g_weatherUi.body, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_top(g_weatherUi.body, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(g_weatherUi.body, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_weatherUi.body, LV_OBJ_FLAG_SCROLLABLE);

  g_weatherUi.city = lv_label_create(g_weatherUi.header);
  lv_obj_set_style_text_font(g_weatherUi.city, lvglFontSmall(), 0);
  lv_obj_set_style_text_color(g_weatherUi.city, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_width(g_weatherUi.city, weatherCardW - 112);
  lv_label_set_long_mode(g_weatherUi.city, LV_LABEL_LONG_DOT);
  lv_obj_align(g_weatherUi.city, LV_ALIGN_LEFT_MID, 12, 2);
  lv_label_set_text(g_weatherUi.city, "Luino");
  lvglForceLabelVisible(g_weatherUi.city);

  g_weatherUi.sun = lv_label_create(g_weatherUi.header);
  lv_obj_set_style_text_font(g_weatherUi.sun, lvglFontSmall(), 0);
  lv_obj_set_style_text_color(g_weatherUi.sun, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_opa(g_weatherUi.sun, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_align(g_weatherUi.sun, LV_ALIGN_RIGHT_MID, -10, 2);
  lv_label_set_text(g_weatherUi.sun, "--:-- | --:--");
  lvglForceLabelVisible(g_weatherUi.sun);

  initLvglWeatherBodyWidgets();
}

// Screensaver: sky, stars, cow, balloon, field, footer.
#if SCREENSAVER_ENABLED
static void initLvglScreensaverUi(lv_obj_t* scr) {
  const int16_t cW = canvasWidth();
  const int16_t cH = canvasHeight();
  const UiThemeLvglTokens &theme = activeUiTheme().lvgl;
  const uint32_t saverReadableText = lvglResolvedSaverReadableText(theme);

  g_saver.root = lv_obj_create(scr);
  lv_obj_set_size(g_saver.root, cW, cH);
  lv_obj_set_pos(g_saver.root, 0, 0);
  lv_obj_set_style_radius(g_saver.root, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_saver.root, lv_color_hex(theme.screenBg), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_saver.root, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_saver.root, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_saver.root, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_saver.root, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_saver.root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(g_saver.root, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_flag(g_saver.root, LV_OBJ_FLAG_HIDDEN);

  g_saver.sky = lv_label_create(g_saver.root);
  lv_obj_set_style_text_font(g_saver.sky, lvglFontMono(), 0);
  lv_obj_set_style_text_color(g_saver.sky, lv_color_hex(theme.saverSky), 0);
  lv_label_set_long_mode(g_saver.sky, LV_LABEL_LONG_WRAP);
  lv_obj_set_size(g_saver.sky, cW - 8, cH - 68);
  lv_obj_set_pos(g_saver.sky, 4, 4);
  lv_label_set_text(g_saver.sky, "");
  lvglForceLabelVisible(g_saver.sky);
  lv_obj_add_flag(g_saver.sky, LV_OBJ_FLAG_HIDDEN);

  for (uint8_t r = 0; r < kSaverSkyRowsMax; ++r) {
    for (uint8_t s = 0; s < kSaverStarsPerRow; ++s) {
      g_saver.starObj[r][s] = lv_label_create(g_saver.root);
      lv_obj_set_style_text_font(g_saver.starObj[r][s], lvglFontMonoTiny(), 0);
      lv_obj_set_style_text_color(g_saver.starObj[r][s], lv_color_hex(theme.saverStarLow), 0);
      lv_label_set_text(g_saver.starObj[r][s], ".");
      lv_obj_set_pos(g_saver.starObj[r][s], 8, 8);
      lv_obj_add_flag(g_saver.starObj[r][s], LV_OBJ_FLAG_HIDDEN);
      lvglForceLabelVisible(g_saver.starObj[r][s]);
    }
  }

  g_saver.field = lv_label_create(g_saver.root);
  lv_obj_set_style_text_font(g_saver.field, lvglFontMonoTiny(), 0);
  lv_obj_set_style_text_color(g_saver.field, lv_color_hex(theme.saverField), 0);
  lv_label_set_long_mode(g_saver.field, LV_LABEL_LONG_WRAP);
  lv_obj_set_size(g_saver.field, cW - 8, 12);
  lv_obj_set_pos(g_saver.field, 4, cH - 24);
  lv_label_set_text(g_saver.field, "");
  lvglForceLabelVisible(g_saver.field);

  g_saver.cow = lv_label_create(g_saver.root);
  lv_obj_set_style_text_font(g_saver.cow, lvglFontMonoTiny(), 0);
  lv_obj_set_style_text_color(g_saver.cow, lv_color_hex(theme.saverCow), 0);
  lv_obj_set_style_text_letter_space(g_saver.cow, 0, 0);
  lv_obj_set_style_text_line_space(g_saver.cow, 0, 0);
  lvglScreenSaverSetCowArt(1);
  lv_obj_set_pos(g_saver.cow, 20, cH - 90);
  lvglForceLabelVisible(g_saver.cow);

  g_saver.balloon = lv_label_create(g_saver.root);
  lv_obj_set_style_text_font(g_saver.balloon, lvglFontScreenSaverBalloonText(), 0);
  lv_obj_set_style_text_color(g_saver.balloon, lv_color_hex(saverReadableText), 0);
  lv_obj_set_style_bg_opa(g_saver.balloon, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_saver.balloon, 0, 0);
  lv_obj_set_style_pad_hor(g_saver.balloon, 0, 0);
  lv_obj_set_style_pad_ver(g_saver.balloon, 0, 0);
  lv_obj_set_style_text_letter_space(g_saver.balloon, 0, 0);
  lv_obj_set_style_text_line_space(g_saver.balloon, 0, 0);
  lv_label_set_long_mode(g_saver.balloon, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(g_saver.balloon, (cW * 56) / 100, LV_SIZE_CONTENT);
  lv_obj_set_pos(g_saver.balloon, 166, cH - 126);
  lv_label_set_text(g_saver.balloon, "");
  lvglForceLabelVisible(g_saver.balloon);
  lv_obj_add_flag(g_saver.balloon, LV_OBJ_FLAG_HIDDEN);

  g_saver.balloonTail = lv_label_create(g_saver.root);
  lv_obj_set_style_text_font(g_saver.balloonTail, lvglFontScreenSaverTail(), 0);
  lv_obj_set_style_text_color(g_saver.balloonTail, lv_color_hex(saverReadableText), 0);
  lv_label_set_text(g_saver.balloonTail, "- - - - -");
  lv_obj_set_pos(g_saver.balloonTail, 200, cH - 64);
  lvglForceLabelVisible(g_saver.balloonTail);
  lv_obj_add_flag(g_saver.balloonTail, LV_OBJ_FLAG_HIDDEN);

  g_saver.footer = lv_label_create(g_saver.root);
  lv_obj_set_style_text_font(g_saver.footer, lvglFontScreenSaverFooterText(), 0);
  lv_obj_set_style_text_color(g_saver.footer, lv_color_hex(saverReadableText), 0);
  lv_label_set_text(g_saver.footer, "--:--  --/--");
  lv_obj_align(g_saver.footer, LV_ALIGN_BOTTOM_RIGHT, -10, -4);
  lvglForceLabelVisible(g_saver.footer);
}
#endif

// ---------------------------------------------------------------------------
// initLvglUi — orchestrator (M1: reduced from 714 to ~75 lines)
// ---------------------------------------------------------------------------
static bool initLvglUi() {
  if (g_lvglReady) return true;
  if (!initDisplay()) return false;

  lv_init();
  const int16_t cW = canvasWidth();
  const int16_t cH = canvasHeight();
  const uint32_t bufPx = (uint32_t)cW * (uint32_t)cH;

  g_lvglBuf1 = (lv_color_t*)heap_caps_malloc(bufPx * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!g_lvglBuf1) g_lvglBuf1 = (lv_color_t*)malloc(bufPx * sizeof(lv_color_t));
  if (!g_lvglBuf1) {
    Serial.println("[LVGL][ERR] alloc draw buffer fallita");
    return false;
  }

  // Phase 2: second buffer enables render/flush overlap (double-buffering)
  g_lvglBuf2 = (lv_color_t*)heap_caps_malloc(bufPx * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (g_lvglBuf2) {
    Serial.printf("[LVGL] double-buffer enabled (%u + %u KB PSRAM)\n",
                  (unsigned)(bufPx * sizeof(lv_color_t) / 1024),
                  (unsigned)(bufPx * sizeof(lv_color_t) / 1024));
  } else {
    Serial.println("[LVGL][WARN] second buffer alloc failed — single-buffer fallback");
  }

  lv_disp_draw_buf_init(&g_lvglDrawBuf, g_lvglBuf1, g_lvglBuf2, bufPx);  // g_lvglBuf2 may be nullptr
  lv_disp_drv_init(&g_lvglDispDrv);
  g_lvglDispDrv.hor_res = cW;
  g_lvglDispDrv.ver_res = cH;
  g_lvglDispDrv.flush_cb = lvglDisplayFlushCb;
  g_lvglDispDrv.draw_buf = &g_lvglDrawBuf;
  g_lvglDispDrv.full_refresh = 0;
  lv_disp_t *disp = lv_disp_drv_register(&g_lvglDispDrv);

  lv_disp_set_theme(disp, nullptr);
  const UiThemeLvglTokens &theme = activeUiTheme().lvgl;

  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(theme.screenBg), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(scr, lv_color_hex(theme.screenBg), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_NONE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

  // INFO page
  initLvglInfoPanel(scr);

  // HOME page (clock + weather)
  g_lvglHomeRoot = lvglCreatePageRoot(scr, cW, cH);
  initLvglClockPanel(g_lvglHomeRoot);
  initLvglWeatherPanel(g_lvglHomeRoot);

  // AUX (RSS) + WIKI feed decks
  g_lvglAuxRoot = lvglCreatePageRoot(scr, cW, cH);
  g_lvglWikiRoot = lvglCreatePageRoot(scr, cW, cH);
  lv_obj_set_pos(g_lvglWikiRoot, cW, 0);
  lvglInitFeedDeck(g_wikiDeck, g_lvglWikiRoot, true);
  lvglInitFeedDeck(g_auxDeck, g_lvglAuxRoot, false);

  // Now Playing
  g_lvglNowPlayingRoot = lvglCreatePageRoot(scr, cW, cH);
  lv_obj_set_pos(g_lvglNowPlayingRoot, cW, 0);
  lvglInitNowPlayingUi(g_nowPlayingUi, g_lvglNowPlayingRoot);

  // DOOM
  g_lvglDoomRoot = lvglCreatePageRoot(scr, cW, cH);
  lv_obj_set_style_bg_color(g_lvglDoomRoot, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglDoomRoot, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_add_flag(g_lvglDoomRoot, LV_OBJ_FLAG_HIDDEN);

  // Transit Departure Board
  g_lvglTransitRoot = lvglCreatePageRoot(scr, cW, cH);
  g_transitUi.root = g_lvglTransitRoot;
  lv_obj_set_pos(g_lvglTransitRoot, cW, 0);
  lvglInitTransitUi();

  // Launch Page
  g_lvglLaunchRoot = lvglCreatePageRoot(scr, cW, cH);
  lv_obj_set_pos(g_lvglLaunchRoot, cW, 0);
  lvglInitLaunchUi();

  // Screensaver
#if SCREENSAVER_ENABLED
  initLvglScreensaverUi(scr);
#endif

  lvglApplyPageVisibility(false);

  g_lvglReady = true;
  g_lvglLastTickMs = millis();
#if SCREENSAVER_ENABLED
  g_saver.lastUserInteractionMs = millis();
#endif
  g_clockUi.wifiMask = 0xFFFF;
  lvglApplyThemeStyles(true);
  Serial.printf("[LVGL] widgets date=%p clock=%p city=%p temp=%p desc=%p hum=%p sun=%p wind=%p f0=%p f1=%p\n",
                (void*)g_clockUi.date,
                (void*)g_clockUi.l1,
                (void*)g_weatherUi.city,
                (void*)g_weatherUi.icon,
                (void*)g_weatherUi.temp,
                (void*)g_weatherUi.desc,
                (void*)g_weatherUi.humidity,
                (void*)g_weatherUi.sun,
                (void*)g_weatherUi.wind,
                (void*)g_weatherUi.forecastNow,
                (void*)g_weatherUi.forecastTomorrow);
  const int16_t weatherW = (DISPLAY_WEATHER_PANEL_W > (cW / 2)) ? (cW / 3) : DISPLAY_WEATHER_PANEL_W;
  const int16_t leftW = cW - weatherW - 10;
  Serial.printf("[LVGL] init ok ui=%dx%d split=%d/%d color_depth=%d color_size=%u icons=%s\n",
                cW, cH, leftW, weatherW, LV_COLOR_DEPTH, (unsigned)sizeof(lv_color_t), DB_LVGL_WEATHER_ICON_SET);
  return true;
}

// ---------- Weather display helpers (M10 extraction) ----------

static constexpr int kWmoFallbackCode = 2;  // "Partly cloudy" — generic icon when data unavailable

/// Show current-weather icon (bitmap preferred) or glyph fallback.
static void lvglShowWeatherMainIcon(int code, bool isDay, const char* glyphFallback) {
  const lv_img_dsc_t *iconDsc = weatherImageFromCode(code, isDay);
  if (iconDsc && g_weatherUi.icon) {
    lv_img_set_src(g_weatherUi.icon, iconDsc);
    lv_obj_clear_flag(g_weatherUi.icon, LV_OBJ_FLAG_HIDDEN);
    if (g_weatherUi.glyph) lv_obj_add_flag(g_weatherUi.glyph, LV_OBJ_FLAG_HIDDEN);
  } else if (g_weatherUi.glyph) {
    if (g_weatherUi.icon) lv_obj_add_flag(g_weatherUi.icon, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(g_weatherUi.glyph, glyphFallback);
    lvglForceLabelVisible(g_weatherUi.glyph);
  }
}

/// Show forecast icon (bitmap only, no glyph fallback).
static void lvglShowWeatherForecastIcon(int code, bool isDay) {
  if (!g_weatherUi.forecastIcon) return;
  const lv_img_dsc_t *fIconDsc = weatherForecastImageFromCode(code, isDay);
  if (fIconDsc) {
    lv_img_set_src(g_weatherUi.forecastIcon, fIconDsc);
    lv_obj_clear_flag(g_weatherUi.forecastIcon, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(g_weatherUi.forecastIcon, LV_OBJ_FLAG_HIDDEN);
  }
}

/// Set all weather labels to offline/placeholder state.
static void lvglSetWeatherOfflineLabels(const char* descText, const char* glyphFallback,
                                        uint32_t glyphColor, bool setGlyphColor) {
  lv_label_set_text(g_weatherUi.temp, "--\xC2\xB0, --%");
  lvglForceLabelVisible(g_weatherUi.temp);
  lv_label_set_text(g_weatherUi.desc, descText);
  lvglForceLabelVisible(g_weatherUi.desc);
  lv_label_set_text(g_weatherUi.humidity, activeUiStrings()->windNa);
  lvglForceLabelVisible(g_weatherUi.humidity);
  lv_label_set_text(g_weatherUi.sun, "--:-- / --:--");
  lvglForceLabelVisible(g_weatherUi.sun);
  lv_label_set_text(g_weatherUi.forecastNow, activeUiStrings()->forecastNa);
  lvglForceLabelVisible(g_weatherUi.forecastNow);
  lvglShowWeatherMainIcon(kWmoFallbackCode, true, glyphFallback);
  lvglShowWeatherForecastIcon(kWmoFallbackCode, true);
  lv_obj_add_flag(g_weatherUi.forecastTomorrow, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(g_weatherUi.wind, LV_OBJ_FLAG_HIDDEN);
  if (setGlyphColor && g_weatherUi.glyph)
    lv_obj_set_style_text_color(g_weatherUi.glyph, lv_color_hex(glyphColor), 0);
}

/// Update weather widgets on HOME page (live data or offline placeholders).
static void lvglUpdateWeatherDisplay(uint32_t weatherGlyphOnline, uint32_t weatherGlyphOffline) {
#if TEST_WIFI
  if (g_weather.valid) {
    char temp[24];
    snprintf(temp, sizeof(temp), "%d%s, %d%%", (int)lroundf(g_weather.tempC), utf8Degree(), g_weather.humidity);
    lv_label_set_text(g_weatherUi.temp, temp);
    lvglForceLabelVisible(g_weatherUi.temp);

    lvglShowWeatherMainIcon(g_weather.weatherCode, g_weather.isDay,
                            weatherGlyphText(g_weather.weatherCode, g_weather.isDay));
    lv_label_set_text(g_weatherUi.desc, weatherCodeUiLabel(g_weather.weatherCode));
    lvglForceLabelVisible(g_weatherUi.desc);

    char wind[28];
    snprintf(wind, sizeof(wind), activeUiStrings()->windFmt, g_weather.windKmh);
    lv_label_set_text(g_weatherUi.humidity, wind);
    lvglForceLabelVisible(g_weatherUi.humidity);

    char sun[40];
    snprintf(sun, sizeof(sun), "%s / %s", g_weather.sunrise, g_weather.sunset);
    lv_label_set_text(g_weatherUi.sun, sun);
    lvglForceLabelVisible(g_weatherUi.sun);

    const int fIdx = g_weather.nextValid[1] ? 1 : (g_weather.nextValid[0] ? 0 : -1);
    const int fCode = (fIdx >= 0) ? g_weather.nextCode[fIdx] : g_weather.weatherCode;
    lvglShowWeatherForecastIcon(fCode, g_weather.isDay);

    char forecast[64];
    if (fIdx == 1) {
      snprintf(forecast, sizeof(forecast), activeUiStrings()->forecast3h, weatherCodeShort(fCode));
    } else if (fIdx == 0) {
      snprintf(forecast, sizeof(forecast), activeUiStrings()->forecastNow, weatherCodeShort(fCode));
    } else {
      snprintf(forecast, sizeof(forecast), "%s", activeUiStrings()->forecastNa);
    }
    lv_label_set_text(g_weatherUi.forecastNow, forecast);
    lvglForceLabelVisible(g_weatherUi.forecastNow);

    lv_obj_add_flag(g_weatherUi.forecastTomorrow, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_weatherUi.wind, LV_OBJ_FLAG_HIDDEN);
    if (g_weatherUi.glyph) lv_obj_set_style_text_color(g_weatherUi.glyph, lv_color_hex(weatherGlyphOnline), 0);
  } else {
    lvglSetWeatherOfflineLabels(activeUiStrings()->weatherOffline, LV_SYMBOL_REFRESH,
                                weatherGlyphOffline, true);
  }
#else
  lvglSetWeatherOfflineLabels(activeUiStrings()->wifiOff, LV_SYMBOL_WIFI, 0, false);
#endif
}

// ---------- Main UI update orchestrator ----------

static void updateLvglUi(bool force) {
  if (!g_lvglReady || !g_clock.ntpSynced) return;
#if SCREENSAVER_ENABLED
  if (g_saver.active) return;
#endif
  // Consume network results (Phase 1 — async dirty flags)
  if (g_weather.dirty) {
    if (g_netMutex) xSemaphoreTake(g_netMutex, portMAX_DELAY);
    g_weather.dirty = false;
    if (g_netMutex) xSemaphoreGive(g_netMutex);
    g_uiNeedsRedraw = true;
  }
  if (g_rss.dirty) {
    if (g_netMutex) xSemaphoreTake(g_netMutex, portMAX_DELAY);
    g_rss.dirty = false;
    g_auxDeck.lastItemShown = -1;
    g_auxDeck.lastQrPayload[0] = '\0';
    if (g_netMutex) xSemaphoreGive(g_netMutex);
    g_uiNeedsRedraw = true;
  }
  if (g_wiki.dirty) {
    if (g_netMutex) xSemaphoreTake(g_netMutex, portMAX_DELAY);
    g_wiki.dirty = false;
    g_wikiDeck.lastItemShown = -1;
    g_wikiDeck.lastQrPayload[0] = '\0';
    if (g_netMutex) xSemaphoreGive(g_netMutex);
    g_uiNeedsRedraw = true;
  }
  if (g_transitState.dirty) {
    if (g_netMutex) xSemaphoreTake(g_netMutex, portMAX_DELAY);
    g_transitState.dirty = false;
    if (g_netMutex) xSemaphoreGive(g_netMutex);
    if (g_uiPageMode == UI_PAGE_TRANSIT) g_uiNeedsRedraw = true;
  }
  const UiThemeLvglTokens &theme = activeUiTheme().lvgl;
  const uint32_t weatherBg = lvglResolvedWeatherBg(theme);
  const uint32_t weatherPrimary = lvglResolvedWeatherPrimary(theme, weatherBg);
  const uint32_t weatherSecondary = lvglResolvedWeatherSecondary(theme, weatherBg, weatherPrimary);
  const uint32_t weatherGlyphOnline = lvglResolvedWeatherGlyphOnline(theme, weatherBg, weatherPrimary);
  const uint32_t weatherGlyphOffline = lvglResolvedWeatherGlyphOffline(theme, weatherBg, weatherSecondary);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 50)) return;
  const int dateKey = ((timeinfo.tm_year + 1900) * 10000) + ((timeinfo.tm_mon + 1) * 100) + timeinfo.tm_mday;
  if (!force && !g_uiNeedsRedraw && timeinfo.tm_sec == g_clock.lastSecond && dateKey == g_clock.lastDateKey) return;

  lvglApplyPageVisibility(false);
  lvglUpdateWiFiBars(force);
  if (g_uiPageMode == UI_PAGE_DOOM) {
    g_uiNeedsRedraw = false;
    return;
  }
  if (g_uiPageMode == UI_PAGE_INFO) {
    lvglUpdateInfoPanel(force);
    g_clock.lastSecond = timeinfo.tm_sec;
    g_clock.lastDateKey = dateKey;
    g_uiNeedsRedraw = false;
    return;
  }
  if (g_uiPageMode == UI_PAGE_AUX) {
    lvglUpdateFeedDeck(g_auxDeck, g_rss, false, force);
    g_clock.lastSecond = timeinfo.tm_sec;
    g_clock.lastDateKey = dateKey;
    g_uiNeedsRedraw = false;
    return;
  }
  if (g_uiPageMode == UI_PAGE_WIKI) {
    lvglUpdateFeedDeck(g_wikiDeck, g_wiki, true, force);
    g_clock.lastSecond = timeinfo.tm_sec;
    g_clock.lastDateKey = dateKey;
    g_uiNeedsRedraw = false;
    return;
  }
  if (g_uiPageMode == UI_PAGE_NOW_PLAYING) {
    lvglUpdateNowPlayingUi(g_nowPlayingUi, force);
    g_clock.lastSecond = timeinfo.tm_sec;
    g_clock.lastDateKey = dateKey;
    g_uiNeedsRedraw = false;
    return;
  }
  if (g_uiPageMode == UI_PAGE_TRANSIT) {
    lvglUpdateTransitUi(force);
    g_clock.lastSecond = timeinfo.tm_sec;
    g_clock.lastDateKey = dateKey;
    g_uiNeedsRedraw = false;
    return;
  }
  if (g_uiPageMode == UI_PAGE_LAUNCH) {
    if (g_launchState.dirty) lvglUpdateLaunchUi(force);
    lvglTickLaunchCountdown();
    g_clock.lastSecond = timeinfo.tm_sec;
    g_clock.lastDateKey = dateKey;
    g_uiNeedsRedraw = false;
    return;
  }
  char sentence[96], d1[48];
  composeWordClockSentenceActive(timeinfo, sentence, sizeof(sentence));
  sanitizeAsciiBuffer(sentence, sizeof(sentence));
  if (sentence[0] >= 'a' && sentence[0] <= 'z') {
    sentence[0] = (char)toupper((unsigned char)sentence[0]);
  }
  lvglApplyClockSentenceAutoFit(sentence);
  lvglForceLabelVisible(g_clockUi.l1);
  lv_label_set_text(g_clockUi.l2, "");
  lv_label_set_text(g_clockUi.l3, "");
  formatDateActive(timeinfo, d1, sizeof(d1));
  sanitizeAsciiBuffer(d1, sizeof(d1));
  lv_label_set_text(g_clockUi.date, d1);
  lvglForceLabelVisible(g_clockUi.date);
#if TEST_WIFI
  lvglUpdateCityTicker(runtimeWeatherCityLabel(), force);
#else
  lvglUpdateCityTicker(WEATHER_CITY_LABEL, force);
#endif

  lvglUpdateWeatherDisplay(weatherGlyphOnline, weatherGlyphOffline);

  g_clock.lastSecond = timeinfo.tm_sec;
  g_clock.lastDateKey = dateKey;
  g_uiNeedsRedraw = false;

  // No manual invalidation needed — lv_label_set_text / lv_img_set_src
  // already invalidate their objects when content actually changes.
}

static void runLvglLoop() {
  if (!g_lvglReady) return;
  const uint32_t now = millis();
  // Adaptive cadence: tighter during animation for smooth swipes, relaxed at idle.
  const bool animating = (g_pageAnim.untilMs > now) || (lv_anim_count_running() > 0);
  const uint16_t cadenceMs = animating ? 8 : 20;
  if (g_pageAnim.lastRunMs != 0 && (now - g_pageAnim.lastRunMs) < cadenceMs) return;
  g_pageAnim.lastRunMs = now;
  uint32_t elapsed = now - g_lvglLastTickMs;
  if (elapsed > 50) elapsed = 50;
  if (elapsed == 0) elapsed = 1;
  g_lvglLastTickMs = now;
  lv_tick_inc(elapsed);
  if (g_uiPageMode == UI_PAGE_DOOM) {
    g_dispHw.canvasDirty = false;
    return;
  }
  lvglApplyPageVisibility(false);
  lvglUpdateWiFiBars(false);

  const uint32_t t0 = micros();
  lv_timer_handler();
  if (g_dispHw.canvasDirty) {
    dispFlush();
    g_dispHw.canvasDirty = false;
  }
  const uint32_t dt = micros() - t0;
  g_perf.lvglFrameCount++;
  g_perf.lvglTotalUs += dt;
  if (dt > g_perf.lvglMaxUs) g_perf.lvglMaxUs = dt;
}
#endif

static void updateDisplayClock(bool force) {
  if (g_uiPageMode == UI_PAGE_DOOM) return; // direct view owns the display
  if (!g_clock.ntpSynced) return;
  if (!initDisplay()) return;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 50)) return;

  const int dateKey = ((timeinfo.tm_year + 1900) * 10000) + ((timeinfo.tm_mon + 1) * 100) + timeinfo.tm_mday;
  if (!force && !g_uiNeedsRedraw && timeinfo.tm_sec == g_clock.lastSecond && dateKey == g_clock.lastDateKey) return;

  setBacklightPercent(100);
  const int16_t canvasW = canvasWidth();
  const int16_t canvasH = canvasHeight();
  const int16_t weatherW = (DISPLAY_WEATHER_PANEL_W > (canvasW / 2)) ? (canvasW / 3) : DISPLAY_WEATHER_PANEL_W;
  const int16_t leftW = canvasW - weatherW - 2;
  const int16_t weatherX = leftW + 2;

  if (force || !g_clock.staticDrawn) {
    fillRectCanvas(0, 0, canvasW, canvasH, DB_COLOR_BLACK);
    Serial.printf("[CLOCK] canvas=%dx%d mode=%s left=%d weather=%d\n",
                  canvasW, canvasH,
                  uiClockModeName(g_clock.mode),
                  leftW, weatherW);
    g_clock.staticDrawn = true;
    g_clock.lastDateKey = -1;
  }

  drawWordClockInRect(0, 0, leftW, canvasH, timeinfo);
  drawWeatherPanel(weatherX, 0, weatherW, canvasH, timeinfo);

#if DISPLAY_BACKEND_ESP_LCD
  dispFlush();
#endif

  g_clock.lastDateKey = dateKey;
  g_uiNeedsRedraw = false;

  g_clock.lastSecond = timeinfo.tm_sec;
}
#else
static void updateDisplayClock(bool force) {
  (void)force;
}
#endif

static int runI2CScanOnBus(int sdaPin, int sclPin, const char* tag) {
  Serial.println();
  Serial.printf("[I2C][%s] begin SDA=%d SCL=%d\n", tag, sdaPin, sclPin);
  TwoWire *bus = (&I2C_MAIN);
  if (strcmp(tag, "ALT") == 0) {
    bus = (&I2C_ALT);
  }

  bus->begin(sdaPin, sclPin);
  bus->setClock(100000);
  delay(20);

  int found = 0;
  for (uint8_t addr = 0x03; addr <= 0x77; ++addr) {
    bus->beginTransmission(addr);
    uint8_t err = bus->endTransmission();
    if (err == 0) {
      ++found;
      Serial.printf("[I2C][%s][FOUND] 0x%02X", tag, addr);

      if (addr == 0x51) {
        Serial.print(" (possible PCF85063 RTC)");
      } else if (addr >= 0x20 && addr <= 0x27) {
        Serial.print(" (possible TCA9554/TCA9534 expander)");
      } else if (addr == 0x6A || addr == 0x6B) {
        Serial.print(" (possible QMI8658 IMU)");
      } else if (addr == 0x38 || addr == 0x14 || addr == 0x5D || addr == 0x3B) {
        Serial.print(" (possible touch controller)");
      }
      Serial.println();
    }
  }

  if (found == 0) {
    Serial.printf("[I2C][%s][WARN] Nessun device trovato su questo bus.\n", tag);
  } else {
    Serial.printf("[I2C][%s][OK] Trovati %d device.\n", tag, found);
  }

  return found;
}

static void runI2CScanTest() {
  Serial.println();
  Serial.println("=================================================");
  Serial.println("ScryBar | M0.3 I2C scan");
  Serial.println("=================================================");
  Serial.println("[STEP] Cerco touch/IMU/RTC/expander su bus I2C.");

  int totalFound = 0;
  totalFound += runI2CScanOnBus(I2C_SDA_PIN, I2C_SCL_PIN, "MAIN");

#if (I2C_SDA_PIN_ALT >= 0) && (I2C_SCL_PIN_ALT >= 0)
  totalFound += runI2CScanOnBus(I2C_SDA_PIN_ALT, I2C_SCL_PIN_ALT, "ALT");
#else
  Serial.println("[I2C][ALT][SKIP] I2C_SDA_PIN_ALT/I2C_SCL_PIN_ALT non configurati.");
#endif

  if (totalFound == 0) {
    Serial.println("[NEXT] Nessun I2C trovato. Serve la mappa net->GPIO dal PDF schematic.");
  } else {
    Serial.println("[NEXT] Invia il log FOUND. Poi agganciamo touch/IMU/RTC con indirizzi reali.");
  }
}

static int detectTca9554Addr() {
#if (TCA9554_ADDR >= 0)
  return TCA9554_ADDR;
#else
  for (uint8_t addr = 0x20; addr <= 0x27; ++addr) {
    uint8_t cfg = 0;
    if (i2cReadReg(I2C_MAIN, addr, 0x03, cfg)) {
      return addr;
    }
  }
  return -1;
#endif
}

static void runBacklightTest() {
  Serial.println();
  Serial.println("=================================================");
  Serial.println("ScryBar | M0.2 Backlight test");
  Serial.println("=================================================");
  Serial.printf("[INFO] LCD_BL pin=%d\n", LCD_BL_PIN);
  setBacklightPwm(true);

#if BACKLIGHT_USE_TCA9554
  I2C_MAIN.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  I2C_MAIN.setClock(100000);
  Serial.printf("[INFO] I2C begin SDA=%d SCL=%d\n", I2C_SDA_PIN, I2C_SCL_PIN);

  const int tcaAddr = detectTca9554Addr();
  if (tcaAddr < 0) {
    Serial.println("[WARN] TCA9554 non trovato su 0x20..0x27. Continuo senza BL_EN.");
    Serial.println("[HINT] Se il backlight non cambia, ricontrolliamo bus/pin I2C al prossimo step.");
  } else {
    uint8_t cfg = 0xFF;
    uint8_t out = 0x00;
    bool okCfg = i2cReadReg(I2C_MAIN, (uint8_t)tcaAddr, 0x03, cfg);
    bool okOut = i2cReadReg(I2C_MAIN, (uint8_t)tcaAddr, 0x01, out);

    if (okCfg && okOut) {
      Serial.printf("[OK] TCA9554 @0x%02X raggiungibile\n", tcaAddr);

#if BACKLIGHT_FORCE_SYS_EN
      {
        const bool sysOn = (TCA9554_SYS_EN_ACTIVE_HIGH != 0);
        bool wrSys = tcaSetBitOutputAndLevel(I2C_MAIN, (uint8_t)tcaAddr, TCA9554_SYS_EN_BIT, sysOn);
        Serial.printf("[INFO] SYS_EN EXIO%d=%s -> %s\n",
                      TCA9554_SYS_EN_BIT,
                      sysOn ? "HIGH" : "LOW",
                      wrSys ? "OK" : "ERR");
      }
#endif

      const bool blLevel = (TCA9554_BL_EN_ACTIVE_HIGH != 0);
      bool wr = tcaSetBitOutputAndLevel(I2C_MAIN, (uint8_t)tcaAddr, TCA9554_BL_EN_BIT, blLevel);
      if (wr) {
        Serial.printf("[OK] BL_EN fixed: EXIO%d=%s\n",
                      TCA9554_BL_EN_BIT,
                      blLevel ? "HIGH" : "LOW");
      } else {
        Serial.println("[WARN] Non riesco a impostare BL_EN su expander.");
      }

    } else {
      Serial.printf("[WARN] Lettura registri TCA9554 @0x%02X fallita\n", tcaAddr);
    }
  }
#else
  Serial.println("[SKIP] BACKLIGHT_USE_TCA9554=0");
#endif

  Serial.println("[STEP] BL PWM test (Waveshare-style duty inversion).");
  const uint8_t levels[] = {100, 70, 40, 10, 0, 100};
  for (uint8_t level : levels) {
    setBacklightPercent(level);
    Serial.printf("[TEST] BL percent=%u%%\n", level);
    delay(BACKLIGHT_STEP_DELAY_MS);
  }

  setBacklightPwm(true);
  Serial.println("[DONE] Backlight test finito. Lascio LCD_BL=HIGH.");
  Serial.println("[NEXT] Dimmi se HIGH=acceso o LOW=acceso. Poi passiamo al display test.");
}

static void runDisplayTest() {
  Serial.println();
  Serial.println("=================================================");
  Serial.println("ScryBar | M0.4 Display test");
  Serial.println("=================================================");

  if (!initDisplay()) {
    Serial.println("[FAIL] Init display fallita.");
    return;
  }

  setBacklightPercent(100);
  delay(120);

#if DISPLAY_BACKEND_ESP_LCD
  dispFillScreen(DB_COLOR_BLACK);
  dispDrawRect(0, 0, dispWidth(), dispHeight(), DB_COLOR_WHITE);
  dispFlush();
#else
  const uint8_t runtimeRotation = (uint8_t)((DISPLAY_ROTATION + (DISPLAY_FLIP_180 ? 2 : 0)) & 0x03);
  g_gfx->setRotation(runtimeRotation);
  Serial.printf("[DISPLAY] runtime rotation=%d (cfg=%d flip180=%d) width=%d height=%d\n",
                runtimeRotation,
                DISPLAY_ROTATION,
                DISPLAY_FLIP_180 ? 1 : 0,
                g_gfx->width(),
                g_gfx->height());
  g_gfx->fillScreen(DB_COLOR_BLACK);
  g_gfx->drawRect(0, 0, g_gfx->width(), g_gfx->height(), DB_COLOR_WHITE);
#endif
  Serial.println("[OK] Display test completato.");
}

#if TEST_DISPLAY && DISPLAY_BACKEND_ESP_LCD
static bool emitSnapshotOverSerial() {
  if (!g_dispHw.canvasBuf) {
    Serial.println("[SNAP][ERR] canvas non pronto");
    return false;
  }

  char ts[20] = "nosync";
#if TEST_NTP
  struct tm ti;
  if (getLocalTime(&ti, 20)) {
    strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &ti);
  }
#endif

  const uint32_t w = (uint32_t)canvasWidth();
  const uint32_t h = (uint32_t)canvasHeight();
  const uint32_t bytes = w * h * 2U;
#if defined(LV_COLOR_DEPTH) && (LV_COLOR_DEPTH == 16) && defined(LV_COLOR_16_SWAP) && (LV_COLOR_16_SWAP != 0)
  const char *snapFmt = "rgb565be";
#else
  const char *snapFmt = "rgb565le";
#endif
  Serial.printf("[SNAP][BEGIN] ts=%s w=%lu h=%lu bytes=%lu fmt=%s\n",
                ts, (unsigned long)w, (unsigned long)h, (unsigned long)bytes, snapFmt);
  Serial.flush();

  const uint8_t *src = reinterpret_cast<const uint8_t *>(g_dispHw.canvasBuf);
  size_t sent = 0;
  while (sent < bytes) {
    const size_t chunk = ((bytes - sent) > 1024U) ? 1024U : (bytes - sent);
    Serial.write(src + sent, chunk);
    sent += chunk;
  }
  Serial.write('\n');
  Serial.printf("[SNAP][END] sent=%lu\n", (unsigned long)sent);
  return true;
}
#endif

// ── M4: Serial command handler functions ──

static void cmdHelp(const String &args);  // forward decl for table

static void cmdSnap(const String &args) {
#if TEST_DISPLAY && DISPLAY_BACKEND_ESP_LCD
  emitSnapshotOverSerial();
#else
  Serial.println("[SNAP][ERR] non disponibile su questo backend display");
#endif
}

static void cmdViewToggle(const String &args) {
  (void)stepUiPage(1, true);
  Serial.printf("[UI] page=%s\n", uiPageName(g_uiPageMode));
}

static void cmdViewFirst(const String &args) {
  jumpToFirstMainView();
  Serial.printf("[UI] page=%s\n", uiPageName(g_uiPageMode));
}

static void cmdViewLast(const String &args) {
  jumpToLastMainView();
  Serial.printf("[UI] page=%s\n", uiPageName(g_uiPageMode));
}

static void cmdViewInfo(const String &args) {
  setUiPage(UI_PAGE_INFO);
  Serial.printf("[UI] page=%s\n", uiPageName(g_uiPageMode));
}

static void cmdViewHome(const String &args) {
  setUiPage(UI_PAGE_HOME);
  Serial.printf("[UI] page=%s\n", uiPageName(g_uiPageMode));
}

static void cmdViewAux(const String &args) {
  if (!uiPageEnabled(UI_PAGE_AUX)) { Serial.println("[UI] AUX disabled"); return; }
  setUiPage(UI_PAGE_AUX);
  Serial.printf("[UI] page=%s\n", uiPageName(g_uiPageMode));
}

static void cmdViewWiki(const String &args) {
  if (!uiPageEnabled(UI_PAGE_WIKI)) { Serial.println("[UI] WIKI disabled"); return; }
  setUiPage(UI_PAGE_WIKI);
  Serial.printf("[UI] page=%s\n", uiPageName(g_uiPageMode));
}

static void cmdViewNowPlaying(const String &args) {
  setUiPage(UI_PAGE_NOW_PLAYING);
  Serial.printf("[UI] page=%s\n", uiPageName(g_uiPageMode));
}

static void cmdViewDoom(const String &args) {
  if (!uiPageEnabled(UI_PAGE_DOOM)) { Serial.println("[UI] DOOM disabled"); return; }
  setUiPage(UI_PAGE_DOOM);
  Serial.printf("[UI] page=%s\n", uiPageName(g_uiPageMode));
}

static void cmdViewTransit(const String &args) {
  if (!uiPageEnabled(UI_PAGE_TRANSIT)) { Serial.println("[UI] TRANSIT disabled"); return; }
  setUiPage(UI_PAGE_TRANSIT);
  Serial.printf("[UI] page=%s\n", uiPageName(g_uiPageMode));
}

static void cmdViewLaunch(const String &args) {
  if (!uiPageEnabled(UI_PAGE_LAUNCH)) { Serial.println("[UI] LAUNCH disabled"); return; }
  setUiPage(UI_PAGE_LAUNCH);
  Serial.printf("[UI] page=%s\n", uiPageName(g_uiPageMode));
}

static void cmdLaunchDetail(const String &args) {
  int idx = args.length() ? args.toInt() : 0;
  markUserInteraction(millis());
  lvglSetScreenSaverActive(false);
  if (g_launchUi.qrModalOpen) {
    lvglCloseLaunchQr();
    Serial.println("[LAUNCH] qr closed");
  } else {
    lvglOpenLaunchQr(idx);
    Serial.printf("[LAUNCH] qr opened idx=%d\n", idx);
  }
  g_uiNeedsRedraw = true;
}

static void cmdTheme(const String &args) {
  if (args.length() == 0) {
    Serial.printf("[UI] theme='%s' (%s)\n", runtimeUiThemeId(), runtimeUiThemeLabel());
    Serial.print("[UI] themes:");
    for (size_t i = 0; i < UI_THEME_COUNT; ++i) { Serial.print(' '); Serial.print(kUiThemes[i].id); }
    Serial.println();
    return;
  }
  String themeArg(args);
  themeArg.trim();
  themeArg.toLowerCase();
  if (themeArg.length() == 0) { Serial.println("[UI][ERR] THEME richiede un id"); return; }
  const int8_t idx = findUiThemeIndexById(themeArg.c_str());
  if (idx < 0) { Serial.printf("[UI][ERR] theme id non valido: '%s'\n", themeArg.c_str()); return; }
  setActiveUiThemeById(themeArg.c_str());
#if TEST_WIFI
  ensureRuntimeNetConfig();
  copyStringSafe(g_runtimeNetConfig.uiTheme, sizeof(g_runtimeNetConfig.uiTheme), themeArg.c_str());
  saveRuntimeNetConfigToNvs();
#endif
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI
  if (g_lvglReady) lvglApplyThemeStyles(true);
#endif
  g_uiNeedsRedraw = true;
  Serial.printf("[UI] theme set -> '%s' (%s)\n", runtimeUiThemeId(), runtimeUiThemeLabel());
}

static void cmdLangStat(const String &args) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 50)) {
    Serial.printf("[LANG] wc_lang='%s' sentence=<ntp-unavailable>\n", g_wordClockLang);
    return;
  }
  char sentence[96];
  composeWordClockSentenceActive(timeinfo, sentence, sizeof(sentence));
  Serial.printf("[LANG] wc_lang='%s' sentence=\"%s\"\n", g_wordClockLang, sentence);
}

static void cmdLang(const String &args) {
  if (args.length() == 0) { cmdLangStat(args); return; }
  String langArg(args);
  langArg.trim();
  langArg.toLowerCase();
  if (!isValidLangCode(langArg)) {
    Serial.printf("[LANG][ERR] code non valido: '%s'\n", langArg.c_str());
    return;
  }
  copyStringSafe(g_wordClockLang, sizeof(g_wordClockLang), langArg.c_str());
#if TEST_WIFI
  ensureRuntimeNetConfig();
  saveRuntimeNetConfigToNvs();
#endif
  g_uiNeedsRedraw = true;
  Serial.printf("[LANG] set -> '%s'\n", g_wordClockLang);
}

static void cmdQrOn(const String &args) {
  setUiPage(UI_PAGE_AUX);
  lvglSetDeckQrModalOpen(g_auxDeck, true);
  Serial.println("[UI] qr=ON");
}

static void cmdQrOff(const String &args) {
  lvglSetDeckQrModalOpen(g_auxDeck, false);
  Serial.println("[UI] qr=OFF");
}

static void cmdQrToggle(const String &args) {
  setUiPage(UI_PAGE_AUX);
  lvglSetDeckQrModalOpen(g_auxDeck, !g_auxDeck.qrModalOpen);
  Serial.printf("[UI] qr=%s\n", g_auxDeck.qrModalOpen ? "ON" : "OFF");
}

#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI && SCREENSAVER_ENABLED
static void cmdSaverOn(const String &args) { lvglSetScreenSaverActive(true); }

static void cmdSaverOff(const String &args) {
  lvglSetScreenSaverActive(false);
  markUserInteraction(millis());
}

static void cmdSaverStat(const String &args) {
  const uint32_t now = millis();
  const uint32_t idleTargetMs = lvglScreenSaverIdleTargetMs(now);
  Serial.printf("[SCRNSVR] active=%d idle_ms=%lu last_input_ago=%lu\n",
                g_saver.active ? 1 : 0,
                (unsigned long)idleTargetMs,
                (unsigned long)(now - g_saver.lastUserInteractionMs));
}
#endif

static void cmdPwrStat(const String &args) {
  const int rawVal = gpio_get_level((gpio_num_t)PWR_BUTTON_PIN);
  Serial.printf("[PWR] stat pin=%d raw=%d pressed=%d hold_ms=%lu\n",
                PWR_BUTTON_PIN, rawVal, isPwrButtonPressed() ? 1 : 0,
                g_pwrBtn.down ? (unsigned long)(millis() - g_pwrBtn.downMs) : 0UL);
}

static void cmdNavStat(const String &args) {
  const int rawVal = gpio_get_level((gpio_num_t)NAV_FIRST_BUTTON_PIN);
  Serial.printf("[NAV] stat pin=%d raw=%d pressed=%d hold_ms=%lu\n",
                NAV_FIRST_BUTTON_PIN, rawVal, isNavFirstButtonPressed() ? 1 : 0,
                g_navBtn.down ? (unsigned long)(millis() - g_navBtn.downMs) : 0UL);
}

static void cmdPwrOff(const String &args) {
  Serial.println("[PWR] PWROFF command received.");
  shutdownFromPowerButton(false);
}

static void cmdPwrOffHard(const String &args) {
  Serial.println("[PWR] PWROFFHARD command received.");
  shutdownFromPowerButton(true);
}

static void cmdBatStat(const String &args) {
#if TEST_BATTERY
  sampleBatteryNow(millis(), true);
  if (g_batt.hasSample) {
    Serial.printf("[BATT] stat raw=%d vbat=%.3fV soc=%d%%\n", g_batt.raw, g_batt.voltage, g_batt.percent);
  } else {
    Serial.println("[BATT] stat unavailable");
  }
#else
  Serial.println("[BATT] monitor disabled");
#endif
}

static void cmdWebCfg(const String &args) {
#if TEST_WIFI
  ensureRuntimeNetConfig();
#if WEB_CONFIG_ENABLED
  if (g_wifiSt.connected && g_webCfg.serverStarted) {
    Serial.printf("[WEB] url=http://%s:%u\n", WiFi.localIP().toString().c_str(), (unsigned)WEB_CONFIG_PORT);
  } else if (g_wifiSt.connected) {
    Serial.printf("[WEB] WiFi OK, server not started yet (port=%u)\n", (unsigned)WEB_CONFIG_PORT);
  } else if (g_wifiSt.setupApActive) {
    Serial.printf("[WEB] setup-ap ssid='%s' url=http://%s:%u\n",
                  g_wifiSt.setupApSsid, WiFi.softAPIP().toString().c_str(), (unsigned)WEB_CONFIG_PORT);
  } else {
    Serial.println("[WEB] WiFi non connesso");
  }
  Serial.printf("[WEB] city='%s' lat=%.4f lon=%.4f\n",
                runtimeWeatherCityLabel(), runtimeWeatherLat(), runtimeWeatherLon());
  Serial.printf("[WEB] rss_active='%s' feeds=%u\n",
                runtimeRssFeedUrl(), (unsigned)runtimeRssConfiguredFeedCount());
  for (uint8_t i = 0; i < RSS_FEED_SLOT_COUNT; ++i) {
    const RuntimeRssFeedConfig *feed = runtimeRssFeedBySlot(i);
    if (!feed || !feed->url[0]) continue;
    Serial.printf("[WEB] rss%u name='%s' max=%u url='%s'\n",
                  (unsigned)(i + 1), feed->name,
                  (unsigned)clampRssFeedMaxItems(feed->maxItems), feed->url);
  }
  if (runtimeLogoUrl()[0]) Serial.printf("[WEB] logo='%s'\n", runtimeLogoUrl());
  else Serial.println("[WEB] logo=''");
  Serial.printf("[WEB] theme='%s' (%s)\n", runtimeUiThemeId(), runtimeUiThemeLabel());
  Serial.printf("[WEB] views info=%d home=1 aux=%d wiki=%d np=%d transit=%d\n",
                (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_INFO) ? 1 : 0,
                (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_AUX) ? 1 : 0,
                (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_WIKI) ? 1 : 0,
                (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_NOW_PLAYING) ? 1 : 0,
                (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_TRANSIT) ? 1 : 0);
  Serial.printf("[WEB] lang='%s'\n", g_wordClockLang);
  Serial.printf("[WEB] wifi_setup_mode='%s' setup_ap=%d runtime_known=%u\n",
                g_wifiSt.setupMode, g_wifiSt.setupApActive ? 1 : 0, (unsigned)g_wifiSt.runtimeCredCount);
#else
  Serial.println("[WEB] config UI disabled (WEB_CONFIG_ENABLED=0)");
#endif
#else
  Serial.println("[WEB] unavailable (TEST_WIFI=0)");
#endif
}

static void cmdWifiDirect(const String &args) {
  if (args.length() == 0) {
    Serial.printf("[WIFI][DIRECT] mode='%s' ap_active=%d ap_ssid='%s' ap_ip=%s\n",
                  g_wifiSt.setupMode, g_wifiSt.setupApActive ? 1 : 0,
                  g_wifiSt.setupApSsid[0] ? g_wifiSt.setupApSsid : "-",
                  WiFi.softAPIP().toString().c_str());
    return;
  }
  String mode(args);
  mode.trim();
  mode.toLowerCase();
  if (!(mode == "off" || mode == "auto" || mode == "on")) {
    Serial.println("[WIFI][DIRECT][ERR] usa: WIFIDIRECT off|auto|on");
    return;
  }
  copyStringSafe(g_wifiSt.setupMode, sizeof(g_wifiSt.setupMode), mode.c_str());
  normalizeWifiSetupMode();
  ensureRuntimeNetConfig();
  (void)saveRuntimeNetConfigToNvs();
  wifiHandleSetupModeLoop(millis());
  Serial.printf("[WIFI][DIRECT] mode set -> '%s'\n", g_wifiSt.setupMode);
}

static void cmdRssDiag(const String &args) {
#if TEST_WIFI
  runRssDiag();
#else
  Serial.println("[CMD] RSSDIAG unavailable (TEST_WIFI=0)");
#endif
}

static void cmdRssStat(const String &args) {
#if TEST_WIFI && RSS_ENABLED
  Serial.printf("[RSSSTAT] valid=%d items=%u idx=%u http=%d fetched='%s' last_fetch=%lu last_attempt=%lu heap=%u psram=%u\n",
                g_rss.valid ? 1 : 0, (unsigned)g_rss.itemCount, (unsigned)g_rss.currentIndex,
                g_rss.lastHttpCode, g_rss.fetchedAt, (unsigned long)g_rss.lastFetchMs,
                (unsigned long)g_rss.lastAttemptMs, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
#else
  Serial.println("[RSSSTAT] unavailable");
#endif
}

static void cmdWikiStat(const String &args) {
#if TEST_WIFI && RSS_ENABLED
  Serial.printf("[WIKISTAT] valid=%d items=%u idx=%u http=%d fetched='%s' last_fetch=%lu last_attempt=%lu heap=%u psram=%u\n",
                g_wiki.valid ? 1 : 0, (unsigned)g_wiki.itemCount, (unsigned)g_wiki.currentIndex,
                g_wiki.lastHttpCode, g_wiki.fetchedAt, (unsigned long)g_wiki.lastFetchMs,
                (unsigned long)g_wiki.lastAttemptMs, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
#else
  Serial.println("[WIKISTAT] unavailable");
#endif
}

static void cmdRssReload(const String &args) {
#if TEST_WIFI && RSS_ENABLED
  netEnqueue(NET_REQ_RSS, 0);
  Serial.println("[RSSRELOAD] queued");
#else
  Serial.println("[RSSRELOAD] unavailable");
#endif
}

static void cmdWikiReload(const String &args) {
#if TEST_WIFI && RSS_ENABLED
  netEnqueue(NET_REQ_WIKI, 0);
  Serial.println("[WIKIRELOAD] queued");
#else
  Serial.println("[WIKIRELOAD] unavailable");
#endif
}

static void cmdReload(const String &args) {
#if TEST_WIFI
  netEnqueue(NET_REQ_WEATHER, 0);
#if RSS_ENABLED
  netEnqueue(NET_REQ_RSS, 0);
  netEnqueue(NET_REQ_WIKI, 0);
#endif
  Serial.println("[CMD] reload queued");
#else
  Serial.println("[RELOAD] unavailable");
#endif
}

// ── M4: Dispatch table ──

struct SerialCmd {
  const char *name;
  void (*handler)(const String &args);
};

static const SerialCmd kSerialCmds[] = {
  { "HELP",          cmdHelp },
  { "SNAP",          cmdSnap },
  { "SCREENSHOT",    cmdSnap },
  { "VIEW",          cmdViewToggle },
  { "VIEWTOGGLE",    cmdViewToggle },
  { "VIEWFIRST",     cmdViewFirst },
  { "VIEWHOMEFIRST", cmdViewFirst },
  { "VIEWLAST",      cmdViewLast },
  { "VIEWAUXLAST",   cmdViewLast },
  { "VIEWRSSLAST",   cmdViewLast },
  { "VIEW0",         cmdViewInfo },
  { "VIEWINFO",      cmdViewInfo },
  { "VIEW1",         cmdViewHome },
  { "VIEWHOME",      cmdViewHome },
  { "VIEW2",         cmdViewAux },
  { "VIEWAUX",       cmdViewAux },
  { "VIEWRSS",       cmdViewAux },
  { "VIEW3",         cmdViewWiki },
  { "VIEWWIKI",      cmdViewWiki },
  { "VIEW4",         cmdViewNowPlaying },
  { "VIEWNOW",       cmdViewNowPlaying },
  { "VIEWNP",        cmdViewNowPlaying },
  { "VIEWPLAY",      cmdViewNowPlaying },
  { "VIEW5",         cmdViewDoom },
  { "VIEWDOOM",      cmdViewDoom },
  { "DOOM",          cmdViewDoom },
  { "VIEW6",         cmdViewTransit },
  { "VIEWTRANSIT",   cmdViewTransit },
  { "TRANSIT",       cmdViewTransit },
  { "VIEW7",         cmdViewLaunch },
  { "VIEWLAUNCH",    cmdViewLaunch },
  { "LAUNCH",        cmdViewLaunch },
  { "LAUNCHDETAIL",  cmdLaunchDetail },
  { "THEME",         cmdTheme },
  { "LANG",          cmdLang },
  { "LANGSTAT",      cmdLangStat },
  { "QRON",          cmdQrOn },
  { "QROFF",         cmdQrOff },
  { "QRTOGGLE",      cmdQrToggle },
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI && SCREENSAVER_ENABLED
  { "SAVERON",       cmdSaverOn },
  { "SAVEROFF",      cmdSaverOff },
  { "SAVERSTAT",     cmdSaverStat },
#endif
  { "PWRSTAT",       cmdPwrStat },
  { "NAVSTAT",       cmdNavStat },
  { "PWROFF",        cmdPwrOff },
  { "PWROFFHARD",    cmdPwrOffHard },
  { "BATSTAT",       cmdBatStat },
  { "WEBCFG",        cmdWebCfg },
  { "WEB",           cmdWebCfg },
  { "WIFIDIRECT",    cmdWifiDirect },
  { "WIFISETUP",     cmdWifiDirect },
  { "RSSDIAG",       cmdRssDiag },
  { "RSSSTAT",       cmdRssStat },
  { "WIKISTAT",      cmdWikiStat },
  { "RSSRELOAD",     cmdRssReload },
  { "WIKIRELOAD",    cmdWikiReload },
  { "RELOAD",        cmdReload },
};

static void cmdHelp(const String &args) {
  Serial.print("[CMD] Commands:");
  for (size_t i = 0; i < sizeof(kSerialCmds) / sizeof(kSerialCmds[0]); ++i) {
    Serial.print(' ');
    Serial.print(kSerialCmds[i].name);
  }
  Serial.println();
}

// ── M4: Table-driven dispatch orchestrator ──

static void handleSerialCommand(const char *line) {
  if (!line || !*line) return;
  String raw(line);
  raw.trim();
  if (raw.length() == 0) return;
  String cmd(raw);
  cmd.toUpperCase();

  // Split on first space: cmdName (uppercase) + cmdArgs (original case)
  String cmdName, cmdArgs;
  const int sp = cmd.indexOf(' ');
  if (sp >= 0) {
    cmdName = cmd.substring(0, sp);
    cmdArgs = raw.substring(sp + 1);
    cmdArgs.trim();
  } else {
    cmdName = cmd;
  }

  for (size_t i = 0; i < sizeof(kSerialCmds) / sizeof(kSerialCmds[0]); ++i) {
    if (cmdName == kSerialCmds[i].name) {
      kSerialCmds[i].handler(cmdArgs);
      return;
    }
  }

  Serial.printf("[CMD][WARN] comando sconosciuto: %s\n", line);
}

static void pollSerialCommands() {
  static char buf[48];
  static uint8_t len = 0;

  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '\r' || c == '\n') {
      if (len > 0) {
        buf[len] = '\0';
        handleSerialCommand(buf);
        len = 0;
      }
      continue;
    }
    if (len < (sizeof(buf) - 1)) {
      buf[len++] = c;
    }
  }
}

// --- IMU section ---
#if TEST_IMU
static bool imuShouldBeActiveForUi() {
#if TEST_DISPLAY && DOOM_SPIKE_ENABLED
  return g_uiPageMode == UI_PAGE_DOOM;
#else
  return false;
#endif
}

static void imuResetDoomTiltState() {
#if TEST_DISPLAY && DOOM_SPIKE_ENABLED
  g_doom.lastTiltSampleMs = 0;
  g_doom.tiltFilterReady = false;
#endif
}

static void setImuSensorsActive(bool active) {
  if (!g_imu.ready) return;
  if (g_imu.sensorsActive == active) return;
  if (active) {
    g_qmi.enableAccelerometer();
    g_qmi.enableGyroscope();
    g_imu.lastAccelMag = 1.0f;
    g_imu.lastPrintMs = millis();
    imuResetDoomTiltState();
  } else {
    g_qmi.disableGyroscope();
    g_qmi.disableAccelerometer();
    g_imu.lastShakeMs = 0;
    imuResetDoomTiltState();
  }
  g_imu.sensorsActive = active;
}

static void syncImuActiveForUi() {
  setImuSensorsActive(imuShouldBeActiveForUi());
}

static void runImuInitTest() {
  Serial.println();
  Serial.println("=================================================");
  Serial.println("ScryBar | M0.5 IMU + shake");
  Serial.println("=================================================");

  I2C_MAIN.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  I2C_MAIN.setClock(400000);

  const uint8_t imuCandidates[] = {0x6B, 0x6A};
  g_imu.ready = false;

  for (uint8_t i = 0; i < (sizeof(imuCandidates) / sizeof(imuCandidates[0])); ++i) {
    const uint8_t addr = imuCandidates[i];
    if (g_qmi.begin(I2C_MAIN, addr, I2C_SDA_PIN, I2C_SCL_PIN)) {
      g_imu.ready = true;
      g_imu.addr = addr;
      break;
    }
  }

  if (!g_imu.ready) {
    Serial.println("[FAIL] QMI8658 init fallita su 0x6B/0x6A.");
    Serial.println("[HINT] Verifica I2C MAIN (SDA=47, SCL=48) e alimentazione sensori.");
    return;
  }

  Serial.printf("[OK] QMI8658 trovato su 0x%02X, chipId=0x%02X\n", g_imu.addr, g_qmi.getChipID());

  const bool accSelfTest = g_qmi.selfTestAccel();
  const bool gyrSelfTest = g_qmi.selfTestGyro();
  Serial.printf("[IMU] selfTestAccel=%s\n", accSelfTest ? "PASS" : "FAIL");
  Serial.printf("[IMU] selfTestGyro =%s\n", gyrSelfTest ? "PASS" : "FAIL");
  if (!gyrSelfTest) {
    Serial.println("[WARN] Gyro self-test a volte fallisce ma i dati possono essere validi.");
  }

  g_qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_250Hz, SensorQMI8658::LPF_MODE_1);
  g_qmi.configGyroscope(SensorQMI8658::GYR_RANGE_512DPS, SensorQMI8658::GYR_ODR_224_2Hz, SensorQMI8658::LPF_MODE_3);
  g_qmi.enableAccelerometer();
  g_qmi.enableGyroscope();

  g_imu.lastAccelMag = 1.0f;
  g_imu.lastShakeMs = 0;
  g_imu.lastPrintMs = millis();
  g_imu.sensorsActive = true;
  setImuSensorsActive(false);

  Serial.printf("[CFG] shake_jerk=%.2fg, shake_gyro=%.1f dps, debounce=%u ms\n",
                IMU_SHAKE_JERK_G, IMU_SHAKE_GYRO_DPS, IMU_SHAKE_DEBOUNCE_MS);
  Serial.println("[IMU] standby until DOOM view is active.");
}

static void runImuLoop() {
  if (!g_imu.ready) return;
  if (!g_imu.sensorsActive) return;
  if (!g_qmi.getDataReady()) return;

  float ax = 0.0f, ay = 0.0f, az = 0.0f;
  float gx = 0.0f, gy = 0.0f, gz = 0.0f;
  const bool okA = g_qmi.getAccelerometer(ax, ay, az);
  const bool okG = g_qmi.getGyroscope(gx, gy, gz);
  if (!okA || !okG) return;
  const float absGx = fabsf(gx);
  const float absGy = fabsf(gy);
  const float absGz = fabsf(gz);
  const float gyroPeak = fmaxf(absGx, fmaxf(absGy, absGz));

#if TEST_DISPLAY && DOOM_SPIKE_ENABLED
  const uint32_t sampleNow = millis();
  float dt = 0.0f;
  if (g_doom.lastTiltSampleMs != 0 && sampleNow > g_doom.lastTiltSampleMs) {
    dt = (float)(sampleNow - g_doom.lastTiltSampleMs) / 1000.0f;
    if (dt > 0.05f) dt = 0.05f;
  }
  g_doom.lastTiltSampleMs = sampleNow;

  const float moveAccelDeg = atan2f(ay, az) * kDoomRadToDeg;
  const float turnAccelDeg = atan2f(-ax, sqrtf((ay * ay) + (az * az))) * kDoomRadToDeg;

  if (!g_doom.tiltFilterReady || dt <= 0.0f) {
    g_doom.moveTiltDeg = moveAccelDeg;
    g_doom.turnTiltDeg = turnAccelDeg;
    g_doom.tiltFilterReady = true;
  } else {
    g_doom.moveTiltDeg = (kDoomTiltComplementaryAlpha * (g_doom.moveTiltDeg + (gx * dt))) +
                        ((1.0f - kDoomTiltComplementaryAlpha) * moveAccelDeg);
    g_doom.turnTiltDeg = (kDoomTiltComplementaryAlpha * (g_doom.turnTiltDeg + (gy * dt))) +
                        ((1.0f - kDoomTiltComplementaryAlpha) * turnAccelDeg);
  }

  const bool touchBusy = g_touch.down || g_touch.awaitRelease;
  if (g_doom.neutralPending) {
    const bool canSampleNeutral =
        g_doom.tiltFilterReady &&
        sampleNow >= g_doom.neutralArmAtMs &&
        !touchBusy &&
        gyroPeak <= kDoomNeutralCaptureGyroMaxDps;
    if (!canSampleNeutral) {
      g_doom.neutralStableSinceMs = 0;
      g_doom.neutralStableSamples = 0;
      g_doom.neutralAccumMoveDeg = 0.0f;
      g_doom.neutralAccumTurnDeg = 0.0f;
    } else {
      if (g_doom.neutralStableSinceMs == 0) {
        g_doom.neutralStableSinceMs = sampleNow;
        g_doom.neutralStableSamples = 0;
        g_doom.neutralAccumMoveDeg = 0.0f;
        g_doom.neutralAccumTurnDeg = 0.0f;
      }
      g_doom.neutralAccumMoveDeg += g_doom.moveTiltDeg;
      g_doom.neutralAccumTurnDeg += g_doom.turnTiltDeg;
      if (g_doom.neutralStableSamples < UINT16_MAX) ++g_doom.neutralStableSamples;

      if ((sampleNow - g_doom.neutralStableSinceMs) >= kDoomNeutralStableWindowMs &&
          g_doom.neutralStableSamples >= kDoomNeutralStableMinSamples) {
        const float sampleCount = (float)g_doom.neutralStableSamples;
        g_doom.neutralMoveTiltDeg = g_doom.neutralAccumMoveDeg / sampleCount;
        g_doom.neutralTurnTiltDeg = g_doom.neutralAccumTurnDeg / sampleCount;
        g_doom.neutralPending = false;
        g_doom.neutralReady = true;
        g_doom.axisFilterReady = false;
        g_doom.moveDeltaFilteredDeg = 0.0f;
        g_doom.turnDeltaFilteredDeg = 0.0f;
        g_doom.moveBin = 0;
        g_doom.turnBin = 0;
        g_doom.frameDirty = true;
        Serial.printf("[DOOM][IMU] neutral move=%.1f turn=%.1f samples=%u\n",
                      g_doom.neutralMoveTiltDeg,
                      g_doom.neutralTurnTiltDeg,
                      (unsigned)g_doom.neutralStableSamples);
      }
    }
  }

  int8_t moveBin = 0;
  int8_t turnBin = 0;
  if (g_doom.neutralReady) {
    const float moveDeltaDegRaw = (g_doom.moveTiltDeg - g_doom.neutralMoveTiltDeg) * (float)kDoomMoveTiltSign;
    const float turnDeltaDegRaw = (g_doom.turnTiltDeg - g_doom.neutralTurnTiltDeg) * (float)kDoomTurnTiltSign;
    if (!g_doom.axisFilterReady) {
      g_doom.moveDeltaFilteredDeg = moveDeltaDegRaw;
      g_doom.turnDeltaFilteredDeg = turnDeltaDegRaw;
      g_doom.axisFilterReady = true;
    } else {
      g_doom.moveDeltaFilteredDeg += (moveDeltaDegRaw - g_doom.moveDeltaFilteredDeg) * kDoomAxisResponseAlpha;
      g_doom.turnDeltaFilteredDeg += (turnDeltaDegRaw - g_doom.turnDeltaFilteredDeg) * kDoomAxisResponseAlpha;
    }
    moveBin = doomQuantizeAxis(g_doom.moveDeltaFilteredDeg, kDoomMoveEngageDeg, kDoomMoveReleaseDeg,
                               kDoomMoveBinDeg, g_doom.moveBin, kDoomMoveBinMin, kDoomMoveBinMax);
    turnBin = doomQuantizeAxis(g_doom.turnDeltaFilteredDeg, kDoomTurnEngageDeg, kDoomTurnReleaseDeg,
                               kDoomTurnBinDeg, g_doom.turnBin, kDoomTurnBinMin, kDoomTurnBinMax);
  } else {
    g_doom.axisFilterReady = false;
    g_doom.moveDeltaFilteredDeg = 0.0f;
    g_doom.turnDeltaFilteredDeg = 0.0f;
  }
  if (moveBin != g_doom.moveBin || turnBin != g_doom.turnBin) {
    g_doom.moveBin = moveBin;
    g_doom.turnBin = turnBin;
    if (g_uiPageMode == UI_PAGE_DOOM) {
      g_doom.frameDirty = true;
#if IMU_VERBOSE_SERIAL
      Serial.printf("[DOOM][IMU] move=%d turn=%d tilt=(%.1f,%.1f) neutral=(%.1f,%.1f)\n",
                    (int)g_doom.moveBin,
                    (int)g_doom.turnBin,
                    g_doom.moveTiltDeg,
                    g_doom.turnTiltDeg,
                    g_doom.neutralMoveTiltDeg,
                    g_doom.neutralTurnTiltDeg);
#endif
    }
  }
#endif

  const float accMag = sqrtf((ax * ax) + (ay * ay) + (az * az));
  const float jerk = fabsf(accMag - g_imu.lastAccelMag);
  g_imu.lastAccelMag = accMag;

  const uint32_t now = millis();
  const bool shake = (jerk >= IMU_SHAKE_JERK_G) || (gyroPeak >= IMU_SHAKE_GYRO_DPS);
  if (shake && ((now - g_imu.lastShakeMs) >= IMU_SHAKE_DEBOUNCE_MS)) {
    g_imu.lastShakeMs = now;
    Serial.printf("[SHAKE] jerk=%.2fg gyro_peak=%.1f dps\n", jerk, gyroPeak);
  }

#if IMU_VERBOSE_SERIAL
  if ((now - g_imu.lastPrintMs) >= IMU_PRINT_INTERVAL_MS) {
    g_imu.lastPrintMs = now;
    Serial.printf("[IMU] acc=(%.2f,%.2f,%.2f)g gyro=(%.1f,%.1f,%.1f)dps mag=%.2f jerk=%.2f\n",
                  ax, ay, az, gx, gy, gz, accMag, jerk);
  }
#endif
}
#else
static void runImuInitTest() {}
static void runImuLoop() {}
#endif

void setup() {
  Serial.begin(SERIAL_BAUDRATE);
  delay(1200);
  preparePowerButtonPin();
  prepareNavFirstButtonPin();
  ensureSystemPowerLatchOnBoot();
  handleWakeHoldGate();
#if TEST_BATTERY
  initBatteryMonitor();
  sampleBatteryNow(millis(), true);
#endif

  Serial.printf("[FW] Build=%s\n", FW_BUILD_TAG);
  Serial.printf("[BOOT] Serial avviata a %d baud\n", SERIAL_BAUDRATE);
  Serial.printf("[CFG] SERIAL=%d BACKLIGHT=%d DISPLAY=%d TOUCH=%d I2C_SCAN=%d IMU=%d WIFI=%d NTP=%d\n",
                TEST_SERIAL_INFO, TEST_BACKLIGHT, TEST_DISPLAY, TEST_TOUCH,
                TEST_I2C_SCAN, TEST_IMU, TEST_WIFI, TEST_NTP);

  // Mount FAT filesystem
  if (!FFat.begin(false)) {
    Serial.println("[FAT] mount failed");
  } else {
    Serial.printf("[FAT] mounted, %u KB total, %u KB used\n",
                  (unsigned)(FFat.totalBytes() / 1024),
                  (unsigned)((FFat.totalBytes() - FFat.freeBytes()) / 1024));
  }

#if TEST_WIFI
  ensureRuntimeNetConfig();
#endif

#if TEST_SERIAL_INFO
  runSerialInfoTest();
#else
  Serial.println("[SKIP] TEST_SERIAL_INFO=0");
#endif

#if TEST_BACKLIGHT
  runBacklightTest();
#else
  Serial.println("[SKIP] TEST_BACKLIGHT=0");
#endif

#if TEST_DISPLAY
  runDisplayTest();
  runStartupSplash();
#else
  Serial.println("[SKIP] TEST_DISPLAY=0");
  setBacklightPercent(0);
  Serial.println("[INFO] Display test disattivo: backlight spento per evitare artefatti video.");
#endif

#if TEST_I2C_SCAN
  runI2CScanTest();
#else
  Serial.println("[SKIP] TEST_I2C_SCAN=0");
#endif

#if TEST_TOUCH
  initTouchInput();
#else
  Serial.println("[SKIP] TEST_TOUCH=0");
#endif

#if TEST_IMU
  runImuInitTest();
#else
  Serial.println("[SKIP] TEST_IMU=0");
#endif

#if TEST_WIFI
  runWiFiConnectTest();
#else
  Serial.println("[SKIP] TEST_WIFI=0");
#endif

#if TEST_LVGL_UI && TEST_DISPLAY && DISPLAY_BACKEND_ESP_LCD
  if (initLvglUi()) {
    updateLvglUi(true);
    runLvglLoop();
  } else {
    Serial.println("[LVGL][ERR] init fallita.");
  }
#elif TEST_DISPLAY && TEST_NTP
  updateDisplayClock(true);
#endif

  // Start network background task on Core 1
  g_netMutex = xSemaphoreCreateMutex();
  g_netQueue = xQueueCreate(NET_QUEUE_DEPTH, sizeof(NetRequest));
  if (g_netMutex && g_netQueue) {
    xTaskCreatePinnedToCore(
      netTaskMain,
      "net_task",
      NET_TASK_STACK_SIZE,
      nullptr,
      NET_TASK_PRIORITY,
      &g_netTaskHandle,
      1  // Core 1
    );
    Serial.println("[NET] task created on Core 1");
  } else {
    Serial.println("[NET][ERR] failed to create queue/mutex — network stays on Core 0");
  }

#if TEST_NTP
  if (wifiIsConnectedNow()) {
    runNtpTimeTest();
    updateWeatherFromApi(true);
    updateTransitFromApi(false);  // preload transit at boot if station is configured
#if TEST_WIFI && RSS_ENABLED
    updateRssFromFeed(false);  // preload RSS once at boot while still on HOME
    updateWikiFromFeed(false); // preload WIKI once at boot while still on HOME
    wikiPreloadMetaStep();
#endif
  } else {
    Serial.println("[NTP] WiFi non ancora connesso: sync deferred in loop.");
  }
#else
  Serial.println("[SKIP] TEST_NTP=0");
#endif

#if TEST_DISPLAY && DOOM_SPIKE_ENABLED && DOOM_SPIKE_AUTOSTART
  setUiPage(UI_PAGE_DOOM);
#endif

  applyEnergyPolicy(millis(), true);
}

void loop() {
  static uint32_t lastHeartbeat = 0;
  static uint32_t lastSummary = 0;
  const uint32_t now = millis();

  handlePowerButtonLoop(now);
  handleNavFirstButtonLoop(now);
  pollSerialCommands();
#if TEST_WIFI
  handleWiFiReconnectLoop(now);
  wifiHandleSetupModeLoop(now);
  handleWebConfigServerLoop();
#endif
#if TEST_BATTERY
  sampleBatteryNow(now, false);
#endif
  applyEnergyPolicy(now, false);
#if TEST_NTP
  if (!g_clock.ntpSynced && wifiIsConnectedNow()) {
    if (g_wifiSt.lastNtpAttemptMs == 0 || (now - g_wifiSt.lastNtpAttemptMs) >= 10000UL) {
      g_wifiSt.lastNtpAttemptMs = now;
      runNtpTimeTest();
    }
  }
#endif

  if ((now - lastHeartbeat) >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeat = now;
    lv_mem_monitor_t lvMon;
    lv_mem_monitor(&lvMon);
    Serial.printf("[HEARTBEAT] uptime=%lu ms, free_heap=%u, free_psram=%u, lv_free=%u/%u%% lv_frag=%u%%\n",
                  now,
                  ESP.getFreeHeap(),
                  ESP.getFreePsram(),
                  (unsigned)lvMon.free_size,
                  (unsigned)lvMon.used_pct,
                  (unsigned)lvMon.frag_pct);
  }

  if ((now - lastSummary) >= SUMMARY_INTERVAL_MS) {
    lastSummary = now;
    printRuntimeSummary(now);
  }

#if TEST_IMU
  syncImuActiveForUi();
  runImuLoop();
#endif

#if TEST_DISPLAY && TEST_NTP
  updateWeatherFromApi(false);
  updateTransitFromApi(false);
  updateLaunchFromApi(false);
#if TEST_WIFI && RSS_ENABLED
#if TEST_LVGL_UI && DISPLAY_BACKEND_ESP_LCD
  updateRssFromFeed(false);
  updateWikiFromFeed(false);
  wikiPreloadMetaStep();
  wikiPreloadVisibleItemStep();
#else
  updateRssFromFeed(false);
  updateWikiFromFeed(false);
  wikiPreloadMetaStep();
#endif
#endif
  handleTouchSwipeInput();

  // Launch countdown — tick every second when visible
  {
    static uint32_t sLastLaunchTickMs = 0;
    if (g_uiPageMode == UI_PAGE_LAUNCH && (now - sLastLaunchTickMs) >= 1000) {
      sLastLaunchTickMs = now;
      lvglTickLaunchCountdown();
      g_uiNeedsRedraw = true;
    }
  }

#if TEST_LVGL_UI && DISPLAY_BACKEND_ESP_LCD
#if SCREENSAVER_ENABLED
  handleScreenSaverLoop(now);
#endif
  updateLvglUi(false);
  runLvglLoop();
#if TEST_DISPLAY && DISPLAY_BACKEND_ESP_LCD
  doomRenderSpike(false);
#endif
#else
  updateDisplayClock(false);
#endif
#endif
}
