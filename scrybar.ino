#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include "src/ui_strings.h"
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
#include <WebServer.h>
#include <DNSServer.h>
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
LV_FONT_DECLARE(scry_font_space_mono_16);
LV_FONT_DECLARE(scry_font_space_mono_18);
LV_FONT_DECLARE(scry_font_space_mono_12);
LV_FONT_DECLARE(scry_font_space_mono_20);
LV_FONT_DECLARE(scry_font_space_mono_24);
LV_FONT_DECLARE(scry_font_space_mono_28);
LV_FONT_DECLARE(scry_font_space_mono_32);
LV_FONT_DECLARE(scry_font_delius_unicase_12);
LV_FONT_DECLARE(scry_font_delius_unicase_16);
LV_FONT_DECLARE(scry_font_delius_unicase_18);
LV_FONT_DECLARE(scry_font_delius_unicase_20);
LV_FONT_DECLARE(scry_font_delius_unicase_24);
LV_FONT_DECLARE(scry_font_delius_unicase_28);
LV_FONT_DECLARE(scry_font_delius_unicase_32);
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
#elif __has_include("assets/weather_demo/weather_images.h")
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

#ifndef RSS_SHORTENER_TOKEN
#if defined(SPOO_ME_API_KEY)
#define RSS_SHORTENER_TOKEN SPOO_ME_API_KEY
#elif defined(SPOOME_API_KEY)
#define RSS_SHORTENER_TOKEN SPOOME_API_KEY
#else
#define RSS_SHORTENER_TOKEN ""
#endif
#endif
#ifndef RSS_SHORTENER_ENDPOINT
#define RSS_SHORTENER_ENDPOINT "https://spoo.me/api/v1/shorten"
#endif
#ifndef RSS_SHORTENER_ENDPOINT_ALT
#define RSS_SHORTENER_ENDPOINT_ALT "https://spoo.me/shorten"
#endif
#ifndef RSS_SHORTENER_ENDPOINT_HTTP
#define RSS_SHORTENER_ENDPOINT_HTTP "http://spoo.me/api/v1/shorten"
#endif
#ifndef RSS_SHORTENER_ENDPOINT_HTTP_ALT
#define RSS_SHORTENER_ENDPOINT_HTTP_ALT "http://api.spoo.me/v1/shorten"
#endif
#ifndef RSS_SHORTENER_CONNECT_TIMEOUT_MS
#define RSS_SHORTENER_CONNECT_TIMEOUT_MS 12000
#endif
#ifndef RSS_SHORTENER_HTTP_TIMEOUT_MS
#define RSS_SHORTENER_HTTP_TIMEOUT_MS 12000
#endif
#ifndef RSS_SHORTENER_ALLOW_HTTP_FALLBACK
#define RSS_SHORTENER_ALLOW_HTTP_FALLBACK 1
#endif
#ifndef RSS_SHORTENER_CACHE_SIZE
#define RSS_SHORTENER_CACHE_SIZE 24
#endif
#if RSS_SHORTENER_CACHE_SIZE < 4
#undef RSS_SHORTENER_CACHE_SIZE
#define RSS_SHORTENER_CACHE_SIZE 4
#endif
#ifndef RSS_FAVICON_CACHE_SIZE
#define RSS_FAVICON_CACHE_SIZE 12
#endif
#if RSS_FAVICON_CACHE_SIZE < 4
#undef RSS_FAVICON_CACHE_SIZE
#define RSS_FAVICON_CACHE_SIZE 4
#endif
#ifndef RSS_FAVICON_MAX_BYTES
#define RSS_FAVICON_MAX_BYTES 8192
#endif
#ifndef RSS_FAVICON_RETRY_MS
#define RSS_FAVICON_RETRY_MS 300000UL
#endif
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
static bool g_pwrButtonDown = false;
static uint32_t g_pwrButtonDownMs = 0;
static bool g_pwrHoldReported = false;
static int g_pwrLastRawLevel = -1;
static uint32_t g_pwrPressCandidateMs = 0;
static uint32_t g_pwrReleaseCandidateMs = 0;
static constexpr uint32_t kPwrPressDebounceMs = 45UL;
static constexpr uint32_t kPwrShortPressMinMs = 90UL;
static bool g_navFirstButtonDown = false;
static uint32_t g_navFirstButtonDownMs = 0;
static int g_navFirstLastRawLevel = -1;
static uint32_t g_navFirstReleaseCandidateMs = 0;
static bool g_softPowerOff = false;
#if TEST_BATTERY
static adc_oneshot_unit_handle_t g_battAdcHandle = nullptr;
static bool g_battReady = false;
static bool g_battHasSample = false;
static int g_battRaw = 0;
static float g_battVoltage = 0.0f;
static int g_battPercent = -1;
static uint32_t g_battLastSampleMs = 0;
static bool g_battChargingLikely = false;
static uint32_t g_battTrendMs = 0;
static float g_battTrendVoltage = 0.0f;
static bool g_battExternalPowerLikely = false;
static uint32_t g_battExternalPowerHoldUntilMs = 0;
#endif

#if TEST_BATTERY && ENERGY_SAVER_ENABLED
static bool g_energyBatteryMode = false;
static uint32_t g_energyLastEvalMs = 0;
#endif
#if TEST_WIFI || TEST_NTP
static constexpr uint8_t WIFI_STATIC_CREDENTIALS_MAX = 5;
static constexpr uint8_t WIFI_TOTAL_CREDENTIALS_MAX = WIFI_STATIC_CREDENTIALS_MAX + WIFI_RUNTIME_CREDENTIALS_MAX;
static constexpr size_t WIFI_MAX_SSID_LEN = 32;
static constexpr size_t WIFI_MAX_PASSWORD_LEN = 64;
struct RuntimeWiFiCredential {
  char ssid[WIFI_MAX_SSID_LEN + 1] = {0};
  char password[WIFI_MAX_PASSWORD_LEN + 1] = {0};
};
static bool g_wifiConnected = false;
static int g_lastWifiDiscReason = -1;
static bool g_wifiEventRegistered = false;
static bool g_wifiEverConnected = false;
static uint32_t g_wifiLastConnectMs = 0;
static uint32_t g_wifiLastDisconnectMs = 0;
static bool g_shortenerDnsDiagDone = false;
static const char *g_wifiStaticSsids[WIFI_STATIC_CREDENTIALS_MAX] = {nullptr};
static const char *g_wifiStaticPasswords[WIFI_STATIC_CREDENTIALS_MAX] = {nullptr};
static size_t g_wifiStaticCredCount = 0;
static RuntimeWiFiCredential g_wifiRuntimeCreds[WIFI_RUNTIME_CREDENTIALS_MAX];
static uint8_t g_wifiRuntimeCredCount = 0;
static const char *g_wifiCredSsids[WIFI_TOTAL_CREDENTIALS_MAX] = {nullptr};
static const char *g_wifiCredPasswords[WIFI_TOTAL_CREDENTIALS_MAX] = {nullptr};
static size_t g_wifiCredCount = 0;
static char g_wifiPreferredSsid[33] = "";
static char g_wifiSetupMode[8] = WIFI_SETUP_MODE_DEFAULT;  // off | auto | on
static bool g_wifiSetupApActive = false;
static bool g_wifiSetupApAutoStarted = false;
static uint32_t g_wifiNoLinkSinceMs = 0;
static char g_wifiSetupApSsid[33] = "";
static bool g_wifiReconnectAttemptActive = false;
static uint8_t g_wifiReconnectIdx = 0;
static uint32_t g_wifiReconnectAttemptStartMs = 0;
static uint32_t g_wifiReconnectNextAttemptMs = 0;
static uint16_t g_wifiConsecutiveFailCount = 0;
static uint32_t g_wifiLastRadioResetMs = 0;
static bool g_wifiInternalDisconnect = false;
static uint32_t g_lastNtpAttemptMs = 0;
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
static constexpr uint8_t UI_VIEW_FLAG_DOOM = 0x10;
static constexpr uint8_t UI_VIEW_MASK_DEFAULT =
    UI_VIEW_FLAG_INFO |
    UI_VIEW_FLAG_AUX |
    UI_VIEW_FLAG_WIKI |
    UI_VIEW_FLAG_DOOM;

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
static const char* const kAllowedLangs[] = {"it", "tlh", "en", "fr", "de", "es", "pt", "la", "eo", "l33t", "sha", "val", "bellazio", nullptr};
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
static WebServer g_webConfigServer(WEB_CONFIG_PORT);
static bool g_webConfigServerStarted = false;
static bool g_webConfigRoutesRegistered = false;
static DNSServer g_webConfigDnsServer;
static bool g_webConfigDnsStarted = false;
#if DB_HAS_QRCODEGEN
// Allocate QR work buffers lazily in PSRAM to keep internal heap free for TLS.
static uint8_t *g_webQrTempBuf = nullptr;
static uint8_t *g_webQrDataBuf = nullptr;
#endif
#endif

static bool updateRssFromFeed(bool force);
static bool updateWikiFromFeed(bool force);
static bool updateWeatherFromApi(bool force);
static void handleWebConfigServerLoop();
static void formatCityLabel(const char *src, char *out, size_t outLen);
static const char *runtimeUiThemeId();
static const char *runtimeUiThemeLabel();
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI
static void lvglApplyThemeStyles(bool forceInvalidate);
#endif
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI && SCREENSAVER_ENABLED
static void markUserInteraction(uint32_t nowMs);
static void lvglSetScreenSaverActive(bool on);
static void handleScreenSaverLoop(uint32_t nowMs);
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
};
static WeatherState g_weather;

struct RssItem {
  char title[220];
  char link[280];
  char pubDate[32];
  char summary[420];
  char shortLink[96];
  uint8_t feedSlot = 0xFF;
  bool wikiMetaReady = false;
  bool wikiMetaTried = false;
  bool shortReady = false;
  bool shortTried = false;
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
  uint32_t lastShortenAttemptMs = 0;
  int lastHttpCode = 0;
};
static RssState g_rss = {};
static RssState g_wiki = {};
static RssItem *g_rssParseBuf = nullptr;
struct RssShortCacheEntry {
  char longUrl[280];
  char shortUrl[96];
  uint32_t updatedMs = 0;
  bool valid = false;
};
static RssShortCacheEntry g_rssShortCache[RSS_SHORTENER_CACHE_SIZE];
static uint32_t g_wikiMetaPreloadLastMs = 0;
static uint32_t g_wikiVisiblePreloadLastMs = 0;
#endif

#if TEST_NTP
static bool g_ntpSynced = false;
static int g_lastClockSecond = -1;
static int g_lastDateKey = -1;
static bool g_clockStaticDrawn = false;
static uint32_t g_bellazioLastMinuteKey = 0xFFFFFFFFu;
static uint8_t g_bellazioLastLeadIdx = 0;
static uint8_t g_bellazioLastCloserIdx = 0;
enum UiClockMode : uint8_t {
  UI_CLOCK_MODE_CLOCKCLOCK = 0,
  UI_CLOCK_MODE_WORDCLOCK = 1,
};
static UiClockMode g_uiClockMode = UI_CLOCK_MODE_WORDCLOCK;
enum UiPageMode : uint8_t {
  UI_PAGE_INFO = 0,
  UI_PAGE_HOME = 1,
  UI_PAGE_AUX = 2,
  UI_PAGE_WIKI = 3,
  UI_PAGE_DOOM = 4,
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
static lv_obj_t *g_lvglClockL1 = nullptr;
static lv_obj_t *g_lvglClockL2 = nullptr;
static lv_obj_t *g_lvglClockL3 = nullptr;
static lv_obj_t *g_lvglClockDate = nullptr;
static lv_obj_t *g_lvglClockHeader = nullptr;
static lv_obj_t *g_lvglClockHeaderFill = nullptr;
static lv_obj_t *g_lvglClockWiFiBars[4] = {nullptr, nullptr, nullptr, nullptr};
static uint16_t g_lvglClockWiFiMask = 0xFFFF;
static lv_obj_t *g_lvglClockDivider = nullptr;
static lv_obj_t *g_lvglInfoRoot = nullptr;
static lv_obj_t *g_lvglInfoCard = nullptr;
static lv_obj_t *g_lvglInfoHeader = nullptr;
static lv_obj_t *g_lvglInfoHeaderFill = nullptr;
static lv_obj_t *g_lvglInfoTitle = nullptr;
static lv_obj_t *g_lvglInfoEndpoint = nullptr;
static lv_obj_t *g_lvglInfoBodyLeft = nullptr;
static lv_obj_t *g_lvglInfoBodyRight = nullptr;
#if defined(LV_USE_QRCODE) && LV_USE_QRCODE
static lv_obj_t *g_lvglInfoWebQr = nullptr;
#endif
static char g_lvglInfoLastQrPayload[96] = {0};
static lv_obj_t *g_lvglHomeRoot = nullptr;
static lv_obj_t *g_lvglClockBlock = nullptr;
static lv_obj_t *g_lvglWeatherCard = nullptr;
static lv_obj_t *g_lvglWeatherHeader = nullptr;
static lv_obj_t *g_lvglWeatherHeaderFill = nullptr;
static lv_obj_t *g_lvglWeatherBody = nullptr;
static lv_obj_t *g_lvglCity = nullptr;
static bool g_lvglCityTickerScroll = false;
static uint32_t g_lvglCityTickerNextMs = 0;
static uint32_t g_lvglCityTickerEndMs = 0;
static char g_lvglCityRawLast[48] = {0};
static lv_obj_t *g_lvglTemp = nullptr;
static lv_obj_t *g_lvglIcon = nullptr;
static lv_obj_t *g_lvglGlyph = nullptr;
static lv_obj_t *g_lvglDesc = nullptr;
static lv_obj_t *g_lvglHumidity = nullptr;
static lv_obj_t *g_lvglSun = nullptr;
static lv_obj_t *g_lvglWind = nullptr;
static lv_obj_t *g_lvglWeatherSep = nullptr;
static lv_obj_t *g_lvglForecastBar = nullptr;
static lv_obj_t *g_lvglForecastBarFill = nullptr;
static lv_obj_t *g_lvglForecastIcon = nullptr;
static lv_obj_t *g_lvglForecastNow = nullptr;
static lv_obj_t *g_lvglForecastTomorrow = nullptr;
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
static lv_obj_t *g_lvglAuxRoot = nullptr;
static lv_obj_t *g_lvglWikiRoot = nullptr;
static lv_obj_t *g_lvglDoomRoot = nullptr;
#if TEST_DISPLAY && DOOM_SPIKE_ENABLED
static constexpr uint8_t DOOM_TOUCH_NONE = 0;
static constexpr uint8_t DOOM_TOUCH_LEFT = 1;
static constexpr uint8_t DOOM_TOUCH_CENTER = 2;
static constexpr uint8_t DOOM_TOUCH_RIGHT = 3;
static bool      g_doomPaletteReady = false;
static bool      g_doomFrameDirty   = true;
static bool      g_doomLaunchRequested = false;
static uint16_t  g_doomPalette565[256] = {0};
static uint32_t  g_doomLastRenderLogMs = 0;
static constexpr int16_t kDoomFrameW = ((LCD_WIDTH * 4) + 2) / 3;  // 4:3 content in 172px height
static constexpr int16_t kDoomFrameH = LCD_WIDTH;
static constexpr int16_t kDoomFrameX = (LCD_HEIGHT - kDoomFrameW) / 2;
static constexpr int16_t kDoomFrameY = 0;
static constexpr int16_t kDoomLeftBandW = kDoomFrameX;
static constexpr int16_t kDoomRightBandX = kDoomFrameX + kDoomFrameW;
static constexpr float   kDoomRadToDeg = 57.2957795f;
static constexpr float   kDoomTiltComplementaryAlpha = 0.94f;
static constexpr float   kDoomMoveDeadbandDeg = 4.5f;
static constexpr float   kDoomTurnDeadbandDeg = 4.0f;
static constexpr float   kDoomMoveBinDeg = 4.5f;
static constexpr float   kDoomTurnBinDeg = 4.0f;
static constexpr uint16_t kDoomNeutralCaptureDelayMs = 180;
static constexpr uint16_t kDoomNeutralStableWindowMs = 320;
static constexpr uint16_t kDoomNeutralStableMinSamples = 8;
static constexpr float   kDoomNeutralCaptureGyroMaxDps = 12.0f;
static constexpr float   kDoomAxisResponseAlpha = 0.24f;
static constexpr int8_t  kDoomMoveBinMin = -6;
static constexpr int8_t  kDoomMoveBinMax = 6;
static constexpr int8_t  kDoomTurnBinMin = -6;
static constexpr int8_t  kDoomTurnBinMax = 6;
static constexpr int8_t  kDoomMoveTiltSign = -1;
static constexpr int8_t  kDoomTurnTiltSign = 1;
static uint8_t g_doomTouchZone = DOOM_TOUCH_NONE;
static bool g_doomTiltFilterReady = false;
static bool g_doomNeutralPending = false;
static bool g_doomNeutralReady = false;
static uint32_t g_doomNeutralArmAtMs = 0;
static uint32_t g_doomNeutralStableSinceMs = 0;
static uint32_t g_doomLastTiltSampleMs = 0;
static float g_doomMoveTiltDeg = 0.0f;
static float g_doomTurnTiltDeg = 0.0f;
static float g_doomNeutralMoveTiltDeg = 0.0f;
static float g_doomNeutralTurnTiltDeg = 0.0f;
static float g_doomNeutralAccumMoveDeg = 0.0f;
static float g_doomNeutralAccumTurnDeg = 0.0f;
static uint16_t g_doomNeutralStableSamples = 0;
static bool g_doomAxisFilterReady = false;
static float g_doomMoveDeltaFilteredDeg = 0.0f;
static float g_doomTurnDeltaFilteredDeg = 0.0f;
static int8_t g_doomMoveBin = 0;
static int8_t g_doomTurnBin = 0;
#endif
static uint32_t g_lvglPageAnimUntilMs = 0;
static uint32_t g_lvglLastRunMs = 0;
static bool g_lvglPageDragActive = false;
#if SCREENSAVER_ENABLED
static constexpr uint8_t kSaverSkyRowsMax = 10;
static constexpr uint8_t kSaverSkyColsMax = 80;
static constexpr uint8_t kSaverStarsPerRow = 2;
static lv_obj_t *g_lvglScreenSaverRoot = nullptr;
static lv_obj_t *g_lvglScreenSaverSky = nullptr;
static lv_obj_t *g_lvglScreenSaverStarObj[kSaverSkyRowsMax][kSaverStarsPerRow] = {};
static lv_obj_t *g_lvglScreenSaverField = nullptr;
static lv_obj_t *g_lvglScreenSaverCow = nullptr;
static lv_obj_t *g_lvglScreenSaverBalloon = nullptr;
static lv_obj_t *g_lvglScreenSaverBalloonTail = nullptr;
static lv_obj_t *g_lvglScreenSaverFooter = nullptr;
static bool g_lvglScreenSaverActive = false;
static uint32_t g_lastUserInteractionMs = 0;
static uint32_t g_lvglScreenSaverLastStepMs = 0;
static uint32_t g_lvglScreenSaverRand = 0x1A2B3C4Du;
static int16_t g_lvglScreenSaverX = -80;
static int16_t g_lvglScreenSaverY = 0;
static int8_t g_lvglScreenSaverColorIdx = 0;
static uint32_t g_lvglScreenSaverWakeGuardUntilMs = 0;
static uint32_t g_lvglScreenSaverCowNextMoveMs = 0;
static uint8_t g_lvglScreenSaverCowStepsLeft = 0;
static int8_t g_lvglScreenSaverCowDir = 1;
static uint8_t g_lvglScreenSaverCols = 60;
static uint8_t g_lvglScreenSaverRows = 6;
static uint8_t g_lvglScreenSaverStarX[kSaverSkyRowsMax][kSaverStarsPerRow] = {};
static uint8_t g_lvglScreenSaverStarLevel[kSaverSkyRowsMax][kSaverStarsPerRow] = {};
static int8_t g_lvglScreenSaverStarDir[kSaverSkyRowsMax][kSaverStarsPerRow] = {};
static uint32_t g_lvglScreenSaverStarNextMs[kSaverSkyRowsMax][kSaverStarsPerRow] = {};
static uint8_t g_lvglScreenSaverBalloonIdx = 0;
static uint32_t g_lvglScreenSaverBalloonNextMs = 0;
static bool g_lvglScreenSaverBalloonVisible = false;
static uint32_t g_lvglScreenSaverFooterNextMs = 0;
static uint32_t g_lvglScreenSaverFooterJitterNextMs = 0;
static uint8_t g_lvglScreenSaverFooterJitterIdx = 0;
static uint32_t g_lvglScreenSaverFieldNextMs = 0;
static uint8_t g_lvglScreenSaverFieldScroll = 0;
static char g_lvglScreenSaverFieldBuf[256] = {0};
#endif
#endif

#if TEST_TOUCH
static bool g_touchReady = false;
static bool g_touchUseAltBus = true;
static bool g_touchDown = false;
static uint8_t g_touchMissCount = 0;  // consecutive "no touch" frames since last detection
static uint8_t g_touchRawPresenceCount = 0;
static bool g_touchPageDragging = false;
enum TouchAuxButton : uint8_t {
  TOUCH_AUX_BTN_NONE = 0,
  TOUCH_AUX_BTN_QR = 1,
  TOUCH_AUX_BTN_REFRESH = 2,
  TOUCH_AUX_BTN_NEXT = 3,
};
static TouchAuxButton g_touchAuxBtnDown = TOUCH_AUX_BTN_NONE;
static uint32_t g_lastSwipeToggleMs = 0;
static bool g_touchAwaitRelease = false;
static uint32_t g_touchReleaseStartMs = 0;
static int16_t g_touchStartX = 0;
static int16_t g_touchStartY = 0;
static int16_t g_touchLastX = 0;
static int16_t g_touchLastY = 0;
static uint32_t g_touchStartMs = 0;
#endif

#if TEST_IMU
static bool g_imuReady = false;
static bool g_imuSensorsActive = false;
static uint8_t g_imuAddr = 0;
static uint32_t g_lastImuPrintMs = 0;
static uint32_t g_lastShakeMs = 0;
static float g_lastAccelMag = 1.0f;
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
static esp_lcd_panel_io_handle_t g_panelIo = nullptr;
static esp_lcd_panel_handle_t g_panel = nullptr;
static SemaphoreHandle_t g_dispFlushSem = nullptr;
static uint16_t *g_canvasBuf = nullptr;  // logical 640x172
// g_rotBuf eliminated — rotation now done directly into DMA chunks
static uint16_t *g_dmaBuf = nullptr;     // native chunk (172x32) — ping
static uint16_t *g_dmaBuf2 = nullptr;    // native chunk (172x32) — pong
static constexpr int16_t DB_CANVAS_W = LCD_HEIGHT;  // 640
static constexpr int16_t DB_CANVAS_H = LCD_WIDTH;   // 172
static constexpr int16_t DB_NATIVE_W = LCD_WIDTH;   // 172
static constexpr int16_t DB_NATIVE_H = LCD_HEIGHT;  // 640
// Keep DMA chunks small enough to leave internal heap available for TLS handshakes.
static constexpr int16_t DB_CHUNK_ROWS = 32;

// --- Frame performance counters (lightweight, no per-frame logging) ---
static uint32_t g_perfFlushCount = 0;
static uint32_t g_perfFlushTotalUs = 0;
static uint32_t g_perfFlushMaxUs = 0;
static uint32_t g_perfLvglFrameCount = 0;
static uint32_t g_perfLvglTotalUs = 0;
static uint32_t g_perfLvglMaxUs = 0;
static uint32_t g_perfLastResetMs = 0;
static bool g_canvasDirty = false;  // set by flush callback, cleared after dispFlush
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
  if (adc_oneshot_new_unit(&initCfg, &g_battAdcHandle) != ESP_OK) {
    Serial.println("[BATT][ERR] adc_oneshot_new_unit failed");
    g_battReady = false;
    return;
  }

  adc_oneshot_chan_cfg_t chanCfg = {};
  chanCfg.atten = ADC_ATTEN_DB_12;
  chanCfg.bitwidth = ADC_BITWIDTH_12;
  if (adc_oneshot_config_channel(g_battAdcHandle, (adc_channel_t)BATTERY_ADC_CHANNEL, &chanCfg) != ESP_OK) {
    Serial.println("[BATT][ERR] adc_oneshot_config_channel failed");
    g_battReady = false;
    return;
  }

  g_battReady = true;
  Serial.printf("[BATT] monitor ready (ADC1_CH%d)\n", BATTERY_ADC_CHANNEL);
}

static bool sampleBatteryNow(uint32_t nowMs, bool force) {
  if (!g_battReady || !g_battAdcHandle) return false;
  if (!force && (nowMs - g_battLastSampleMs) < BATTERY_SAMPLE_INTERVAL_MS) return false;
  const bool hadPrev = g_battHasSample;
  const uint32_t prevTs = g_battLastSampleMs;
  const float prevV = g_battVoltage;
  g_battLastSampleMs = nowMs;

  int raw = 0;
  if (adc_oneshot_read(g_battAdcHandle, (adc_channel_t)BATTERY_ADC_CHANNEL, &raw) != ESP_OK) {
    Serial.println("[BATT][ERR] adc_oneshot_read failed");
    return false;
  }

  // Vendor demo applies x3 divider factor to recover battery voltage.
  const float adcVolts = ((float)raw * 3.3f) / 4095.0f;
  const float vbat = adcVolts * BATTERY_DIVIDER_RATIO;
  const int pct = batteryPercentFromVoltage(vbat);

  g_battRaw = raw;
  g_battVoltage = vbat;
  g_battPercent = pct;
  g_battHasSample = true;

  if (!hadPrev) {
    g_battTrendMs = nowMs;
    g_battTrendVoltage = vbat;
  } else if (prevTs > 0 && nowMs > prevTs) {
    const uint32_t dtMs = nowMs - prevTs;
    const float dvNow = vbat - prevV;
    const float mvPerMinNow = (dvNow * 1000.0f) * (60000.0f / (float)dtMs);
    // Fast hint for cable plug/unplug responsiveness between two consecutive samples.
    if (mvPerMinNow >= 18.0f) {
      g_battExternalPowerLikely = true;
      g_battExternalPowerHoldUntilMs = nowMs + 180000UL;
    } else if (mvPerMinNow <= -18.0f) {
      g_battExternalPowerLikely = false;
      g_battExternalPowerHoldUntilMs = 0;
    }
    if (g_battTrendMs == 0) {
      g_battTrendMs = prevTs;
      g_battTrendVoltage = prevV;
    }
    const uint32_t trendDtMs = nowMs - g_battTrendMs;
    // Evaluate slope over a longer window to avoid ADC jitter flips.
    if (trendDtMs >= 45000UL) {
      const float dv = vbat - g_battTrendVoltage;
      const float mvPerMin = (dv * 1000.0f) * (60000.0f / (float)trendDtMs);
      if (mvPerMin >= 6.0f) g_battChargingLikely = true;
      else if (mvPerMin <= -6.0f) g_battChargingLikely = false;
      g_battTrendMs = nowMs;
      g_battTrendVoltage = vbat;
    }
  }

  if (g_battChargingLikely) {
    g_battExternalPowerLikely = true;
    g_battExternalPowerHoldUntilMs = nowMs + 180000UL;
  } else if (g_battExternalPowerLikely && nowMs >= g_battExternalPowerHoldUntilMs) {
    g_battExternalPowerLikely = false;
  }

  Serial.printf("[BATT] raw=%d vbat=%.3fV soc=%d%%\n", raw, vbat, pct);
  return true;
}

static const char *batteryPowerModeText() {
  if (!g_battHasSample) return "UNKNOWN";
  return g_battChargingLikely ? "CHARGING" : "BATTERY";
}

static bool batteryExternalPowerLikelyNow(uint32_t nowMs) {
  if (!g_battHasSample) return false;
  if (g_battChargingLikely) return true;
  if (g_battExternalPowerLikely && nowMs < g_battExternalPowerHoldUntilMs) return true;
  return false;
}

static const char *batteryPowerSourceText(uint32_t nowMs) {
  if (!g_battHasSample) return "UNKNOWN";
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
  if (!force && (nowMs - g_energyLastEvalMs) < 2000UL) return;
  g_energyLastEvalMs = nowMs;
  const bool nextBatteryMode = g_battHasSample && !batteryExternalPowerLikelyNow(nowMs);
  if (!force && nextBatteryMode == g_energyBatteryMode) return;
  g_energyBatteryMode = nextBatteryMode;
  const uint8_t targetBacklight = g_energyBatteryMode ? ENERGY_BACKLIGHT_ON_BATTERY : 100;
  setBacklightPercent(targetBacklight);
  Serial.printf("[ENERGY] mode=%s backlight=%u%% batt=%d%% src=%s\n",
                g_energyBatteryMode ? "BATTERY" : "USB-C",
                (unsigned)targetBacklight,
                g_battPercent,
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
  if (g_ntpSynced) {
    struct tm ti;
    if (getLocalTime(&ti, 20)) {
      strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &ti);
    }
  }
#endif

#if TEST_WIFI
  const bool wifiOk = (WiFi.status() == WL_CONNECTED) && g_wifiConnected;
  const char *wifiState = wifiOk ? "CONNECTED" : "DISCONNECTED";
  const char *themeState = runtimeUiThemeId();
#else
  const char *wifiState = "OFF";
  const char *themeState = activeUiThemeId();
#endif

#if TEST_NTP
  const char *ntpState = g_ntpSynced ? "SYNCED" : "UNSYNCED";
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
  if (g_battHasSample) {
    snprintf(battBuf, sizeof(battBuf), "%s %.3fV %d%%", batteryPowerModeText(), g_battVoltage, g_battPercent);
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
    const uint32_t window = nowMs - g_perfLastResetMs;
    const uint32_t flushAvg = g_perfFlushCount ? (g_perfFlushTotalUs / g_perfFlushCount) : 0;
    const uint32_t lvglAvg = g_perfLvglFrameCount ? (g_perfLvglTotalUs / g_perfLvglFrameCount) : 0;
    const uint32_t fps = (window > 0 && g_perfFlushCount > 0) ? (g_perfFlushCount * 1000UL / window) : 0;
    Serial.printf("[PERF] window=%lums flush=%lu frames avg=%luus max=%luus lvgl_handler=%lu calls avg=%luus max=%luus fps=%lu\n",
                  window, g_perfFlushCount, flushAvg, g_perfFlushMaxUs,
                  g_perfLvglFrameCount, lvglAvg, g_perfLvglMaxUs, fps);
    g_perfFlushCount = 0; g_perfFlushTotalUs = 0; g_perfFlushMaxUs = 0;
    g_perfLvglFrameCount = 0; g_perfLvglTotalUs = 0; g_perfLvglMaxUs = 0;
    g_perfLastResetMs = nowMs;
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
  g_pwrLastRawLevel = gpio_get_level((gpio_num_t)PWR_BUTTON_PIN);
  Serial.printf("[PWR] init pin=%d raw=%d active_low=%d\n",
                PWR_BUTTON_PIN,
                g_pwrLastRawLevel,
                PWR_BUTTON_ACTIVE_LOW ? 1 : 0);
}

static void prepareNavFirstButtonPin() {
  pinMode(NAV_FIRST_BUTTON_PIN, NAV_FIRST_BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
  g_navFirstLastRawLevel = gpio_get_level((gpio_num_t)NAV_FIRST_BUTTON_PIN);
  Serial.printf("[NAV] first-btn init pin=%d raw=%d active_low=%d\n",
                NAV_FIRST_BUTTON_PIN,
                g_navFirstLastRawLevel,
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
  g_wifiConnected = false;
#endif
#if TEST_NTP
  g_ntpSynced = false;
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

static void enterSoftPowerOffFromPowerButton() {
  g_softPowerOff = true;
  Serial.println("[PWR] Entering soft-off fallback (USB-safe).");
  setBacklightPercent(0);
#if TEST_WIFI
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
  g_wifiConnected = false;
#endif
#if TEST_NTP
  g_ntpSynced = false;
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
        Serial.println("[PWR] Soft-off wake confirmed, restarting.");
        delay(80);
        esp_restart();
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
  (void)nowMs;
  if (!g_lvglReady) {
    Serial.println("[PWR] Short press: screensaver unavailable (LVGL not ready).");
    return;
  }
  lvglSetScreenSaverActive(true);
  Serial.println("[PWR] Short press: screensaver ON.");
#else
  (void)nowMs;
  Serial.println("[PWR] Short press ignored (screensaver disabled).");
#endif
}

static void handlePowerButtonLoop(uint32_t nowMs) {
  const int rawLevel = gpio_get_level((gpio_num_t)PWR_BUTTON_PIN);
  if (rawLevel != g_pwrLastRawLevel) {
    Serial.printf("[PWR] raw level change: %d -> %d\n", g_pwrLastRawLevel, rawLevel);
    g_pwrLastRawLevel = rawLevel;
  }

  const bool pressed = isPwrButtonPressed();
  if (pressed) {
    g_pwrReleaseCandidateMs = 0;
    if (!g_pwrButtonDown) {
      if (g_pwrPressCandidateMs == 0) {
        g_pwrPressCandidateMs = nowMs;
        return;
      }
      if ((nowMs - g_pwrPressCandidateMs) < kPwrPressDebounceMs) {
        return;
      }
      g_pwrButtonDown = true;
      g_pwrButtonDownMs = g_pwrPressCandidateMs;
      g_pwrHoldReported = false;
      Serial.println("[PWR] Button down.");
    } else {
      const uint32_t heldMs = nowMs - g_pwrButtonDownMs;
      if (!g_pwrHoldReported && heldMs >= 1000UL) {
        g_pwrHoldReported = true;
        Serial.printf("[PWR] Keep holding (%lu/%d ms)\n", (unsigned long)heldMs, PWR_HOLD_SHUTDOWN_MS);
      }
    }
    return;
  }

  g_pwrPressCandidateMs = 0;
  if (!g_pwrButtonDown) return;

  if (g_pwrReleaseCandidateMs == 0) {
    g_pwrReleaseCandidateMs = nowMs;
    return;
  }
  if ((nowMs - g_pwrReleaseCandidateMs) < (uint32_t)PWR_RELEASE_DEBOUNCE_MS) {
    return;
  }

  const uint32_t heldMs = g_pwrReleaseCandidateMs - g_pwrButtonDownMs;
  if (heldMs >= (uint32_t)PWR_HOLD_SHUTDOWN_MS) {
    shutdownFromPowerButton(false);
  } else if (heldMs >= kPwrShortPressMinMs) {
    Serial.printf("[PWR] Short press (%lu ms).\n", (unsigned long)heldMs);
    onPowerButtonShortPress(nowMs);
  } else {
    Serial.printf("[PWR] Ignored bounce/glitch (%lu ms).\n", (unsigned long)heldMs);
  }
  g_pwrButtonDown = false;
  g_pwrHoldReported = false;
  g_pwrReleaseCandidateMs = 0;
}

static void onNavFirstButtonShortPress(uint32_t nowMs) {
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI && SCREENSAVER_ENABLED
  markUserInteraction(nowMs);
  if (g_lvglReady && g_lvglScreenSaverActive) {
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
  if (rawLevel != g_navFirstLastRawLevel) {
    Serial.printf("[NAV] first-btn raw level change: %d -> %d\n", g_navFirstLastRawLevel, rawLevel);
    g_navFirstLastRawLevel = rawLevel;
  }

  const bool pressed = isNavFirstButtonPressed();
  if (pressed) {
    g_navFirstReleaseCandidateMs = 0;
    if (!g_navFirstButtonDown) {
      g_navFirstButtonDown = true;
      g_navFirstButtonDownMs = nowMs;
    }
    return;
  }

  if (!g_navFirstButtonDown) return;
  if (g_navFirstReleaseCandidateMs == 0) {
    g_navFirstReleaseCandidateMs = nowMs;
    return;
  }
  if ((nowMs - g_navFirstReleaseCandidateMs) < (uint32_t)NAV_BUTTON_RELEASE_DEBOUNCE_MS) {
    return;
  }

  const uint32_t heldMs = g_navFirstReleaseCandidateMs - g_navFirstButtonDownMs;
  if (heldMs <= (uint32_t)NAV_BUTTON_TAP_MAX_MS) {
    onNavFirstButtonShortPress(nowMs);
  } else {
    Serial.printf("[NAV] first-btn long press ignored (%lu ms)\n", (unsigned long)heldMs);
  }

  g_navFirstButtonDown = false;
  g_navFirstReleaseCandidateMs = 0;
}

static void handleWakeHoldGate() {
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) return;
  Serial.println("[PWR] Wake from PWR key, waiting 5s hold to continue boot.");
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
  if (g_dispFlushSem) xSemaphoreGiveFromISR(g_dispFlushSem, &taskWoken);
  return false;
}

static inline int16_t dispWidth() { return DB_CANVAS_W; }
static inline int16_t dispHeight() { return DB_CANVAS_H; }

static void dispFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (!g_canvasBuf || w <= 0 || h <= 0) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x >= DB_CANVAS_W || y >= DB_CANVAS_H) return;
  if ((x + w) > DB_CANVAS_W) w = DB_CANVAS_W - x;
  if ((y + h) > DB_CANVAS_H) h = DB_CANVAS_H - y;
  for (int16_t yy = y; yy < (y + h); ++yy) {
    uint16_t *row = g_canvasBuf + (yy * DB_CANVAS_W) + x;
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
  if (!g_canvasBuf) return;
  for (int i = 0; i < (DB_CANVAS_W * DB_CANVAS_H); ++i) g_canvasBuf[i] = color;
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

static inline int8_t doomQuantizeAxis(float deltaDeg, float deadbandDeg, float binDeg,
                                      int8_t minBin, int8_t maxBin) {
  if (fabsf(deltaDeg) < deadbandDeg) return 0;
  return (int8_t)constrain((int)lroundf(deltaDeg / binDeg), (int)minBin, (int)maxBin);
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
  g_doomNeutralPending = true;
  g_doomNeutralReady = false;
  g_doomNeutralArmAtMs = millis() + kDoomNeutralCaptureDelayMs;
  g_doomNeutralStableSinceMs = 0;
  g_doomNeutralStableSamples = 0;
  g_doomNeutralAccumMoveDeg = 0.0f;
  g_doomNeutralAccumTurnDeg = 0.0f;
  g_doomTiltFilterReady = false;
  g_doomLastTiltSampleMs = 0;
  g_doomMoveTiltDeg = 0.0f;
  g_doomTurnTiltDeg = 0.0f;
  g_doomAxisFilterReady = false;
  g_doomMoveDeltaFilteredDeg = 0.0f;
  g_doomTurnDeltaFilteredDeg = 0.0f;
  g_doomMoveBin = 0;
  g_doomTurnBin = 0;
  g_doomFrameDirty = true;
  Serial.println("[DOOM][IMU] neutral calibration requested");
}

static inline void doomFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (!g_canvasBuf || w <= 0 || h <= 0) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if ((x + w) > DB_CANVAS_W) w = DB_CANVAS_W - x;
  if ((y + h) > DB_CANVAS_H) h = DB_CANVAS_H - y;
  if (w <= 0 || h <= 0) return;
  for (int16_t yy = y; yy < (y + h); ++yy) {
    uint16_t *row = g_canvasBuf + ((size_t)yy * DB_CANVAS_W) + x;
    for (int16_t xx = 0; xx < w; ++xx) row[xx] = color;
  }
}

static void doomDrawText(int16_t x, int16_t y, const char *text, uint8_t fgIdx, uint8_t bgIdx,
                         int16_t fontW = 6, int16_t fontH = 12) {
  if (!g_canvasBuf || !text || !*text) return;
  int16_t cursorX = x;
  while (*text) {
    if ((cursorX + fontW) > 0 && cursorX < DB_CANVAS_W &&
        (y + fontH) > 0 && y < DB_CANVAS_H) {
      drawCgaChar(g_canvasBuf, DB_CANVAS_W, cursorX, y, (uint8_t)*text, fgIdx, bgIdx, fontW, fontH);
    }
    cursorX += fontW;
    ++text;
  }
}

static inline void doomSetPixel(int16_t x, int16_t y, uint16_t color) {
  if (!g_canvasBuf) return;
  if (x < 0 || x >= DB_CANVAS_W || y < 0 || y >= DB_CANVAS_H) return;
  g_canvasBuf[((size_t)y * DB_CANVAS_W) + (size_t)x] = color;
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
  if (!g_canvasBuf || !text || !*text || !font) return;
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

static void doomDrawHudBackdrop(int16_t x, int16_t y, int16_t w, int16_t h,
                                uint16_t baseColor, uint16_t lineColor, uint16_t accentColor) {
  if (w <= 0 || h <= 0) return;
  doomFillRect(x, y, w, h, baseColor);
  for (int16_t yy = y + 2; yy < (y + h); yy += 4) {
    doomFillRect(x, yy, w, 1, lineColor);
  }
  for (int16_t yy = y + 10; yy < (y + h); yy += 18) {
    const int16_t inset = (int16_t)(((yy - y) / 6) % 18);
    const int16_t stripeW = min<int16_t>(32, max<int16_t>(12, w - 28));
    doomFillRect(x + 10 + inset, yy, stripeW, 1, accentColor);
    doomFillRect(x + w - stripeW - 10 - inset, yy + 6, stripeW, 1, accentColor);
  }
}

static void doomDrawHudPanel(int16_t x, int16_t y, int16_t w, int16_t h,
                             uint16_t accentColor, bool active) {
  const uint16_t panelBg = lv_color_make(8, 12, 18).full;
  const uint16_t panelStripe = lv_color_make(14, 18, 26).full;
  const uint16_t frameColor = active ? accentColor : lv_color_make(74, 80, 92).full;
  const uint16_t topGlow = active ? accentColor : lv_color_make(42, 48, 56).full;
  const uint16_t shadow = lv_color_make(2, 3, 5).full;
  doomFillRect(x, y, w, h, panelBg);
  for (int16_t yy = y + 7; yy < (y + h - 4); yy += 8) {
    doomFillRect(x + 2, yy, w - 4, 1, panelStripe);
  }
  doomDrawRectOutline(x, y, w, h, frameColor);
  doomFillRect(x + 1, y + 1, w - 2, 2, topGlow);
  doomFillRect(x + 1, y + h - 3, w - 2, 2, shadow);
  doomFillRect(x + 6, y + 6, 10, h - 12, active ? accentColor : panelStripe);
  doomFillRect(x + 5, y + 5, 16, 2, accentColor);
  doomFillRect(x + w - 21, y + 5, 16, 2, accentColor);
  doomFillRect(x + 5, y + h - 7, 16, 2, accentColor);
  doomFillRect(x + w - 21, y + h - 7, 16, 2, accentColor);
}

static void doomDrawHudBadge(int16_t x, int16_t y, int16_t w, int16_t h,
                             const char *text, const lv_font_t *font,
                             uint16_t accentColor, uint16_t textColor) {
  const uint16_t badgeBg = lv_color_make(14, 12, 6).full;
  const uint16_t badgeGlow = lv_color_make(42, 32, 10).full;
  doomFillRect(x, y, w, h, badgeBg);
  doomDrawRectOutline(x, y, w, h, accentColor);
  doomFillRect(x + 1, y + 1, w - 2, 2, badgeGlow);
  doomDrawFontTextCentered(x + (w / 2), y + 3, text, font, textColor);
}

static void doomDrawAxisMeter(int16_t x, int16_t y, int16_t w, int16_t h,
                              int8_t value, int8_t maxAbs,
                              uint16_t bgColor, uint16_t fillColor, uint16_t centerColor) {
  if (w <= 4 || h <= 4 || maxAbs <= 0) return;
  doomFillRect(x, y, w, h, bgColor);
  doomDrawRectOutline(x, y, w, h, centerColor);
  for (int16_t tick = 1; tick < maxAbs; ++tick) {
    const int16_t tickX = x + (w / 2) + (int16_t)((tick * ((w / 2) - 3)) / maxAbs);
    doomFillRect(tickX, y + 2, 1, h - 4, lv_color_make(36, 42, 50).full);
    doomFillRect((x + w - 1) - (tickX - x), y + 2, 1, h - 4, lv_color_make(36, 42, 50).full);
  }
  const int16_t centerX = x + (w / 2);
  doomFillRect(centerX - 1, y - 2, 2, h + 4, centerColor);
  if (value == 0) return;
  const int16_t halfW = (w / 2) - 3;
  if (halfW <= 0) return;
  const int16_t span = max<int16_t>(1, (int16_t)((abs(value) * halfW) / maxAbs));
  if (value < 0) doomFillRect(centerX - span, y + 2, span, h - 4, fillColor);
  else doomFillRect(centerX + 1, y + 2, span, h - 4, fillColor);
}

static void doomDrawAxisMeterVertical(int16_t x, int16_t y, int16_t w, int16_t h,
                                      int8_t value, int8_t maxAbs,
                              uint16_t bgColor, uint16_t fillColor, uint16_t centerColor) {
  if (w <= 4 || h <= 4 || maxAbs <= 0) return;
  doomFillRect(x, y, w, h, bgColor);
  doomDrawRectOutline(x, y, w, h, centerColor);
  for (int16_t tick = 1; tick < maxAbs; ++tick) {
    const int16_t tickY = y + (h / 2) + (int16_t)((tick * ((h / 2) - 3)) / maxAbs);
    doomFillRect(x + 2, tickY, w - 4, 1, lv_color_make(36, 42, 50).full);
    doomFillRect(x + 2, (y + h - 1) - (tickY - y), w - 4, 1, lv_color_make(36, 42, 50).full);
  }
  const int16_t centerY = y + (h / 2);
  doomFillRect(x - 2, centerY - 1, w + 4, 2, centerColor);
  if (value == 0) return;
  const int16_t halfH = (h / 2) - 3;
  if (halfH <= 0) return;
  const int16_t span = max<int16_t>(1, (int16_t)((abs(value) * halfH) / maxAbs));
  if (value < 0) doomFillRect(x + 2, centerY - span, w - 4, span, fillColor);
  else doomFillRect(x + 2, centerY + 1, w - 4, span, fillColor);
}

static void doomDrawActionButton(int16_t x, int16_t y, int16_t w, int16_t h,
                                 const char *label, bool active, uint16_t accentColor) {
  const uint16_t face = active ? lv_color_make(32, 38, 48).full : lv_color_make(18, 22, 30).full;
  const uint16_t border = active ? accentColor : lv_color_make(82, 88, 98).full;
  const uint16_t topGlow = active ? kCgaPalette16[15] : lv_color_make(88, 96, 110).full;
  const uint16_t shadow = lv_color_make(4, 6, 10).full;
  const uint16_t textColor = active ? kCgaPalette16[15] : kCgaPalette16[7];
  doomFillRect(x, y, w, h, shadow);
  doomFillRect(x + 1, y + 1, w - 2, h - 2, face);
  doomDrawRectOutline(x, y, w, h, border);
  doomFillRect(x + 2, y + 2, w - 4, 2, topGlow);
  doomFillRect(x + 2, y + h - 4, w - 4, 2, shadow);
  doomFillRect(x + 8, y + 4, 10, h - 8, accentColor);
  doomDrawFontTextCentered(x + (w / 2) + 8, y + 4, label, &scry_font_space_mono_18, textColor);
}

static void doomDrawBandOverlay() {
  const bool leftActive = (g_doomTouchZone == DOOM_TOUCH_LEFT);
  const bool rightActive = (g_doomTouchZone == DOOM_TOUCH_RIGHT);
  const uint16_t labelColor = kCgaPalette16[15];
  const uint16_t mutedColor = kCgaPalette16[8];
  const uint16_t valueColor = g_imuReady ? kCgaPalette16[14] : kCgaPalette16[12];
  const uint16_t moveColor = kCgaPalette16[11];
  const uint16_t turnColor = kCgaPalette16[14];
  const uint16_t meterBg = lv_color_make(6, 8, 12).full;
  const uint16_t centerLine = lv_color_make(124, 132, 148).full;
  const uint16_t leftBandBg = lv_color_make(4, 6, 11).full;
  const uint16_t rightBandBg = lv_color_make(11, 7, 4).full;
  const uint16_t leftBandLine = lv_color_make(10, 16, 24).full;
  const uint16_t rightBandLine = lv_color_make(24, 16, 8).full;

  const int16_t leftCardX = 24;
  const int16_t leftCardY = 12;
  const int16_t leftCardW = kDoomLeftBandW - 48;
  const int16_t leftCardH = 122;
  const int16_t rightCardX = kDoomRightBandX + 24;
  const int16_t rightCardY = 12;
  const int16_t rightCardW = (DB_CANVAS_W - kDoomRightBandX) - 48;
  const int16_t rightCardH = 122;
  char moveBuf[16];
  char turnBuf[16];
  char promptBuf[24];

#if TEST_IMU
  if (!g_imuReady) {
    snprintf(moveBuf, sizeof(moveBuf), "IMU?");
    snprintf(turnBuf, sizeof(turnBuf), "IMU?");
  } else if (!g_doomNeutralReady) {
    snprintf(moveBuf, sizeof(moveBuf), "CAL");
    snprintf(turnBuf, sizeof(turnBuf), "CAL");
  } else {
    snprintf(moveBuf, sizeof(moveBuf), "FB %+d", (int)g_doomMoveBin);
    snprintf(turnBuf, sizeof(turnBuf), "LR %+d", (int)g_doomTurnBin);
  }
#else
  snprintf(moveBuf, sizeof(moveBuf), "IMU OFF");
  snprintf(turnBuf, sizeof(turnBuf), "IMU OFF");
#endif

  doomDrawHudBackdrop(0, 0, kDoomLeftBandW, DB_CANVAS_H, leftBandBg, leftBandLine, lv_color_make(12, 26, 40).full);
  doomDrawHudBackdrop(kDoomRightBandX, 0, DB_CANVAS_W - kDoomRightBandX, DB_CANVAS_H,
                      rightBandBg, rightBandLine, lv_color_make(40, 26, 10).full);
  doomFillRect(kDoomFrameX - 4, 0, 4, DB_CANVAS_H, leftActive ? moveColor : mutedColor);
  doomFillRect(kDoomRightBandX, 0, 4, DB_CANVAS_H, rightActive ? turnColor : mutedColor);

  doomDrawHudPanel(leftCardX, leftCardY, leftCardW, leftCardH, moveColor, leftActive);
  doomDrawHudPanel(rightCardX, rightCardY, rightCardW, rightCardH, turnColor, rightActive);

  doomDrawFontTextCentered(kDoomLeftBandW / 2, 20, "MOVE", &scry_font_space_mono_20, labelColor);
  doomDrawFontTextCentered(kDoomLeftBandW / 2, 44, moveBuf, &scry_font_space_mono_16, valueColor);
  doomDrawFontTextCentered(kDoomLeftBandW / 2, 58, "FWD", &scry_font_space_mono_12, moveColor);
  doomDrawAxisMeterVertical((kDoomLeftBandW / 2) - 12, 70, 24, 44,
                            g_doomMoveBin, kDoomMoveBinMax,
                            meterBg, moveColor, centerLine);
  doomDrawFontTextCentered(kDoomLeftBandW / 2, 116, "BACK", &scry_font_space_mono_12, mutedColor);

  const int16_t rightCenterX = kDoomRightBandX + ((DB_CANVAS_W - kDoomRightBandX) / 2);
  doomDrawFontTextCentered(rightCenterX, 20, "TURN", &scry_font_space_mono_20, labelColor);
  doomDrawFontTextCentered(rightCenterX, 44, turnBuf, &scry_font_space_mono_16, valueColor);
  doomDrawFontText(rightCardX + 16, 66, "L", &scry_font_space_mono_12, mutedColor);
  doomDrawFontText(rightCardX + rightCardW - 24, 66, "R", &scry_font_space_mono_12, mutedColor);
  doomDrawAxisMeter(rightCardX + 22, 78, rightCardW - 44, 12,
                    g_doomTurnBin, kDoomTurnBinMax,
                    meterBg, turnColor, centerLine);

#if DB_HAS_PRBOOM_DONOR
  if (!doomPrboomHasFrame()) {
    if (!g_doomLaunchRequested) snprintf(promptBuf, sizeof(promptBuf), "PRESS FIRE");
    else snprintf(promptBuf, sizeof(promptBuf), "%s", doomPrboomStatus() ? doomPrboomStatus() : "BOOT");
    doomDrawHudBadge(rightCardX + 24, 102, rightCardW - 48, 20, promptBuf,
                     &scry_font_space_mono_16, turnColor, valueColor);
  }
#endif

  doomDrawActionButton(24, DB_CANVAS_H - 38, kDoomLeftBandW - 48, 26, "USE", leftActive, moveColor);
  doomDrawActionButton(kDoomRightBandX + 24, DB_CANVAS_H - 38,
                       (DB_CANVAS_W - kDoomRightBandX) - 48, 26,
                       "FIRE", rightActive, turnColor);
}

bool doomScrybarPageVisible() {
  return g_uiPageMode == UI_PAGE_DOOM;
}

bool doomScrybarGetInputState(int8_t *moveBin, int8_t *turnBin, uint8_t *touchZone) {
  const bool visible = (g_uiPageMode == UI_PAGE_DOOM);
  if (moveBin) *moveBin = visible ? g_doomMoveBin : 0;
  if (turnBin) *turnBin = visible ? g_doomTurnBin : 0;
  if (touchZone) *touchZone = visible ? g_doomTouchZone : DOOM_TOUCH_NONE;
  return visible;
}

bool doomScrybarBlitIndexedFrame(const uint8_t *pixels,
                                 int srcWidth,
                                 int srcHeight,
                                 const uint16_t *palette565be,
                                 bool forceFlush) {
  if (!pixels || !palette565be) return false;
  if (g_uiPageMode != UI_PAGE_DOOM) return false;
  if (!g_canvasBuf) return false;
  if (!initDisplay()) return false;

  memset(g_canvasBuf, 0, (size_t)DB_CANVAS_W * DB_CANVAS_H * sizeof(uint16_t));
  for (int16_t y = 0; y < kDoomFrameH; ++y) {
    const int srcY = ((int32_t)y * srcHeight) / kDoomFrameH;
    const size_t srcRow = (size_t)srcY * (size_t)srcWidth;
    uint16_t *dst = g_canvasBuf + ((size_t)(kDoomFrameY + y) * DB_CANVAS_W) + kDoomFrameX;
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
  if (g_doomPaletteReady) return;
  for (uint16_t i = 0; i < 256; ++i) {
    const uint8_t r = pgm_read_byte(kDoomTitlePicPalette + (i * 3) + 0);
    const uint8_t g = pgm_read_byte(kDoomTitlePicPalette + (i * 3) + 1);
    const uint8_t b = pgm_read_byte(kDoomTitlePicPalette + (i * 3) + 2);
    const uint16_t raw565 = doomRgb888To565(r, g, b);
    g_doomPalette565[i] = (uint16_t)((raw565 << 8) | (raw565 >> 8));
  }
  g_doomPaletteReady = true;
}

static void doomBlitTitlePicToCanvas() {
  if (!g_canvasBuf) return;
  doomBuildPalette();
  memset(g_canvasBuf, 0, (size_t)DB_CANVAS_W * DB_CANVAS_H * sizeof(uint16_t));

  for (int16_t y = 0; y < kDoomFrameH; ++y) {
    const int16_t srcY = (int16_t)(((int32_t)y * kDoomTitlePicHeight) / kDoomFrameH);
    const size_t srcRow = (size_t)srcY * kDoomTitlePicWidth;
    uint16_t *dst = g_canvasBuf + ((size_t)(kDoomFrameY + y) * DB_CANVAS_W) + kDoomFrameX;
    for (int16_t x = 0; x < kDoomFrameW; ++x) {
      const int16_t srcX = (int16_t)(((int32_t)x * kDoomTitlePicWidth) / kDoomFrameW);
      const uint8_t idx = pgm_read_byte(kDoomTitlePicPixels + srcRow + srcX);
      const uint16_t color = g_doomPalette565[idx];
      dst[x] = color;
    }
  }

  doomDrawBandOverlay();
}

static void doomRenderSpike(bool force) {
  if (g_uiPageMode != UI_PAGE_DOOM) return;
  if (!initDisplay()) return;
#if DB_HAS_PRBOOM_DONOR
  if (g_doomLaunchRequested) {
    doomPrboomEnsureStarted();
    if (doomPrboomHasFrame()) {
      g_doomFrameDirty = false;
      return;
    }
  }
#endif
  if (!force && !g_doomFrameDirty) return;

  setBacklightPercent(100);
  doomBlitTitlePicToCanvas();
  dispFlush();
  g_doomFrameDirty = false;

  const uint32_t now = millis();
  if (force || g_doomLastRenderLogMs == 0 || (now - g_doomLastRenderLogMs) >= 2000) {
    g_doomLastRenderLogMs = now;
    const char *status = doomPrboomStatus();
    Serial.printf("[DOOM] TITLEPIC rendered src=%ux%u frame=%dx%d@x=%d bands=%d/%d move=%d turn=%d status=%s donor=prboom-go\n",
                  (unsigned)kDoomTitlePicWidth,
                  (unsigned)kDoomTitlePicHeight,
                  (int)kDoomFrameW,
                  (int)kDoomFrameH,
                  (int)kDoomFrameX,
                  (int)kDoomLeftBandW,
                  (int)(DB_CANVAS_W - kDoomRightBandX),
                  (int)g_doomMoveBin,
                  (int)g_doomTurnBin,
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
// 8×8 tiles reduce PSRAM cache misses ~8× vs pixel-by-pixel stride.
static inline void dispRotateChunk(uint16_t *dst, int16_t colBase) {
  constexpr int T = 8;
  for (int16_t dj0 = 0; dj0 < DB_CHUNK_ROWS; dj0 += T) {
    for (int16_t di0 = 0; di0 < DB_CANVAS_H; di0 += T) {
      const int16_t diEnd = (di0 + T <= DB_CANVAS_H) ? (di0 + T) : DB_CANVAS_H;
      for (int16_t dj = dj0; dj < dj0 + T; ++dj) {
        uint16_t *d = &dst[dj * DB_NATIVE_W + di0];
        for (int16_t di = di0; di < diEnd; ++di) {
          d[di - di0] = g_canvasBuf[(DB_CANVAS_H - 1 - di) * DB_CANVAS_W + colBase + dj];
        }
      }
    }
  }
}

static bool dispFlush() {
  if (!g_panel || !g_canvasBuf || !g_dmaBuf || !g_dmaBuf2 || !g_dispFlushSem) return false;

  const uint32_t t0 = micros();
  const int chunks = DB_NATIVE_H / DB_CHUNK_ROWS;  // 640/64 = 10
  uint16_t *bufCur = g_dmaBuf;
  uint16_t *bufNext = g_dmaBuf2;

  // Rotate first chunk (no DMA overlap yet)
  dispRotateChunk(bufCur, 0);

  // Start DMA on first chunk
  xSemaphoreGive(g_dispFlushSem);
  xSemaphoreTake(g_dispFlushSem, portMAX_DELAY);
  esp_lcd_panel_draw_bitmap(g_panel, 0, 0, DB_NATIVE_W, DB_CHUNK_ROWS, bufCur);

  // Pipeline: rotate chunk c into bufNext while DMA sends chunk c-1 from bufCur
  for (int c = 1; c < chunks; ++c) {
    dispRotateChunk(bufNext, c * DB_CHUNK_ROWS);

    xSemaphoreTake(g_dispFlushSem, portMAX_DELAY);
    esp_lcd_panel_draw_bitmap(g_panel, 0, c * DB_CHUNK_ROWS, DB_NATIVE_W, (c + 1) * DB_CHUNK_ROWS, bufNext);

    uint16_t *tmp = bufCur;
    bufCur = bufNext;
    bufNext = tmp;
  }

  // Wait for last DMA to complete
  xSemaphoreTake(g_dispFlushSem, portMAX_DELAY);

  const uint32_t dt = micros() - t0;
  g_perfFlushCount++;
  g_perfFlushTotalUs += dt;
  if (dt > g_perfFlushMaxUs) g_perfFlushMaxUs = dt;

  return true;
}
#endif

static bool initDisplay() {
#if TEST_DISPLAY && DISPLAY_BACKEND_ESP_LCD
  if (g_panel != nullptr) return true;

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

  g_dispFlushSem = xSemaphoreCreateBinary();
  if (!g_dispFlushSem) {
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
  if (esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &g_panelIo) != ESP_OK) {
    Serial.println("[ERR] esp_lcd_new_panel_io_spi failed.");
    return false;
  }

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
  if (esp_lcd_new_panel_axs15231b(g_panelIo, &panel_config, &g_panel) != ESP_OK) {
    Serial.println("[ERR] esp_lcd_new_panel_axs15231b failed.");
    return false;
  }

  gpio_set_level((gpio_num_t)LCD_RST_PIN, 1);
  delay(30);
  gpio_set_level((gpio_num_t)LCD_RST_PIN, 0);
  delay(250);
  gpio_set_level((gpio_num_t)LCD_RST_PIN, 1);
  delay(30);
  if (esp_lcd_panel_init(g_panel) != ESP_OK) {
    Serial.println("[ERR] esp_lcd_panel_init failed.");
    return false;
  }
#if DISPLAY_FLIP_180
  if (esp_lcd_panel_mirror(g_panel, true, true) != ESP_OK) {
    Serial.println("[ERR] esp_lcd_panel_mirror(180) failed.");
    return false;
  }
#endif

  g_canvasBuf = (uint16_t *)heap_caps_malloc(DB_CANVAS_W * DB_CANVAS_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
  g_dmaBuf = (uint16_t *)heap_caps_malloc(DB_NATIVE_W * DB_CHUNK_ROWS * sizeof(uint16_t), MALLOC_CAP_DMA);
  g_dmaBuf2 = (uint16_t *)heap_caps_malloc(DB_NATIVE_W * DB_CHUNK_ROWS * sizeof(uint16_t), MALLOC_CAP_DMA);
  if (!g_canvasBuf || !g_dmaBuf || !g_dmaBuf2) {
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
static bool wifiSetupModeIsOff() { return strcmp(g_wifiSetupMode, "off") == 0; }
static bool wifiSetupModeIsOn() { return strcmp(g_wifiSetupMode, "on") == 0; }
static bool wifiSetupModeIsAuto() { return !wifiSetupModeIsOff() && !wifiSetupModeIsOn(); }

static void wifiBuildSetupApSsid() {
  if (g_wifiSetupApSsid[0]) return;
  const uint64_t mac = ESP.getEfuseMac();
  const uint8_t tailA = (uint8_t)((mac >> 8) & 0xFF);
  const uint8_t tailB = (uint8_t)(mac & 0xFF);
  snprintf(g_wifiSetupApSsid, sizeof(g_wifiSetupApSsid), "%s-%02X%02X", WIFI_SETUP_AP_SSID_PREFIX, tailA, tailB);
}

static void wifiBuildSetupPortalUrl(char *out, size_t outLen) {
  if (!out || outLen == 0) return;
  IPAddress apIp = WiFi.softAPIP();
  if ((uint32_t)apIp == 0U) apIp = IPAddress(192, 168, 4, 1);
  snprintf(out, outLen, "http://%s:%u", apIp.toString().c_str(), (unsigned)WEB_CONFIG_PORT);
  out[outLen - 1] = '\0';
}

static bool wifiStartSetupAp(bool autoStart) {
  if (g_wifiSetupApActive) return true;
  wifiBuildSetupApSsid();
  WiFi.mode(WIFI_AP_STA);
  bool ok = false;
  if (strlen(WIFI_SETUP_AP_PASSWORD) >= 8) {
    ok = WiFi.softAP(g_wifiSetupApSsid, WIFI_SETUP_AP_PASSWORD, WIFI_SETUP_AP_CHANNEL, false, WIFI_SETUP_AP_MAX_CLIENTS);
  } else {
    ok = WiFi.softAP(g_wifiSetupApSsid, nullptr, WIFI_SETUP_AP_CHANNEL, false, WIFI_SETUP_AP_MAX_CLIENTS);
  }
  if (!ok) {
    Serial.println("[WIFI][AP][ERR] impossibile avviare setup AP");
    return false;
  }
  g_wifiSetupApActive = true;
  g_wifiSetupApAutoStarted = autoStart;
#if WEB_CONFIG_ENABLED
  webConfigStartCaptiveDnsIfNeeded();
#endif
  Serial.printf("[WIFI][AP] setup active ssid='%s' ip=%s mode=%s\n",
                g_wifiSetupApSsid,
                WiFi.softAPIP().toString().c_str(),
                autoStart ? "auto" : "manual");
  return true;
}

static void wifiStopSetupAp() {
  if (!g_wifiSetupApActive) return;
  WiFi.softAPdisconnect(true);
  g_wifiSetupApActive = false;
  g_wifiSetupApAutoStarted = false;
#if WEB_CONFIG_ENABLED
  webConfigStopCaptiveDns();
#endif
  WiFi.mode(WIFI_STA);
  Serial.println("[WIFI][AP] setup disabled");
}

static void wifiScheduleNextAttempt(uint32_t nowMs, uint32_t delayMs) {
  g_wifiReconnectAttemptActive = false;
  g_wifiReconnectNextAttemptMs = nowMs + delayMs;
}

static void wifiRotateCredentialIndex() {
  if (g_wifiCredCount <= 1) return;
  g_wifiReconnectIdx = (uint8_t)((g_wifiReconnectIdx + 1U) % g_wifiCredCount);
}

static void wifiRearmStationRadio(uint32_t nowMs, const char *cause) {
  if ((nowMs - g_wifiLastRadioResetMs) < 1500UL) {
    wifiScheduleNextAttempt(nowMs, WIFI_RETRY_AFTER_RADIO_RESET_MS);
    return;
  }
  Serial.printf("[WIFI][HEAL] radio reset cause=%s\n", cause ? cause : "-");
  g_wifiInternalDisconnect = true;
  WiFi.disconnect(true, false);
  delay(20);
  WiFi.mode(WIFI_OFF);
  g_wifiSetupApActive = false;
  g_wifiSetupApAutoStarted = false;
  delay(60);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  g_wifiInternalDisconnect = false;
  g_lastWifiDiscReason = -1;
  g_wifiLastRadioResetMs = millis();
  wifiScheduleNextAttempt(g_wifiLastRadioResetMs, WIFI_RETRY_AFTER_RADIO_RESET_MS);
}

static void wifiHandleFailure(uint32_t nowMs, const char *cause) {
  const uint8_t failedIdx = g_wifiReconnectIdx;
  const char *failedSsid = g_wifiCredSsids[failedIdx] ? g_wifiCredSsids[failedIdx] : "-";
  wifiRotateCredentialIndex();
  const char *nextSsid = g_wifiCredSsids[g_wifiReconnectIdx] ? g_wifiCredSsids[g_wifiReconnectIdx] : "-";
  ++g_wifiConsecutiveFailCount;
  Serial.printf("[WIFI][RETRY] cause=%s fail_streak=%u failed='%s' next='%s'\n",
                cause ? cause : "-",
                (unsigned)g_wifiConsecutiveFailCount,
                failedSsid,
                nextSsid);
  if (g_wifiConsecutiveFailCount >= WIFI_RETRY_FAILS_BEFORE_RADIO_RESET) {
    g_wifiConsecutiveFailCount = 0;
    wifiRearmStationRadio(nowMs, cause);
    return;
  }
  wifiScheduleNextAttempt(nowMs, WIFI_RETRY_STEP_DELAY_MS);
}

static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    g_wifiConnected = true;
    g_wifiEverConnected = true;
    g_wifiLastConnectMs = millis();
    g_wifiNoLinkSinceMs = 0;
    g_wifiConsecutiveFailCount = 0;
    Serial.printf("[WIFI][GOT_IP] ip=%s\n", WiFi.localIP().toString().c_str());
    const String activeSsid = WiFi.SSID();
    for (uint8_t i = 0; i < g_wifiCredCount; ++i) {
      const char *known = g_wifiCredSsids[i];
      if (known && activeSsid.equals(known)) {
        g_wifiReconnectIdx = i;
        break;
      }
    }
    const bool dnsOk = applyWiFiDnsOverrideIfEnabled(false);
    Serial.printf("[WIFI][DNS] active=%s/%s (%s)\n",
                  WiFi.dnsIP(0).toString().c_str(),
                  WiFi.dnsIP(1).toString().c_str(),
                  dnsOk ? "OK" : "ERR");
    wifiScheduleNextAttempt(millis(), 0UL);
  } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    const bool attemptWasActive = g_wifiReconnectAttemptActive;
    g_wifiConnected = false;
    g_wifiLastDisconnectMs = millis();
    if (g_wifiNoLinkSinceMs == 0) g_wifiNoLinkSinceMs = g_wifiLastDisconnectMs;
    g_lastWifiDiscReason = (int)info.wifi_sta_disconnected.reason;
    Serial.printf("[WIFI][DISC] reason=%d (%s)\n",
                  g_lastWifiDiscReason,
                  WiFi.disconnectReasonName((wifi_err_reason_t)g_lastWifiDiscReason));
    if (g_wifiCredCount == 0) return;
    if (g_wifiInternalDisconnect) {
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
  for (uint8_t i = 0; i < g_wifiCredCount; ++i) {
    const char *known = g_wifiCredSsids[i];
    if (!known || !known[0]) continue;
    if (strcmp(known, ssid) == 0) return (int8_t)i;
  }
  return -1;
}

static void wifiPrepareCredentialCache() {
  g_wifiStaticCredCount = buildWiFiStaticCredentialList(g_wifiStaticSsids, g_wifiStaticPasswords, WIFI_STATIC_CREDENTIALS_MAX);
  g_wifiCredCount = 0;
  for (uint8_t i = 0; i < g_wifiRuntimeCredCount && g_wifiCredCount < WIFI_TOTAL_CREDENTIALS_MAX; ++i) {
    const char *ssid = g_wifiRuntimeCreds[i].ssid;
    if (!ssid[0]) continue;
    g_wifiCredSsids[g_wifiCredCount] = g_wifiRuntimeCreds[i].ssid;
    g_wifiCredPasswords[g_wifiCredCount] = g_wifiRuntimeCreds[i].password;
    ++g_wifiCredCount;
  }
  for (size_t i = 0; i < g_wifiStaticCredCount && g_wifiCredCount < WIFI_TOTAL_CREDENTIALS_MAX; ++i) {
    const char *ssid = g_wifiStaticSsids[i];
    if (!ssid || !ssid[0]) continue;
    bool duplicate = false;
    for (size_t j = 0; j < g_wifiCredCount; ++j) {
      if (g_wifiCredSsids[j] && strcmp(g_wifiCredSsids[j], ssid) == 0) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) continue;
    g_wifiCredSsids[g_wifiCredCount] = ssid;
    g_wifiCredPasswords[g_wifiCredCount] = g_wifiStaticPasswords[i];
    ++g_wifiCredCount;
  }
  if (g_wifiCredCount == 0) {
    Serial.println("[WIFI][WARN] Nessuna rete nota configurata (secrets + NVS)");
  } else {
    Serial.printf("[WIFI] reti note: %u (secrets=%u runtime=%u)\n",
                  (unsigned)g_wifiCredCount,
                  (unsigned)g_wifiStaticCredCount,
                  (unsigned)g_wifiRuntimeCredCount);
  }
  g_wifiReconnectIdx = 0;
  if (g_wifiPreferredSsid[0]) {
    const int8_t preferredIdx = findWiFiCredentialIndexBySsid(g_wifiPreferredSsid);
    if (preferredIdx >= 0) g_wifiReconnectIdx = (uint8_t)preferredIdx;
  }
  g_wifiReconnectAttemptActive = false;
  g_wifiReconnectAttemptStartMs = 0;
  g_wifiReconnectNextAttemptMs = 0;
  g_wifiConsecutiveFailCount = 0;
  g_wifiLastRadioResetMs = 0;
  g_wifiInternalDisconnect = false;
}

static bool wifiIsConnectedNow();

static void wifiHandleSetupModeLoop(uint32_t nowMs) {
  if (wifiSetupModeIsOn()) {
    (void)wifiStartSetupAp(false);
    if (wifiIsConnectedNow()) g_wifiNoLinkSinceMs = 0;
    return;
  }

  if (wifiIsConnectedNow()) {
    g_wifiNoLinkSinceMs = 0;
    if (g_wifiSetupApActive) wifiStopSetupAp();
    return;
  }
  if (g_wifiNoLinkSinceMs == 0) g_wifiNoLinkSinceMs = nowMs;

  if (wifiSetupModeIsOff()) {
    if (g_wifiSetupApActive) wifiStopSetupAp();
    return;
  }
  if (!wifiSetupModeIsAuto()) return;

  const uint32_t bootstrapDelay = (g_wifiCredCount == 0) ? 500UL : WIFI_SETUP_AP_AUTOSTART_MS;
  if (!g_wifiSetupApActive && (nowMs - g_wifiNoLinkSinceMs) >= bootstrapDelay) {
    (void)wifiStartSetupAp(true);
  }
}

static bool wifiIsConnectedNow() {
  return (WiFi.status() == WL_CONNECTED) && g_wifiConnected;
}

static void wifiBeginAttempt(uint8_t idx) {
  if (idx >= g_wifiCredCount) return;
  const char *ssid = g_wifiCredSsids[idx];
  const char *password = g_wifiCredPasswords[idx];
  if (!ssid || !ssid[0]) return;

  g_wifiConnected = false;
  g_lastWifiDiscReason = -1;
  g_wifiInternalDisconnect = true;
  WiFi.disconnect(true, false);
  delay(20);
  g_wifiInternalDisconnect = false;

  Serial.printf("[WIFI] try %u/%u ssid='%s' timeout=%ums\n",
                (unsigned)(idx + 1),
                (unsigned)g_wifiCredCount,
                ssid,
                (unsigned)WIFI_CONNECT_TIMEOUT_MS);
  if (password && password[0]) WiFi.begin(ssid, password);
  else WiFi.begin(ssid);

  g_wifiReconnectAttemptActive = true;
  g_wifiReconnectAttemptStartMs = millis();
}

static void handleWiFiReconnectLoop(uint32_t nowMs) {
  if (g_wifiCredCount == 0) return;
  if (wifiIsConnectedNow()) {
    g_wifiReconnectAttemptActive = false;
    return;
  }

  if (!g_wifiReconnectAttemptActive) {
    if (nowMs < g_wifiReconnectNextAttemptMs) return;
    wifiBeginAttempt(g_wifiReconnectIdx);
    return;
  }

  if ((nowMs - g_wifiReconnectAttemptStartMs) < (uint32_t)WIFI_CONNECT_TIMEOUT_MS) return;

  const uint8_t failedIdx = g_wifiReconnectIdx;
  const char *failedSsid = g_wifiCredSsids[failedIdx] ? g_wifiCredSsids[failedIdx] : "-";
  Serial.printf("[WIFI][FAIL] ssid='%s' status=%s (%d)\n",
                failedSsid,
                wlStatusToStr(WiFi.status()),
                (int)WiFi.status());
  if (g_lastWifiDiscReason >= 0) {
    Serial.printf("[WIFI][FAIL] reason=%d (%s)\n",
                  g_lastWifiDiscReason,
                  WiFi.disconnectReasonName((wifi_err_reason_t)g_lastWifiDiscReason));
  }

  // Close current attempt first; avoid event-side double handling.
  g_wifiReconnectAttemptActive = false;
  g_wifiInternalDisconnect = true;
  WiFi.disconnect(true, false);
  delay(10);
  g_wifiInternalDisconnect = false;
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

  g_wifiConnected = false;
  g_lastWifiDiscReason = -1;
  if (!g_wifiEventRegistered) {
    WiFi.onEvent(onWiFiEvent);
    g_wifiEventRegistered = true;
  }
  normalizeWifiSetupMode();
  wifiPrepareCredentialCache();
  g_wifiNoLinkSinceMs = millis();

  if (g_wifiCredCount > 0) {
    // Non-blocking strategy: one SSID attempt at a time, cycled in loop().
    wifiBeginAttempt(g_wifiReconnectIdx);
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
      g_ntpSynced = true;
      g_lastClockSecond = -1;
      g_lastDateKey = -1;
      g_clockStaticDrawn = false;
      return true;
    }
  }

  g_ntpSynced = false;
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

static bool startsWithHttp(const char *url) {
  if (!url) return false;
  return (strncmp(url, "http://", 7) == 0) || (strncmp(url, "https://", 8) == 0);
}

static void normalizeWifiSetupMode() {
  if (wifiSetupModeIsOff() || wifiSetupModeIsOn() || wifiSetupModeIsAuto()) return;
  copyStringSafe(g_wifiSetupMode, sizeof(g_wifiSetupMode), WIFI_SETUP_MODE_DEFAULT);
  if (!wifiSetupModeIsOff() && !wifiSetupModeIsOn() && !wifiSetupModeIsAuto()) {
    copyStringSafe(g_wifiSetupMode, sizeof(g_wifiSetupMode), "auto");
  }
}

static int8_t findRuntimeWiFiCredentialBySsid(const char *ssid) {
  if (!ssid || !ssid[0]) return -1;
  for (uint8_t i = 0; i < g_wifiRuntimeCredCount; ++i) {
    if (strcmp(g_wifiRuntimeCreds[i].ssid, ssid) == 0) return (int8_t)i;
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
    copyStringSafe(g_wifiRuntimeCreds[(uint8_t)existing].password, sizeof(g_wifiRuntimeCreds[(uint8_t)existing].password), safePass);
    return true;
  }

  if (g_wifiRuntimeCredCount < WIFI_RUNTIME_CREDENTIALS_MAX) {
    copyStringSafe(g_wifiRuntimeCreds[g_wifiRuntimeCredCount].ssid, sizeof(g_wifiRuntimeCreds[g_wifiRuntimeCredCount].ssid), ssid);
    copyStringSafe(g_wifiRuntimeCreds[g_wifiRuntimeCredCount].password, sizeof(g_wifiRuntimeCreds[g_wifiRuntimeCredCount].password), safePass);
    ++g_wifiRuntimeCredCount;
    return true;
  }

  // FIFO eviction when runtime slots are full.
  for (uint8_t i = 1; i < WIFI_RUNTIME_CREDENTIALS_MAX; ++i) {
    copyStringSafe(g_wifiRuntimeCreds[i - 1].ssid, sizeof(g_wifiRuntimeCreds[i - 1].ssid), g_wifiRuntimeCreds[i].ssid);
    copyStringSafe(g_wifiRuntimeCreds[i - 1].password, sizeof(g_wifiRuntimeCreds[i - 1].password), g_wifiRuntimeCreds[i].password);
  }
  const uint8_t tail = WIFI_RUNTIME_CREDENTIALS_MAX - 1;
  copyStringSafe(g_wifiRuntimeCreds[tail].ssid, sizeof(g_wifiRuntimeCreds[tail].ssid), ssid);
  copyStringSafe(g_wifiRuntimeCreds[tail].password, sizeof(g_wifiRuntimeCreds[tail].password), safePass);
  return true;
}

static void loadRuntimeWiFiCredentialsFromPrefs(Preferences &prefs, bool &loadedAny) {
  g_wifiRuntimeCredCount = 0;
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
    copyStringSafe(g_wifiRuntimeCreds[g_wifiRuntimeCredCount].ssid, sizeof(g_wifiRuntimeCreds[g_wifiRuntimeCredCount].ssid), ssid.c_str());
    copyStringSafe(g_wifiRuntimeCreds[g_wifiRuntimeCredCount].password, sizeof(g_wifiRuntimeCreds[g_wifiRuntimeCredCount].password), pass.c_str());
    ++g_wifiRuntimeCredCount;
    loadedAny = true;
  }
}

static size_t saveRuntimeWiFiCredentialsToPrefs(Preferences &prefs) {
  size_t bytes = prefs.putUChar("wifi_dyn_n", g_wifiRuntimeCredCount);
  for (uint8_t i = 0; i < WIFI_RUNTIME_CREDENTIALS_MAX; ++i) {
    char keySsid[16];
    char keyPass[16];
    snprintf(keySsid, sizeof(keySsid), "wifi_ds%u", (unsigned)(i + 1));
    snprintf(keyPass, sizeof(keyPass), "wifi_dp%u", (unsigned)(i + 1));
    if (i < g_wifiRuntimeCredCount) {
      bytes += prefs.putString(keySsid, g_wifiRuntimeCreds[i].ssid);
      bytes += prefs.putString(keyPass, g_wifiRuntimeCreds[i].password);
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
    if (wifiPref.length() < sizeof(g_wifiPreferredSsid)) {
      copyStringSafe(g_wifiPreferredSsid, sizeof(g_wifiPreferredSsid), wifiPref.c_str());
      loadedAny = true;
    }
  }
  if (prefs.isKey("wifi_setup_mode")) {
    String setupMode = prefs.getString("wifi_setup_mode", WIFI_SETUP_MODE_DEFAULT);
    setupMode.trim();
    setupMode.toLowerCase();
    if (setupMode.length() < sizeof(g_wifiSetupMode)) {
      copyStringSafe(g_wifiSetupMode, sizeof(g_wifiSetupMode), setupMode.c_str());
      loadedAny = true;
    }
  }
  loadRuntimeWiFiCredentialsFromPrefs(prefs, loadedAny);
  if (prefs.isKey("wc_lang")) {
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
      // Backward compatibility: old alias before rename to Bellazio.
      lang = "bellazio";
      langNeedsPersist = true;
      Serial.println("[CFG][NVS] wc_lang legacy alias -> 'bellazio'");
    }
    if (lang.equalsIgnoreCase("bellazi")) {
      // Backward compatibility: old truncated value caused by 8-byte lang buffer.
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
  if (prefs.isKey("ui_theme")) {
    const String theme = prefs.getString("ui_theme", "");
    if (theme.length() > 0 && theme.length() < sizeof(g_runtimeNetConfig.uiTheme)) {
      copyStringSafe(g_runtimeNetConfig.uiTheme, sizeof(g_runtimeNetConfig.uiTheme), theme.c_str());
      loadedAny = true;
    }
  }
  if (prefs.isKey("ui_views")) {
    g_runtimeNetConfig.enabledViewsMask = normalizeRuntimeViewMask(prefs.getUChar("ui_views", UI_VIEW_MASK_DEFAULT));
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

  prefs.end();

  normalizeWifiSetupMode();
  wifiPrepareCredentialCache();
  if (g_wifiPreferredSsid[0]) {
    const int8_t preferredIdx = findWiFiCredentialIndexBySsid(g_wifiPreferredSsid);
    if (preferredIdx >= 0) g_wifiReconnectIdx = (uint8_t)preferredIdx;
    else g_wifiPreferredSsid[0] = '\0';
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
                  g_wifiPreferredSsid[0] ? g_wifiPreferredSsid : "auto",
                  g_wifiSetupMode,
                  (unsigned)g_wifiRuntimeCredCount);
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
  const size_t nWikiLang = prefs.putString("wiki_lang", g_wikiLang);
  const size_t n8 = prefs.putString("wifi_pref", g_wifiPreferredSsid);
  const size_t n9 = prefs.putString("wifi_setup_mode", g_wifiSetupMode);
  const size_t n10 = saveRuntimeWiFiCredentialsToPrefs(prefs);
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
  if (g_webQrTempBuf && g_webQrDataBuf) return true;
  if (!g_webQrTempBuf) {
    g_webQrTempBuf = (uint8_t *)ps_malloc(qrcodegen_BUFFER_LEN_MAX);
    if (!g_webQrTempBuf) g_webQrTempBuf = (uint8_t *)malloc(qrcodegen_BUFFER_LEN_MAX);
  }
  if (!g_webQrDataBuf) {
    g_webQrDataBuf = (uint8_t *)ps_malloc(qrcodegen_BUFFER_LEN_MAX);
    if (!g_webQrDataBuf) g_webQrDataBuf = (uint8_t *)malloc(qrcodegen_BUFFER_LEN_MAX);
  }
  if (!g_webQrTempBuf || !g_webQrDataBuf) {
    Serial.printf("[WEB][QR][ERR] alloc failed temp=%d data=%d heap=%u psram=%u\n",
                  g_webQrTempBuf ? 1 : 0,
                  g_webQrDataBuf ? 1 : 0,
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getFreePsram());
    return false;
  }
  return true;
}
#endif

static String buildWebConfigPage(const char *statusMsg) {
  ensureRuntimeNetConfig();
  char latBuf[24];
  char lonBuf[24];
  snprintf(latBuf, sizeof(latBuf), "%.4f", runtimeWeatherLat());
  snprintf(lonBuf, sizeof(lonBuf), "%.4f", runtimeWeatherLon());
  const uint8_t configuredFeeds = runtimeRssConfiguredFeedCount();
  const UiThemeDefinition &themeDef = activeUiTheme();
  const UiThemeWebTokens &webTheme = themeDef.web;
  const bool infoViewOn = (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_INFO) != 0;
  const bool auxViewOn = (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_AUX) != 0;
  const bool wikiViewOn = (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_WIKI) != 0;
  const bool doomFeatureAvailable =
#if TEST_DISPLAY && DOOM_SPIKE_ENABLED
      true;
#else
      false;
#endif
  const bool doomViewOn = doomFeatureAvailable && ((g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_DOOM) != 0);

  String html;
  html.reserve(32000);
  html += F("<!doctype html><html lang='en'><head><meta charset='utf-8'>");
  html += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>ScryBar Control Surface</title>");
  html += F("<link rel='preconnect' href='https://fonts.googleapis.com'>");
  html += F("<link rel='preconnect' href='https://fonts.gstatic.com' crossorigin>");
  // Load remote CSS asynchronously so UI stays paintable even if CDN/fonts are slow or blocked.
  html += F("<link rel='stylesheet' href='https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@400;500;600;700&family=Delius+Unicase:wght@400;700&family=IBM+Plex+Mono:wght@400;500;600;700&family=Montserrat:wght@400;500;600;700;800&family=Space+Mono:wght@400;700&display=swap' media='print' onload=\"this.media='all'\">");
  html += F("<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.7.2/css/all.min.css' media='print' onload=\"this.media='all'\">");
  html += F("<noscript><link href='https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@400;500;600;700&family=Delius+Unicase:wght@400;700&family=IBM+Plex+Mono:wght@400;500;600;700&family=Montserrat:wght@400;500;600;700;800&family=Space+Mono:wght@400;700&display=swap' rel='stylesheet'><link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.7.2/css/all.min.css'></noscript>");
  html += F("<style>");
  // Tron-grid animated background; tuned to stay visible on mobile while keeping form readability.
  appendWebThemeCssVars(html, webTheme);
  html += F("*{box-sizing:border-box}html,body{margin:0}body{font-family:var(--font-main);background:var(--bg-deepest);color:var(--txt);overflow-x:hidden}");
  html += F(".fx-grid{position:fixed;inset:0;z-index:0;overflow:hidden;perspective:600px;pointer-events:none}");
  html += F(".fx-grid__plane{position:absolute;bottom:-8vh;left:-35%;width:170%;height:62vh;transform:rotateX(62deg);transform-origin:center bottom;background-image:linear-gradient(rgba(117,81,255,0.20) 1px,transparent 1px),linear-gradient(90deg,rgba(57,184,255,0.18) 1px,transparent 1px);background-size:36px 36px;opacity:.95;animation:gridScroll 5s linear infinite;will-change:background-position}");
  html += F(".fx-grid__glow{position:absolute;bottom:0;left:0;right:0;height:62%;background:linear-gradient(to top,rgba(117,81,255,0.18) 0%,rgba(57,184,255,0.08) 45%,transparent 100%);animation:gridGlowPulse 6s ease-in-out infinite}");
  html += F(".fx-grid__horizon{position:absolute;bottom:60%;left:0;right:0;height:2px;background:linear-gradient(90deg,transparent 0%,rgba(117,81,255,0.40) 20%,rgba(57,184,255,0.34) 50%,rgba(117,81,255,0.40) 80%,transparent 100%);box-shadow:0 0 22px rgba(117,81,255,0.24),0 0 70px rgba(57,184,255,0.10)}");
  html += F(".fx-grid__scanline{position:absolute;left:0;right:0;height:2px;background:linear-gradient(90deg,transparent,rgba(57,184,255,0.32),transparent);box-shadow:0 0 16px rgba(57,184,255,0.18);animation:scanMove 4s ease-in-out infinite;will-change:transform}");
  html += F(".fx-grid__vline{position:absolute;top:0;bottom:0;width:1px;opacity:0;animation:vlinePulse 5s ease-in-out infinite}");
  html += F(".fx-grid__vline:nth-child(5){left:20%;background:linear-gradient(to bottom,transparent,rgba(117,81,255,0.20),transparent);animation-delay:0s}");
  html += F(".fx-grid__vline:nth-child(6){left:50%;background:linear-gradient(to bottom,transparent,rgba(57,184,255,0.18),transparent);animation-delay:-1.8s}");
  html += F(".fx-grid__vline:nth-child(7){left:80%;background:linear-gradient(to bottom,transparent,rgba(117,81,255,0.20),transparent);animation-delay:-3.5s}");
  html += F(".fx-grid__vignette{position:absolute;inset:0;background:radial-gradient(ellipse at center,transparent 45%,rgba(7,13,45,0.54) 100%)}");
  html += F("@keyframes gridScroll{0%{background-position:0 0}100%{background-position:0 36px}}@keyframes gridGlowPulse{0%,100%{opacity:0.78}50%{opacity:1}}@keyframes scanMove{0%,100%{transform:translateY(100vh);opacity:0}5%{opacity:1}95%{opacity:1}99%{transform:translateY(0);opacity:0}}@keyframes vlinePulse{0%,100%{opacity:0}50%{opacity:1}}@keyframes savePulse{0%,100%{box-shadow:0 0 14px rgba(117,81,255,.32),0 0 0 0 rgba(57,184,255,0)}55%{box-shadow:0 0 28px rgba(117,81,255,.70),0 0 18px rgba(57,184,255,.38),0 0 0 4px rgba(57,184,255,.12)}}");
  html += F(".wrap{position:relative;z-index:1;max-width:1140px;margin:0 auto;padding:18px 14px 26px}.hero{background:transparent;border:0;border-radius:0;padding:0;margin-bottom:14px;backdrop-filter:none;-webkit-backdrop-filter:none}");
  html += F(".hero-top-card{border:1px solid rgba(117,81,255,.34);border-radius:14px;padding:12px;background:rgba(11,20,55,.30);backdrop-filter:blur(12px);-webkit-backdrop-filter:blur(12px)}.hero-top{display:flex;align-items:flex-start;justify-content:space-between;gap:14px;flex-wrap:wrap}.hero-left{min-width:290px;flex:1 1 560px}.logo{height:62px;display:block;object-fit:contain;filter:brightness(1.08)}");
  html += F(".hero-copy{margin-top:10px;border:1px solid rgba(57,184,255,.22);border-radius:14px;padding:12px;background:rgba(9,16,44,.36)}.lede{margin:0;color:#c5d2f4;font-size:13px;line-height:1.46;max-width:none}.hero-right{display:grid;gap:8px;justify-items:end}");
  html += F(".release-box{display:grid;grid-template-columns:auto auto;gap:4px 12px;padding:8px 10px;border-radius:10px;border:1px solid rgba(117,81,255,.42);background:rgba(7,13,38,.72);font:600 11px ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,'Liberation Mono',monospace}");
  html += F(".release-box .k{color:#79d8ff;text-transform:uppercase;letter-spacing:.08em}.release-box .v{color:#e9f0ff;letter-spacing:.01em}");
  html += F(".pill{display:inline-block;padding:4px 10px;border-radius:999px;background:rgba(57,184,255,.15);color:#9ed8ff;font-size:11px;font-weight:700}.pill i{margin-right:6px;color:#7de2ff}");
  html += F(".panel{background:transparent;backdrop-filter:none;-webkit-backdrop-filter:none;border:0;border-radius:0;padding:0;box-shadow:none}");
  html += F(".msg{margin:0 0 12px;padding:10px 12px;border-radius:10px;border:1px solid rgba(1,181,116,.45);background:var(--okbg);color:#c9fce9;font-weight:600}");
  html += F(".msg.fixed-top{position:fixed;top:10px;left:50%;transform:translateX(-50%);width:min(94vw,980px);margin:0;padding:11px 14px;z-index:99999;backdrop-filter:blur(6px);box-shadow:0 10px 22px rgba(0,0,0,.35)}");
  html += F(".sec{margin-top:14px;padding:14px;border:1px solid rgba(117,81,255,.26);border-radius:14px;background:rgba(11,20,55,.48);backdrop-filter:blur(12px);-webkit-backdrop-filter:blur(12px)}.sec h2{margin:0 0 10px;display:flex;align-items:center;gap:8px;font-size:13px;font-weight:700;letter-spacing:.08em;text-transform:uppercase;color:#9fb2e4}.sec h2 i{color:#39B8FF}.sec h2 .pill{margin-left:auto}");
  html += F(".grid2{display:grid;grid-template-columns:1fr 1fr;gap:12px}.card{border:1px solid rgba(117,81,255,.34);border-radius:12px;padding:12px 12px 10px;background:rgba(11,20,55,.43);backdrop-filter:blur(10px);-webkit-backdrop-filter:blur(10px);box-shadow:0 0 0 1px rgba(57,184,255,.18) inset}");
  html += F(".key{font-size:11px;color:var(--txt3);font-weight:700;margin:0 0 4px;letter-spacing:.04em}");
  html += F("input{width:100%;padding:11px 12px;border-radius:10px;border:1px solid rgba(255,255,255,.16);background:rgba(17,28,68,.76);color:var(--txt);outline:none;font:500 14px 'Montserrat',sans-serif;margin:0 0 9px}");
  html += F("input:focus{border-color:var(--acc2);box-shadow:0 0 0 2px rgba(57,184,255,.18)}");
  html += F("select{width:100%;padding:11px 12px;border-radius:var(--radius);border:1px solid rgba(255,255,255,.16);background:rgba(17,28,68,.76);color:var(--txt);outline:none;font:500 14px 'Montserrat',sans-serif;margin:0 0 9px;cursor:pointer}select:focus{border-color:var(--acc2);box-shadow:0 0 0 2px var(--border)}");
  html += F(".badge-soon{display:inline-block;margin-left:6px;padding:1px 6px;border-radius:999px;background:rgba(117,81,255,.22);color:#b8a8ff;font-size:10px;font-weight:700;vertical-align:middle;letter-spacing:.04em}");
  html += F(".hint{margin:4px 0 14px;color:var(--txt2);font-size:12px}.geo-status{margin:2px 0 8px;color:#9bb3ee;font-size:12px;min-height:16px}.secret-input-wrap{display:flex;gap:8px;align-items:stretch;margin:0 0 9px}.secret-input-wrap input{margin:0}.secret-toggle{min-width:44px;padding:0 12px}.secret-toggle i{margin:0}#wifi_new_password{font-family:var(--font-mono),ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,'Liberation Mono',monospace!important;text-transform:none!important;letter-spacing:.02em}");
  html += F(".btns{display:flex;gap:10px;flex-wrap:wrap;margin-top:16px}.btn{appearance:none;display:inline-flex;align-items:center;justify-content:center;gap:8px;border:0;border-radius:11px;padding:11px 15px;font:700 13px 'Montserrat',sans-serif;cursor:pointer;transition:all .18s ease}");
  html += F(".btn.primary{background:linear-gradient(135deg,var(--acc1),var(--acc2));color:#fff;box-shadow:0 0 14px rgba(117,81,255,.32);animation:savePulse 2.6s ease-in-out infinite}.btn.ghost{background:#1A2558;color:#d9e4ff;border:1px solid rgba(255,255,255,.14)}.btn:hover{transform:translateY(-1px)}");
  html += F(".btn.sm{padding:8px 11px;border-radius:9px;font-size:12px}.btn.warn{background:rgba(117,81,255,.18);color:#decfff;border:1px solid rgba(117,81,255,.5)}");
  html += F(".btn.danger{background:rgba(238,93,80,.18);color:#ffccc7;border:1px solid rgba(238,93,80,.5)}");
  html += F(".rss-composer{display:grid;grid-template-columns:1fr 1.9fr .55fr auto auto;gap:10px;align-items:end;margin-top:4px}");
  html += F(".rss-list{display:grid;gap:8px;margin-top:10px}.rss-row{display:grid;grid-template-columns:1fr auto;gap:8px;align-items:center;border:1px solid rgba(57,184,255,.28);border-left:3px solid rgba(117,81,255,.78);border-radius:12px;padding:10px;background:rgba(9,16,44,.76)}");
  html += F(".rss-title{display:flex;align-items:center;gap:6px;font-size:14px;font-weight:700;color:#e7eeff;margin:0 0 2px}.rss-title i{color:#39B8FF;font-size:12px}.rss-meta{font-size:12px;color:#98acd8;margin:0;word-break:break-all}.rss-chip{display:inline-block;margin-left:7px;padding:2px 7px;border-radius:999px;background:rgba(57,184,255,.17);color:#9fd9ff;font-size:11px;font-weight:700}");
  html += F(".rss-actions{display:flex;gap:6px;flex-wrap:wrap;justify-content:flex-end}.rss-status{margin:6px 0 2px;color:#9bb3ee;font-size:12px;min-height:16px}.rss-empty{padding:12px;border:1px dashed rgba(255,255,255,.2);border-radius:12px;color:#8ea2cf;font-size:12px;background:rgba(10,18,50,.35)}.hidden{display:none}");
  html += F(".view-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:10px}.view-card{display:flex;gap:10px;align-items:flex-start;padding:11px 12px;border:1px solid rgba(57,184,255,.24);border-radius:12px;background:rgba(9,16,44,.62)}.view-card input{width:18px;height:18px;margin:2px 0 0;accent-color:#39B8FF;flex:0 0 auto}.view-copy{display:grid;gap:3px;min-width:0}.view-copy strong{font-size:13px;color:#eef4ff}.view-copy small{color:#93a6d5;line-height:1.35}.view-card.fixed{border-style:dashed}.view-card.disabled{opacity:.55}");
  html += F(".api-note{margin-top:12px;padding:8px 10px;border-radius:10px;background:rgba(57,184,255,.10);border:1px solid rgba(57,184,255,.28)}.setup-quick{display:grid;grid-template-columns:auto 1fr;gap:12px;align-items:center;margin-top:10px}.setup-qr{width:138px;height:138px;border:1px solid rgba(255,255,255,.2);background:#fff;padding:6px;border-radius:8px;display:block}.setup-url{font:600 13px var(--font-mono);word-break:break-all}");
  html += F(".site-footer{margin-top:18px;padding:12px 2px 4px;border-top:1px solid rgba(255,255,255,.08);font-size:12px;color:#8fa4d2;line-height:1.5}.site-footer strong{color:#dbe7ff}.site-footer a{color:#9fd9ff;text-decoration:none}.site-footer a:hover{color:#c7ebff;text-decoration:underline}");
  html += F("small{color:var(--txt2)}code{color:#d2ddff}@media(prefers-reduced-motion:reduce){.fx-grid *{animation-duration:0.01ms !important;transition-duration:0.01ms !important}}@media(max-width:980px){.grid2,.rss-composer{grid-template-columns:1fr}.fx-grid__plane{height:68vh;bottom:-4vh;background-size:32px 32px;opacity:1}.fx-grid__glow{height:66%;background:linear-gradient(to top,rgba(117,81,255,0.24) 0%,rgba(57,184,255,0.11) 45%,transparent 100%)}.fx-grid__horizon{bottom:58%}.hero-top{flex-wrap:nowrap;align-items:center}.hero-left{min-width:0;flex:1 1 auto}.logo{height:54px}.hero-right{justify-items:end;flex:0 0 auto}.release-box{width:auto}.hero-copy{padding:10px}}@media(max-width:420px){.hero-top{flex-wrap:wrap}.hero-right{width:100%;justify-items:start}}@media(max-width:480px){.btns{flex-direction:column}.btn.primary{width:100%;justify-content:center}}");
  html += F(".fx-grid__plane{background-image:linear-gradient(var(--grid-line-a) 1px,transparent 1px),linear-gradient(90deg,var(--grid-line-b) 1px,transparent 1px)}.fx-grid__glow{background:linear-gradient(to top,var(--grid-glow-a) 0%,var(--grid-glow-b) 45%,transparent 100%)}.fx-grid__horizon{background:linear-gradient(90deg,transparent 0%,var(--grid-horizon-a) 20%,var(--grid-horizon-b) 50%,var(--grid-horizon-a) 80%,transparent 100%)}.fx-grid__scanline{background:linear-gradient(90deg,transparent,var(--scanline),transparent)}.fx-grid__vline:nth-child(5){background:linear-gradient(to bottom,transparent,var(--vline-a),transparent)}.fx-grid__vline:nth-child(6){background:linear-gradient(to bottom,transparent,var(--vline-b),transparent)}.fx-grid__vline:nth-child(7){background:linear-gradient(to bottom,transparent,var(--vline-a),transparent)}.hero-top-card{border-color:var(--hero-border)}.hero-copy{border-color:var(--hero-copy-border);background:var(--hero-copy-bg)}.release-box{border-color:var(--release-border);background:var(--release-bg);font-family:var(--font-mono)}.release-box .k{color:var(--release-key)}.release-box .v{color:var(--release-value)}.sec{border-color:var(--border);background:var(--sec-bg)}.btn{font-family:var(--font-main)}.btn.ghost{background:var(--btn-ghost-bg);color:var(--btn-ghost-text)}input,select{font-family:var(--font-main)}");
  html += F("body[data-theme='minimal-brutalist-mono'] .fx-grid{display:none}body[data-theme='minimal-brutalist-mono'] .hero-top-card,body[data-theme='minimal-brutalist-mono'] .hero-copy,body[data-theme='minimal-brutalist-mono'] .sec,body[data-theme='minimal-brutalist-mono'] .card,body[data-theme='minimal-brutalist-mono'] .rss-row,body[data-theme='minimal-brutalist-mono'] .rss-empty,body[data-theme='minimal-brutalist-mono'] .api-note,body[data-theme='minimal-brutalist-mono'] .msg{background:var(--bg-surface)!important;border-color:var(--line)!important;backdrop-filter:none!important;-webkit-backdrop-filter:none!important;box-shadow:none!important}body[data-theme='minimal-brutalist-mono'] .sec h2,body[data-theme='minimal-brutalist-mono'] .rss-title{color:var(--txt)!important}body[data-theme='minimal-brutalist-mono'] .sec h2 i,body[data-theme='minimal-brutalist-mono'] .rss-title i,body[data-theme='minimal-brutalist-mono'] .site-footer a{color:var(--acc2)!important}body[data-theme='minimal-brutalist-mono'] .lede,body[data-theme='minimal-brutalist-mono'] .rss-meta,body[data-theme='minimal-brutalist-mono'] .hint,body[data-theme='minimal-brutalist-mono'] .geo-status,body[data-theme='minimal-brutalist-mono'] .site-footer{color:var(--txt2)!important}body[data-theme='minimal-brutalist-mono'] input,body[data-theme='minimal-brutalist-mono'] select{background:var(--bg-surface)!important;border-color:var(--line)!important;color:var(--txt)!important;box-shadow:none!important}body[data-theme='minimal-brutalist-mono'] .btn{border:1px solid var(--line)!important;box-shadow:none!important}body[data-theme='minimal-brutalist-mono'] .btn.primary{background:var(--acc2)!important;color:#FFFFFF!important;animation:none!important}body[data-theme='minimal-brutalist-mono'] .btn.ghost,body[data-theme='minimal-brutalist-mono'] .btn.warn,body[data-theme='minimal-brutalist-mono'] .btn.danger{background:var(--bg-surface)!important;color:var(--txt)!important}body[data-theme='minimal-brutalist-mono'] .pill,body[data-theme='minimal-brutalist-mono'] .rss-chip,body[data-theme='minimal-brutalist-mono'] .badge-soon{background:var(--line-soft)!important;color:var(--txt2)!important;border:1px solid var(--line)!important}body[data-theme='minimal-brutalist-mono'] .rss-row{border-left-color:var(--acc2)!important}body[data-theme='minimal-brutalist-mono'] *,body[data-theme='minimal-brutalist-mono'] *::before,body[data-theme='minimal-brutalist-mono'] *::after{border-radius:0!important}");
  html += F("</style></head><body data-theme='");
  appendHtmlEscaped(html, themeDef.id);
  html += F("'><div class='fx-grid'><div class='fx-grid__plane'></div><div class='fx-grid__glow'></div><div class='fx-grid__horizon'></div><div class='fx-grid__scanline'></div><div class='fx-grid__vline'></div><div class='fx-grid__vline'></div><div class='fx-grid__vline'></div><div class='fx-grid__vignette'></div></div><main class='wrap'>");
  html += F("<section class='hero'><div class='hero-top-card'><div class='hero-top'><div class='hero-left'><img class='logo' alt='Netmilk Studio' src='");
  appendHtmlEscaped(html, runtimeLogoUrl());
  html += F("'></div><div class='hero-right'><div class='release-box'><span class='k'><i class='fa-regular fa-calendar'></i> last release</span><span class='v'>");
  appendHtmlEscaped(html, FW_RELEASE_DATE);
  html += F("</span><span class='k'><i class='fa-solid fa-code-branch'></i> version</span><span class='v'>");
  appendHtmlEscaped(html, FW_BUILD_TAG);
  html += F("</span></div></div></div><div class='hero-copy'><p class='lede'>✨ <b>ScryBar</b> is a mass of sensors, pixels, and unresolved ambition, pretending to be furniture.<br>Time, weather, news, and a talking oracle. Everything you could faster check on your phone, but won't.<br>Overengineered with pride by <b>enuzzo</b>, stealing billable hours at <b>Netmilk Studio</b>. Reflashed at 2 AM with no regrets.<br><em>Your desk knows things now.</em></p></div></section>");
  html += F("<section class='panel'>");
  if (statusMsg && statusMsg[0]) {
    html += F("<p class='msg fixed-top'>");
    appendHtmlEscaped(html, statusMsg);
    html += F("</p>");
  }
  html += F("<form id='cfg_form' method='post' action='/config'>");
  // Visual theme section
  {
    html += F("<div class='sec'><h2><i class='fa-solid fa-palette'></i>Visual Theme</h2><div class='key'>THEME</div><select name='ui_theme'>");
    for (size_t i = 0; i < UI_THEME_COUNT; ++i) {
      html += F("<option value='");
      html += kUiThemes[i].id;
      html += '\'';
      if (strcmp(runtimeUiThemeId(), kUiThemes[i].id) == 0) html += F(" selected");
      html += '>';
      html += kUiThemes[i].label;
      html += F("</option>");
    }
    html += F("</select><p class='hint'><i class='fa-solid fa-bolt'></i> One selector drives both interfaces: this web control surface and the ESP32 display UI. Switching theme applies instantly and persists in NVS.</p></div>");
  }
  {
    html += F("<div class='sec'><h2><i class='fa-solid fa-table-cells-large'></i>Views</h2><div class='card'><div class='view-grid'>");
    html += F("<label class='view-card'><input id='view_info_cb' type='checkbox'");
    if (infoViewOn) html += F(" checked");
    html += F("><span class='view-copy'><strong>Info</strong><small>Word clock and ambient status page.</small></span></label>");
    html += F("<label class='view-card fixed'><input type='checkbox' checked disabled><span class='view-copy'><strong>Home</strong><small>Always on. Safe fallback when other pages are disabled.</small></span></label>");
    html += F("<label class='view-card'><input id='view_aux_cb' type='checkbox'");
    if (auxViewOn) html += F(" checked");
    html += F("><span class='view-copy'><strong>RSS / AUX</strong><small>News feed deck, QR and refresh actions.</small></span></label>");
    html += F("<label class='view-card'><input id='view_wiki_cb' type='checkbox'");
    if (wikiViewOn) html += F(" checked");
    html += F("><span class='view-copy'><strong>Wikipedia</strong><small>Featured, On This Day and random article cards.</small></span></label>");
    html += F("<label class='view-card");
    if (!doomFeatureAvailable) html += F(" disabled");
    html += F("'><input id='view_doom_cb' type='checkbox'");
    if (doomViewOn) html += F(" checked");
    if (!doomFeatureAvailable) html += F(" disabled");
    html += F("><span class='view-copy'><strong>DOOM</strong><small>");
    if (doomFeatureAvailable) html += F("Swipe-reachable game page with gyro + touch controls.");
    else html += F("Not available in this firmware build.");
    html += F("</small></span></label>");
    html += F("</div><p class='hint'><i class='fa-solid fa-hand-pointer'></i> Swipe navigation only includes enabled pages.</p><div id='view_hidden_inputs' class='hidden'></div></div></div>");
  }
#if TEST_WIFI
  // Wi-Fi preferred network section
  {
    const bool wifiOk = (WiFi.status() == WL_CONNECTED) && g_wifiConnected;
    const String activeSsid = wifiOk ? WiFi.SSID() : String("");
    char setupUrl[96] = "";
    wifiBuildSetupPortalUrl(setupUrl, sizeof(setupUrl));
    html += F("<div class='sec'><h2><i class='fa-solid fa-wifi'></i>Wi-Fi Known Networks</h2><div class='key'>PREFERRED SSID</div><select name='wifi_pref_ssid'>");
    html += F("<option value=''");
    if (!g_wifiPreferredSsid[0]) html += F(" selected");
    html += F(">Auto (smart rotation)</option>");
    for (uint8_t i = 0; i < g_wifiCredCount; ++i) {
      const char *ssid = g_wifiCredSsids[i];
      if (!ssid || !ssid[0]) continue;
      html += F("<option value='");
      appendHtmlEscaped(html, ssid);
      html += '\'';
      const bool selected = (strcmp(g_wifiPreferredSsid, ssid) == 0);
      if (selected) html += F(" selected");
      html += '>';
      appendHtmlEscaped(html, ssid);
      if (wifiOk && activeSsid.equals(ssid)) html += F(" (connected)");
      html += F("</option>");
    }
    html += F("</select><div class='key'>WIFI DIRECT MODE</div><select name='wifi_setup_mode'>");
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
    html += F("<p class='hint'><i class='fa-solid fa-circle-info'></i> Auto mode cycles known SSIDs first, then starts setup AP if disconnected too long. ScryBar supports <b>2.4 GHz only</b> (5 GHz is ignored).</p>");
  html += F("<div class='card'><div class='key'>PROVISION NEW NETWORK (2.4 GHZ)</div><div class='grid2'><div><button id='wifi_scan_btn' class='btn ghost sm' type='button'><i class='fa-solid fa-tower-cell'></i> Scan networks</button><p id='wifi_scan_status' class='rss-status'></p><div class='key'>SCAN RESULTS</div><select id='wifi_scan_results'><option value=''>Press scan first...</option></select></div><div><div class='key'>SSID</div><input id='wifi_new_ssid' name='wifi_new_ssid' maxlength='32' placeholder='MyPhone Hotspot'><div class='key'>PASSWORD</div><div class='secret-input-wrap'><input id='wifi_new_password' name='wifi_new_password' maxlength='64' type='password' placeholder='Leave empty if open network'><button id='wifi_pwd_toggle' class='btn ghost sm secret-toggle' type='button' aria-label='Show password' title='Show password'><i class='fa-solid fa-eye'></i></button></div></div></div>");
    if (g_wifiSetupApActive) {
      html += F("<p class='hint'><i class='fa-solid fa-satellite-dish'></i> Setup AP active: <code>");
      appendHtmlEscaped(html, g_wifiSetupApSsid);
      html += F("</code> @ <code>");
      html += WiFi.softAPIP().toString();
      html += F("</code></p>");
      html += F("<div class='setup-quick'><img class='setup-qr' src='/api/wifi/setup-qr.svg' alt='Setup QR'><div><div class='key'>SETUP URL</div><div class='setup-url'>");
      appendHtmlEscaped(html, setupUrl);
      html += F("</div><p class='hint'><i class='fa-solid fa-mobile-screen-button'></i> Scan this QR or open the URL manually to access setup instantly. Captive portal probes are redirected to this page.</p></div></div>");
    }
    html += F("<p class='hint'><i class='fa-solid fa-floppy-disk'></i> Save Config to store SSID/password in NVS (persistent across reboot/reflash; cleared only by NVS erase).</p></div>");
  }
#endif
  // System Language section
  {
    // Helper macro-style: emit one <option> with runtime selected check
    struct { const char *code; const char *label; } kLangsFun[] = {
      {"bellazio", "Bellazio"},
      {"val",  "Valley Girl"},
      {"l33t", "1337 5P34K"},
      {"sha",  "Shakespearean English"},

      {"eo",   "Esperanto"},
      {"la",   "Latina"},
      {"tlh",  "tlhIngan Hol (Klingon)"},
    };
    struct { const char *code; const char *label; } kLangsStd[] = {
      {"en",  "English"},
      {"it",  "Italiano"},
      {"es",  "Espa\xC3\xB1" "ol"},
      {"fr",  "Fran\xC3\xA7" "ais"},
      {"de",  "Deutsch"},
      {"pt",  "Portugu\xC3\xAA" "s"},
    };
    html += F("<div class='sec'><h2><i class='fa-solid fa-language'></i>System Language</h2><div class='key'>LANGUAGE</div><select name='wc_lang'>");
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
    html += F("</optgroup></select><p class='hint'><i class='fa-solid fa-circle-info'></i> Controls the language of the entire display UI: word clock, weather labels, RSS status and touch hints. Saved to NVS, persists across reboots.</p></div>");
  }
  {
    struct { const char *code; const char *label; } kWikiLangs[] = {
      {"en", "English"}, {"it", "Italiano"},
      {"fr", "Fran\xC3\xA7" "ais"}, {"de", "Deutsch"},
      {"es", "Espa\xC3\xB1" "ol"}, {"pt", "Portugu\xC3\xAA" "s"},
      {"la", "Latina"}, {"eo", "Esperanto"},
    };
    html += F("<div class='sec'><h2><i class='fa-solid fa-book-open'></i>Wikipedia Language</h2>"
              "<div class='key'>WIKI LANGUAGE</div><select name='wiki_lang'>");
    for (unsigned i = 0; i < sizeof(kWikiLangs)/sizeof(kWikiLangs[0]); ++i) {
      html += F("<option value='");
      html += kWikiLangs[i].code;
      html += '\'';
      if (strcmp(g_wikiLang, kWikiLangs[i].code) == 0) html += F(" selected");
      html += '>';
      html += kWikiLangs[i].label;
      html += F("</option>");
    }
    html += F("</select><p class='hint'><i class='fa-solid fa-circle-info'></i> "
              "Language for Wikipedia feeds (Featured, On This Day, Random). Independent from the system language.</p></div>");
  }
  html += F("<div class='sec'><h2><i class='fa-solid fa-location-dot'></i>Weather & Location</h2><div class='grid2'><div><div class='key'>PLACE SEARCH</div><input id='geo_query' type='search' list='geo_hits' placeholder='Search city or place'><datalist id='geo_hits'></datalist><p id='geo_status' class='geo-status'></p><div class='key'>CITY LABEL</div><input id='weather_city' name='weather_city' maxlength='31' value='");
  appendHtmlEscaped(html, runtimeWeatherCityLabel());
  html += F("'></div><div class='grid2'><div><div class='key'>LATITUDE</div><input id='weather_lat' name='weather_lat' value='");
  appendHtmlEscaped(html, latBuf);
  html += F("'></div><div><div class='key'>LONGITUDE</div><input id='weather_lon' name='weather_lon' value='");
  appendHtmlEscaped(html, lonBuf);
  html += F("'></div></div></div></div>");
  html += F("<div class='sec'><h2><i class='fa-solid fa-square-rss'></i>RSS Feed Builder <span id='rss_count_pill' class='pill'><i class='fa-solid fa-rss'></i> RSS feeds ");
  html += configuredFeeds;
  html += F("/5</span></h2><p class='hint'>One composer for name, URL and max posts. Press + to add to the list (max 5 feeds).</p>");
  html += F("<div class='card'><div class='rss-composer'><div><div class='key'>FRIENDLY NAME</div><input id='rss_name' maxlength='23' placeholder='Nintendo'></div><div><div class='key'>FEED URL</div><input id='rss_url' type='url' placeholder='https://example.com/feed.xml'></div><div><div class='key'>MAX POSTS</div><input id='rss_max' type='number' min='1' max='8' value='8'></div><button id='rss_add' class='btn primary' type='button'><i class='fa-solid fa-circle-plus'></i>Add</button><button id='rss_reset' class='btn ghost' type='button'><i class='fa-solid fa-broom'></i>Reset</button></div><p id='rss_status' class='rss-status'></p></div>");
  html += F("<div id='rss_list' class='rss-list'></div><p id='rss_empty' class='rss-empty'>No feeds configured.</p><div id='rss_hidden_inputs' class='hidden'></div></div>");
  html += F("<div class='btns'><button class='btn primary' type='submit'><i class='fa-solid fa-floppy-disk'></i>Save Config</button><button class='btn ghost' type='submit' formaction='/reload' formmethod='post'><i class='fa-solid fa-rotate-right'></i>Force Weather + RSS + Wiki Reload</button></div></form>");
  // System Info section — built at request time from live globals
  {
    char siBuf[48];
    html += F("<div class='sec'><h2><i class='fa-solid fa-microchip'></i>System Info</h2><div class='grid2'>");
    // Network card
    html += F("<div class='card'><div class='key'>NETWORK</div>");
#if TEST_WIFI
    {
      const bool wOk = (WiFi.status() == WL_CONNECTED) && g_wifiConnected;
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
      if (g_wifiPreferredSsid[0]) appendHtmlEscaped(html, g_wifiPreferredSsid);
      else html += F("auto");
      html += F("</code><br><small>direct mode: </small><code>");
      appendHtmlEscaped(html, g_wifiSetupMode);
      html += F("</code><br><small>setup ap: </small><code>");
      if (g_wifiSetupApActive) {
        appendHtmlEscaped(html, g_wifiSetupApSsid);
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
    html += F("<div class='card'><div class='key'>FIRMWARE &amp; RUNTIME</div>");
    html += F("<small>fw: </small><code>"); appendHtmlEscaped(html, FW_BUILD_TAG); html += F("</code><br>");
    html += F("<small>date: </small><code>"); appendHtmlEscaped(html, FW_RELEASE_DATE); html += F("</code><br>");
    html += F("<small>lang: </small><code>"); appendHtmlEscaped(html, g_wordClockLang); html += F("</code><br>");
    html += F("<small>theme: </small><code>"); appendHtmlEscaped(html, runtimeUiThemeLabel()); html += F("</code><br>");
    snprintf(siBuf, sizeof(siBuf), "%lus", (unsigned long)(millis() / 1000UL));
    html += F("<small>uptime: </small><code>"); html += siBuf; html += F("</code><br>");
#if TEST_NTP
    html += F("<small>ntp: </small><code>"); html += g_ntpSynced ? "SYNCED" : "WAIT"; html += F("</code><br>");
#endif
    snprintf(siBuf, sizeof(siBuf), "%u KB", (unsigned)(ESP.getFreeHeap() / 1024));
    html += F("<small>free heap: </small><code>"); html += siBuf; html += F("</code>");
#if TEST_BATTERY
    html += F("<br><small>battery: </small><code>");
    if (g_battHasSample) {
      snprintf(siBuf, sizeof(siBuf), "%d%%", g_battPercent);
      html += siBuf;
      if (g_battChargingLikely) html += F(" +CHG");
    } else { html += F("N/A"); }
    html += F("</code>");
#endif
    html += F("</div>");
    html += F("</div></div>");
  }
  html += F("<p class='api-note'><small><i class='fa-solid fa-terminal'></i> API ready: <code>GET /api/config</code>, <code>POST /api/config</code>, <code>GET /api/wifi/scan</code>, <code>GET /api/wifi/setup-qr.svg</code>.</small></p>");
  html += F("<footer class='site-footer'><strong>A project by Netmilk Studio sagl</strong> | Copyright 2026<br>Open Source under the <a href='https://opensource.org/license/mit' target='_blank' rel='noopener noreferrer'>MIT License</a> | Feel free to steal, fork, remix, and ship. \xF0\x9F\x96\x96</footer>");
  html += F("<script>(function(){");
  html += F("(function(){const t=document.querySelector('.msg.fixed-top');if(t){setTimeout(function(){t.style.transition='opacity .6s';t.style.opacity='0';setTimeout(function(){t.style.display='none';},650);},4200);}})();");
  html += F("const q=document.getElementById('geo_query');const dl=document.getElementById('geo_hits');const st=document.getElementById('geo_status');const city=document.getElementById('weather_city');const lat=document.getElementById('weather_lat');const lon=document.getElementById('weather_lon');");
  html += F("if(!q||!dl||!city||!lat||!lon)return;let t=0;let map={};function setStatus(msg){if(st)st.textContent=msg||'';}function clearHits(){dl.innerHTML='';map={};}");
  html += F("function applyPick(key){const r=map[key];if(!r)return false;city.value=r.name||city.value;lat.value=Number(r.latitude).toFixed(4);lon.value=Number(r.longitude).toFixed(4);setStatus('Coordinates filled in automatically.');return true;}");
  html += F("q.addEventListener('change',function(){applyPick(q.value);});q.addEventListener('blur',function(){applyPick(q.value);});");
  html += F("q.addEventListener('input',function(){const term=q.value.trim();if(term.length<2){clearHits();setStatus('');return;}clearTimeout(t);t=setTimeout(async function(){try{setStatus('Searching...');const u='https://geocoding-api.open-meteo.com/v1/search?count=6&language=en&format=json&name='+encodeURIComponent(term);const r=await fetch(u,{cache:'no-store'});if(!r.ok)throw new Error('http '+r.status);const data=await r.json();const rows=(data&&data.results)?data.results:[];clearHits();if(!rows.length){setStatus('No results found.');return;}rows.forEach(function(it){const label=[it.name,it.admin1,it.country].filter(Boolean).join(', ');const opt=document.createElement('option');opt.value=label;opt.label=(Number(it.latitude).toFixed(4)+', '+Number(it.longitude).toFixed(4));dl.appendChild(opt);map[label]=it;});setStatus('Select a result to fill city / lat / lon.');if(rows.length===1){const one=[rows[0].name,rows[0].admin1,rows[0].country].filter(Boolean).join(', ');q.value=one;applyPick(one);}}catch(e){clearHits();setStatus('Search unavailable, try again later.');}} ,280);});");
  html += F("const wifiScanBtn=document.getElementById('wifi_scan_btn');const wifiScanResults=document.getElementById('wifi_scan_results');const wifiScanStatus=document.getElementById('wifi_scan_status');const wifiNewSsid=document.getElementById('wifi_new_ssid');const wifiNewPassword=document.getElementById('wifi_new_password');const wifiPwdToggle=document.getElementById('wifi_pwd_toggle');");
  html += F("function setWifiStatus(msg){if(wifiScanStatus)wifiScanStatus.textContent=msg||'';}function escHtml(s){return (s||'').replace(/[&<>]/g,function(c){return({'&':'&amp;','<':'&lt;','>':'&gt;'})[c];});}");
  html += F("async function scanWifiNow(){if(!wifiScanResults)return;const ctl=(window.AbortController?new AbortController():null);let tm=0;try{setWifiStatus('Scanning 2.4 GHz networks...');wifiScanResults.innerHTML=\"<option value=''>Scanning...</option>\";if(ctl){tm=window.setTimeout(function(){ctl.abort();},9000);}const res=await fetch('/api/wifi/scan',{cache:'no-store',signal:ctl?ctl.signal:undefined});if(tm)window.clearTimeout(tm);if(!res.ok)throw new Error('http '+res.status);const data=await res.json();const rows=(data&&data.networks)?data.networks:[];wifiScanResults.innerHTML=\"\";if(!rows.length){wifiScanResults.innerHTML=\"<option value=''>No 2.4 GHz network found</option>\";setWifiStatus((data&&data.message==='scan_timeout')?'Scan timed out, retry in a few seconds.':'No 2.4 GHz networks found.');return;}rows.forEach(function(n){const opt=document.createElement('option');const lock=n.secure?'SEC':'OPEN';opt.value=n.ssid||'';opt.dataset.secure=n.secure?'1':'0';opt.dataset.channel=String(n.channel||0);opt.innerHTML=escHtml((n.ssid||'(hidden)')+'  '+lock+'  ch'+(n.channel||'?')+'  '+(n.rssi||0)+'dBm');wifiScanResults.appendChild(opt);});setWifiStatus('Scan complete. Pick an SSID and press Save Config.');if(rows[0]&&rows[0].ssid&&wifiNewSsid&&!wifiNewSsid.value){wifiNewSsid.value=rows[0].ssid;}}catch(e){if(tm)window.clearTimeout(tm);wifiScanResults.innerHTML=\"<option value=''>Scan failed</option>\";setWifiStatus((e&&e.name==='AbortError')?'Scan timeout. Retry.':'Scan unavailable right now.');}}");
  html += F("function syncWifiPwdToggle(){if(!wifiPwdToggle||!wifiNewPassword)return;const visible=wifiNewPassword.type==='text';wifiPwdToggle.innerHTML=visible?\"<i class='fa-solid fa-eye-slash'></i>\":\"<i class='fa-solid fa-eye'></i>\";wifiPwdToggle.title=visible?'Hide password':'Show password';wifiPwdToggle.setAttribute('aria-label',wifiPwdToggle.title);}if(wifiPwdToggle&&wifiNewPassword){wifiPwdToggle.addEventListener('click',function(){wifiNewPassword.type=(wifiNewPassword.type==='password')?'text':'password';syncWifiPwdToggle();});syncWifiPwdToggle();}if(wifiScanBtn)wifiScanBtn.addEventListener('click',function(){scanWifiNow();});if(wifiScanResults)wifiScanResults.addEventListener('change',function(){const v=wifiScanResults.value||'';if(wifiNewSsid&&v)wifiNewSsid.value=v;const sel=wifiScanResults.options[wifiScanResults.selectedIndex];if(wifiNewPassword&&sel&&sel.dataset.secure==='0'){wifiNewPassword.value='';wifiNewPassword.placeholder='Open network (no password)';}else if(wifiNewPassword){wifiNewPassword.placeholder='Password (WPA/WPA2)';}});");
  html += F("const form=document.getElementById('cfg_form');const rssName=document.getElementById('rss_name');const rssUrl=document.getElementById('rss_url');const rssMax=document.getElementById('rss_max');const rssAdd=document.getElementById('rss_add');const rssReset=document.getElementById('rss_reset');const rssList=document.getElementById('rss_list');const rssEmpty=document.getElementById('rss_empty');const rssStatus=document.getElementById('rss_status');const rssHidden=document.getElementById('rss_hidden_inputs');const rssPill=document.getElementById('rss_count_pill');const viewHidden=document.getElementById('view_hidden_inputs');");
  html += F("const maxSlots=5;const minPosts=1;const maxPosts=8;let editIndex=-1;");
  html += F("const initialFeeds=[");
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
  html += F("];");
  html += F("let feeds=initialFeeds.filter(f=>f&&f.url&&/^https?:\\/\\//i.test(f.url));");
  html += F("function clampPosts(n){n=parseInt(n,10);if(isNaN(n))n=maxPosts;if(n<minPosts)n=minPosts;if(n>maxPosts)n=maxPosts;return n;}function startsHttp(v){return /^https?:\\/\\//i.test((v||'').trim());}");
  html += F("function defName(i){return 'Feed '+(i+1);}function setRssStatus(m){if(rssStatus)rssStatus.textContent=m||'';}");
  html += F("function clearComposer(){editIndex=-1;rssName.value='';rssUrl.value='';rssMax.value='8';rssAdd.innerHTML=\"<i class='fa-solid fa-circle-plus'></i>Add\";setRssStatus('');}");
  html += F("function renderFeeds(){if(!rssList)return;rssList.innerHTML='';if(rssPill)rssPill.innerHTML=\"<i class='fa-solid fa-rss'></i> RSS feeds \"+feeds.length+'/5';if(rssEmpty)rssEmpty.style.display=feeds.length?'none':'block';feeds.forEach(function(f,idx){const row=document.createElement('div');row.className='rss-row';const left=document.createElement('div');const t=document.createElement('p');t.className='rss-title';t.innerHTML=\"<i class='fa-solid fa-signal'></i>\";t.appendChild(document.createTextNode(f.name||defName(idx)));const chip=document.createElement('span');chip.className='rss-chip';chip.textContent='max '+clampPosts(f.max);t.appendChild(chip);const m=document.createElement('p');m.className='rss-meta';m.textContent=f.url||'';left.appendChild(t);left.appendChild(m);const act=document.createElement('div');act.className='rss-actions';const bEdit=document.createElement('button');bEdit.type='button';bEdit.className='btn sm warn';bEdit.innerHTML=\"<i class='fa-solid fa-pen-to-square'></i>Edit\";bEdit.addEventListener('click',function(){editIndex=idx;rssName.value=f.name||'';rssUrl.value=f.url||'';rssMax.value=String(clampPosts(f.max));rssAdd.innerHTML=\"<i class='fa-solid fa-floppy-disk'></i>Update\";setRssStatus('Editing feed '+(idx+1));});const bDel=document.createElement('button');bDel.type='button';bDel.className='btn sm danger';bDel.innerHTML=\"<i class='fa-solid fa-trash-can'></i>Delete\";bDel.addEventListener('click',function(){feeds.splice(idx,1);if(editIndex===idx)clearComposer();else if(editIndex>idx)editIndex-=1;renderFeeds();setRssStatus('Feed removed.');});act.appendChild(bEdit);act.appendChild(bDel);row.appendChild(left);row.appendChild(act);rssList.appendChild(row);});}");
  html += F("function pushOrUpdate(){const name=(rssName.value||'').trim();const url=(rssUrl.value||'').trim();const max=clampPosts(rssMax.value);if(!url){setRssStatus('Please enter a feed URL.');return;}if(!startsHttp(url)){setRssStatus('URL must start with http:// or https://');return;}const item={name:name||defName(editIndex>=0?editIndex:feeds.length),url:url,max:max};if(editIndex>=0){feeds[editIndex]=item;clearComposer();setRssStatus('Feed updated.');renderFeeds();return;}if(feeds.length>=maxSlots){setRssStatus('Maximum limit: 5 feeds.');return;}feeds.push(item);clearComposer();renderFeeds();setRssStatus('Feed added.');}");
  html += F("function addHidden(k,v){const i=document.createElement('input');i.type='hidden';i.name=k;i.value=v;rssHidden.appendChild(i);}function buildHiddenInputs(){if(!rssHidden)return;rssHidden.innerHTML='';for(let i=0;i<maxSlots;i+=1){const f=feeds[i]||{name:defName(i),url:'',max:maxPosts};addHidden('rss_feed_name_'+(i+1),f.name||defName(i));addHidden('rss_feed_url_'+(i+1),f.url||'');addHidden('rss_feed_items_'+(i+1),String(clampPosts(f.max)));}const f0=feeds[0]||{name:defName(0),url:'',max:maxPosts};addHidden('rss_feed_name',f0.name||defName(0));addHidden('rss_feed_url',f0.url||'');addHidden('rss_feed_items',String(clampPosts(f0.max)));}");
  html += F("function addViewHidden(k,v){if(!viewHidden)return;const i=document.createElement('input');i.type='hidden';i.name=k;i.value=v;viewHidden.appendChild(i);}function buildViewHiddenInputs(){if(!viewHidden)return;viewHidden.innerHTML='';[['view_info','view_info_cb'],['view_aux','view_aux_cb'],['view_wiki','view_wiki_cb'],['view_doom','view_doom_cb']].forEach(function(pair){const el=document.getElementById(pair[1]);addViewHidden(pair[0],(el&&el.checked)?'1':'0');});}");
  html += F("if(rssAdd)rssAdd.addEventListener('click',function(){pushOrUpdate();});if(rssReset)rssReset.addEventListener('click',function(){clearComposer();setRssStatus('Composer cleared.');});if(form)form.addEventListener('submit',function(){buildHiddenInputs();buildViewHiddenInputs();});renderFeeds();");
  html += F("})();</script>");
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
  appendJsonEscaped(out, g_wifiPreferredSsid);
  out += F("\",\"known\":[");
  bool firstSsid = true;
  for (uint8_t i = 0; i < g_wifiCredCount; ++i) {
    const char *ssid = g_wifiCredSsids[i];
    if (!ssid || !ssid[0]) continue;
    if (!firstSsid) out += ',';
    firstSsid = false;
    out += '"';
    appendJsonEscaped(out, ssid);
    out += '"';
  }
  out += F("],\"setup_mode\":\"");
  appendJsonEscaped(out, g_wifiSetupMode);
  out += F("\",\"setup_ap_active\":");
  out += g_wifiSetupApActive ? F("true") : F("false");
  out += F(",\"setup_ap_ssid\":\"");
  appendJsonEscaped(out, g_wifiSetupApSsid);
  out += F("\",\"setup_ap_ip\":\"");
  appendJsonEscaped(out, WiFi.softAPIP().toString().c_str());
  out += F("\",\"runtime_known\":");
  out += (unsigned)g_wifiRuntimeCredCount;
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
  out += F(",\"doom\":");
  out += (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_DOOM) ? F("true") : F("false");
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
  g_webConfigServer.send(code, "application/json", out);
}

static void webConfigRedirect(const char *status) {
  String location = "/";
  if (status && status[0]) {
    location += "?status=";
    location += status;
  }
  g_webConfigServer.sendHeader("Location", location, true);
  g_webConfigServer.send(303, "text/plain", "");
}

static bool applyRuntimeConfigFromRequest(String &errorOut) {
  ensureRuntimeNetConfig();
  RuntimeNetConfig next = g_runtimeNetConfig;
  bool hasInput = false;
  bool langChanged = false;
  bool wikiLangChanged = false;
  bool wifiPrefChanged = false;
  bool wifiSetupModeChanged = false;
  bool wifiProvisioned = false;
  int8_t wifiPrefIdx = -1;

  if (g_webConfigServer.hasArg("weather_city")) {
    hasInput = true;
    String city = g_webConfigServer.arg("weather_city");
    city.trim();
    if (city.length() == 0) {
      errorOut = "weather_city vuota";
      return false;
    }
    copyStringSafe(next.weatherCity, sizeof(next.weatherCity), city.c_str());
  }

  if (g_webConfigServer.hasArg("weather_lat")) {
    hasInput = true;
    float lat = 0.0f;
    if (!parseStrictFloat(g_webConfigServer.arg("weather_lat"), lat) || !isfinite(lat) || lat < -90.0f || lat > 90.0f) {
      errorOut = "weather_lat non valida";
      return false;
    }
    next.weatherLat = lat;
  }

  if (g_webConfigServer.hasArg("weather_lon")) {
    hasInput = true;
    float lon = 0.0f;
    if (!parseStrictFloat(g_webConfigServer.arg("weather_lon"), lon) || !isfinite(lon) || lon < -180.0f || lon > 180.0f) {
      errorOut = "weather_lon non valida";
      return false;
    }
    next.weatherLon = lon;
  }

  bool rssInput = false;
  if (g_webConfigServer.hasArg("rss_feed_url")) {
    hasInput = true;
    rssInput = true;
    String rss = g_webConfigServer.arg("rss_feed_url");
    rss.trim();
    if (rss.length() > 0 && !isHttpUrl(rss)) {
      errorOut = "rss_feed_url deve iniziare con http:// o https://";
      return false;
    }
    if (rss.length() == 0) next.rssFeeds[0].url[0] = '\0';
    else copyStringSafe(next.rssFeeds[0].url, sizeof(next.rssFeeds[0].url), rss.c_str());
  }
  if (g_webConfigServer.hasArg("rss_feed_name")) {
    hasInput = true;
    rssInput = true;
    String name = g_webConfigServer.arg("rss_feed_name");
    name.trim();
    copyStringSafe(next.rssFeeds[0].name, sizeof(next.rssFeeds[0].name), name.c_str());
  }
  if (g_webConfigServer.hasArg("rss_feed_items")) {
    hasInput = true;
    rssInput = true;
    uint8_t maxItems = 0;
    if (!parseStrictUint8(g_webConfigServer.arg("rss_feed_items"), maxItems)) {
      errorOut = "rss_feed_items non valido";
      return false;
    }
    next.rssFeeds[0].maxItems = clampRssFeedMaxItems(maxItems);
  }

  for (uint8_t i = 0; i < RSS_FEED_SLOT_COUNT; ++i) {
    char keyName[32];
    char keyUrl[32];
    char keyItems[32];
    snprintf(keyName, sizeof(keyName), "rss_feed_name_%u", (unsigned)(i + 1));
    snprintf(keyUrl, sizeof(keyUrl), "rss_feed_url_%u", (unsigned)(i + 1));
    snprintf(keyItems, sizeof(keyItems), "rss_feed_items_%u", (unsigned)(i + 1));

    if (g_webConfigServer.hasArg(keyName)) {
      hasInput = true;
      rssInput = true;
      String name = g_webConfigServer.arg(keyName);
      name.trim();
      copyStringSafe(next.rssFeeds[i].name, sizeof(next.rssFeeds[i].name), name.c_str());
    }

    if (g_webConfigServer.hasArg(keyUrl)) {
      hasInput = true;
      rssInput = true;
      String url = g_webConfigServer.arg(keyUrl);
      url.trim();
      if (url.length() > 0 && !isHttpUrl(url)) {
        errorOut = "rss_feed_url_N deve iniziare con http:// o https://";
        return false;
      }
      if (url.length() == 0) next.rssFeeds[i].url[0] = '\0';
      else copyStringSafe(next.rssFeeds[i].url, sizeof(next.rssFeeds[i].url), url.c_str());
    }

    if (g_webConfigServer.hasArg(keyItems)) {
      hasInput = true;
      rssInput = true;
      uint8_t maxItems = 0;
      if (!parseStrictUint8(g_webConfigServer.arg(keyItems), maxItems)) {
        errorOut = "rss_feed_items_N non valido";
        return false;
      }
      next.rssFeeds[i].maxItems = clampRssFeedMaxItems(maxItems);
    }
  }

  if (rssInput) {
    normalizeRuntimeRssFeeds(next);
  }

  if (g_webConfigServer.hasArg("logo_url")) {
    hasInput = true;
    String logo = g_webConfigServer.arg("logo_url");
    logo.trim();
    if (logo.length() > 0 && !isHttpUrl(logo)) {
      errorOut = "logo_url deve iniziare con http:// o https://";
      return false;
    }
    copyStringSafe(next.logoUrl, sizeof(next.logoUrl), logo.c_str());
  }

  if (g_webConfigServer.hasArg("ui_theme")) {
    hasInput = true;
    String theme = g_webConfigServer.arg("ui_theme");
    theme.trim();
    if (theme.length() == 0) {
      errorOut = "ui_theme vuoto";
      return false;
    }
    if (findUiThemeIndexById(theme.c_str()) < 0) {
      errorOut = "ui_theme non valido";
      return false;
    }
    copyStringSafe(next.uiTheme, sizeof(next.uiTheme), theme.c_str());
  }

  bool viewsInput = false;
  struct ViewArgDef { const char *key; uint8_t bit; };
  static const ViewArgDef kViewArgs[] = {
      {"view_info", UI_VIEW_FLAG_INFO},
      {"view_aux", UI_VIEW_FLAG_AUX},
      {"view_wiki", UI_VIEW_FLAG_WIKI},
      {"view_doom", UI_VIEW_FLAG_DOOM},
  };
  for (const ViewArgDef &viewArg : kViewArgs) {
    if (!g_webConfigServer.hasArg(viewArg.key)) continue;
    hasInput = true;
    viewsInput = true;
    bool enabled = false;
    if (!parseStrictBool(g_webConfigServer.arg(viewArg.key), enabled)) {
      errorOut = String(viewArg.key) + " non valido";
      return false;
    }
    if (enabled) next.enabledViewsMask |= viewArg.bit;
    else next.enabledViewsMask &= (uint8_t)~viewArg.bit;
  }
  if (viewsInput) {
    next.enabledViewsMask = normalizeRuntimeViewMask(next.enabledViewsMask);
  }

  if (g_webConfigServer.hasArg("wifi_setup_mode")) {
    hasInput = true;
    String setupMode = g_webConfigServer.arg("wifi_setup_mode");
    setupMode.trim();
    setupMode.toLowerCase();
    if (!(setupMode == "off" || setupMode == "auto" || setupMode == "on")) {
      errorOut = "wifi_setup_mode non valido";
      return false;
    }
    if (strncmp(g_wifiSetupMode, setupMode.c_str(), sizeof(g_wifiSetupMode)) != 0) {
      copyStringSafe(g_wifiSetupMode, sizeof(g_wifiSetupMode), setupMode.c_str());
      wifiSetupModeChanged = true;
    }
  }

  if (g_webConfigServer.hasArg("wifi_new_ssid") || g_webConfigServer.hasArg("wifi_new_password")) {
    hasInput = true;
    String ssid = g_webConfigServer.arg("wifi_new_ssid");
    String pass = g_webConfigServer.arg("wifi_new_password");
    ssid.trim();
    if (ssid.length() > 0) {
      if (ssid.length() > WIFI_MAX_SSID_LEN) {
        errorOut = "wifi_new_ssid troppo lungo";
        return false;
      }
      if (pass.length() > WIFI_MAX_PASSWORD_LEN) {
        errorOut = "wifi_new_password troppo lunga";
        return false;
      }
      if (!upsertRuntimeWiFiCredential(ssid.c_str(), pass.c_str())) {
        errorOut = "impossibile salvare rete runtime";
        return false;
      }
      wifiPrepareCredentialCache();
      char previousPref[sizeof(g_wifiPreferredSsid)] = {0};
      copyStringSafe(previousPref, sizeof(previousPref), g_wifiPreferredSsid);
      copyStringSafe(g_wifiPreferredSsid, sizeof(g_wifiPreferredSsid), ssid.c_str());
      wifiPrefChanged = true;
      wifiProvisioned = true;
      wifiPrefIdx = findWiFiCredentialIndexBySsid(g_wifiPreferredSsid);
      if (wifiPrefIdx < 0) wifiPrefChanged = (strcmp(previousPref, g_wifiPreferredSsid) != 0);
      Serial.printf("[WIFI][PROVISION] added ssid='%s' runtime_known=%u\n",
                    g_wifiPreferredSsid,
                    (unsigned)g_wifiRuntimeCredCount);
    }
  }

  if (!wifiProvisioned && g_webConfigServer.hasArg("wifi_pref_ssid")) {
    hasInput = true;
    String preferred = g_webConfigServer.arg("wifi_pref_ssid");
    preferred.trim();
    char previousPref[sizeof(g_wifiPreferredSsid)] = {0};
    copyStringSafe(previousPref, sizeof(previousPref), g_wifiPreferredSsid);
    if (preferred.length() == 0) {
      g_wifiPreferredSsid[0] = '\0';
      wifiPrefChanged = (previousPref[0] != '\0');
    } else {
      if (preferred.length() >= sizeof(g_wifiPreferredSsid)) {
        errorOut = "wifi_pref_ssid troppo lungo";
        return false;
      }
      wifiPrefIdx = findWiFiCredentialIndexBySsid(preferred.c_str());
      if (wifiPrefIdx < 0) {
        errorOut = "wifi_pref_ssid non presente nelle reti note";
        return false;
      }
      copyStringSafe(g_wifiPreferredSsid, sizeof(g_wifiPreferredSsid), preferred.c_str());
      wifiPrefChanged = (strcmp(previousPref, g_wifiPreferredSsid) != 0);
    }
  }

  if (g_webConfigServer.hasArg("wc_lang")) {
    hasInput = true;
    String lang = g_webConfigServer.arg("wc_lang");
    lang.trim();
    lang.toLowerCase();
    if (!isValidLangCode(lang)) {
      errorOut = "wc_lang non valido";
      return false;
    }
    if (strncmp(g_wordClockLang, lang.c_str(), sizeof(g_wordClockLang)) != 0) {
      copyStringSafe(g_wordClockLang, sizeof(g_wordClockLang), lang.c_str());
      langChanged = true;
      Serial.printf("[CFG][WEB] wc_lang='%s'\n", g_wordClockLang);
    }
  }

  if (g_webConfigServer.hasArg("wiki_lang")) {
    hasInput = true;
    String wl = g_webConfigServer.arg("wiki_lang");
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

  if (!hasInput) {
    errorOut = "nessun parametro";
    return false;
  }

  normalizeRuntimeUiTheme(next);

  const bool weatherChanged =
      (strncmp(g_runtimeNetConfig.weatherCity, next.weatherCity, sizeof(next.weatherCity)) != 0) ||
      (fabsf(g_runtimeNetConfig.weatherLat - next.weatherLat) > 0.00005f) ||
      (fabsf(g_runtimeNetConfig.weatherLon - next.weatherLon) > 0.00005f);
  bool rssChanged = false;
  for (uint8_t i = 0; i < RSS_FEED_SLOT_COUNT; ++i) {
    if (!runtimeRssFeedEntriesEqual(g_runtimeNetConfig.rssFeeds[i], next.rssFeeds[i])) {
      rssChanged = true;
      break;
    }
  }
  const bool brandingChanged =
      (strncmp(g_runtimeNetConfig.logoUrl, next.logoUrl, sizeof(next.logoUrl)) != 0);
  const bool themeChanged =
      (strncmp(g_runtimeNetConfig.uiTheme, next.uiTheme, sizeof(next.uiTheme)) != 0);
  const bool viewsChanged =
      (g_runtimeNetConfig.enabledViewsMask != next.enabledViewsMask);

  g_runtimeNetConfig = next;
  normalizeRuntimeUiTheme(g_runtimeNetConfig);
  g_runtimeNetConfig.enabledViewsMask = normalizeRuntimeViewMask(g_runtimeNetConfig.enabledViewsMask);
  syncActiveUiThemeFromRuntimeConfig(g_runtimeNetConfig);
  g_runtimeNetConfig.ready = true;
  const bool nvsSaved = saveRuntimeNetConfigToNvs();
  if (!nvsSaved) {
    Serial.println("[CFG][NVS] warning: config aggiornata in RAM ma non salvata su flash");
  }

  if (weatherChanged) {
    g_weather.valid = false;
    g_weather.lastFetchMs = 0;
  }
  if (rssChanged) {
    g_rss.valid = false;
    g_rss.itemCount = 0;
    g_rss.currentIndex = 0;
    g_rss.lastFetchMs = 0;
    g_rss.lastAttemptMs = 0;
    g_rss.lastRotateMs = 0;
    g_rss.lastHttpCode = 0;
    strncpy(g_rss.fetchedAt, "--/-- --:--", sizeof(g_rss.fetchedAt) - 1);
    g_rss.fetchedAt[sizeof(g_rss.fetchedAt) - 1] = '\0';
  }
  if (wikiLangChanged) {
    g_wiki.valid = false;
    g_wiki.itemCount = 0;
    g_wiki.currentIndex = 0;
    g_wiki.lastFetchMs = 0;
    g_wiki.lastAttemptMs = 0;
    g_wiki.lastRotateMs = 0;
    g_wiki.lastShortenAttemptMs = 0;
    g_wiki.lastHttpCode = 0;
    strncpy(g_wiki.fetchedAt, "--/-- --:--", sizeof(g_wiki.fetchedAt) - 1);
    g_wiki.fetchedAt[sizeof(g_wiki.fetchedAt) - 1] = '\0';
  }
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI
  if (themeChanged && g_lvglReady) lvglApplyThemeStyles(true);
#endif
#if TEST_NTP
  if (weatherChanged || rssChanged || brandingChanged || themeChanged || langChanged || wikiLangChanged || viewsChanged) g_uiNeedsRedraw = true;
#endif
  if (wifiPrefChanged) {
    if (wifiPrefIdx >= 0) g_wifiReconnectIdx = (uint8_t)wifiPrefIdx;
    else if (g_wifiReconnectIdx >= g_wifiCredCount) g_wifiReconnectIdx = 0;
    if (g_wifiCredCount > 0) {
      const bool wifiUp = wifiIsConnectedNow();
      bool alreadyOnTarget = false;
      if (wifiUp && wifiPrefIdx >= 0) {
        alreadyOnTarget = WiFi.SSID().equals(g_wifiCredSsids[wifiPrefIdx]);
      }
      if (!alreadyOnTarget) {
        wifiScheduleNextAttempt(millis(), 0UL);
        if (wifiUp) {
          g_wifiInternalDisconnect = true;
          WiFi.disconnect(true, false);
          delay(20);
          g_wifiInternalDisconnect = false;
        }
      }
    }
  }
  if (wifiSetupModeChanged) {
    normalizeWifiSetupMode();
    if (wifiSetupModeIsOff()) wifiStopSetupAp();
    else wifiHandleSetupModeLoop(millis());
  }
  if (weatherChanged) (void)updateWeatherFromApi(true);
  if (rssChanged) (void)updateRssFromFeed(true);
  if (wikiLangChanged) (void)updateWikiFromFeed(true);
  if (viewsChanged && !uiPageEnabledNoEnsure(g_uiPageMode)) {
    setUiPage(uiLastEnabledMainViewNoEnsure());
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
  if (g_webConfigServer.hasArg("status")) msg = g_webConfigServer.arg("status");
  const String html = buildWebConfigPage(statusMessageFromCode(msg));
  g_webConfigServer.send(200, "text/html; charset=utf-8", html);
}

static void sendWebCaptiveRedirect() {
  char setupUrl[96] = "";
  wifiBuildSetupPortalUrl(setupUrl, sizeof(setupUrl));
  g_webConfigServer.sendHeader("Cache-Control", "no-store", true);
  g_webConfigServer.sendHeader("Location", setupUrl, true);
  g_webConfigServer.send(302, "text/plain", "");
}

static void handleWebCaptivePortalProbe() {
  sendWebCaptiveRedirect();
}

#if WEB_CONFIG_ENABLED
static void webConfigStartCaptiveDnsIfNeeded() {
  if (!g_wifiSetupApActive || g_webConfigDnsStarted) return;
  if (!g_webConfigDnsServer.start(53, "*", WiFi.softAPIP())) {
    Serial.println("[WEB][DNS][ERR] captive dns start failed");
    return;
  }
  g_webConfigDnsStarted = true;
  Serial.printf("[WEB][DNS] captive resolver active on %s\n", WiFi.softAPIP().toString().c_str());
}

static void webConfigStopCaptiveDns() {
  if (!g_webConfigDnsStarted) return;
  g_webConfigDnsServer.stop();
  g_webConfigDnsStarted = false;
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
      if (g_webConfigDnsStarted) g_webConfigDnsServer.processNextRequest();
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
                g_wifiSetupApActive ? 1 : 0,
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
  g_webConfigServer.send(200, "application/json", out);
}

static void handleWebWifiSetupQrSvgApi() {
  char setupUrl[96];
  wifiBuildSetupPortalUrl(setupUrl, sizeof(setupUrl));

#if DB_HAS_QRCODEGEN
  if (!ensureWebQrBuffers()) {
    g_webConfigServer.send(500, "text/plain", "QR buffers unavailable");
    return;
  }
  const bool ok = qrcodegen_encodeText(
      setupUrl,
      g_webQrTempBuf,
      g_webQrDataBuf,
      qrcodegen_Ecc_MEDIUM,
      1,
      8,
      qrcodegen_Mask_AUTO,
      true);
  if (!ok) {
    g_webConfigServer.send(500, "text/plain", "QR encode failed");
    return;
  }

  const int qrSize = qrcodegen_getSize(g_webQrDataBuf);
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
      if (!qrcodegen_getModule(g_webQrDataBuf, x, y)) continue;
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
  g_webConfigServer.send(200, "image/svg+xml", svg);
#else
  String fallback;
  fallback.reserve(256);
  fallback += F("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 360 80'><rect width='100%' height='100%' fill='#111827'/>");
  fallback += F("<text x='12' y='34' font-family='monospace' font-size='14' fill='#f9fafb'>QR encoder not available</text>");
  fallback += F("<text x='12' y='56' font-family='monospace' font-size='12' fill='#93c5fd'>");
  appendHtmlEscaped(fallback, setupUrl);
  fallback += F("</text></svg>");
  g_webConfigServer.send(200, "image/svg+xml", fallback);
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
  if (g_webConfigServer.hasArg("weather_city")) return true;
  if (g_webConfigServer.hasArg("weather_lat")) return true;
  if (g_webConfigServer.hasArg("weather_lon")) return true;
  if (g_webConfigServer.hasArg("wc_lang")) return true;
  if (g_webConfigServer.hasArg("wiki_lang")) return true;
  if (g_webConfigServer.hasArg("wifi_pref_ssid")) return true;
  if (g_webConfigServer.hasArg("wifi_setup_mode")) return true;
  if (g_webConfigServer.hasArg("wifi_new_ssid")) return true;
  if (g_webConfigServer.hasArg("wifi_new_password")) return true;
  if (g_webConfigServer.hasArg("ui_theme")) return true;
  if (g_webConfigServer.hasArg("view_info")) return true;
  if (g_webConfigServer.hasArg("view_aux")) return true;
  if (g_webConfigServer.hasArg("view_wiki")) return true;
  if (g_webConfigServer.hasArg("view_doom")) return true;
  if (g_webConfigServer.hasArg("rss_feed_url")) return true;
  if (g_webConfigServer.hasArg("logo_url")) return true;
  for (uint8_t i = 1; i <= RSS_FEED_SLOT_COUNT; ++i) {
    const String keyUrl = String("rss_feed_url_") + String(i);
    if (g_webConfigServer.hasArg(keyUrl)) return true;
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
  const bool wOk = updateWeatherFromApi(true);
  const bool rOk = updateRssFromFeed(true);
  const bool kOk = updateWikiFromFeed(true);
#if TEST_NTP
  g_uiNeedsRedraw = true;
#endif
  Serial.printf("[WEB] reload weather=%d rss=%d wiki=%d\n", wOk ? 1 : 0, rOk ? 1 : 0, kOk ? 1 : 0);
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
  const bool wOk = updateWeatherFromApi(true);
  const bool rOk = updateRssFromFeed(true);
  const bool kOk = updateWikiFromFeed(true);
  String msg = "weather=";
  msg += (wOk ? "ok" : "fail");
  msg += ",rss=";
  msg += (rOk ? "ok" : "fail");
  msg += ",wiki=";
  msg += (kOk ? "ok" : "fail");
  sendWebConfigJson((wOk || rOk || kOk) ? 200 : 503, (wOk || rOk || kOk), msg.c_str());
}

static void ensureWebConfigServerStarted() {
  ensureRuntimeNetConfig();
  const bool staUp = (WiFi.status() == WL_CONNECTED) && g_wifiConnected;
  if (!staUp && !g_wifiSetupApActive) {
    if (g_webConfigDnsStarted) webConfigStopCaptiveDns();
    return;
  }
  if (g_webConfigServerStarted) {
    if (g_wifiSetupApActive) webConfigStartCaptiveDnsIfNeeded();
    else if (g_webConfigDnsStarted) webConfigStopCaptiveDns();
    return;
  }

  if (!g_webConfigRoutesRegistered) {
    g_webConfigServer.on("/", HTTP_GET, handleWebConfigRoot);
    g_webConfigServer.on("/generate_204", HTTP_GET, handleWebCaptivePortalProbe);
    g_webConfigServer.on("/gen_204", HTTP_GET, handleWebCaptivePortalProbe);
    g_webConfigServer.on("/hotspot-detect.html", HTTP_GET, handleWebCaptivePortalProbe);
    g_webConfigServer.on("/connecttest.txt", HTTP_GET, handleWebCaptivePortalProbe);
    g_webConfigServer.on("/ncsi.txt", HTTP_GET, handleWebCaptivePortalProbe);
    g_webConfigServer.on("/fwlink", HTTP_GET, handleWebCaptivePortalProbe);
    g_webConfigServer.on("/success.txt", HTTP_GET, handleWebCaptivePortalProbe);
    g_webConfigServer.on("/config", HTTP_POST, handleWebConfigApplyForm);
    g_webConfigServer.on("/reload", HTTP_POST, handleWebReloadForm);
    g_webConfigServer.on("/api/config", HTTP_GET, handleWebConfigGet);
    g_webConfigServer.on("/api/config", HTTP_POST, handleWebConfigApplyApi);
    g_webConfigServer.on("/api/wifi/scan", HTTP_GET, handleWebWifiScanApi);
    g_webConfigServer.on("/api/wifi/setup-qr.svg", HTTP_GET, handleWebWifiSetupQrSvgApi);
    g_webConfigServer.on("/api/reload", HTTP_POST, handleWebReloadApi);
    g_webConfigServer.onNotFound([]() {
      if (g_wifiSetupApActive) {
        sendWebCaptiveRedirect();
        return;
      }
      g_webConfigServer.send(404, "text/plain", "Not found");
    });
    g_webConfigRoutesRegistered = true;
  }

  g_webConfigServer.begin();
  g_webConfigServerStarted = true;
  if (staUp) {
    Serial.printf("[WEB] config ui ready (STA): http://%s:%u\n",
                  WiFi.localIP().toString().c_str(),
                  (unsigned)WEB_CONFIG_PORT);
  }
  if (g_wifiSetupApActive) {
    webConfigStartCaptiveDnsIfNeeded();
    Serial.printf("[WEB] config ui ready (AP): ssid='%s' url=http://%s:%u\n",
                  g_wifiSetupApSsid,
                  WiFi.softAPIP().toString().c_str(),
                  (unsigned)WEB_CONFIG_PORT);
  }
}

static void handleWebConfigServerLoop() {
  ensureWebConfigServerStarted();
  if (g_webConfigDnsStarted) g_webConfigDnsServer.processNextRequest();
  if (g_webConfigServerStarted) g_webConfigServer.handleClient();
}
#else
static void ensureWebConfigServerStarted() {
  ensureRuntimeNetConfig();
}

static void handleWebConfigServerLoop() {}
#endif

static bool extractJsonNumberField(const String &json, const char *key, float &out) {
  int pos = -1;
  while (true) {
    pos = json.indexOf(key, pos + 1);
    if (pos < 0) return false;
    int i = pos + (int)strlen(key);
    while (i < (int)json.length() && (json[i] == ' ' || json[i] == '\t' || json[i] == ':' || json[i] == '\n' || json[i] == '\r')) ++i;
    if (i >= (int)json.length()) return false;
    if (json[i] == '"') continue;  // skip units string occurrence, keep searching numeric one

    int j = i;
    while (j < (int)json.length()) {
      const char c = json[j];
      if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.') {
        ++j;
      } else {
        break;
      }
    }
    if (j <= i) continue;
    out = json.substring(i, j).toFloat();
    return true;
  }
}

static bool extractJsonArrayNumberAt(const String &json, const char *key, int index, float &out) {
  if (index < 0) return false;
  const int keyPos = json.indexOf(key);
  if (keyPos < 0) return false;
  int pos = json.indexOf('[', keyPos);
  if (pos < 0) return false;
  ++pos;

  for (int i = 0; i <= index; ++i) {
    while (pos < (int)json.length() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t')) ++pos;
    if (pos >= (int)json.length() || json[pos] == ']') return false;
    int end = pos;
    while (end < (int)json.length() && json[end] != ',' && json[end] != ']') ++end;
    if (i == index) {
      String token = json.substring(pos, end);
      token.trim();
      out = token.toFloat();
      return true;
    }
    pos = end;
    if (pos < (int)json.length() && json[pos] == ',') ++pos;
  }
  return false;
}

static bool extractJsonArrayStringAt(const String &json, const char *key, int index, String &out) {
  if (index < 0) return false;
  const int keyPos = json.indexOf(key);
  if (keyPos < 0) return false;
  int pos = json.indexOf('[', keyPos);
  if (pos < 0) return false;
  ++pos;

  for (int i = 0; i <= index; ++i) {
    while (pos < (int)json.length() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t')) ++pos;
    if (pos >= (int)json.length() || json[pos] == ']') return false;
    if (json[pos] != '"') return false;
    ++pos;
    int end = json.indexOf('"', pos);
    if (end < 0) return false;
    if (i == index) {
      out = json.substring(pos, end);
      return true;
    }
    pos = end + 1;
    while (pos < (int)json.length() && json[pos] != ',' && json[pos] != ']') ++pos;
    if (pos < (int)json.length() && json[pos] == ',') ++pos;
  }
  return false;
}

static void isoToHhMm(const String &iso, char out[6]) {
  if (iso.length() >= 16) {
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

static const char* weatherCodeShortIt(int code) {
  if (code == 0 || code == 1) return "Sereno";
  if (code == 2) return "Nuvoloso";
  if (code == 3) return "Coperto";
  if (code == 45 || code == 48) return "Nebbia";
  if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return "Pioggia";
  if (code >= 71 && code <= 77) return "Neve";
  if (code >= 95) return "Temporale";
  return "N/D";
}

static const char* weatherCodeUiLabelIt(int code) {
  if (code == 0) return "Sereno";
  if (code == 1) return "Sole prevalente";
  if (code == 2) return "Parz. nuvoloso";
  if (code == 3) return "Coperto";
  if (code == 45) return "Nebbia";
  if (code == 48) return "Nebbia gelata";
  if (code == 51) return "Pioviggine";
  if (code == 53) return "Pioviggia mod.";
  if (code == 55) return "Pioviggia forte";
  if (code == 56 || code == 57) return "Pioviggia gel.";
  if (code == 61) return "Pioggia debole";
  if (code == 63) return "Pioggia mod.";
  if (code == 65) return "Pioggia forte";
  if (code == 66 || code == 67) return "Pioggia gelata";
  if (code == 71) return "Neve debole";
  if (code == 73) return "Neve moderata";
  if (code == 75) return "Neve forte";
  if (code == 77) return "Granuli neve";
  if (code == 80) return "Rovesci deboli";
  if (code == 81) return "Rovesci mod.";
  if (code == 82) return "Rovesci forti";
  if (code == 85 || code == 86) return "Rovesci neve";
  if (code == 95) return "Temporale";
  if (code == 96 || code == 99) return "Temp. grandine";
  return "N/D";
}

// ---------------------------------------------------------------------------
// English weather labels
// ---------------------------------------------------------------------------
static const char* weatherCodeShortEn(int code) {
  if (code == 0 || code == 1) return "Clear";
  if (code == 2) return "Cloudy";
  if (code == 3) return "Overcast";
  if (code == 45 || code == 48) return "Fog";
  if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return "Rain";
  if (code >= 71 && code <= 77) return "Snow";
  if (code >= 95) return "Storm";
  return "N/A";
}

static const char* weatherCodeUiLabelEn(int code) {
  if (code == 0) return "Clear";
  if (code == 1) return "Mainly clear";
  if (code == 2) return "Partly cloudy";
  if (code == 3) return "Overcast";
  if (code == 45) return "Fog";
  if (code == 48) return "Icy fog";
  if (code == 51) return "Light drizzle";
  if (code == 53) return "Mod. drizzle";
  if (code == 55) return "Heavy drizzle";
  if (code == 56 || code == 57) return "Freezing drizzle";
  if (code == 61) return "Light rain";
  if (code == 63) return "Moderate rain";
  if (code == 65) return "Heavy rain";
  if (code == 66 || code == 67) return "Freezing rain";
  if (code == 71) return "Light snow";
  if (code == 73) return "Moderate snow";
  if (code == 75) return "Heavy snow";
  if (code == 77) return "Snow grains";
  if (code == 80) return "Light showers";
  if (code == 81) return "Mod. showers";
  if (code == 82) return "Heavy showers";
  if (code == 85 || code == 86) return "Snow showers";
  if (code == 95) return "Thunderstorm";
  if (code == 96 || code == 99) return "Storm w/ hail";
  return "N/A";
}

// ---------------------------------------------------------------------------
// French weather labels
// ---------------------------------------------------------------------------
static const char* weatherCodeShortFr(int code) {
  if (code == 0 || code == 1) return "Clair";
  if (code == 2) return "Nuageux";
  if (code == 3) return "Couvert";
  if (code == 45 || code == 48) return "Brouillard";
  if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return "Pluie";
  if (code >= 71 && code <= 77) return "Neige";
  if (code >= 95) return "Orage";
  return "N/D";
}

static const char* weatherCodeUiLabelFr(int code) {
  if (code == 0) return "Clair";
  if (code == 1) return "Principalement clair";
  if (code == 2) return "Part. nuageux";
  if (code == 3) return "Couvert";
  if (code == 45) return "Brouillard";
  if (code == 48) return "Brouillard glac.";
  if (code == 51) return "Bruine legere";
  if (code == 53) return "Bruine mod.";
  if (code == 55) return "Bruine forte";
  if (code == 56 || code == 57) return "Bruine verglac.";
  if (code == 61) return "Pluie faible";
  if (code == 63) return "Pluie mod.";
  if (code == 65) return "Pluie forte";
  if (code == 66 || code == 67) return "Pluie verglac.";
  if (code == 71) return "Neige faible";
  if (code == 73) return "Neige mod.";
  if (code == 75) return "Neige forte";
  if (code == 77) return "Grains de neige";
  if (code == 80) return "Averses faibles";
  if (code == 81) return "Averses mod.";
  if (code == 82) return "Averses fortes";
  if (code == 85 || code == 86) return "Averses de neige";
  if (code == 95) return "Orage";
  if (code == 96 || code == 99) return "Orage avec grele";
  return "N/D";
}

// ---------------------------------------------------------------------------
// German weather labels
// ---------------------------------------------------------------------------
static const char* weatherCodeShortDe(int code) {
  if (code == 0 || code == 1) return "Klar";
  if (code == 2) return "Bewoelkt";
  if (code == 3) return "Bedeckt";
  if (code == 45 || code == 48) return "Nebel";
  if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return "Regen";
  if (code >= 71 && code <= 77) return "Schnee";
  if (code >= 95) return "Gewitter";
  return "N/V";
}

static const char* weatherCodeUiLabelDe(int code) {
  if (code == 0) return "Klar";
  if (code == 1) return "Ueberwiegend klar";
  if (code == 2) return "Teils bewoelkt";
  if (code == 3) return "Bedeckt";
  if (code == 45) return "Nebel";
  if (code == 48) return "Eisnebel";
  if (code == 51) return "Leichter Nieseln";
  if (code == 53) return "Maess. Nieseln";
  if (code == 55) return "Starkes Nieseln";
  if (code == 56 || code == 57) return "Gefrierender Niesel";
  if (code == 61) return "Leichter Regen";
  if (code == 63) return "Maess. Regen";
  if (code == 65) return "Starker Regen";
  if (code == 66 || code == 67) return "Gefrierender Regen";
  if (code == 71) return "Leichter Schnee";
  if (code == 73) return "Maess. Schnee";
  if (code == 75) return "Starker Schnee";
  if (code == 77) return "Schneekristalle";
  if (code == 80) return "Leichte Schauer";
  if (code == 81) return "Maess. Schauer";
  if (code == 82) return "Starke Schauer";
  if (code == 85 || code == 86) return "Schneeschauer";
  if (code == 95) return "Gewitter";
  if (code == 96 || code == 99) return "Gewitter m. Hagel";
  return "N/V";
}

// ---------------------------------------------------------------------------
// Spanish weather labels
// ---------------------------------------------------------------------------
static const char* weatherCodeShortEs(int code) {
  if (code == 0 || code == 1) return "Despejado";
  if (code == 2) return "Nublado";
  if (code == 3) return "Cubierto";
  if (code == 45 || code == 48) return "Niebla";
  if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return "Lluvia";
  if (code >= 71 && code <= 77) return "Nieve";
  if (code >= 95) return "Tormenta";
  return "N/D";
}

static const char* weatherCodeUiLabelEs(int code) {
  if (code == 0) return "Despejado";
  if (code == 1) return "Mainly despejado";
  if (code == 2) return "Parc. nublado";
  if (code == 3) return "Cubierto";
  if (code == 45) return "Niebla";
  if (code == 48) return "Niebla helada";
  if (code == 51) return "Llovizna leve";
  if (code == 53) return "Llovizna mod.";
  if (code == 55) return "Llovizna fuerte";
  if (code == 56 || code == 57) return "Llovizna helada";
  if (code == 61) return "Lluvia leve";
  if (code == 63) return "Lluvia mod.";
  if (code == 65) return "Lluvia fuerte";
  if (code == 66 || code == 67) return "Lluvia helada";
  if (code == 71) return "Nieve leve";
  if (code == 73) return "Nieve mod.";
  if (code == 75) return "Nieve fuerte";
  if (code == 77) return "Granulos nieve";
  if (code == 80) return "Chubascos leves";
  if (code == 81) return "Chubascos mod.";
  if (code == 82) return "Chubascos fuertes";
  if (code == 85 || code == 86) return "Chubascos nieve";
  if (code == 95) return "Tormenta";
  if (code == 96 || code == 99) return "Torm. con granizo";
  return "N/D";
}

// ---------------------------------------------------------------------------
// Portuguese weather labels
// ---------------------------------------------------------------------------
static const char* weatherCodeShortPt(int code) {
  if (code == 0 || code == 1) return "Limpo";
  if (code == 2) return "Nublado";
  if (code == 3) return "Encoberto";
  if (code == 45 || code == 48) return "Nevoeiro";
  if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return "Chuva";
  if (code >= 71 && code <= 77) return "Neve";
  if (code >= 95) return "Temporal";
  return "N/D";
}

static const char* weatherCodeUiLabelPt(int code) {
  if (code == 0) return "Limpo";
  if (code == 1) return "Principalmente limpo";
  if (code == 2) return "Parc. nublado";
  if (code == 3) return "Encoberto";
  if (code == 45) return "Nevoeiro";
  if (code == 48) return "Nevoeiro gelado";
  if (code == 51) return "Chuvisco fraco";
  if (code == 53) return "Chuvisco mod.";
  if (code == 55) return "Chuvisco forte";
  if (code == 56 || code == 57) return "Chuvisco gelado";
  if (code == 61) return "Chuva fraca";
  if (code == 63) return "Chuva mod.";
  if (code == 65) return "Chuva forte";
  if (code == 66 || code == 67) return "Chuva gelada";
  if (code == 71) return "Neve fraca";
  if (code == 73) return "Neve mod.";
  if (code == 75) return "Neve forte";
  if (code == 77) return "Graos de neve";
  if (code == 80) return "Aguaceiros fracos";
  if (code == 81) return "Aguaceiros mod.";
  if (code == 82) return "Aguaceiros fortes";
  if (code == 85 || code == 86) return "Aguaceiros neve";
  if (code == 95) return "Temporal";
  if (code == 96 || code == 99) return "Temp. com granizo";
  return "N/D";
}

// ---------------------------------------------------------------------------
// Latin weather labels
// ---------------------------------------------------------------------------
static const char* weatherCodeShortLa(int code) {
  if (code == 0 || code == 1) return "Serenum";
  if (code == 2) return "Nubilum";
  if (code == 3) return "Opertum";
  if (code == 45 || code == 48) return "Nebula";
  if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return "Imber";
  if (code >= 71 && code <= 77) return "Nix";
  if (code >= 95) return "Procella";
  return "N/D";
}

static const char* weatherCodeUiLabelLa(int code) {
  if (code == 0) return "Serenum";
  if (code == 1) return "Fere serenum";
  if (code == 2) return "Part. nubilum";
  if (code == 3) return "Opertum";
  if (code == 45) return "Nebula";
  if (code == 48) return "Nebula glacialis";
  if (code == 51) return "Pluvia levis";
  if (code == 53) return "Pluvia mod.";
  if (code == 55) return "Pluvia magna";
  if (code == 56 || code == 57) return "Pluvia glacialis";
  if (code == 61) return "Imber levis";
  if (code == 63) return "Imber mod.";
  if (code == 65) return "Imber magnus";
  if (code == 66 || code == 67) return "Imber glacialis";
  if (code == 71) return "Nix levis";
  if (code == 73) return "Nix mod.";
  if (code == 75) return "Nix magna";
  if (code == 77) return "Grana nivis";
  if (code == 80) return "Imbres leves";
  if (code == 81) return "Imbres mod.";
  if (code == 82) return "Imbres magni";
  if (code == 85 || code == 86) return "Imbres nivis";
  if (code == 95) return "Procella";
  if (code == 96 || code == 99) return "Proc. cum grandine";
  return "N/D";
}

// ---------------------------------------------------------------------------
// Esperanto weather labels
// ---------------------------------------------------------------------------
static const char* weatherCodeShortEo(int code) {
  if (code == 0 || code == 1) return "Klara";
  if (code == 2) return "Nuba";
  if (code == 3) return "Kovrita";
  if (code == 45 || code == 48) return "Nebulo";
  if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return "Pluvo";
  if (code >= 71 && code <= 77) return "Nego";
  if (code >= 95) return "Fulmotondro";
  return "N/D";
}

static const char* weatherCodeUiLabelEo(int code) {
  if (code == 0) return "Klara";
  if (code == 1) return "Cefe klara";
  if (code == 2) return "Part. nuba";
  if (code == 3) return "Kovrita";
  if (code == 45) return "Nebulo";
  if (code == 48) return "Glacia nebulo";
  if (code == 51) return "Malpeza drizzle";
  if (code == 53) return "Mod. drizzle";
  if (code == 55) return "Peza drizzle";
  if (code == 56 || code == 57) return "Glacia drizzle";
  if (code == 61) return "Malpeza pluvo";
  if (code == 63) return "Mod. pluvo";
  if (code == 65) return "Peza pluvo";
  if (code == 66 || code == 67) return "Glacia pluvo";
  if (code == 71) return "Malpeza nego";
  if (code == 73) return "Mod. nego";
  if (code == 75) return "Peza nego";
  if (code == 77) return "Negaj grenoj";
  if (code == 80) return "Malpezaj soversoj";
  if (code == 81) return "Mod. soversoj";
  if (code == 82) return "Pezaj soversoj";
  if (code == 85 || code == 86) return "Negaj soversoj";
  if (code == 95) return "Fulmotondro";
  if (code == 96 || code == 99) return "Fulmont. kun hajlo";
  return "N/D";
}

// ---------------------------------------------------------------------------
// Klingon weather labels
// ---------------------------------------------------------------------------
static const char* weatherCodeShortTlh(int code) {
  if (code == 0 || code == 1) return "muD QaQ";
  if (code == 2 || code == 3) return "muD Hurgh";
  if (code == 45 || code == 48) return "muD Duj";
  if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return "SIS";
  if (code >= 71 && code <= 77) return "chuch";
  if (code >= 95) return "muD QeH";
  return "Duj";
}

static const char* weatherCodeUiLabelTlh(int code) {
  if (code == 0) return "muD QaQ";
  if (code == 1) return "muD QaQ law'";
  if (code == 2) return "muD Hurgh";
  if (code == 3) return "muD Hurgh HoS";
  if (code == 45) return "muD Duj";
  if (code == 48) return "muD chuch Duj";
  if (code == 51) return "SIS mach";
  if (code == 53) return "SIS mod.";
  if (code == 55) return "SIS HoS";
  if (code == 56 || code == 57) return "SIS chuch";
  if (code == 61) return "bIQ mach";
  if (code == 63) return "bIQ mod.";
  if (code == 65) return "bIQ HoS";
  if (code == 66 || code == 67) return "bIQ chuch";
  if (code == 71) return "chuch mach";
  if (code == 73) return "chuch mod.";
  if (code == 75) return "chuch HoS";
  if (code == 77) return "chuch Hap";
  if (code == 80) return "SIS mach bIQ";
  if (code == 81) return "SIS mod. bIQ";
  if (code == 82) return "SIS HoS bIQ";
  if (code == 85 || code == 86) return "SIS chuch bIQ";
  if (code == 95) return "muD QeH";
  if (code == 96 || code == 99) return "muD QeH begh";
  return "Duj";
}

// ---------------------------------------------------------------------------
// 1337 Speak weather labels
// ---------------------------------------------------------------------------
static const char* weatherCodeShortL33t(int code) {
  if (code == 0 || code == 1) return "CL34R";
  if (code == 2) return "CL0UDY";
  if (code == 3) return "0VCST";
  if (code == 45 || code == 48) return "F09";
  if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return "R41N";
  if (code >= 71 && code <= 77) return "5N0W";
  if (code >= 95) return "570RM";
  return "N/4";
}

static const char* weatherCodeUiLabelL33t(int code) {
  if (code == 0) return "CL34R 5KY";
  if (code == 1) return "M41NLY CL34R";
  if (code == 2) return "P4R7LY CL0UDY";
  if (code == 3) return "0V3RC457";
  if (code == 45) return "F09";
  if (code == 48) return "1CY F09";
  if (code == 51) return "L1GH7 DR1ZZL3";
  if (code == 53) return "M0D DR1ZZL3";
  if (code == 55) return "H34VY DR1ZZL3";
  if (code == 56 || code == 57) return "FR33Z1N9 DR1ZZ";
  if (code == 61) return "L1GH7 R41N";
  if (code == 63) return "M0D R41N";
  if (code == 65) return "H34VY R41N";
  if (code == 66 || code == 67) return "FR33Z1N9 R41N";
  if (code == 71) return "L1GH7 5N0W";
  if (code == 73) return "M0D 5N0W";
  if (code == 75) return "H34VY 5N0W";
  if (code == 77) return "5N0W 9R41N5";
  if (code == 80) return "L1GH7 5H0W3R";
  if (code == 81) return "M0D 5H0W3R";
  if (code == 82) return "H34VY 5H0W3R";
  if (code == 85 || code == 86) return "5N0W 5H0W3R";
  if (code == 95) return "7HuND3R570RM";
  if (code == 96 || code == 99) return "570RM+H41L";
  return "N/4";
}

// ---------------------------------------------------------------------------
// Shakespearean English weather labels
// ---------------------------------------------------------------------------
static const char* weatherCodeShortSha(int code) {
  if (code == 0 || code == 1) return "Faire";
  if (code == 2) return "Cloudie";
  if (code == 3) return "Overcast";
  if (code == 45 || code == 48) return "Mist";
  if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return "Raineth";
  if (code >= 71 && code <= 77) return "Snoweth";
  if (code >= 95) return "Tempest";
  return "N/A";
}

static const char* weatherCodeUiLabelSha(int code) {
  if (code == 0) return "Faire skies";
  if (code == 1) return "Mainly faire";
  if (code == 2) return "Partly cloudie";
  if (code == 3) return "Overcast";
  if (code == 45) return "Mist";
  if (code == 48) return "Icy mist";
  if (code == 51) return "Light drizzle";
  if (code == 53) return "Mod. drizzle";
  if (code == 55) return "Heavy drizzle";
  if (code == 56 || code == 57) return "Freezing driz.";
  if (code == 61) return "Light rain";
  if (code == 63) return "Moderate rain";
  if (code == 65) return "Heavy rain";
  if (code == 66 || code == 67) return "Freezing rain";
  if (code == 71) return "Light snoweth";
  if (code == 73) return "Mod. snoweth";
  if (code == 75) return "Heavy snoweth";
  if (code == 77) return "Snow grains";
  if (code == 80) return "Light showers";
  if (code == 81) return "Mod. showers";
  if (code == 82) return "Heavy showers";
  if (code == 85 || code == 86) return "Snow showers";
  if (code == 95) return "Thunderstorm";
  if (code == 96 || code == 99) return "Storm+hail";
  return "N/A";
}

// ---------------------------------------------------------------------------
// Valley Girl weather labels
// ---------------------------------------------------------------------------
static const char* weatherCodeShortVal(int code) {
  if (code == 0 || code == 1) return "Sunny!";
  if (code == 2) return "Cloudy";
  if (code == 3) return "Ugh Gray";
  if (code == 45 || code == 48) return "Like Fog";
  if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return "Ugh Rain";
  if (code >= 71 && code <= 77) return "OMG Snow";
  if (code >= 95) return "Storm!";
  return "N/A";
}

static const char* weatherCodeUiLabelVal(int code) {
  if (code == 0) return "Totally Sunny";
  if (code == 1) return "Like Sunny";
  if (code == 2) return "Kinda Cloudy";
  if (code == 3) return "So Overcast";
  if (code == 45) return "Like Foggy";
  if (code == 48) return "Icy Fog Ew";
  if (code == 51) return "Light Drizzle";
  if (code == 53) return "Some Drizzle";
  if (code == 55) return "Heavy Drizzle";
  if (code == 56 || code == 57) return "Freezing Rain";
  if (code == 61) return "Light Rain";
  if (code == 63) return "Moderate Rain";
  if (code == 65) return "Heavy Rain";
  if (code == 66 || code == 67) return "Freezing Rain";
  if (code == 71) return "Light Snow";
  if (code == 73) return "Like Snow";
  if (code == 75) return "Heavy Snow!";
  if (code == 77) return "Snow Grains";
  if (code == 80) return "Light Shower";
  if (code == 81) return "Mod. Shower";
  if (code == 82) return "Heavy Shower";
  if (code == 85 || code == 86) return "Snow Shower";
  if (code == 95) return "Thunderstorm";
  if (code == 96 || code == 99) return "Storm+Hail";
  return "N/A";
}

// ---------------------------------------------------------------------------
// Italian Bellazio weather labels
// ---------------------------------------------------------------------------
static const char* weatherCodeShortBellazio(int code) {
  if (code == 0 || code == 1) return "Sereno";
  if (code == 2) return "Nuvoloso";
  if (code == 3) return "Coperto";
  if (code == 45 || code == 48) return "Nebbia";
  if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return "Pioggia";
  if (code >= 71 && code <= 77) return "Neve";
  if (code >= 95) return "Temporale";
  return "N/D";
}

static const char* weatherCodeUiLabelBellazio(int code) {
  if (code == 0) return "Sereno pieno";
  if (code == 1) return "Sole, tipo";
  if (code == 2) return "Un po' nuv.";
  if (code == 3) return "Tutto coperto";
  if (code == 45) return "Nebbia ugh";
  if (code == 48) return "Nebbia gelata";
  if (code == 51) return "Pioggerella";
  if (code == 53) return "Piovigg. mid";
  if (code == 55) return "Pioggia forte";
  if (code == 56 || code == 57) return "Pioggia ghiac.";
  if (code == 61) return "Pioggia lieve";
  if (code == 63) return "Pioggia boh";
  if (code == 65) return "Pioggia forte";
  if (code == 66 || code == 67) return "Pioggia gel.";
  if (code == 71) return "Neve lowkey";
  if (code == 73) return "Neve mod.";
  if (code == 75) return "Neve fr fr";
  if (code == 77) return "Granuli neve";
  if (code == 80) return "Rovesci lievi";
  if (code == 81) return "Rovesci mid";
  if (code == 82) return "Rovesci forti";
  if (code == 85 || code == 86) return "Rovesci neve";
  if (code == 95) return "Temporale!";
  if (code == 96 || code == 99) return "Temp.+grandine";
  return "N/D";
}

// ---------------------------------------------------------------------------
// Language dispatchers
// ---------------------------------------------------------------------------
static const char* weatherCodeUiLabel(int code) {
  if (strcmp(g_wordClockLang, "en")   == 0) return weatherCodeUiLabelEn  (code);
  if (strcmp(g_wordClockLang, "fr")   == 0) return weatherCodeUiLabelFr  (code);
  if (strcmp(g_wordClockLang, "de")   == 0) return weatherCodeUiLabelDe  (code);
  if (strcmp(g_wordClockLang, "es")   == 0) return weatherCodeUiLabelEs  (code);
  if (strcmp(g_wordClockLang, "pt")   == 0) return weatherCodeUiLabelPt  (code);
  if (strcmp(g_wordClockLang, "la")   == 0) return weatherCodeUiLabelLa  (code);
  if (strcmp(g_wordClockLang, "eo")   == 0) return weatherCodeUiLabelEo  (code);

  if (strcmp(g_wordClockLang, "tlh")  == 0) return weatherCodeUiLabelTlh (code);
  if (strcmp(g_wordClockLang, "l33t") == 0) return weatherCodeUiLabelL33t(code);
  if (strcmp(g_wordClockLang, "sha")  == 0) return weatherCodeUiLabelSha (code);
  if (strcmp(g_wordClockLang, "val")  == 0) return weatherCodeUiLabelVal (code);
  if (strcmp(g_wordClockLang, "bellazio") == 0) return weatherCodeUiLabelBellazio(code);
  return weatherCodeUiLabelIt(code);
}

static const char* weatherCodeShort(int code) {
  if (strcmp(g_wordClockLang, "en")   == 0) return weatherCodeShortEn  (code);
  if (strcmp(g_wordClockLang, "fr")   == 0) return weatherCodeShortFr  (code);
  if (strcmp(g_wordClockLang, "de")   == 0) return weatherCodeShortDe  (code);
  if (strcmp(g_wordClockLang, "es")   == 0) return weatherCodeShortEs  (code);
  if (strcmp(g_wordClockLang, "pt")   == 0) return weatherCodeShortPt  (code);
  if (strcmp(g_wordClockLang, "la")   == 0) return weatherCodeShortLa  (code);
  if (strcmp(g_wordClockLang, "eo")   == 0) return weatherCodeShortEo  (code);

  if (strcmp(g_wordClockLang, "tlh")  == 0) return weatherCodeShortTlh (code);
  if (strcmp(g_wordClockLang, "l33t") == 0) return weatherCodeShortL33t(code);
  if (strcmp(g_wordClockLang, "sha")  == 0) return weatherCodeShortSha (code);
  if (strcmp(g_wordClockLang, "val")  == 0) return weatherCodeShortVal (code);
  if (strcmp(g_wordClockLang, "bellazio") == 0) return weatherCodeShortBellazio(code);
  return weatherCodeShortIt(code);
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

static bool parseShortenerResponse(String resp, String &shortUrl) {
  if (!extractJsonStringFieldLoose(resp, "\"short_url\"", shortUrl) &&
      !extractJsonStringFieldLoose(resp, "\"shortUrl\"", shortUrl) &&
      !extractJsonStringFieldLoose(resp, "\"shortened_url\"", shortUrl) &&
      !extractJsonStringFieldLoose(resp, "\"url\"", shortUrl)) {
    resp.trim();
    if (resp.startsWith("http")) shortUrl = resp;
  }
  shortUrl.trim();
  return shortUrl.startsWith("http");
}

static bool urlStartsWith(const char *url, const char *prefix) {
  if (!url || !prefix) return false;
  const size_t n = strlen(prefix);
  return strncmp(url, prefix, n) == 0;
}

static void logShortenerHttpError(const char *endpoint, int httpCode) {
  String err = HTTPClient::errorToString(httpCode);
  err.trim();
  Serial.printf("[RSS][HTTP] endpoint=%s code=%d err=%s\n",
                endpoint ? endpoint : "-",
                httpCode,
                err.length() ? err.c_str() : "-");
}

static bool rssShortCacheLookup(const char *longUrl, char *outShort, size_t outLen) {
  if (!longUrl || !longUrl[0] || !outShort || outLen == 0) return false;
  outShort[0] = '\0';
  for (size_t i = 0; i < RSS_SHORTENER_CACHE_SIZE; ++i) {
    const RssShortCacheEntry &e = g_rssShortCache[i];
    if (!e.valid || !e.longUrl[0] || !e.shortUrl[0]) continue;
    if (strcmp(e.longUrl, longUrl) != 0) continue;
    strncpy(outShort, e.shortUrl, outLen - 1);
    outShort[outLen - 1] = '\0';
    return outShort[0] != '\0';
  }
  return false;
}

static void rssShortCacheStore(const char *longUrl, const char *shortUrl) {
  if (!longUrl || !longUrl[0] || !shortUrl || !shortUrl[0]) return;
  const uint32_t now = millis();
  int freeIdx = -1;
  int oldestIdx = 0;
  uint32_t oldestAge = 0;
  for (size_t i = 0; i < RSS_SHORTENER_CACHE_SIZE; ++i) {
    RssShortCacheEntry &e = g_rssShortCache[i];
    if (e.valid && strcmp(e.longUrl, longUrl) == 0) {
      strncpy(e.shortUrl, shortUrl, sizeof(e.shortUrl) - 1);
      e.shortUrl[sizeof(e.shortUrl) - 1] = '\0';
      e.updatedMs = now;
      return;
    }
    if (!e.valid && freeIdx < 0) freeIdx = (int)i;
    if (e.valid) {
      const uint32_t age = now - e.updatedMs;
      if ((int)i == 0 || age > oldestAge) {
        oldestAge = age;
        oldestIdx = (int)i;
      }
    }
  }
  const int slot = (freeIdx >= 0) ? freeIdx : oldestIdx;
  RssShortCacheEntry &dst = g_rssShortCache[slot];
  strncpy(dst.longUrl, longUrl, sizeof(dst.longUrl) - 1);
  dst.longUrl[sizeof(dst.longUrl) - 1] = '\0';
  strncpy(dst.shortUrl, shortUrl, sizeof(dst.shortUrl) - 1);
  dst.shortUrl[sizeof(dst.shortUrl) - 1] = '\0';
  dst.updatedMs = now;
  dst.valid = true;
}

static void rssShortCacheRetainCurrentFeedLinks(const RssItem *items, uint8_t count) {
  for (size_t i = 0; i < RSS_SHORTENER_CACHE_SIZE; ++i) {
    RssShortCacheEntry &e = g_rssShortCache[i];
    if (!e.valid || !e.longUrl[0]) continue;
    bool keep = false;
    for (uint8_t k = 0; k < count; ++k) {
      if (!items[k].link[0]) continue;
      if (strcmp(items[k].link, e.longUrl) == 0) {
        keep = true;
        break;
      }
    }
    if (!keep) {
      e.valid = false;
      e.longUrl[0] = '\0';
      e.shortUrl[0] = '\0';
      e.updatedMs = 0;
    }
  }
}

static bool rssShortenViaJsonApi(const char *endpoint,
                                 const char *longUrl,
                                 String &shortUrl,
                                 int &httpCode,
                                 bool allowAuthHeader) {
  if (!endpoint || !endpoint[0] || !longUrl || !longUrl[0]) return false;
  HTTPClient http;
  http.setConnectTimeout(RSS_SHORTENER_CONNECT_TIMEOUT_MS);
  http.setTimeout(RSS_SHORTENER_HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.useHTTP10(true);

  bool beginOk = false;
  const bool isHttps = urlStartsWith(endpoint, "https://");
  WiFiClientSecure tls;
  if (isHttps) {
    tls.setInsecure();
    tls.setTimeout((RSS_SHORTENER_HTTP_TIMEOUT_MS + 999U) / 1000U);
    beginOk = http.begin(tls, endpoint);
  } else {
    beginOk = http.begin(endpoint);
  }
  if (!beginOk) {
    httpCode = -1;
    Serial.printf("[RSS][HTTP] begin failed endpoint=%s\n", endpoint);
    return false;
  }

  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "ScryBar/esp32");
  http.addHeader("Connection", "close");
  if (allowAuthHeader && strlen(RSS_SHORTENER_TOKEN) > 0) {
    String auth = String("Bearer ") + RSS_SHORTENER_TOKEN;
    http.addHeader("Authorization", auth);
  }
  const String body = String("{\"long_url\":\"") + jsonEscape(String(longUrl)) + "\"}";
  httpCode = http.POST((uint8_t *)body.c_str(), body.length());
  String resp;
  if (httpCode > 0) {
    resp = http.getString();
  } else {
    logShortenerHttpError(endpoint, httpCode);
  }
  http.end();
  if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_CREATED) return false;
  const bool parsed = parseShortenerResponse(resp, shortUrl);
  if (!parsed) {
    String preview = resp;
    preview.replace("\n", " ");
    preview.replace("\r", " ");
    if (preview.length() > 160) preview = preview.substring(0, 160) + "...";
    Serial.printf("[RSS][HTTP] parse fail endpoint=%s body=%s\n", endpoint, preview.c_str());
  }
  return parsed;
}

static bool rssTryShortenUrlForState(RssState &state, uint8_t idx, const char *logPrefix) {
  if (idx >= state.itemCount) return false;
  RssItem &it = state.items[idx];
  if (!it.link[0]) return false;
  if (it.shortReady && it.shortLink[0]) return true;
  char cachedShort[sizeof(it.shortLink)] = {0};
  if (rssShortCacheLookup(it.link, cachedShort, sizeof(cachedShort))) {
    strncpy(it.shortLink, cachedShort, sizeof(it.shortLink) - 1);
    it.shortLink[sizeof(it.shortLink) - 1] = '\0';
    it.shortReady = true;
    it.shortTried = true;
    return true;
  }
  const uint32_t now = millis();
  if (it.shortTried && (now - state.lastShortenAttemptMs) < RSS_SHORTENER_RETRY_MS) return false;
  if (WiFi.status() != WL_CONNECTED || !g_wifiConnected) return false;

  int codeApi = 0;
  int codeAlt = 0;
  int codeApiHost = 0;
  int codeApiHostAlt = 0;
  String shortUrl;
  const char *usedEndpoint = nullptr;
  bool ok = rssShortenViaJsonApi(RSS_SHORTENER_ENDPOINT, it.link, shortUrl, codeApi, true);
  if (ok) usedEndpoint = RSS_SHORTENER_ENDPOINT;
  if (!ok) {
    ok = rssShortenViaJsonApi(RSS_SHORTENER_ENDPOINT_ALT, it.link, shortUrl, codeAlt, true);
    if (ok) usedEndpoint = RSS_SHORTENER_ENDPOINT_ALT;
  }
  if (!ok) {
    ok = rssShortenViaJsonApi("https://api.spoo.me/v1/shorten", it.link, shortUrl, codeApiHost, true);
    if (ok) usedEndpoint = "https://api.spoo.me/v1/shorten";
  }
  if (!ok) {
    ok = rssShortenViaJsonApi("https://api.spoo.me/shorten", it.link, shortUrl, codeApiHostAlt, true);
    if (ok) usedEndpoint = "https://api.spoo.me/shorten";
  }
#if RSS_SHORTENER_ALLOW_HTTP_FALLBACK
  int codeHttp = 0;
  int codeHttpAlt = 0;
  if (!ok) {
    // HTTP fallback keeps Spoo API path but avoids HTTPS handshake blockers on some APs/firmware.
    // For safety, do not send bearer token over plain HTTP.
    ok = rssShortenViaJsonApi(RSS_SHORTENER_ENDPOINT_HTTP, it.link, shortUrl, codeHttp, false);
    if (ok) usedEndpoint = RSS_SHORTENER_ENDPOINT_HTTP;
  }
  if (!ok) {
    ok = rssShortenViaJsonApi(RSS_SHORTENER_ENDPOINT_HTTP_ALT, it.link, shortUrl, codeHttpAlt, false);
    if (ok) usedEndpoint = RSS_SHORTENER_ENDPOINT_HTTP_ALT;
  }
#endif
  it.shortTried = true;
  state.lastShortenAttemptMs = now;
  if (!ok) {
    if (!g_shortenerDnsDiagDone && codeApi == -1 && codeAlt == -1 && codeApiHost == -1 && codeApiHostAlt == -1) {
      g_shortenerDnsDiagDone = true;
      IPAddress ipSpoo, ipApi, ipAnsa;
      bool okSpoo = WiFi.hostByName("spoo.me", ipSpoo);
      Serial.printf("[RSS][DNS] spoo.me ok=%d ip=%s\n", okSpoo ? 1 : 0, okSpoo ? ipSpoo.toString().c_str() : "-");
      bool okApi = WiFi.hostByName("api.spoo.me", ipApi);
      Serial.printf("[RSS][DNS] api.spoo.me ok=%d ip=%s\n", okApi ? 1 : 0, okApi ? ipApi.toString().c_str() : "-");
      bool okAnsa = WiFi.hostByName("www.ansa.it", ipAnsa);
      Serial.printf("[RSS][DNS] www.ansa.it ok=%d ip=%s\n", okAnsa ? 1 : 0, okAnsa ? ipAnsa.toString().c_str() : "-");

      WiFiClient tcp;
      bool tcpSpoo = okSpoo ? tcp.connect(ipSpoo, 443) : false;
      Serial.printf("[RSS][NET] tcp spoo.me:443 ok=%d\n", tcpSpoo ? 1 : 0);
      if (tcpSpoo) tcp.stop();
      bool tcpApi = okApi ? tcp.connect(ipApi, 443) : false;
      Serial.printf("[RSS][NET] tcp api.spoo.me:443 ok=%d\n", tcpApi ? 1 : 0);
      if (tcpApi) tcp.stop();

      WiFiClientSecure tls;
      tls.setInsecure();
      bool tlsSpoo = tls.connect("spoo.me", 443);
      Serial.printf("[RSS][NET] tls spoo.me:443 ok=%d\n", tlsSpoo ? 1 : 0);
      if (tlsSpoo) tls.stop();
      bool tlsApi = tls.connect("api.spoo.me", 443);
      Serial.printf("[RSS][NET] tls api.spoo.me:443 ok=%d\n", tlsApi ? 1 : 0);
      if (tlsApi) tls.stop();
    }
    Serial.printf("[%s] short fail idx=%u api=%d alt=%d apiHost=%d apiHostAlt=%d http=%d httpAlt=%d\n",
                  logPrefix ? logPrefix : "RSS",
                  (unsigned)(idx + 1), codeApi, codeAlt, codeApiHost, codeApiHostAlt,
#if RSS_SHORTENER_ALLOW_HTTP_FALLBACK
                  codeHttp, codeHttpAlt);
#else
                  0, 0);
#endif
    return false;
  }

  strncpy(it.shortLink, shortUrl.c_str(), sizeof(it.shortLink) - 1);
  it.shortLink[sizeof(it.shortLink) - 1] = '\0';
  it.shortReady = true;
  rssShortCacheStore(it.link, it.shortLink);
#if TEST_NTP
  g_uiNeedsRedraw = true;
#endif
  Serial.printf("[%s] short %u -> %s via %s\n",
                logPrefix ? logPrefix : "RSS",
                (unsigned)(idx + 1),
                it.shortLink,
                usedEndpoint ? usedEndpoint : "endpoint-unknown");
  return true;
}

static bool rssTryShortenUrl(uint8_t idx) {
  return rssTryShortenUrlForState(g_rss, idx, "RSS");
}

static bool wikiTryShortenUrl(uint8_t idx) {
  return rssTryShortenUrlForState(g_wiki, idx, "WIKI");
}

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
  if (WiFi.status() != WL_CONNECTED || !g_wifiConnected) return false;

  String wikiHost, wikiTitlePath;
  if (!rssExtractWikipediaArticleRef(articleUrl, wikiHost, wikiTitlePath)) return false;

  const String apiUrl = String("https://") + wikiHost + "/api/rest_v1/page/summary/" + wikiTitlePath;
  char httpFallback[320];
  const char *requestUrl = apiUrl.c_str();
  bool triedHttpFallback = false;

  while (true) {
    HTTPClient http;
    http.setConnectTimeout(RSS_HTTP_TIMEOUT_MS);
    http.setTimeout(RSS_HTTP_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.useHTTP10(true);

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
    http.addHeader("Connection", "close");
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
  char httpFallback[RSS_FEED_URL_LEN];
  const char *requestUrl = feedUrl;
  bool triedHttpFallback = false;

  while (true) {
    HTTPClient http;
    http.setConnectTimeout(RSS_HTTP_TIMEOUT_MS);
    http.setTimeout(RSS_HTTP_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.useHTTP10(true);

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
    http.addHeader("Connection", "close");

    pumpWebUiDuringIo();
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

    pumpWebUiDuringIo();
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
  if (WiFi.status() != WL_CONNECTED || !g_wifiConnected) return false;
  RssItem *parseBuf = ensureRssParseBuf();
  if (!parseBuf) return false;
  const uint32_t now = millis();
  const uint32_t waitMs = g_rss.valid ? rssRefreshIntervalByEnergy() : rssRetryIntervalByEnergy();
  if (!force && g_rss.lastAttemptMs != 0 && (now - g_rss.lastAttemptMs) < waitMs) return g_rss.valid;
  g_rss.lastAttemptMs = now;

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
    pumpWebUiDuringIo();
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
    pumpWebUiDuringIo();
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

  if (count == 0 && startsWithHttp(RSS_FEED_URL)) {
    bool alreadyConfigured = false;
    for (uint8_t slot = 0; slot < RSS_FEED_SLOT_COUNT; ++slot) {
      const RuntimeRssFeedConfig *feed = runtimeRssFeedBySlot(slot);
      if (!feed || !startsWithHttp(feed->url)) continue;
      if (strncmp(feed->url, RSS_FEED_URL, sizeof(feed->url)) == 0) {
        alreadyConfigured = true;
        break;
      }
    }
    if (!alreadyConfigured || configuredFeeds == 0) {
      ++feedsTried;
      int httpCode = 0;
      const uint8_t got = fetchRssItemsFromUrl(RSS_FEED_URL,
                                               &parseBuf[count],
                                               clampRssFeedMaxItems(RSS_DEFAULT_FEED_ITEMS),
                                               &httpCode);
      if (got > 0) {
        for (uint8_t k = 0; k < got; ++k) {
          parseBuf[count + k].feedSlot = 0;
        }
        count = (uint8_t)(count + got);
        ++feedsWithItems;
        Serial.printf("[RSS] fallback default feed used url=%s items=%u\n", RSS_FEED_URL, (unsigned)got);
      } else if (firstHttpErr == 0 && httpCode != 0) {
        firstHttpErr = httpCode;
      }
    }
  }

  if (count == 0) {
    g_rss.lastHttpCode = (firstHttpErr != 0) ? firstHttpErr : -1;
    return false;
  }

  g_rss.lastHttpCode = HTTP_CODE_OK;

  const bool changed =
      (!g_rss.valid) ||
      (g_rss.itemCount != count) ||
      (strncmp(g_rss.items[0].title, parseBuf[0].title, sizeof(g_rss.items[0].title) - 1) != 0);

  g_rss.itemCount = count;
  for (uint8_t i = 0; i < count; ++i) {
    g_rss.items[i] = parseBuf[i];
    char cachedShort[sizeof(g_rss.items[i].shortLink)] = {0};
    if (g_rss.items[i].link[0] && rssShortCacheLookup(g_rss.items[i].link, cachedShort, sizeof(cachedShort))) {
      strncpy(g_rss.items[i].shortLink, cachedShort, sizeof(g_rss.items[i].shortLink) - 1);
      g_rss.items[i].shortLink[sizeof(g_rss.items[i].shortLink) - 1] = '\0';
      g_rss.items[i].shortReady = true;
      g_rss.items[i].shortTried = true;
    }
  }
  rssShortCacheRetainCurrentFeedLinks(g_rss.items, count);
  if (count < RSS_MAX_ITEMS) {
    for (uint8_t i = count; i < RSS_MAX_ITEMS; ++i) {
      g_rss.items[i].title[0] = '\0';
      g_rss.items[i].link[0] = '\0';
      g_rss.items[i].pubDate[0] = '\0';
      g_rss.items[i].summary[0] = '\0';
      g_rss.items[i].shortLink[0] = '\0';
      g_rss.items[i].feedSlot = 0xFF;
      g_rss.items[i].wikiMetaReady = false;
      g_rss.items[i].wikiMetaTried = false;
      g_rss.items[i].shortReady = false;
      g_rss.items[i].shortTried = false;
    }
  }
  g_rss.currentIndex = 0;
  g_rss.lastRotateMs = now;
  g_rss.valid = true;
  g_rss.lastFetchMs = now;
  if (changed) {
    g_auxDeck.lastItemShown = -1;
    g_auxDeck.lastQrPayload[0] = '\0';
  }
  struct tm tinfo;
  if (getLocalTime(&tinfo, 50)) {
    snprintf(g_rss.fetchedAt, sizeof(g_rss.fetchedAt), "%02d/%02d %02d:%02d",
             tinfo.tm_mday, tinfo.tm_mon + 1, tinfo.tm_hour, tinfo.tm_min);
  } else {
    strncpy(g_rss.fetchedAt, "--/-- --:--", sizeof(g_rss.fetchedAt) - 1);
    g_rss.fetchedAt[sizeof(g_rss.fetchedAt) - 1] = '\0';
  }
#if TEST_NTP
  if (changed) g_uiNeedsRedraw = true;
#endif
  Serial.printf("[RSS] feeds=%u/%u items=%u first='%s'\n",
                (unsigned)feedsWithItems,
                (unsigned)feedsTried,
                (unsigned)g_rss.itemCount,
                g_rss.items[0].title);
  return true;
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

// Fetch a random Wikipedia article via REST API. Returns true if item populated.
static bool fetchWikiRandomArticle(RssItem &item) {
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
    http.useHTTP10(true);

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
    http.addHeader("Connection", "close");

    pumpWebUiDuringIo();
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

    pumpWebUiDuringIo();
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

  // Store thumbnail URL in shortLink for later fetch
  if (thumbUrl.length() > 0) {
    copyStringSafe(item.shortLink, sizeof(item.shortLink), thumbUrl.c_str());
    item.shortReady = false;
    item.shortTried = false;
  }

  return true;
}

static bool updateWikiFromFeed(bool force) {
  if (WiFi.status() != WL_CONNECTED || !g_wifiConnected) return false;
  RssItem *parseBuf = ensureRssParseBuf();
  if (!parseBuf) return false;
  const uint32_t now = millis();
  const uint32_t waitMs = g_wiki.valid ? rssRefreshIntervalByEnergy() : rssRetryIntervalByEnergy();
  if (!force && g_wiki.lastAttemptMs != 0 && (now - g_wiki.lastAttemptMs) < waitMs) return g_wiki.valid;
  g_wiki.lastAttemptMs = now;

  memset(parseBuf, 0, sizeof(RssItem) * RSS_MAX_ITEMS);
  uint8_t count = 0;
  uint8_t feedsTried = 0;
  uint8_t feedsWithItems = 0;
  int firstHttpErr = 0;

  // Build language-parameterized URLs for Featured (slot 0) and On This Day (slot 1).
  // Slot 2 (Random) is handled separately via REST API JSON below.
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
    pumpWebUiDuringIo();
    ++feedsTried;

    const uint8_t remaining = (uint8_t)(RSS_MAX_ITEMS - count);
    uint8_t feedCap = RSS_DEFAULT_FEED_ITEMS;
    if (feedCap > remaining) feedCap = remaining;
    int httpCode = 0;

    // Wikipedia feeds return ~10 days oldest-first.
    // Parse all into a PSRAM buffer and keep only the last feedCap (most recent).
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

    pumpWebUiDuringIo();
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
  // If the feed endpoints are empty or blocked, keep this view alive by pulling
  // multiple random summaries instead of failing hard with ERR -1.
  const uint8_t randomTarget = (count == 0) ? 3 : 1;
  for (uint8_t attempt = 0; attempt < randomTarget && count < RSS_MAX_ITEMS; ++attempt) {
    ++feedsTried;
    pumpWebUiDuringIo();
    RssItem randomItem;
    memset(&randomItem, 0, sizeof(randomItem));
    if (fetchWikiRandomArticle(randomItem)) {
      parseBuf[count] = randomItem;
      ++count;
      ++feedsWithItems;
    }
  }

  if (count == 0) {
    g_wiki.lastHttpCode = (firstHttpErr != 0) ? firstHttpErr : -1;
    return false;
  }

  g_wiki.lastHttpCode = HTTP_CODE_OK;
  const bool changed =
      (!g_wiki.valid) ||
      (g_wiki.itemCount != count) ||
      (strncmp(g_wiki.items[0].title, parseBuf[0].title, sizeof(g_wiki.items[0].title) - 1) != 0);

  g_wiki.itemCount = count;
  for (uint8_t i = 0; i < count; ++i) {
    g_wiki.items[i] = parseBuf[i];
    char cachedShort[sizeof(g_wiki.items[i].shortLink)] = {0};
    if (g_wiki.items[i].link[0] && rssShortCacheLookup(g_wiki.items[i].link, cachedShort, sizeof(cachedShort))) {
      strncpy(g_wiki.items[i].shortLink, cachedShort, sizeof(g_wiki.items[i].shortLink) - 1);
      g_wiki.items[i].shortLink[sizeof(g_wiki.items[i].shortLink) - 1] = '\0';
      g_wiki.items[i].shortReady = true;
      g_wiki.items[i].shortTried = true;
    }
  }
  if (count < RSS_MAX_ITEMS) {
    for (uint8_t i = count; i < RSS_MAX_ITEMS; ++i) {
      g_wiki.items[i].title[0] = '\0';
      g_wiki.items[i].link[0] = '\0';
      g_wiki.items[i].pubDate[0] = '\0';
      g_wiki.items[i].summary[0] = '\0';
      g_wiki.items[i].shortLink[0] = '\0';
      g_wiki.items[i].feedSlot = 0xFF;
      g_wiki.items[i].wikiMetaReady = false;
      g_wiki.items[i].wikiMetaTried = false;
      g_wiki.items[i].shortReady = false;
      g_wiki.items[i].shortTried = false;
    }
  }
  g_wiki.currentIndex = 0;
  g_wiki.lastRotateMs = now;
  g_wiki.valid = true;
  g_wiki.lastFetchMs = now;
  g_wikiMetaPreloadLastMs = 0;
  if (changed && g_uiPageMode == UI_PAGE_WIKI) {
    g_auxDeck.lastItemShown = -1;
    g_auxDeck.lastQrPayload[0] = '\0';
#if TEST_NTP
    g_uiNeedsRedraw = true;
#endif
  }
  struct tm tinfo;
  if (getLocalTime(&tinfo, 50)) {
    snprintf(g_wiki.fetchedAt, sizeof(g_wiki.fetchedAt), "%02d/%02d %02d:%02d",
             tinfo.tm_mday, tinfo.tm_mon + 1, tinfo.tm_hour, tinfo.tm_min);
  } else {
    strncpy(g_wiki.fetchedAt, "--/-- --:--", sizeof(g_wiki.fetchedAt) - 1);
    g_wiki.fetchedAt[sizeof(g_wiki.fetchedAt) - 1] = '\0';
  }
  Serial.printf("[WIKI] feeds=%u/%u items=%u first='%s'\n",
                (unsigned)feedsWithItems,
                (unsigned)feedsTried,
                (unsigned)g_wiki.itemCount,
                g_wiki.items[0].title);
  return true;
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
  if (WiFi.status() != WL_CONNECTED || !g_wifiConnected) return;
  const uint32_t now = millis();
  if ((now - g_wikiMetaPreloadLastMs) < 2500UL) return;
  g_wikiMetaPreloadLastMs = now;

  for (uint8_t i = 0; i < g_wiki.itemCount; ++i) {
    RssItem &item = g_wiki.items[i];
    if (item.wikiMetaTried && item.wikiMetaReady) continue;
    if (item.summary[0]) {
      item.wikiMetaReady = true;
      item.wikiMetaTried = true;
      continue;
    }
    if (rssTryEnrichItemWikipediaMeta(item)) {
      if (i == g_wiki.currentIndex && g_uiPageMode == UI_PAGE_WIKI) {
#if TEST_NTP
        g_uiNeedsRedraw = true;
#endif
      }
      return;
    }
  }
}
// Keep Wiki visuals progressing even while user stays on WIKI view.
// Runs one lightweight preload action per tick on the currently visible item.
static void wikiPreloadVisibleItemStep() {
  if (g_uiPageMode != UI_PAGE_WIKI) return;
  if (!g_wiki.valid || g_wiki.itemCount == 0) return;
  if (WiFi.status() != WL_CONNECTED || !g_wifiConnected) return;

  const uint32_t now = millis();
  if ((now - g_wikiVisiblePreloadLastMs) < 2200UL) return;
  g_wikiVisiblePreloadLastMs = now;

  const uint8_t idx = (uint8_t)(g_wiki.currentIndex % g_wiki.itemCount);
  RssItem &item = g_wiki.items[idx];

  // 1) Enrich summary/image metadata first (one HTTP call max).
  if (!item.summary[0] && !(item.wikiMetaTried && item.wikiMetaReady)) {
    if (rssTryEnrichItemWikipediaMeta(item)) {
#if TEST_NTP
      g_uiNeedsRedraw = true;
#endif
    }
    return;
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

static void runRssShortenerDiag() {
  Serial.println("[RSSDIAG] begin");
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
  IPAddress ipSpoo, ipApi, ipAnsa;
  const bool okSpoo = WiFi.hostByName("spoo.me", ipSpoo);
  const bool okApi = WiFi.hostByName("api.spoo.me", ipApi);
  const bool okAnsa = WiFi.hostByName("www.ansa.it", ipAnsa);
  Serial.printf("[RSSDIAG] dns spoo.me ok=%d ip=%s\n", okSpoo ? 1 : 0, okSpoo ? ipSpoo.toString().c_str() : "-");
  Serial.printf("[RSSDIAG] dns api.spoo.me ok=%d ip=%s\n", okApi ? 1 : 0, okApi ? ipApi.toString().c_str() : "-");
  Serial.printf("[RSSDIAG] dns www.ansa.it ok=%d ip=%s\n", okAnsa ? 1 : 0, okAnsa ? ipAnsa.toString().c_str() : "-");

  WiFiClient tcp;
  const bool tcpSpoo = okSpoo ? tcp.connect(ipSpoo, 443) : false;
  Serial.printf("[RSSDIAG] tcp spoo.me:443 ok=%d\n", tcpSpoo ? 1 : 0);
  if (tcpSpoo) tcp.stop();
  const bool tcpApi = okApi ? tcp.connect(ipApi, 443) : false;
  Serial.printf("[RSSDIAG] tcp api.spoo.me:443 ok=%d\n", tcpApi ? 1 : 0);
  if (tcpApi) tcp.stop();

  {
    WiFiClientSecure tls;
    tls.setInsecure();
    tls.setHandshakeTimeout((RSS_SHORTENER_HTTP_TIMEOUT_MS + 999U) / 1000U);
    const bool ok = tls.connect("spoo.me", 443);
    printTlsDiagResult("spoo.me:443", tls, ok);
    if (ok) tls.stop();
  }
  {
    WiFiClientSecure tls;
    tls.setInsecure();
    tls.setHandshakeTimeout((RSS_SHORTENER_HTTP_TIMEOUT_MS + 999U) / 1000U);
    const bool ok = tls.connect("api.spoo.me", 443);
    printTlsDiagResult("api.spoo.me:443", tls, ok);
    if (ok) tls.stop();
  }
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

  {
    String shortUrl;
    int codeHttps = 0;
    const bool okHttps = rssShortenViaJsonApi(
      RSS_SHORTENER_ENDPOINT,
      "https://www.ansa.it",
      shortUrl,
      codeHttps,
      true
    );
    Serial.printf("[RSSDIAG] shorten https endpoint=%s code=%d ok=%d short=%s\n",
                  RSS_SHORTENER_ENDPOINT,
                  codeHttps,
                  okHttps ? 1 : 0,
                  okHttps ? shortUrl.c_str() : "-");
  }

  HTTPClient http;
  http.setConnectTimeout(RSS_SHORTENER_CONNECT_TIMEOUT_MS);
  http.setTimeout(RSS_SHORTENER_HTTP_TIMEOUT_MS);
  if (http.begin("http://spoo.me/api/v1/shorten")) {
    http.addHeader("Accept", "application/json");
    http.addHeader("Content-Type", "application/json");
    const String body = "{\"long_url\":\"https://www.ansa.it\"}";
    const int code = http.POST((uint8_t *)body.c_str(), body.length());
    Serial.printf("[RSSDIAG] http spoo.me/api/v1/shorten code=%d\n", code);
    if (code > 0) {
      String resp = http.getString();
      resp.replace("\n", " ");
      resp.replace("\r", " ");
      if (resp.length() > 140) resp = resp.substring(0, 140) + "...";
      Serial.printf("[RSSDIAG] http body=%s\n", resp.c_str());
    } else {
      logShortenerHttpError("http://spoo.me/api/v1/shorten", code);
    }
    http.end();
  } else {
    Serial.println("[RSSDIAG] http begin failed for spoo.me/api/v1/shorten");
  }
  Serial.println("[RSSDIAG] end");
}
#else
static bool updateRssFromFeed(bool force) {
  (void)force;
  return false;
}

static void runRssShortenerDiag() {
  Serial.println("[RSSDIAG] RSS disabled");
}
#endif

static bool updateWeatherFromApi(bool force) {
  if (WiFi.status() != WL_CONNECTED || !g_wifiConnected) return false;
  const uint32_t now = millis();
  const uint32_t waitMs = g_weather.valid ? weatherRefreshIntervalByEnergy() : weatherRetryIntervalByEnergy();
  if (!force && (now - g_weather.lastFetchMs) < waitMs) return g_weather.valid;
  g_weather.lastFetchMs = now;

  const float lat = runtimeWeatherLat();
  const float lon = runtimeWeatherLon();
  String url = "http://api.open-meteo.com/v1/forecast?latitude=";
  url += String(lat, 4);
  url += "&longitude=";
  url += String(lon, 4);
  url += "&current=temperature_2m,relative_humidity_2m,weather_code,is_day,wind_speed_10m";
  url += "&hourly=temperature_2m,weather_code";
  url += "&daily=sunrise,sunset";
  url += "&timezone=Europe%2FRome&forecast_hours=36&forecast_days=2";

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
  const String payload = http.getString();
  http.end();

  float temp = 0.0f, hum = 0.0f, wc = 0.0f, day = 1.0f, wind = 0.0f;
  const bool okTemp = extractJsonNumberField(payload, "\"temperature_2m\"", temp);
  const bool okHum = extractJsonNumberField(payload, "\"relative_humidity_2m\"", hum);
  const bool okCode = extractJsonNumberField(payload, "\"weather_code\"", wc);
  const bool okDay = extractJsonNumberField(payload, "\"is_day\"", day);
  const bool okWind = extractJsonNumberField(payload, "\"wind_speed_10m\"", wind);

  String sunriseIso, sunsetIso;
  const bool okSunrise = extractJsonArrayStringAt(payload, "\"sunrise\":[", 0, sunriseIso);
  const bool okSunset = extractJsonArrayStringAt(payload, "\"sunset\":[", 0, sunsetIso);

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

  const int newHum = (int)lroundf(hum);
  const int newCode = (int)lroundf(wc);
  const bool newIsDay = ((int)lroundf(day) != 0);
  const bool changed =
      (!g_weather.valid) ||
      (fabsf(g_weather.tempC - temp) > 0.09f) ||
      (g_weather.humidity != newHum) ||
      (g_weather.weatherCode != newCode) ||
      (g_weather.isDay != newIsDay) ||
      (fabsf(g_weather.windKmh - wind) > 0.09f);

  g_weather.valid = true;
  g_weather.tempC = temp;
  g_weather.humidity = newHum;
  g_weather.weatherCode = newCode;
  g_weather.isDay = newIsDay;
  g_weather.windKmh = wind;
  if (okSunrise) isoToHhMm(sunriseIso, g_weather.sunrise);
  if (okSunset) isoToHhMm(sunsetIso, g_weather.sunset);

  g_weather.nextValid[0] = okNext0;
  g_weather.nextValid[1] = okNext3;
  g_weather.nextValid[2] = okNext6;
  if (okNext0) {
    g_weather.nextTemp[0] = (int)lroundf(next0);
    g_weather.nextCode[0] = (int)lroundf(nextCode0);
  }
  if (okNext3) {
    g_weather.nextTemp[1] = (int)lroundf(next3);
    g_weather.nextCode[1] = (int)lroundf(nextCode3);
  }
  if (okNext6) {
    g_weather.nextTemp[2] = (int)lroundf(next6);
    g_weather.nextCode[2] = (int)lroundf(nextCode6);
  }

  g_weather.tomorrowValid = okTomorrow;
  if (okTomorrow) {
    g_weather.tomorrowTemp = (int)lroundf(tomTemp);
    g_weather.tomorrowCode = (int)lroundf(tomCode);
  }

#if TEST_NTP
  if (changed) g_uiNeedsRedraw = true;
#endif
  Serial.printf("[METEO] %s %.1fC RH=%d%% wind=%.1fkm/h code=%d day=%d sun=%s/%s\n",
                runtimeWeatherCityLabel(),
                g_weather.tempC,
                g_weather.humidity,
                g_weather.windKmh,
                g_weather.weatherCode,
                g_weather.isDay ? 1 : 0,
                g_weather.sunrise,
                g_weather.sunset);
  return true;
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
  if (!g_canvasBuf) return;
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
    uint8_t *dst = reinterpret_cast<uint8_t *>(&g_canvasBuf[(size_t)yy * (size_t)DB_CANVAS_W + (size_t)startX]);
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
    case UI_PAGE_DOOM:
      return "DOOM";
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
    case UI_PAGE_DOOM: return UI_VIEW_FLAG_DOOM;
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
    case UI_PAGE_DOOM:
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
    UI_PAGE_DOOM,
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

static uint16_t lvglColorLuma(uint32_t rgb) {
  const uint16_t r = (uint16_t)((rgb >> 16) & 0xFFu);
  const uint16_t g = (uint16_t)((rgb >> 8) & 0xFFu);
  const uint16_t b = (uint16_t)(rgb & 0xFFu);
  return (uint16_t)((299u * r + 587u * g + 114u * b) / 1000u);
}

static uint16_t lvglColorContrastLuma(uint32_t fg, uint32_t bg) {
  const uint16_t lf = lvglColorLuma(fg);
  const uint16_t lb = lvglColorLuma(bg);
  return (lf >= lb) ? (uint16_t)(lf - lb) : (uint16_t)(lb - lf);
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
  const uint32_t bg = t.screenBg;
  const uint32_t candidates[] = {
      t.saverBalloon,
      t.saverFooter,
      t.infoText,
      t.auxSourceText,
      t.headerText,
      0xFFFFFF,
  };
  uint32_t best = 0xFFFFFF;
  uint16_t bestScore = 0;
  for (size_t i = 0; i < (sizeof(candidates) / sizeof(candidates[0])); ++i) {
    const uint32_t c = candidates[i];
    if (c == t.saverCow) continue;
    if (lvglColorLuma(c) < 145u) continue;
    const uint16_t score = lvglColorContrastLuma(c, bg);
    if (score > bestScore) {
      bestScore = score;
      best = c;
    }
  }
  if (bestScore < 95u) return 0xFFFFFF;
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

static void lvglApplyThemeStyles(bool forceInvalidate) {
  const UiThemeLvglTokens &t = activeUiTheme().lvgl;
  const bool cyberpunk = lvglThemeIsCyberpunk();
  const bool tokyo = lvglThemeIsTokyoTransit();
  const bool minimal = lvglThemeIsMinimalBrutalistMono();
  const lv_coord_t cardRadius = minimal ? 0 : 10;
  const lv_coord_t infoRadius = minimal ? 0 : 8;
  const lv_coord_t buttonRadius = minimal ? 0 : 4;
  const lv_coord_t badgeRadius = minimal ? 0 : 6;
  const lv_coord_t wifiBarRadius = minimal ? 0 : 1;
  const bool headerBordered = cyberpunk || minimal;
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
  uint32_t clockLine1 = cyberpunk ? t.infoText : t.headerText;
  uint32_t clockLine2 = t.infoText;
  uint32_t clockLine3 = cyberpunk ? t.auxMeta : headerMeta;
  uint32_t clockDivider = t.divider;
  if (tokyo) {
    // Tokyo transit clock copy sits on dark panel, so keep it bright and neon-accented.
    clockLine1 = t.auxSourceText;
    clockLine2 = t.infoText;
    clockLine3 = t.auxWhenText;
    clockDivider = t.auxSourceText;
  } else if (minimal) {
    // Brutalist mono keeps high contrast on dark clock panel with a single red accent.
    clockLine1 = t.infoText;
    clockLine2 = t.infoText;
    clockLine3 = t.auxMeta;
    clockDivider = t.auxSourceText;
  }

  lv_obj_t *scr = lv_scr_act();
  if (scr) {
    lv_obj_set_style_bg_color(scr, lv_color_hex(t.screenBg), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(scr, lv_color_hex(t.screenBg), LV_PART_MAIN);
  }

  if (g_lvglClockBlock) {
    lv_obj_set_style_bg_color(g_lvglClockBlock, lv_color_hex(panelBg), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(g_lvglClockBlock, lv_color_hex(panelBg), LV_PART_MAIN);
    lv_obj_set_style_radius(g_lvglClockBlock, cardRadius, LV_PART_MAIN);
  }
  if (g_lvglWeatherCard) {
    lv_obj_set_style_bg_color(g_lvglWeatherCard, lv_color_hex(weatherBg), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(g_lvglWeatherCard, lv_color_hex(weatherBg), LV_PART_MAIN);
    lv_obj_set_style_radius(g_lvglWeatherCard, cardRadius, LV_PART_MAIN);
  }
  if (g_lvglForecastBar) {
    lv_obj_set_style_bg_color(g_lvglForecastBar, lv_color_hex(weatherBg), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(g_lvglForecastBar, lv_color_hex(weatherBg), LV_PART_MAIN);
    lv_obj_set_style_radius(g_lvglForecastBar, cardRadius, LV_PART_MAIN);
  }
  if (g_lvglForecastBarFill) {
    lv_obj_set_style_bg_color(g_lvglForecastBarFill, lv_color_hex(weatherBg), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(g_lvglForecastBarFill, lv_color_hex(weatherBg), LV_PART_MAIN);
  }

  if (g_lvglClockHeader) {
    lv_obj_set_style_bg_color(g_lvglClockHeader, lv_color_hex(headerBg), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(g_lvglClockHeader, lv_color_hex(headerBg), LV_PART_MAIN);
    lv_obj_set_style_radius(g_lvglClockHeader, cardRadius, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_lvglClockHeader, headerBordered ? 1 : 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_lvglClockHeader, lv_color_hex(cyberpunk ? t.auxSourceText : t.divider), LV_PART_MAIN);
    lv_obj_set_style_border_opa(g_lvglClockHeader, headerBordered ? LV_OPA_80 : LV_OPA_0, LV_PART_MAIN);
  }
  if (g_lvglClockHeaderFill) {
    lv_obj_set_style_bg_color(g_lvglClockHeaderFill, lv_color_hex(headerBg), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(g_lvglClockHeaderFill, lv_color_hex(headerBg), LV_PART_MAIN);
  }
  if (g_lvglWeatherHeader) {
    lv_obj_set_style_bg_color(g_lvglWeatherHeader, lv_color_hex(headerBg), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(g_lvglWeatherHeader, lv_color_hex(headerBg), LV_PART_MAIN);
    lv_obj_set_style_radius(g_lvglWeatherHeader, cardRadius, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_lvglWeatherHeader, headerBordered ? 1 : 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_lvglWeatherHeader, lv_color_hex(cyberpunk ? t.auxSourceText : t.divider), LV_PART_MAIN);
    lv_obj_set_style_border_opa(g_lvglWeatherHeader, headerBordered ? LV_OPA_80 : LV_OPA_0, LV_PART_MAIN);
  }
  if (g_lvglWeatherHeaderFill) {
    lv_obj_set_style_bg_color(g_lvglWeatherHeaderFill, lv_color_hex(headerBg), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(g_lvglWeatherHeaderFill, lv_color_hex(headerBg), LV_PART_MAIN);
  }
  if (g_lvglInfoCard) {
    lv_obj_set_style_bg_color(g_lvglInfoCard, lv_color_hex(t.infoBg), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(g_lvglInfoCard, lv_color_hex(t.infoBg), LV_PART_MAIN);
    lv_obj_set_style_radius(g_lvglInfoCard, infoRadius, LV_PART_MAIN);
  }
  if (g_lvglInfoHeader) {
    lv_obj_set_style_bg_color(g_lvglInfoHeader, lv_color_hex(infoHeaderBg), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(g_lvglInfoHeader, lv_color_hex(infoHeaderBg), LV_PART_MAIN);
    lv_obj_set_style_border_color(g_lvglInfoHeader, lv_color_hex(infoHeaderBorder), LV_PART_MAIN);
    lv_obj_set_style_radius(g_lvglInfoHeader, infoRadius, LV_PART_MAIN);
  }
  if (g_lvglInfoHeaderFill) {
    lv_obj_set_style_bg_color(g_lvglInfoHeaderFill, lv_color_hex(infoHeaderBg), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(g_lvglInfoHeaderFill, lv_color_hex(infoHeaderBg), LV_PART_MAIN);
  }

  if (g_lvglClockDate) lv_obj_set_style_text_color(g_lvglClockDate, lv_color_hex(headerText), 0);
  if (g_lvglCity) lv_obj_set_style_text_color(g_lvglCity, lv_color_hex(headerText), 0);
  if (g_lvglSun) lv_obj_set_style_text_color(g_lvglSun, lv_color_hex(headerText), 0);
  if (g_lvglClockL1) lv_obj_set_style_text_color(g_lvglClockL1, lv_color_hex(clockLine1), 0);
  if (g_lvglClockL2) lv_obj_set_style_text_color(g_lvglClockL2, lv_color_hex(clockLine2), 0);
  if (g_lvglClockL3) lv_obj_set_style_text_color(g_lvglClockL3, lv_color_hex(clockLine3), 0);
  if (g_lvglClockDivider) lv_obj_set_style_bg_color(g_lvglClockDivider, lv_color_hex(clockDivider), LV_PART_MAIN);

  if (g_lvglTemp) lv_obj_set_style_text_color(g_lvglTemp, lv_color_hex(weatherTextPrimary), 0);
  if (g_lvglDesc) lv_obj_set_style_text_color(g_lvglDesc, lv_color_hex(weatherTextPrimary), 0);
  if (g_lvglHumidity) lv_obj_set_style_text_color(g_lvglHumidity, lv_color_hex(weatherTextPrimary), 0);
  if (g_lvglWind) lv_obj_set_style_text_color(g_lvglWind, lv_color_hex(weatherTextSecondary), 0);
  if (g_lvglForecastNow) lv_obj_set_style_text_color(g_lvglForecastNow, lv_color_hex(forecastText), 0);
  if (g_lvglForecastTomorrow) lv_obj_set_style_text_color(g_lvglForecastTomorrow, lv_color_hex(weatherTextSecondary), 0);
  if (g_lvglWeatherSep) {
    lv_obj_set_style_bg_color(g_lvglWeatherSep, lv_color_hex(weatherTextSecondary), LV_PART_MAIN);
  }
  if (g_lvglGlyph) {
    const bool weatherOnline = g_weather.valid;
    const uint32_t glyph = weatherOnline ? weatherGlyphOnline : weatherGlyphOffline;
    lv_obj_set_style_text_color(g_lvglGlyph, lv_color_hex(glyph), 0);
  }

  if (g_lvglInfoTitle) lv_obj_set_style_text_color(g_lvglInfoTitle, lv_color_hex(infoHeaderText), 0);
  if (g_lvglInfoEndpoint) lv_obj_set_style_text_color(g_lvglInfoEndpoint, lv_color_hex(infoHeaderText), 0);
  if (g_lvglInfoBodyLeft) lv_obj_set_style_text_color(g_lvglInfoBodyLeft, lv_color_hex(t.infoText), 0);

#if defined(LV_USE_QRCODE) && LV_USE_QRCODE
  if (g_lvglInfoWebQr) {
    lv_obj_t *infoQrParent = lv_obj_get_parent(g_lvglInfoWebQr);
    lv_coord_t infoQrSize = lv_obj_get_width(g_lvglInfoWebQr);
    if (infoQrSize < 32 && infoQrParent) {
      const lv_coord_t pw = lv_obj_get_width(infoQrParent);
      const lv_coord_t ph = lv_obj_get_height(infoQrParent);
      infoQrSize = ((pw < ph) ? pw : ph) - 4;
    }
    if (infoQrSize < 64) infoQrSize = 64;
    char infoPayload[sizeof(g_lvglInfoLastQrPayload)];
    char infoFallback[sizeof(g_lvglInfoLastQrPayload)] = "http://--:8080";
    if (g_wifiSetupApActive) wifiBuildSetupPortalUrl(infoFallback, sizeof(infoFallback));
    copyStringSafe(
      infoPayload,
      sizeof(infoPayload),
      g_lvglInfoLastQrPayload[0] ? g_lvglInfoLastQrPayload : infoFallback
    );
    lv_obj_del(g_lvglInfoWebQr);
    const lv_color_t infoQrDark = lv_color_hex(t.infoQrDark);
    const lv_color_t infoQrLight = lv_color_hex(t.infoQrLight);
    g_lvglInfoWebQr = lv_qrcode_create(infoQrParent, infoQrSize, infoQrDark, infoQrLight);
    lv_obj_align(g_lvglInfoWebQr, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(g_lvglInfoWebQr, infoQrLight, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_lvglInfoWebQr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_lvglInfoWebQr, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_lvglInfoWebQr, lv_color_hex(t.infoHeaderBorder), LV_PART_MAIN);
    lv_obj_set_style_border_opa(g_lvglInfoWebQr, LV_OPA_80, LV_PART_MAIN);
    lv_qrcode_update(g_lvglInfoWebQr, infoPayload, strlen(infoPayload));
  }
#endif

  for (uint8_t i = 0; i < 4; ++i) {
    if (!g_lvglClockWiFiBars[i]) continue;
    lv_obj_set_style_radius(g_lvglClockWiFiBars[i], wifiBarRadius, LV_PART_MAIN);
  }
  const uint32_t btnBorderHex = (lvglColorContrastLuma(t.auxSourceText, panelBg) >= 36u) ? t.auxSourceText : 0xEAF0FF;
  // ── Feed Deck theming (AUX + WIKI) ──────────────────────────────────────────
  {
    FeedDeckUi *feedDecks[] = {&g_auxDeck, &g_wikiDeck};
    for (FeedDeckUi *d : feedDecks) {
      if (d->card) {
        lv_obj_set_style_bg_color(d->card, lv_color_hex(panelBg), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_color(d->card, lv_color_hex(panelBg), LV_PART_MAIN);
        lv_obj_set_style_radius(d->card, cardRadius, LV_PART_MAIN);
      }
      if (d->header) {
        lv_obj_set_style_bg_color(d->header, lv_color_hex(headerBg), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_color(d->header, lv_color_hex(headerBg), LV_PART_MAIN);
        lv_obj_set_style_radius(d->header, cardRadius, LV_PART_MAIN);
        lv_obj_set_style_border_width(d->header, headerBordered ? 1 : 0, LV_PART_MAIN);
        lv_obj_set_style_border_color(d->header, lv_color_hex(cyberpunk ? t.auxSourceText : t.divider), LV_PART_MAIN);
        lv_obj_set_style_border_opa(d->header, headerBordered ? LV_OPA_80 : LV_OPA_0, LV_PART_MAIN);
      }
      if (d->headerFill) {
        lv_obj_set_style_bg_color(d->headerFill, lv_color_hex(headerBg), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_color(d->headerFill, lv_color_hex(headerBg), LV_PART_MAIN);
      }
      if (d->title)  lv_obj_set_style_text_color(d->title,  lv_color_hex(headerText), 0);
      if (d->status) lv_obj_set_style_text_color(d->status, lv_color_hex(headerText), 0);
      if (d->meta)   lv_obj_set_style_text_color(d->meta,   lv_color_hex(headerMeta), 0);
      if (d->sourceBadge) {
        lv_obj_set_style_bg_color(d->sourceBadge, lv_color_hex(t.auxBadgeBg), LV_PART_MAIN);
        lv_obj_set_style_border_width(d->sourceBadge, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(d->sourceBadge, lv_color_hex(t.auxSourceText), LV_PART_MAIN);
        lv_obj_set_style_border_opa(d->sourceBadge, LV_OPA_70, LV_PART_MAIN);
        lv_obj_set_style_radius(d->sourceBadge, badgeRadius, LV_PART_MAIN);
      }
      if (d->sourceBadgeText) lv_obj_set_style_text_color(d->sourceBadgeText, lv_color_hex(t.auxBadgeText), 0);
      if (d->sourceSite)      lv_obj_set_style_text_color(d->sourceSite,      lv_color_hex(t.auxSourceText), 0);
      if (d->news)            lv_obj_set_style_text_color(d->news,            lv_color_hex(t.auxText), 0);
      if (d->qrOverlay)       lv_obj_set_style_bg_color(d->qrOverlay,         lv_color_hex(t.screenBg), LV_PART_MAIN);
      if (d->qrHint)          lv_obj_set_style_text_color(d->qrHint,          lv_color_hex(t.auxQrHint), 0);
      if (d->nextFeedBtn) lv_obj_set_style_radius(d->nextFeedBtn, buttonRadius, LV_PART_MAIN);
      if (d->refreshBtn)  lv_obj_set_style_radius(d->refreshBtn,  buttonRadius, LV_PART_MAIN);
      if (d->qrBtn)       lv_obj_set_style_radius(d->qrBtn,       buttonRadius, LV_PART_MAIN);
      if (d->nextFeedBtn) {
        lv_obj_set_style_border_width(d->nextFeedBtn, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(d->nextFeedBtn, lv_color_hex(btnBorderHex), LV_PART_MAIN);
        lv_obj_set_style_border_opa(d->nextFeedBtn, LV_OPA_80, LV_PART_MAIN);
      }
      if (d->refreshBtn) {
        lv_obj_set_style_border_width(d->refreshBtn, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(d->refreshBtn, lv_color_hex(btnBorderHex), LV_PART_MAIN);
        lv_obj_set_style_border_opa(d->refreshBtn, LV_OPA_80, LV_PART_MAIN);
      }
      if (d->qrBtn) {
        lv_obj_set_style_border_width(d->qrBtn, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(d->qrBtn, lv_color_hex(btnBorderHex), LV_PART_MAIN);
        lv_obj_set_style_border_opa(d->qrBtn, LV_OPA_80, LV_PART_MAIN);
      }
#if defined(LV_USE_QRCODE) && LV_USE_QRCODE
      if (d->qr) {
        lv_obj_t *qrParent = lv_obj_get_parent(d->qr);
        lv_coord_t qrSize  = lv_obj_get_width(d->qr);
        if (qrSize < 32 && qrParent) {
          const lv_coord_t pw = lv_obj_get_width(qrParent);
          const lv_coord_t ph = lv_obj_get_height(qrParent);
          qrSize = ((pw < ph) ? pw : ph) - 4;
        }
        if (qrSize < 90) qrSize = 90;
        const bool qrHidden = lv_obj_has_flag(d->qr, LV_OBJ_FLAG_HIDDEN);
        const char *feedFallbackUrl = (d == &g_wikiDeck) ? "https://en.wikipedia.org" : "https://ansa.it";
        char qrPayload[sizeof(d->lastQrPayload)];
        copyStringSafe(qrPayload, sizeof(qrPayload),
          d->lastQrPayload[0] ? d->lastQrPayload : feedFallbackUrl);
        lv_obj_del(d->qr);
        const lv_color_t qrDark  = lv_color_hex(t.auxQrDark);
        const lv_color_t qrLight = lv_color_hex(t.auxQrLight);
        d->qr = lv_qrcode_create(qrParent, qrSize, qrDark, qrLight);
        lv_obj_center(d->qr);
        lv_obj_set_style_bg_color(d->qr, qrLight, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(d->qr, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(d->qr, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(d->qr, lv_color_hex(t.auxQrHint), LV_PART_MAIN);
        lv_obj_set_style_border_opa(d->qr, LV_OPA_70, LV_PART_MAIN);
        lv_qrcode_update(d->qr, qrPayload, strlen(qrPayload));
        if (qrHidden) lv_obj_add_flag(d->qr, LV_OBJ_FLAG_HIDDEN);
      }
#endif
      lvglSetDeckNextFeedButtonPressed(*d, false);
      lvglSetDeckRefreshButtonPressed(*d, false);
      lvglSetDeckQrButtonPressed(*d, false);
    }
  }

  lvglApplyThemeFonts();
  lvglCenterClockSentenceLabel();

#if SCREENSAVER_ENABLED
  if (g_lvglScreenSaverRoot) lv_obj_set_style_bg_color(g_lvglScreenSaverRoot, lv_color_hex(t.screenBg), LV_PART_MAIN);
  if (g_lvglScreenSaverSky) lv_obj_set_style_text_color(g_lvglScreenSaverSky, lv_color_hex(t.saverSky), 0);
  if (g_lvglScreenSaverField) lv_obj_set_style_text_color(g_lvglScreenSaverField, lv_color_hex(t.saverField), 0);
  if (g_lvglScreenSaverCow) lv_obj_set_style_text_color(g_lvglScreenSaverCow, lv_color_hex(t.saverCow), 0);
  if (g_lvglScreenSaverBalloon) lv_obj_set_style_text_color(g_lvglScreenSaverBalloon, lv_color_hex(saverReadableText), 0);
  if (g_lvglScreenSaverBalloonTail) lv_obj_set_style_text_color(g_lvglScreenSaverBalloonTail, lv_color_hex(saverReadableText), 0);
  if (g_lvglScreenSaverFooter) lv_obj_set_style_text_color(g_lvglScreenSaverFooter, lv_color_hex(saverReadableText), 0);
  for (uint8_t r = 0; r < kSaverSkyRowsMax; ++r) {
    for (uint8_t s = 0; s < kSaverStarsPerRow; ++s) {
      if (!g_lvglScreenSaverStarObj[r][s]) continue;
      lv_obj_set_style_text_color(g_lvglScreenSaverStarObj[r][s], lv_color_hex(t.saverStarLow), 0);
    }
  }
#endif

  g_lvglClockWiFiMask = 0xFFFF;
  lvglUpdateWiFiBars(true);

  if (!forceInvalidate) return;
  g_uiNeedsRedraw = true;
  if (g_lvglInfoRoot) lv_obj_invalidate(g_lvglInfoRoot);
  if (g_lvglHomeRoot) lv_obj_invalidate(g_lvglHomeRoot);
  if (g_lvglAuxRoot) lv_obj_invalidate(g_lvglAuxRoot);
#if SCREENSAVER_ENABLED
  if (g_lvglScreenSaverRoot) lv_obj_invalidate(g_lvglScreenSaverRoot);
#endif
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
    g_doomTouchZone = DOOM_TOUCH_NONE;
    g_doomNeutralPending = false;
  }
  if (mode == UI_PAGE_DOOM && g_uiPageMode != UI_PAGE_DOOM) {
    if (g_lvglDoomRoot) lv_obj_clear_flag(g_lvglDoomRoot, LV_OBJ_FLAG_HIDDEN);
    g_doomTouchZone = DOOM_TOUCH_NONE;
#if DB_HAS_PRBOOM_DONOR
    g_doomLaunchRequested = doomPrboomIsRunning();
#else
    g_doomLaunchRequested = false;
#endif
    doomRequestNeutralCalibrate();
    g_doomFrameDirty = true;
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
  g_uiClockMode = UI_CLOCK_MODE_WORDCLOCK;
  g_uiNeedsRedraw = true;
}

#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI && SCREENSAVER_ENABLED
static void markUserInteraction(uint32_t nowMs) {
  g_lastUserInteractionMs = nowMs;
}

static uint32_t lvglScreenSaverRandNext() {
  g_lvglScreenSaverRand = (g_lvglScreenSaverRand * 1664525UL) + 1013904223UL;
  return g_lvglScreenSaverRand;
}

static void lvglScreenSaverSetCowArt(int8_t dir) {
  if (!g_lvglScreenSaverCow) return;
  static const char *kCowRight =
      " _(__)_        V\n"
      "'-e e -'__,--.__)\n"
      "(o_o)        )\n"
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
  lv_label_set_text(g_lvglScreenSaverCow, (dir >= 0) ? kCowLeft : kCowRight);
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
  const uint8_t cols = g_lvglScreenSaverCols;
  if (cols < 24 || cols > kSaverSkyColsMax) return;
  static const char kPattern[] = "~`~~^~";
  const uint8_t patLen = (uint8_t)(sizeof(kPattern) - 1U);
  for (uint8_t i = 0; i < cols; ++i) {
    g_lvglScreenSaverFieldBuf[i] = kPattern[(uint8_t)((i + g_lvglScreenSaverFieldScroll) % patLen)];
  }
  // Tiny ASCII tree drifts with the field to avoid static burn-in lines.
  if (cols > 14) {
    uint8_t tx = (uint8_t)(3U + ((g_lvglScreenSaverFieldScroll / 2U) % (cols - 10U)));
    g_lvglScreenSaverFieldBuf[tx] = '/';
    g_lvglScreenSaverFieldBuf[tx + 1] = '^';
    g_lvglScreenSaverFieldBuf[tx + 2] = '\\';
    g_lvglScreenSaverFieldBuf[tx + 3] = '|';
  }
  g_lvglScreenSaverFieldBuf[cols] = '\0';
  if (g_lvglScreenSaverField) lv_label_set_text(g_lvglScreenSaverField, g_lvglScreenSaverFieldBuf);
}

static void lvglScreenSaverUpdateField(uint32_t nowMs) {
  if (!g_lvglScreenSaverField) return;
  if (nowMs < g_lvglScreenSaverFieldNextMs) return;
  g_lvglScreenSaverFieldNextMs = nowMs + 1400UL;
  g_lvglScreenSaverFieldScroll = (uint8_t)(g_lvglScreenSaverFieldScroll + 1U);
  lvglScreenSaverBuildFieldLine();
}

static void lvglScreenSaverUpdateFooter(uint32_t nowMs) {
  if (!g_lvglScreenSaverFooter) return;
  if (nowMs >= g_lvglScreenSaverFooterJitterNextMs) {
    g_lvglScreenSaverFooterJitterNextMs = nowMs + (18000UL + (lvglScreenSaverRandNext() % 9000UL));
    g_lvglScreenSaverFooterJitterIdx = (uint8_t)((g_lvglScreenSaverFooterJitterIdx + 1U) % 4U);
  }
  if (nowMs < g_lvglScreenSaverFooterNextMs) return;
  g_lvglScreenSaverFooterNextMs = nowMs + 1000UL;
  char buf[32];
  if (g_ntpSynced) {
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
  lv_label_set_text(g_lvglScreenSaverFooter, buf);
  lv_obj_align(g_lvglScreenSaverFooter, LV_ALIGN_BOTTOM_RIGHT,
               kJitterXY[g_lvglScreenSaverFooterJitterIdx][0],
               kJitterXY[g_lvglScreenSaverFooterJitterIdx][1]);
}

static uint32_t lvglScreenSaverIdleTargetMs(uint32_t nowMs) {
#if TEST_BATTERY
  if (g_battHasSample && !batteryExternalPowerLikelyNow(nowMs)) {
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
      g_lvglScreenSaverStarX[r][s] = 0;
      g_lvglScreenSaverStarLevel[r][s] = 0;
      g_lvglScreenSaverStarDir[r][s] = 1;
      g_lvglScreenSaverStarNextMs[r][s] = nowMs;
      if (g_lvglScreenSaverStarObj[r][s]) lv_obj_add_flag(g_lvglScreenSaverStarObj[r][s], LV_OBJ_FLAG_HIDDEN);
    }
  }
  const int16_t rowPitch = 12;
  const int16_t topY = 8;
  for (uint8_t r = 0; r < g_lvglScreenSaverRows; ++r) {
    const uint8_t seg = (uint8_t)(g_lvglScreenSaverCols / kSaverStarsPerRow);
    for (uint8_t s = 0; s < kSaverStarsPerRow; ++s) {
      const uint8_t start = (uint8_t)(s * seg);
      const uint8_t span = (seg > 8) ? (seg - 4) : seg;
      g_lvglScreenSaverStarX[r][s] = (uint8_t)(start + 2 + (lvglScreenSaverRandNext() % (span ? span : 1)));
      const bool startLit = (s == 0U) || ((lvglScreenSaverRandNext() % 100U) < 35U);
      if (startLit) {
        g_lvglScreenSaverStarLevel[r][s] = (uint8_t)(1U + (lvglScreenSaverRandNext() % 2U));
        g_lvglScreenSaverStarDir[r][s] = -1;
        g_lvglScreenSaverStarNextMs[r][s] = nowMs + (420UL + (lvglScreenSaverRandNext() % 760UL));
      } else {
        g_lvglScreenSaverStarLevel[r][s] = 0;
        g_lvglScreenSaverStarDir[r][s] = 1;
        g_lvglScreenSaverStarNextMs[r][s] = nowMs + (3500UL + (lvglScreenSaverRandNext() % 8500UL));
      }
      if (g_lvglScreenSaverStarObj[r][s]) {
        lv_obj_set_pos(g_lvglScreenSaverStarObj[r][s], 4 + ((int16_t)g_lvglScreenSaverStarX[r][s] * 8), topY + ((int16_t)r * rowPitch));
      }
    }
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

static void lvglScreenSaverQuotePackForLang(const char *const **items, uint8_t *count) {
  if (!items || !count) return;
  *items = kSaverQuotesIt;
  *count = (uint8_t)(sizeof(kSaverQuotesIt) / sizeof(kSaverQuotesIt[0]));
  if (strcmp(g_wordClockLang, "en") == 0) { *items = kSaverQuotesEn; *count = (uint8_t)(sizeof(kSaverQuotesEn) / sizeof(kSaverQuotesEn[0])); return; }
  if (strcmp(g_wordClockLang, "fr") == 0) { *items = kSaverQuotesFr; *count = (uint8_t)(sizeof(kSaverQuotesFr) / sizeof(kSaverQuotesFr[0])); return; }
  if (strcmp(g_wordClockLang, "de") == 0) { *items = kSaverQuotesDe; *count = (uint8_t)(sizeof(kSaverQuotesDe) / sizeof(kSaverQuotesDe[0])); return; }
  if (strcmp(g_wordClockLang, "es") == 0) { *items = kSaverQuotesEs; *count = (uint8_t)(sizeof(kSaverQuotesEs) / sizeof(kSaverQuotesEs[0])); return; }
  if (strcmp(g_wordClockLang, "pt") == 0) { *items = kSaverQuotesPt; *count = (uint8_t)(sizeof(kSaverQuotesPt) / sizeof(kSaverQuotesPt[0])); return; }
  if (strcmp(g_wordClockLang, "la") == 0) { *items = kSaverQuotesLa; *count = (uint8_t)(sizeof(kSaverQuotesLa) / sizeof(kSaverQuotesLa[0])); return; }
  if (strcmp(g_wordClockLang, "eo") == 0) { *items = kSaverQuotesEo; *count = (uint8_t)(sizeof(kSaverQuotesEo) / sizeof(kSaverQuotesEo[0])); return; }

  if (strcmp(g_wordClockLang, "tlh") == 0) { *items = kSaverQuotesTlh; *count = (uint8_t)(sizeof(kSaverQuotesTlh) / sizeof(kSaverQuotesTlh[0])); return; }
  if (strcmp(g_wordClockLang, "l33t") == 0) { *items = kSaverQuotesL33t; *count = (uint8_t)(sizeof(kSaverQuotesL33t) / sizeof(kSaverQuotesL33t[0])); return; }
  if (strcmp(g_wordClockLang, "sha") == 0) { *items = kSaverQuotesSha; *count = (uint8_t)(sizeof(kSaverQuotesSha) / sizeof(kSaverQuotesSha[0])); return; }
  if (strcmp(g_wordClockLang, "val") == 0) { *items = kSaverQuotesVal; *count = (uint8_t)(sizeof(kSaverQuotesVal) / sizeof(kSaverQuotesVal[0])); return; }
  if (strcmp(g_wordClockLang, "bellazio") == 0) { *items = kSaverQuotesBellazio; *count = (uint8_t)(sizeof(kSaverQuotesBellazio) / sizeof(kSaverQuotesBellazio[0])); return; }
}

static void toUpperAsciiInPlace(char *s) {
  if (!s) return;
  for (size_t i = 0; s[i]; ++i) {
    const unsigned char c = (unsigned char)s[i];
    if (c >= 'a' && c <= 'z') s[i] = (char)(c - ('a' - 'A'));
  }
}

static void lvglScreenSaverSetBalloonText() {
  if (!g_lvglScreenSaverBalloon) return;
  const char *const *quotes = nullptr;
  uint8_t n = 0;
  lvglScreenSaverQuotePackForLang(&quotes, &n);
  if (!quotes || n == 0) return;
  if (g_lvglScreenSaverBalloonIdx >= n) g_lvglScreenSaverBalloonIdx = 0;
  if (n > 1U) {
    g_lvglScreenSaverBalloonIdx =
        (uint8_t)((g_lvglScreenSaverBalloonIdx + 1U + (lvglScreenSaverRandNext() % (n - 1U))) % n);
  }
  const char *quote = quotes[g_lvglScreenSaverBalloonIdx];
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
  lv_label_set_text(g_lvglScreenSaverBalloon, wrapped);
  lv_label_set_long_mode(g_lvglScreenSaverBalloon, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(g_lvglScreenSaverBalloon, lvglScreenSaverResolvedBalloonWidth(wrapped), LV_SIZE_CONTENT);
  lv_obj_update_layout(g_lvglScreenSaverBalloon);
  if (g_lvglScreenSaverBalloonTail) {
    const int16_t bw = lv_obj_get_width(g_lvglScreenSaverBalloon);
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
    lv_label_set_text(g_lvglScreenSaverBalloonTail, ruleBuf);
  }
}

static void lvglScreenSaverUpdateBalloon(uint32_t nowMs) {
  if (!g_lvglScreenSaverBalloon) return;
  const uint32_t kCycleMs = 25000UL;   // 10s OFF + 15s ON
  const uint32_t kShowFromMs = 10000UL;
  const bool shouldShow = ((nowMs % kCycleMs) >= kShowFromMs);
  if (!shouldShow && g_lvglScreenSaverBalloonVisible) {
    g_lvglScreenSaverBalloonVisible = false;
    lv_obj_add_flag(g_lvglScreenSaverBalloon, LV_OBJ_FLAG_HIDDEN);
    if (g_lvglScreenSaverBalloonTail) lv_obj_add_flag(g_lvglScreenSaverBalloonTail, LV_OBJ_FLAG_HIDDEN);
  } else if (shouldShow && !g_lvglScreenSaverBalloonVisible) {
    g_lvglScreenSaverBalloonVisible = true;
    lvglScreenSaverSetBalloonText();
    lvglForceLabelVisible(g_lvglScreenSaverBalloon);
    if (g_lvglScreenSaverBalloonTail) lvglForceLabelVisible(g_lvglScreenSaverBalloonTail);
  }
}

static void lvglScreenSaverUpdateStars(uint32_t nowMs) {
  if (!g_lvglScreenSaverRoot) return;
  const UiThemeLvglTokens &t = activeUiTheme().lvgl;
  for (uint8_t r = 0; r < g_lvglScreenSaverRows; ++r) {
    for (uint8_t s = 0; s < kSaverStarsPerRow; ++s) {
      if (nowMs < g_lvglScreenSaverStarNextMs[r][s]) continue;
      uint8_t &lvl = g_lvglScreenSaverStarLevel[r][s];
      int8_t &dir = g_lvglScreenSaverStarDir[r][s];
      if (dir > 0) {
        if (lvl < 3) ++lvl;
        if (lvl >= 3) dir = -1;
        g_lvglScreenSaverStarNextMs[r][s] = nowMs + 520UL;
      } else {
        if (lvl > 0) --lvl;
        if (lvl == 0) {
          dir = 1;
          g_lvglScreenSaverStarNextMs[r][s] = nowMs + (3500UL + (lvglScreenSaverRandNext() % 8500UL));
        } else {
          g_lvglScreenSaverStarNextMs[r][s] = nowMs + 520UL;
        }
      }
    }
  }
  for (uint8_t r = 0; r < g_lvglScreenSaverRows; ++r) {
    for (uint8_t s = 0; s < kSaverStarsPerRow; ++s) {
      lv_obj_t *star = g_lvglScreenSaverStarObj[r][s];
      if (!star) continue;
      const uint8_t lvl = g_lvglScreenSaverStarLevel[r][s];
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
}

static void lvglScreenSaverRespawnCow() {
  const int16_t h = canvasHeight();
  g_lvglScreenSaverY = (h > 96) ? (h - 90) : 14;
  if (g_lvglScreenSaverX < 16 || g_lvglScreenSaverX > (canvasWidth() - 112)) {
    g_lvglScreenSaverX = (int16_t)(24 + (lvglScreenSaverRandNext() % 180U));
  }
  g_lvglScreenSaverColorIdx = (int8_t)((g_lvglScreenSaverColorIdx + 1) % 3);
  if (g_lvglScreenSaverCow) {
    const UiThemeLvglTokens &t = activeUiTheme().lvgl;
    (void)g_lvglScreenSaverColorIdx;
    lv_obj_set_style_text_color(g_lvglScreenSaverCow, lv_color_hex(t.saverCow), 0);
    lvglScreenSaverSetCowArt(g_lvglScreenSaverCowDir);
    lv_obj_set_pos(g_lvglScreenSaverCow, g_lvglScreenSaverX, g_lvglScreenSaverY);
  }
  g_lvglScreenSaverCowStepsLeft = 0;
  g_lvglScreenSaverCowDir = ((lvglScreenSaverRandNext() & 1U) == 0U) ? 1 : -1;
  lvglScreenSaverSetCowArt(g_lvglScreenSaverCowDir);
  g_lvglScreenSaverCowNextMoveMs = millis() + (1000UL + (lvglScreenSaverRandNext() % 5000UL));
  g_lvglScreenSaverBalloonVisible = false;
  g_lvglScreenSaverBalloonNextMs = millis() + 15000UL;
}

static void lvglSetScreenSaverActive(bool on) {
  if (!g_lvglScreenSaverRoot || !g_lvglReady) return;
  if (g_lvglScreenSaverActive == on) return;
  g_lvglScreenSaverActive = on;
  if (on) {
    lv_obj_clear_flag(g_lvglScreenSaverRoot, LV_OBJ_FLAG_HIDDEN);
    g_lvglScreenSaverCols = (uint8_t)((canvasWidth() / 8) - 2);
    if (g_lvglScreenSaverCols > kSaverSkyColsMax) g_lvglScreenSaverCols = kSaverSkyColsMax;
    if (g_lvglScreenSaverCols < 36) g_lvglScreenSaverCols = 36;
    g_lvglScreenSaverRows = (uint8_t)(((canvasHeight() - 68) / 12));
    if (g_lvglScreenSaverRows > kSaverSkyRowsMax) g_lvglScreenSaverRows = kSaverSkyRowsMax;
    if (g_lvglScreenSaverRows < 4) g_lvglScreenSaverRows = 4;
    lvglScreenSaverInitStars();
    g_lvglScreenSaverFieldScroll = (uint8_t)(lvglScreenSaverRandNext() & 0x0FU);
    g_lvglScreenSaverFieldNextMs = millis() + 1200UL;
    g_lvglScreenSaverFooterJitterIdx = (uint8_t)(lvglScreenSaverRandNext() % 4U);
    g_lvglScreenSaverFooterJitterNextMs = millis() + 10000UL;
    lvglScreenSaverBuildFieldLine();
    lvglScreenSaverUpdateStars(millis());
    lvglScreenSaverRespawnCow();
    lvglScreenSaverUpdateFooter(millis());
    g_lvglScreenSaverLastStepMs = millis();
    lv_obj_move_foreground(g_lvglScreenSaverRoot);
    if (g_lvglScreenSaverFooter) lv_obj_clear_flag(g_lvglScreenSaverFooter, LV_OBJ_FLAG_HIDDEN);
    if (g_lvglScreenSaverBalloon) lv_obj_add_flag(g_lvglScreenSaverBalloon, LV_OBJ_FLAG_HIDDEN);
    if (g_lvglScreenSaverBalloonTail) lv_obj_add_flag(g_lvglScreenSaverBalloonTail, LV_OBJ_FLAG_HIDDEN);
    Serial.println("[SCRNSVR] ON");
  } else {
    lv_obj_add_flag(g_lvglScreenSaverRoot, LV_OBJ_FLAG_HIDDEN);
    if (g_lvglScreenSaverFooter) lv_obj_add_flag(g_lvglScreenSaverFooter, LV_OBJ_FLAG_HIDDEN);
    if (g_lvglScreenSaverBalloon) lv_obj_add_flag(g_lvglScreenSaverBalloon, LV_OBJ_FLAG_HIDDEN);
    if (g_lvglScreenSaverBalloonTail) lv_obj_add_flag(g_lvglScreenSaverBalloonTail, LV_OBJ_FLAG_HIDDEN);
    g_lvglScreenSaverWakeGuardUntilMs = millis() + 900UL;
    g_uiNeedsRedraw = true;
    Serial.println("[SCRNSVR] OFF");
  }
}

static void handleScreenSaverLoop(uint32_t nowMs) {
  if (!g_lvglReady || !g_lvglScreenSaverRoot) return;
#if TEST_TOUCH
  const bool rawTouch = isAnyTouchPresentRaw();
  if (rawTouch) {
    if (g_touchRawPresenceCount < 6) ++g_touchRawPresenceCount;
  } else {
    g_touchRawPresenceCount = 0;
  }
  const bool touching = g_touchDown || (g_touchRawPresenceCount >= 2);
#else
  const bool rawTouch = false;
  const bool touching = false;
#endif
  if (!g_lvglScreenSaverActive && g_lastUserInteractionMs == 0) g_lastUserInteractionMs = nowMs;

  if (!g_lvglScreenSaverActive) {
    if (nowMs < g_lvglScreenSaverWakeGuardUntilMs) return;
    const uint32_t idleTargetMs = lvglScreenSaverIdleTargetMs(nowMs);
    if (!rawTouch && !touching && (nowMs - g_lastUserInteractionMs) >= idleTargetMs) {
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
  if (g_lastShakeMs != 0 && (nowMs - g_lastShakeMs) < 1200UL) {
    lvglSetScreenSaverActive(false);
    markUserInteraction(nowMs);
    return;
  }
#endif

  if ((nowMs - g_lvglScreenSaverLastStepMs) < SCREENSAVER_STEP_MS) return;
  g_lvglScreenSaverLastStepMs = nowMs;
  lvglScreenSaverUpdateStars(nowMs);
  lvglScreenSaverUpdateField(nowMs);
  lvglScreenSaverUpdateBalloon(nowMs);
  lvglScreenSaverUpdateFooter(nowMs);
  if (nowMs >= g_lvglScreenSaverCowNextMoveMs) {
    bool dirChanged = false;
    if (g_lvglScreenSaverCowStepsLeft == 0) {
      g_lvglScreenSaverCowStepsLeft = (uint8_t)(2U + (lvglScreenSaverRandNext() % 5U));  // short walk burst
      if ((lvglScreenSaverRandNext() % 5U) == 0U) {
        g_lvglScreenSaverCowDir = -g_lvglScreenSaverCowDir;
        dirChanged = true;
      }
    }
    const int16_t minX = 8;
    const int16_t maxX = canvasWidth() - 250;
    int16_t nx = (int16_t)(g_lvglScreenSaverX + (g_lvglScreenSaverCowDir * 6));
    if (nx < minX) {
      nx = minX;
      g_lvglScreenSaverCowDir = 1;
      dirChanged = true;
    } else if (nx > maxX) {
      nx = maxX;
      g_lvglScreenSaverCowDir = -1;
      dirChanged = true;
    }
    if (dirChanged) lvglScreenSaverSetCowArt(g_lvglScreenSaverCowDir);
    g_lvglScreenSaverX = nx;
    if (g_lvglScreenSaverCowStepsLeft > 0) --g_lvglScreenSaverCowStepsLeft;
    g_lvglScreenSaverCowNextMoveMs = nowMs + ((g_lvglScreenSaverCowStepsLeft > 0) ? 180UL : (1000UL + (lvglScreenSaverRandNext() % 5000UL)));
  }
  if (g_lvglScreenSaverCow) {
    lv_obj_set_pos(g_lvglScreenSaverCow, g_lvglScreenSaverX, g_lvglScreenSaverY);
  }
  if (g_lvglScreenSaverBalloon && g_lvglScreenSaverBalloonVisible) {
    const int16_t bw = lv_obj_get_width(g_lvglScreenSaverBalloon);
    const int16_t bh = lv_obj_get_height(g_lvglScreenSaverBalloon);
    int16_t bx = g_lvglScreenSaverX + ((g_lvglScreenSaverCowDir >= 0) ? 120 : -bw + 96);
    const int16_t maxX = canvasWidth() - bw - 8;
    if (bx < 8) bx = 8;
    if (bx > maxX) bx = maxX;
    const int16_t by = g_lvglScreenSaverY - bh - 10;
    lv_obj_set_pos(g_lvglScreenSaverBalloon, bx, (by < 4) ? 4 : by);
    if (g_lvglScreenSaverBalloonTail) {
      lv_obj_set_pos(g_lvglScreenSaverBalloonTail, bx, lv_obj_get_y(g_lvglScreenSaverBalloon) + bh + 2);
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
    g_touchReady = true;
    g_touchUseAltBus = true;
  } else if (errMain == 0) {
    g_touchReady = true;
    g_touchUseAltBus = false;
  } else {
    g_touchReady = false;
  }

  Serial.printf("[TOUCH] probe addr=0x%02X main=%d alt=%d -> %s (%s)\n",
                TOUCH_I2C_ADDR, errMain, errAlt,
                g_touchReady ? "OK" : "FAIL",
                g_touchReady ? (g_touchUseAltBus ? "ALT" : "MAIN") : "-");
  return g_touchReady;
}

static bool readTouchLogicalPoint(int16_t &lx, int16_t &ly) {
  if (!g_touchReady) return false;
  static const uint8_t kReadCmd[11] = {0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00};
  uint8_t data[8] = {0};

  TwoWire &tb = g_touchUseAltBus ? I2C_ALT : I2C_MAIN;
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
  if (!g_touchReady) return false;
  static const uint8_t kReadCmd[11] = {0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00};
  uint8_t data[8] = {0};
  TwoWire &tb = g_touchUseAltBus ? I2C_ALT : I2C_MAIN;
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

static void handleTouchSwipeInput() {
  int16_t x = 0, y = 0;
  const bool touched = readTouchLogicalPoint(x, y);
  const uint32_t now = millis();

#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI && SCREENSAVER_ENABLED
  if (touched) {
    markUserInteraction(now);
    if (g_lvglScreenSaverActive) {
      lvglSetScreenSaverActive(false);
      g_touchDown = false;
      g_touchPageDragging = false;
      g_touchAuxBtnDown = TOUCH_AUX_BTN_NONE;
      g_touchAwaitRelease = true;
      g_touchReleaseStartMs = 0;
      return;
    }
  }
#endif

  // After a page commit, wait for a full release before accepting a new gesture.
  if (g_touchAwaitRelease) {
    if (touched) {
      g_touchReleaseStartMs = 0;
      return;
    }
    if (g_touchReleaseStartMs == 0) {
      g_touchReleaseStartMs = now;
      return;
    }
    if ((now - g_touchReleaseStartMs) < 70) return;
    g_touchAwaitRelease = false;
    g_touchReleaseStartMs = 0;
    return;
  }

  if (touched) {
    g_touchMissCount = 0;  // reset miss counter on any detection
    if (!g_touchDown) {
      g_touchDown = true;
      g_touchStartX = x;
      g_touchStartY = y;
      g_touchStartMs = now;
      g_touchPageDragging = false;
      g_touchAuxBtnDown = TOUCH_AUX_BTN_NONE;
      if (uiPageIsFeedDeck(g_uiPageMode)) {
        if (lvglFeedQrButtonContainsPoint(x, y)) g_touchAuxBtnDown = TOUCH_AUX_BTN_QR;
        else if (lvglFeedRefreshButtonContainsPoint(x, y)) g_touchAuxBtnDown = TOUCH_AUX_BTN_REFRESH;
        else if (lvglFeedNextFeedButtonContainsPoint(x, y)) g_touchAuxBtnDown = TOUCH_AUX_BTN_NEXT;
      }
      if (g_touchAuxBtnDown == TOUCH_AUX_BTN_QR) {
        lvglSetFeedQrButtonPressed(true);
        Serial.printf("[TOUCH] btn-down QR x=%d y=%d\n", x, y);
      } else if (g_touchAuxBtnDown == TOUCH_AUX_BTN_REFRESH) {
        lvglSetFeedRefreshButtonPressed(true);
        Serial.printf("[TOUCH] btn-down SKIP x=%d y=%d\n", x, y);
      } else if (g_touchAuxBtnDown == TOUCH_AUX_BTN_NEXT) {
        lvglSetFeedNextFeedButtonPressed(true);
        Serial.printf("[TOUCH] btn-down NXT x=%d y=%d\n", x, y);
      }
#if TEST_DISPLAY && DOOM_SPIKE_ENABLED
      if (g_uiPageMode == UI_PAGE_DOOM) {
        g_doomTouchZone = doomTouchZoneFromX(x);
        g_doomFrameDirty = true;
        Serial.printf("[DOOM][TOUCH] down zone=%s x=%d y=%d\n",
                      doomTouchZoneName(g_doomTouchZone), x, y);
      }
#endif
    }
    g_touchLastX = x;
    g_touchLastY = y;
#if TEST_DISPLAY
    if (g_uiPageMode == UI_PAGE_DOOM) {
      return;
    }
#endif
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI
    if (g_lvglReady) {
      if (g_touchAuxBtnDown != TOUCH_AUX_BTN_NONE) {
        return;
      }
      const int16_t liveDx = g_touchLastX - g_touchStartX;
      const int16_t liveDy = g_touchLastY - g_touchStartY;
      constexpr int16_t kDragStartPx = 5;
      if (g_touchPageDragging) {
        lvglApplyPageDrag(liveDx);
        return;
      }
      if (abs(liveDx) >= kDragStartPx && abs(liveDx) >= abs(liveDy)) {
        g_touchPageDragging = true;
        lvglApplyPageDrag(liveDx);
        return;
      }
    }
#endif
    return;
  }

  // AXS15231B touch controller has a finite scan rate (~60Hz). The read command
  // clears the buffer, so rapid polling can see "no touch" between scans even
  // while the finger is still on the screen. Bridge these gaps by requiring
  // several consecutive miss frames before declaring a true release.
  if (g_touchDown) {
    if (++g_touchMissCount < 12) return;  // ~12 frames ≈ 25-50ms hold-off
    g_touchMissCount = 0;
  }

  if (!g_touchDown) return;
  g_touchDown = false;
  const int16_t dx = g_touchLastX - g_touchStartX;
  const int16_t dy = g_touchLastY - g_touchStartY;
  const uint32_t durMs = millis() - g_touchStartMs;
  const bool horizontalIntent = (abs(dx) >= abs(dy));
  const bool fastFlick = (durMs <= 220) && (abs(dx) >= ((DISPLAY_TOUCH_SWIPE_MIN_PX / 2) + 2));
  constexpr int16_t kSwipePageMinPx = DISPLAY_TOUCH_SWIPE_MIN_PX;
  const bool pageSwipe = horizontalIntent && ((abs(dx) >= kSwipePageMinPx) || fastFlick);
  const bool isTap = (durMs <= DISPLAY_TOUCH_TAP_MAX_MS &&
                      abs(dx) <= DISPLAY_TOUCH_TAP_MAX_PX &&
                      abs(dy) <= DISPLAY_TOUCH_TAP_MAX_PX);
  const bool isBtnTap = (durMs <= 1400 &&
                         abs(dx) <= 72 &&
                         abs(dy) <= 72);
  const TouchAuxButton touchAuxBtnDown = g_touchAuxBtnDown;
  g_touchAuxBtnDown = TOUCH_AUX_BTN_NONE;
  const uint8_t doomTouchZone = g_doomTouchZone;
  g_doomTouchZone = DOOM_TOUCH_NONE;
#if TEST_DISPLAY && DOOM_SPIKE_ENABLED
  if (doomTouchZone != DOOM_TOUCH_NONE) g_doomFrameDirty = true;
#endif
  if (touchAuxBtnDown == TOUCH_AUX_BTN_QR) lvglSetFeedQrButtonPressed(false);
  else if (touchAuxBtnDown == TOUCH_AUX_BTN_REFRESH) lvglSetFeedRefreshButtonPressed(false);
  else if (touchAuxBtnDown == TOUCH_AUX_BTN_NEXT) lvglSetFeedNextFeedButtonPressed(false);

  if (touchAuxBtnDown != TOUCH_AUX_BTN_NONE) {
    Serial.printf("[TOUCH] btn-up kind=%u tap=%d dx=%d dy=%d dur=%lums\n",
                  (unsigned)touchAuxBtnDown, isBtnTap ? 1 : 0, dx, dy, (unsigned long)durMs);
    if (isBtnTap && uiPageIsFeedDeck(g_uiPageMode)) {
      RssState &content = (g_uiPageMode == UI_PAGE_WIKI) ? g_wiki : g_rss;
      const char *tag = (g_uiPageMode == UI_PAGE_WIKI) ? "wiki" : "rss";
      if (touchAuxBtnDown == TOUCH_AUX_BTN_QR) {
        if (lvglFeedQrModalIsOpen()) {
          lvglSetFeedQrModalOpen(false);
          Serial.println("[TOUCH] qr-close");
        } else {
          lv_obj_t *feedStatus = (g_uiPageMode == UI_PAGE_WIKI) ? g_wikiDeck.status : g_auxDeck.status;
          if (feedStatus) { lv_label_set_text(feedStatus, "QR..."); lvglForceLabelVisible(feedStatus); }
          lvglSetFeedQrModalOpen(true);
          Serial.println("[TOUCH] qr-open");
        }
      } else if (touchAuxBtnDown == TOUCH_AUX_BTN_REFRESH) {
#if TEST_WIFI && RSS_ENABLED
        {
          lv_obj_t *feedStatus = (g_uiPageMode == UI_PAGE_WIKI) ? g_wikiDeck.status : g_auxDeck.status;
          if (feedStatus) { lv_label_set_text(feedStatus, "SKIP"); lvglForceLabelVisible(feedStatus); }
        }
        const bool moved = (g_uiPageMode == UI_PAGE_WIKI) ? wikiAdvanceToNextItem() : rssAdvanceToNextItem();
        if (moved) g_uiNeedsRedraw = true;
        Serial.printf("[TOUCH] %s-skip moved=%d idx=%u/%u\n",
                      tag,
                      moved ? 1 : 0,
                      (unsigned)(content.currentIndex + 1),
                      (unsigned)content.itemCount);
#endif
      } else if (touchAuxBtnDown == TOUCH_AUX_BTN_NEXT) {
#if TEST_WIFI && RSS_ENABLED
        {
          lv_obj_t *feedStatus = (g_uiPageMode == UI_PAGE_WIKI) ? g_wikiDeck.status : g_auxDeck.status;
          if (feedStatus) { lv_label_set_text(feedStatus, "FEED"); lvglForceLabelVisible(feedStatus); }
        }
        const bool moved = (g_uiPageMode == UI_PAGE_WIKI) ? wikiAdvanceToNextFeed() : rssAdvanceToNextFeed();
        if (moved) g_uiNeedsRedraw = true;
        Serial.printf("[TOUCH] %s-next-feed moved=%d idx=%u/%u\n",
                      tag,
                      moved ? 1 : 0,
                      (unsigned)(content.currentIndex + 1),
                      (unsigned)content.itemCount);
#endif
      }
    }
    g_touchAwaitRelease = true;
    g_touchReleaseStartMs = 0;
    return;
  }

  if (g_touchPageDragging) {
    g_touchPageDragging = false;
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI
    g_lvglPageDragActive = false;
    if (uiPageIsFeedDeck(g_uiPageMode) && lvglFeedQrModalIsOpen()) {
      lvglApplyPageVisibility(true);
      g_touchAwaitRelease = true;
      g_touchReleaseStartMs = 0;
      Serial.printf("[TOUCH] drag ignored (qr-open) dx=%d dy=%d dur=%lums\n",
                    dx, dy, (unsigned long)durMs);
      return;
    }
    if (durMs <= 3000 && pageSwipe && ((millis() - g_lastSwipeToggleMs) >= 140)) {
      const int8_t dir = (dx < 0) ? 1 : -1;
      bool moved = false;
      moved = stepUiPage(dir, false);
      g_lastSwipeToggleMs = millis();
      g_touchAwaitRelease = true;
      g_touchReleaseStartMs = 0;
      Serial.printf("[TOUCH] drag-swipe dx=%d dy=%d dur=%lums -> page=%s moved=%d\n",
                    dx, dy, (unsigned long)durMs, uiPageName(g_uiPageMode), moved ? 1 : 0);
    } else {
      lvglApplyPageVisibility(true);  // snap back to current page
      g_touchAwaitRelease = true;
      g_touchReleaseStartMs = 0;
      Serial.printf("[TOUCH] drag-cancel dx=%d dy=%d dur=%lums\n",
                    dx, dy, (unsigned long)durMs);
    }
    return;
#endif
  }

#if TEST_DISPLAY && DOOM_SPIKE_ENABLED
  if (g_uiPageMode == UI_PAGE_DOOM) {
    if (pageSwipe) {
      UiPageMode doomExit = uiLastEnabledMainViewNoEnsure();
      const int8_t doomOrd = uiPageOrdinal(UI_PAGE_DOOM);
      if (doomOrd > 0) doomExit = uiPageFromOrdinal(doomOrd - 1);
      setUiPage(doomExit);
      g_lastSwipeToggleMs = millis();
      g_touchAwaitRelease = true;
      g_touchReleaseStartMs = 0;
      Serial.printf("[DOOM][TOUCH] swipe-exit zone=%s dx=%d dy=%d dur=%lums -> %s\n",
                    doomTouchZoneName(doomTouchZone),
                    dx,
                    dy,
                    (unsigned long)durMs,
                    uiPageName(g_uiPageMode));
      return;
    }
    if (doomTouchZone == DOOM_TOUCH_LEFT || doomTouchZone == DOOM_TOUCH_RIGHT) {
      bool doomLaunching = g_doomLaunchRequested;
#if DB_HAS_PRBOOM_DONOR
      doomLaunching = doomLaunching || doomPrboomIsRunning();
#endif
      if (!doomLaunching && doomTouchZone == DOOM_TOUCH_RIGHT && isTap) {
        g_doomLaunchRequested = true;
        g_doomFrameDirty = true;
#if DB_HAS_PRBOOM_DONOR
        doomPrboomEnsureStarted();
#endif
        Serial.printf("[DOOM][TOUCH] zone=%s action=START tap=1 -> boot core\n",
                      doomTouchZoneName(doomTouchZone));
        g_touchAwaitRelease = true;
        g_touchReleaseStartMs = 0;
        return;
      }
      const char *action = (doomTouchZone == DOOM_TOUCH_LEFT) ? "USE" : "FIRE";
      Serial.printf("[DOOM][TOUCH] zone=%s action=%s tap=%d dx=%d dy=%d dur=%lums\n",
                    doomTouchZoneName(doomTouchZone),
                    action,
                    isTap ? 1 : 0,
                    dx,
                    dy,
                    (unsigned long)durMs);
      g_touchAwaitRelease = true;
      g_touchReleaseStartMs = 0;
      return;
    }
    if (isTap) {
      doomRequestNeutralCalibrate();
      g_touchAwaitRelease = true;
      g_touchReleaseStartMs = 0;
      Serial.printf("[DOOM][TOUCH] center-tap recalibrate x=%d y=%d\n", g_touchStartX, g_touchStartY);
      return;
    }
  }
#endif

  if (durMs > 3000) return;
  if (uiPageIsFeedDeck(g_uiPageMode) && lvglFeedQrModalIsOpen()) {
    if (durMs <= 2500) {
      lvglSetFeedQrModalOpen(false);
      Serial.println("[TOUCH] qr-close-overlay");
    }
    g_touchAwaitRelease = true;
    g_touchReleaseStartMs = 0;
    return;
  }
  if ((millis() - g_lastSwipeToggleMs) < 140) return;

#if TEST_DISPLAY
  if (g_uiPageMode == UI_PAGE_DOOM && pageSwipe) {
    UiPageMode doomExit = uiLastEnabledMainViewNoEnsure();
    const int8_t doomOrd = uiPageOrdinal(UI_PAGE_DOOM);
    if (doomOrd > 0) doomExit = uiPageFromOrdinal(doomOrd - 1);
    setUiPage(doomExit);
    g_lastSwipeToggleMs = millis();
    g_touchAwaitRelease = true;
    g_touchReleaseStartMs = 0;
    Serial.printf("[TOUCH] doom-exit dx=%d dy=%d tap=%d -> %s\n", dx, dy, isTap ? 1 : 0, uiPageName(g_uiPageMode));
    return;
  }
#endif

  // Primary gesture: horizontal swipe changes page across the enabled carousel.
  if (pageSwipe) {
    const int8_t dir = (dx < 0) ? 1 : -1;
    bool moved = false;
    moved = stepUiPage(dir, false);
    g_lastSwipeToggleMs = millis();
    g_touchAwaitRelease = true;
    g_touchReleaseStartMs = 0;
    const char *dirLabel = (dx < 0) ? "LEFT" : "RIGHT";
    Serial.printf("[TOUCH] swipe %s dx=%d dy=%d dur=%lums -> page=%s moved=%d\n",
                  dirLabel, dx, dy, (unsigned long)durMs, uiPageName(g_uiPageMode), moved ? 1 : 0);
    return;
  }

  // AUX/WIKI ergonomics: tap on current news advances to next item.
  if (isTap && uiPageIsFeedDeck(g_uiPageMode) &&
      (lvglFeedNewsContainsPoint(g_touchStartX, g_touchStartY) ||
       lvglAuxHeroContainsPoint(g_touchStartX, g_touchStartY))) {
#if TEST_WIFI && RSS_ENABLED
    const bool moved = (g_uiPageMode == UI_PAGE_WIKI) ? wikiAdvanceToNextItem() : rssAdvanceToNextItem();
    RssState &content = (g_uiPageMode == UI_PAGE_WIKI) ? g_wiki : g_rss;
    const char *tag = (g_uiPageMode == UI_PAGE_WIKI) ? "wiki" : "rss";
    if (moved) {
      g_uiNeedsRedraw = true;
      g_touchAwaitRelease = true;
      g_touchReleaseStartMs = 0;
      Serial.printf("[TOUCH] aux-news-tap -> %s %u/%u\n",
                    tag,
                    (unsigned)(content.currentIndex + 1),
                    (unsigned)content.itemCount);
      return;
    }
#endif
  }

  // In AUX/WIKI, ignore neutral taps that are not on actionable regions.
  if (isTap && uiPageIsFeedDeck(g_uiPageMode)) return;

  // Fallback gesture: quick tap anywhere on left panel toggles clock mode.
  if (isTap &&
      g_uiPageMode == UI_PAGE_HOME &&
      g_touchStartX < (canvasWidth() - DISPLAY_WEATHER_PANEL_W)) {
    toggleClockMode();
    Serial.printf("[TOUCH] tap x=%d y=%d -> mode=%s\n",
                  g_touchStartX, g_touchStartY, uiClockModeName(g_uiClockMode));
  }
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
  if (minuteKey != g_bellazioLastMinuteKey) {
    size_t nextLeadIdx = (size_t)(bellazioMixSeed(minuteKey ^ 0xA53C9E5Du) % (uint32_t)leadCount);
    size_t nextCloserIdx = (size_t)(bellazioMixSeed(minuteKey ^ 0x61C88647u) % (uint32_t)closerCount);
    if (leadCount > 1 && nextLeadIdx == g_bellazioLastLeadIdx) {
      nextLeadIdx = (nextLeadIdx + 1 + (minuteKey % (leadCount - 1))) % leadCount;
    }
    if (closerCount > 1 && nextCloserIdx == g_bellazioLastCloserIdx) {
      nextCloserIdx = (nextCloserIdx + 1 + (minuteKey % (closerCount - 1))) % closerCount;
    }
    g_bellazioLastLeadIdx = (uint8_t)nextLeadIdx;
    g_bellazioLastCloserIdx = (uint8_t)nextCloserIdx;
    g_bellazioLastMinuteKey = minuteKey;
  }
  const char* lead = kBellazioLeads[g_bellazioLastLeadIdx % leadCount];
  const BellazioCloser* closer = &kBellazioClosers[g_bellazioLastCloserIdx % closerCount];
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

// --- Language dispatcher ---

static void composeWordClockSentenceActive(const tm &timeinfo, char *out, size_t outLen) {
  if      (strcmp(g_wordClockLang, "tlh")  == 0) composeWordClockSentenceTlh (timeinfo, out, outLen);
  else if (strcmp(g_wordClockLang, "en")   == 0) composeWordClockSentenceEn  (timeinfo, out, outLen);
  else if (strcmp(g_wordClockLang, "fr")   == 0) composeWordClockSentenceFr  (timeinfo, out, outLen);
  else if (strcmp(g_wordClockLang, "de")   == 0) composeWordClockSentenceDe  (timeinfo, out, outLen);
  else if (strcmp(g_wordClockLang, "es")   == 0) composeWordClockSentenceEs  (timeinfo, out, outLen);
  else if (strcmp(g_wordClockLang, "pt")   == 0) composeWordClockSentencePt  (timeinfo, out, outLen);
  else if (strcmp(g_wordClockLang, "la")   == 0) composeWordClockSentenceLa  (timeinfo, out, outLen);
  else if (strcmp(g_wordClockLang, "eo")   == 0) composeWordClockSentenceEo  (timeinfo, out, outLen);

  else if (strcmp(g_wordClockLang, "l33t") == 0) composeWordClockSentenceL33t(timeinfo, out, outLen);
  else if (strcmp(g_wordClockLang, "sha")  == 0) composeWordClockSentenceSha (timeinfo, out, outLen);
  else if (strcmp(g_wordClockLang, "val")  == 0) composeWordClockSentenceVal (timeinfo, out, outLen);
  else if (strcmp(g_wordClockLang, "bellazio") == 0) composeWordClockSentenceBellazio(timeinfo, out, outLen);
  else                                            composeWordClockSentenceIt  (timeinfo, out, outLen);
}

static const UiStrings* activeUiStrings() {
  if (strcmp(g_wordClockLang, "en")   == 0) return &kUiLang_en;
  if (strcmp(g_wordClockLang, "fr")   == 0) return &kUiLang_fr;
  if (strcmp(g_wordClockLang, "de")   == 0) return &kUiLang_de;
  if (strcmp(g_wordClockLang, "es")   == 0) return &kUiLang_es;
  if (strcmp(g_wordClockLang, "pt")   == 0) return &kUiLang_pt;
  if (strcmp(g_wordClockLang, "la")   == 0) return &kUiLang_la;
  if (strcmp(g_wordClockLang, "eo")   == 0) return &kUiLang_eo;

  if (strcmp(g_wordClockLang, "tlh")  == 0) return &kUiLang_tlh;
  if (strcmp(g_wordClockLang, "l33t") == 0) return &kUiLang_l33t;
  if (strcmp(g_wordClockLang, "sha")  == 0) return &kUiLang_sha;
  if (strcmp(g_wordClockLang, "val")  == 0) return &kUiLang_val;
  if (strcmp(g_wordClockLang, "bellazio") == 0) return &kUiLang_bellazio;
  return &kUiLang_it;
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
static const lv_font_t* lvglFontTerminal() {
  return &scry_font_space_mono_16;
}

static const lv_font_t* lvglFontTerminalTiny() {
  return &scry_font_space_mono_12;
}

static const lv_font_t* lvglFontToxicTiny() {
  return &scry_font_delius_unicase_12;
}

static const lv_font_t* lvglFontToxicSmall() {
  return &scry_font_delius_unicase_16;
}

static const lv_font_t* lvglFontToxicBody() {
  return &scry_font_delius_unicase_20;
}

static const lv_font_t* lvglFontToxicTitle() {
  return &scry_font_delius_unicase_24;
}

static const lv_font_t* lvglFontToxicBig() {
  return &scry_font_delius_unicase_28;
}

static const lv_font_t* lvglFontToxicClock() {
  return &scry_font_delius_unicase_28;
}

static const lv_font_t* lvglFontTitle() {
  if (lvglThemeIsCyberpunk() || lvglThemeIsTokyoTransit() || lvglThemeIsMinimalBrutalistMono()) return lvglFontTerminal();
  if (lvglThemeIsToxicCandy()) return lvglFontToxicTitle();
#if defined(LV_FONT_MONTSERRAT_30) && LV_FONT_MONTSERRAT_30
  return &lv_font_montserrat_30;
#elif defined(LV_FONT_MONTSERRAT_28) && LV_FONT_MONTSERRAT_28
  return &lv_font_montserrat_28;
#else
  return LV_FONT_DEFAULT;
#endif
}

static const lv_font_t* lvglFontBody() {
  if (lvglThemeIsCyberpunk() || lvglThemeIsTokyoTransit() || lvglThemeIsMinimalBrutalistMono()) return lvglFontTerminal();
  if (lvglThemeIsToxicCandy()) return lvglFontToxicBody();
#if defined(LV_FONT_MONTSERRAT_24) && LV_FONT_MONTSERRAT_24
  return &lv_font_montserrat_24;
#elif defined(LV_FONT_MONTSERRAT_22) && LV_FONT_MONTSERRAT_22
  return &lv_font_montserrat_22;
#elif defined(LV_FONT_MONTSERRAT_20) && LV_FONT_MONTSERRAT_20
  return &lv_font_montserrat_20;
#else
  return LV_FONT_DEFAULT;
#endif
}

static const lv_font_t* lvglFontSmall() {
  if (lvglThemeIsCyberpunk() || lvglThemeIsTokyoTransit() || lvglThemeIsMinimalBrutalistMono()) return lvglFontTerminal();
  if (lvglThemeIsToxicCandy()) return lvglFontToxicSmall();
#if defined(LV_FONT_MONTSERRAT_18) && LV_FONT_MONTSERRAT_18
  return &lv_font_montserrat_18;
#elif defined(LV_FONT_MONTSERRAT_16) && LV_FONT_MONTSERRAT_16
  return &lv_font_montserrat_16;
#else
  return LV_FONT_DEFAULT;
#endif
}

static const lv_font_t* lvglFontTiny() {
  if (lvglThemeIsCyberpunk() || lvglThemeIsTokyoTransit() || lvglThemeIsMinimalBrutalistMono()) return lvglFontTerminalTiny();
  if (lvglThemeIsToxicCandy()) return lvglFontToxicTiny();
#if defined(LV_FONT_MONTSERRAT_14) && LV_FONT_MONTSERRAT_14
  return &lv_font_montserrat_14;
#else
  return lvglFontSmall();
#endif
}

static const lv_font_t* lvglFontMini() {
  if (lvglThemeIsCyberpunk() || lvglThemeIsTokyoTransit() || lvglThemeIsMinimalBrutalistMono()) return lvglFontTerminal();
  if (lvglThemeIsToxicCandy()) return lvglFontToxicSmall();
#if defined(LV_FONT_MONTSERRAT_16) && LV_FONT_MONTSERRAT_16
  return &lv_font_montserrat_16;
#else
  return lvglFontTiny();
#endif
}

static const lv_font_t* lvglFontMono() {
  return lvglFontTerminal();
}

static const lv_font_t* lvglFontMonoTiny() {
  return lvglFontTerminalTiny();
}

static const lv_font_t* lvglFontScreenSaverBalloonText() {
  if (lvglThemeIsToxicCandy()) return &scry_font_delius_unicase_18;
  return &scry_font_space_mono_18;
}

static const lv_font_t* lvglFontScreenSaverFooterText() {
  if (lvglThemeIsToxicCandy()) return &scry_font_delius_unicase_24;
  return &scry_font_space_mono_24;
}

static const lv_font_t* lvglFontScreenSaverTail() {
  if (lvglThemeIsToxicCandy()) return &scry_font_delius_unicase_16;
  return &scry_font_space_mono_16;
}

static const lv_font_t* lvglFontInfoBody() {
  if (lvglThemeIsCyberpunk() || lvglThemeIsTokyoTransit() || lvglThemeIsMinimalBrutalistMono()) return lvglFontTerminalTiny();
  if (lvglThemeIsToxicCandy()) return lvglFontToxicSmall();
#if defined(LV_FONT_MONTSERRAT_16) && LV_FONT_MONTSERRAT_16
  return &lv_font_montserrat_16;
#elif defined(LV_FONT_MONTSERRAT_14) && LV_FONT_MONTSERRAT_14
  return &lv_font_montserrat_14;
#else
  return lvglFontSmall();
#endif
}

static const lv_font_t* lvglFontMeta() {
  if (lvglThemeIsCyberpunk() || lvglThemeIsTokyoTransit() || lvglThemeIsMinimalBrutalistMono()) return lvglFontTerminal();
  if (lvglThemeIsToxicCandy()) return lvglFontToxicBody();
#if defined(LV_FONT_MONTSERRAT_20) && LV_FONT_MONTSERRAT_20
  return &lv_font_montserrat_20;
#else
  return lvglFontSmall();
#endif
}

static const lv_font_t* lvglFontRssNews() {
  if (lvglThemeIsCyberpunk() || lvglThemeIsTokyoTransit() || lvglThemeIsMinimalBrutalistMono()) return lvglFontTerminal();
  if (lvglThemeIsToxicCandy()) return lvglFontToxicBody();
#if defined(LV_FONT_MONTSERRAT_22) && LV_FONT_MONTSERRAT_22
  return &lv_font_montserrat_22;
#elif defined(LV_FONT_MONTSERRAT_20) && LV_FONT_MONTSERRAT_20
  return &lv_font_montserrat_20;
#elif defined(LV_FONT_MONTSERRAT_18) && LV_FONT_MONTSERRAT_18
  return &lv_font_montserrat_18;
#else
  return lvglFontSmall();
#endif
}

static const lv_font_t* lvglFontClock() {
  if (lvglThemeIsCyberpunk() || lvglThemeIsTokyoTransit() || lvglThemeIsMinimalBrutalistMono()) return &scry_font_space_mono_32;
  if (lvglThemeIsToxicCandy()) return lvglFontToxicClock();
#if defined(LV_FONT_MONTSERRAT_38) && LV_FONT_MONTSERRAT_38
  return &lv_font_montserrat_38;
#elif defined(LV_FONT_MONTSERRAT_36) && LV_FONT_MONTSERRAT_36
  return &lv_font_montserrat_36;
#elif defined(LV_FONT_MONTSERRAT_32) && LV_FONT_MONTSERRAT_32
  return &lv_font_montserrat_32;
#elif defined(LV_FONT_MONTSERRAT_30) && LV_FONT_MONTSERRAT_30
  return &lv_font_montserrat_30;
#elif defined(LV_FONT_MONTSERRAT_28) && LV_FONT_MONTSERRAT_28
  return &lv_font_montserrat_28;
#elif defined(LV_FONT_MONTSERRAT_24) && LV_FONT_MONTSERRAT_24
  return &lv_font_montserrat_24;
#elif defined(LV_FONT_MONTSERRAT_22) && LV_FONT_MONTSERRAT_22
  return &lv_font_montserrat_22;
#else
  return lvglFontMeta();
#endif
}

static const lv_font_t* lvglFontBig() {
  if (lvglThemeIsCyberpunk() || lvglThemeIsTokyoTransit() || lvglThemeIsMinimalBrutalistMono()) return lvglFontTerminal();
  if (lvglThemeIsToxicCandy()) return lvglFontToxicBig();
#if defined(LV_FONT_MONTSERRAT_32) && LV_FONT_MONTSERRAT_32
  return &lv_font_montserrat_32;
#elif defined(LV_FONT_MONTSERRAT_30) && LV_FONT_MONTSERRAT_30
  return &lv_font_montserrat_30;
#else
  return lvglFontTitle();
#endif
}

static const lv_font_t* lvglFontTemp() {
  if (lvglThemeIsCyberpunk() || lvglThemeIsTokyoTransit() || lvglThemeIsMinimalBrutalistMono()) return lvglFontTerminal();
  if (lvglThemeIsToxicCandy()) return lvglFontToxicTitle();
#if defined(LV_FONT_MONTSERRAT_24) && LV_FONT_MONTSERRAT_24
  return &lv_font_montserrat_24;
#elif defined(LV_FONT_MONTSERRAT_22) && LV_FONT_MONTSERRAT_22
  return &lv_font_montserrat_22;
#elif defined(LV_FONT_MONTSERRAT_20) && LV_FONT_MONTSERRAT_20
  return &lv_font_montserrat_20;
#else
  return lvglFontTitle();
#endif
}

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
  if (lvglThemeIsCyberpunk() || lvglThemeIsTokyoTransit() || lvglThemeIsMinimalBrutalistMono()) {
    out[n++] = &scry_font_space_mono_32;
    if (n < cap) out[n++] = &scry_font_space_mono_28;
    if (n < cap) out[n++] = &scry_font_space_mono_24;
    if (n < cap) out[n++] = &scry_font_space_mono_20;
    if (n < cap) out[n++] = &scry_font_space_mono_16;
    if (n < cap) out[n++] = &scry_font_space_mono_12;
    return n;
  }
  if (lvglThemeIsToxicCandy()) {
    out[n++] = &scry_font_delius_unicase_28;
    if (n < cap) out[n++] = &scry_font_delius_unicase_24;
    if (n < cap) out[n++] = &scry_font_delius_unicase_20;
    if (n < cap) out[n++] = &scry_font_delius_unicase_16;
    if (n < cap) out[n++] = &scry_font_delius_unicase_12;
    return n;
  }
#if defined(LV_FONT_MONTSERRAT_38) && LV_FONT_MONTSERRAT_38
  out[n++] = &lv_font_montserrat_38;
#endif
#if defined(LV_FONT_MONTSERRAT_36) && LV_FONT_MONTSERRAT_36
  if (n < cap) out[n++] = &lv_font_montserrat_36;
#endif
#if defined(LV_FONT_MONTSERRAT_32) && LV_FONT_MONTSERRAT_32
  if (n < cap) out[n++] = &lv_font_montserrat_32;
#endif
#if defined(LV_FONT_MONTSERRAT_30) && LV_FONT_MONTSERRAT_30
  if (n < cap) out[n++] = &lv_font_montserrat_30;
#endif
#if defined(LV_FONT_MONTSERRAT_28) && LV_FONT_MONTSERRAT_28
  if (n < cap) out[n++] = &lv_font_montserrat_28;
#endif
#if defined(LV_FONT_MONTSERRAT_24) && LV_FONT_MONTSERRAT_24
  if (n < cap) out[n++] = &lv_font_montserrat_24;
#endif
#if defined(LV_FONT_MONTSERRAT_22) && LV_FONT_MONTSERRAT_22
  if (n < cap) out[n++] = &lv_font_montserrat_22;
#endif
#if defined(LV_FONT_MONTSERRAT_20) && LV_FONT_MONTSERRAT_20
  if (n < cap) out[n++] = &lv_font_montserrat_20;
#endif
  if (n == 0) out[n++] = lvglFontMeta();
  return n;
}

static void lvglApplyClockSentenceAutoFit(const char *text) {
  if (!g_lvglClockL1 || !g_lvglClockBlock || !g_lvglClockHeader || !text) return;
  const int16_t blockH = lv_obj_get_height(g_lvglClockBlock);
  const int16_t headerH = lv_obj_get_height(g_lvglClockHeader);
  const int16_t maxTextH = (blockH - headerH) - 6;
  const lv_font_t *fonts[10];
  const uint8_t count = lvglCollectClockFonts(fonts, (uint8_t)(sizeof(fonts) / sizeof(fonts[0])));
  if (count == 0) return;

  const lv_font_t *chosen = fonts[count - 1];
  lv_coord_t chosenLineSpace = lvglClockLineSpaceForFont(chosen);

  for (uint8_t i = 0; i < count; ++i) {
    const lv_font_t *f = fonts[i];
    const lv_coord_t ls = lvglClockLineSpaceForFont(f);
    lv_obj_set_style_text_font(g_lvglClockL1, f, 0);
    lv_obj_set_style_text_line_space(g_lvglClockL1, ls, 0);
    lv_label_set_text(g_lvglClockL1, text);
    lvglCenterClockSentenceLabel();
    lv_obj_update_layout(g_lvglClockL1);
    const int16_t textH = lv_obj_get_height(g_lvglClockL1);
    if (textH <= maxTextH) {
      chosen = f;
      chosenLineSpace = ls;
      break;
    }
  }

  lv_obj_set_style_text_font(g_lvglClockL1, chosen, 0);
  lv_obj_set_style_text_line_space(g_lvglClockL1, chosenLineSpace, 0);
  lv_label_set_text(g_lvglClockL1, text);
  lvglCenterClockSentenceLabel();
}

static void lvglApplyThemeFonts() {
  if (g_lvglInfoTitle) lv_obj_set_style_text_font(g_lvglInfoTitle, lvglFontSmall(), 0);
  if (g_lvglInfoEndpoint) lv_obj_set_style_text_font(g_lvglInfoEndpoint, lvglFontSmall(), 0);
  if (g_lvglInfoBodyLeft) lv_obj_set_style_text_font(g_lvglInfoBodyLeft, lvglFontInfoBody(), 0);

  if (g_lvglClockDate) lv_obj_set_style_text_font(g_lvglClockDate, lvglFontSmall(), 0);
  if (g_lvglClockL1) lv_obj_set_style_text_font(g_lvglClockL1, lvglFontClock(), 0);
  if (g_lvglClockL2) lv_obj_set_style_text_font(g_lvglClockL2, lvglFontTitle(), 0);
  if (g_lvglClockL3) lv_obj_set_style_text_font(g_lvglClockL3, lvglFontTitle(), 0);
  if (g_lvglCity) lv_obj_set_style_text_font(g_lvglCity, lvglFontSmall(), 0);
  if (g_lvglSun) lv_obj_set_style_text_font(g_lvglSun, lvglFontSmall(), 0);
  if (g_lvglTemp) lv_obj_set_style_text_font(g_lvglTemp, lvglFontTemp(), 0);
  if (g_lvglGlyph) lv_obj_set_style_text_font(g_lvglGlyph, lvglFontBig(), 0);
  if (g_lvglDesc) lv_obj_set_style_text_font(g_lvglDesc, lvglFontMeta(), 0);
  if (g_lvglHumidity) lv_obj_set_style_text_font(g_lvglHumidity, lvglFontMini(), 0);
  if (g_lvglWind) lv_obj_set_style_text_font(g_lvglWind, lvglFontTiny(), 0);
  if (g_lvglForecastNow) lv_obj_set_style_text_font(g_lvglForecastNow, lvglFontSmall(), 0);
  if (g_lvglForecastTomorrow) lv_obj_set_style_text_font(g_lvglForecastTomorrow, lvglFontTiny(), 0);

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
      if (d->qrHint)          lv_obj_set_style_text_font(d->qrHint,          lvglFontTiny(),    0);
    }
  }

#if SCREENSAVER_ENABLED
  if (g_lvglScreenSaverSky) lv_obj_set_style_text_font(g_lvglScreenSaverSky, lvglFontMono(), 0);
  for (uint8_t r = 0; r < kSaverSkyRowsMax; ++r) {
    for (uint8_t s = 0; s < kSaverStarsPerRow; ++s) {
      if (!g_lvglScreenSaverStarObj[r][s]) continue;
      lv_obj_set_style_text_font(g_lvglScreenSaverStarObj[r][s], lvglFontMonoTiny(), 0);
    }
  }
  if (g_lvglScreenSaverField) lv_obj_set_style_text_font(g_lvglScreenSaverField, lvglFontMonoTiny(), 0);
  if (g_lvglScreenSaverCow) lv_obj_set_style_text_font(g_lvglScreenSaverCow, lvglFontMonoTiny(), 0);
  if (g_lvglScreenSaverBalloon) lv_obj_set_style_text_font(g_lvglScreenSaverBalloon, lvglFontScreenSaverBalloonText(), 0);
  if (g_lvglScreenSaverBalloonTail) lv_obj_set_style_text_font(g_lvglScreenSaverBalloonTail, lvglFontScreenSaverTail(), 0);
  if (g_lvglScreenSaverFooter) lv_obj_set_style_text_font(g_lvglScreenSaverFooter, lvglFontScreenSaverFooterText(), 0);
#endif
  if (g_lvglClockL1) lvglApplyClockSentenceAutoFit(lv_label_get_text(g_lvglClockL1));
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
  if (!g_lvglClockL1 || !g_lvglClockBlock || !g_lvglClockHeader) return;
  const int16_t blockW = lv_obj_get_width(g_lvglClockBlock);
  const int16_t blockH = lv_obj_get_height(g_lvglClockBlock);
  const int16_t headerH = lv_obj_get_height(g_lvglClockHeader);
  constexpr int16_t kSidePad = 8;
  const int16_t labelW = blockW - (kSidePad * 2);
  if (labelW < 24) return;
  lv_obj_set_width(g_lvglClockL1, labelW);
  lv_obj_set_x(g_lvglClockL1, kSidePad);
  lv_obj_update_layout(g_lvglClockL1);
  const int16_t textH = lv_obj_get_height(g_lvglClockL1);
  const int16_t bodyH = blockH - headerH;
  int16_t y = headerH + ((bodyH - textH) / 2);
  if (y < (headerH + 2)) y = headerH + 2;
  lv_obj_set_y(g_lvglClockL1, y);
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
  if (st == WL_CONNECTED && g_wifiConnected) return false;
  if (st == WL_DISCONNECTED || st == WL_CONNECT_FAILED || st == WL_CONNECTION_LOST) return true;
  if (st == WL_IDLE_STATUS || st == WL_SCAN_COMPLETED || st == WL_NO_SSID_AVAIL) return true;
  if (g_wifiReconnectAttemptActive) return true;
  if (g_wifiEverConnected) return true;
  const uint32_t now = millis();
  if (g_wifiLastDisconnectMs > 0 && (now - g_wifiLastDisconnectMs) < 20000UL) return true;
#endif
  return false;
}

static void lvglUpdateWiFiBars(bool force) {
  if (!g_lvglClockWiFiBars[0]) return;

  const UiThemeLvglTokens &t = activeUiTheme().lvgl;
  const lv_color_t kBarOff = lv_color_hex(t.wifiBarOff);
  const lv_color_t kBarOn = lv_color_hex(t.wifiBarOn);
  const lv_color_t kBarWave = lv_color_hex(t.wifiBarWave);
  uint8_t mask = 0;
  uint8_t waveMask = 0;

#if TEST_WIFI || TEST_NTP
  const wl_status_t st = WiFi.status();
  const bool connected = (st == WL_CONNECTED) && g_wifiConnected;
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
  if (!force && styleKey == g_lvglClockWiFiMask) return;
  g_lvglClockWiFiMask = styleKey;

  for (uint8_t i = 0; i < 4; ++i) {
    lv_obj_t *bar = g_lvglClockWiFiBars[i];
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
  if (g_wifiConnected && content.valid && content.itemCount > 1 && content.lastRotateMs != 0 &&
      !d.qrModalOpen && (now - content.lastRotateMs) >= RSS_ROTATE_MS) {
    content.currentIndex = (uint8_t)((content.currentIndex + 1) % content.itemCount);
    content.lastRotateMs = now;
    force = true;
  }
#endif

  char title3[260];
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
  if (!g_wifiConnected) {
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
  snprintf(status, sizeof(status), g_wifiConnected ? "WiFi" : "OFF");
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
          if (!item.shortReady) {
            if (isWiki) (void)wikiTryShortenUrl((uint8_t)showIndex);
            else        (void)rssTryShortenUrl((uint8_t)showIndex);
          }
          if (item.shortReady && item.shortLink[0]) url = item.shortLink;
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
            lv_label_set_text(d.qrHint, activeUiStrings()->touchToCloseAnywhere);
            lv_obj_clear_flag(d.qrHint, LV_OBJ_FLAG_HIDDEN);
          }
          if (d.status) lv_label_set_text(d.status, status);
        } else {
          lv_obj_add_flag(d.qr, LV_OBJ_FLAG_HIDDEN);
          if (d.qrHint) {
            lv_label_set_text(d.qrHint, activeUiStrings()->generatingQr);
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

static bool lvglApplyPageDrag(int16_t dragDx) {
  if (!g_lvglInfoRoot || !g_lvglHomeRoot || !g_lvglAuxRoot || !g_lvglWikiRoot) return false;
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

  lv_anim_del(g_lvglInfoRoot, lvglSetObjXAnim);
  lv_anim_del(g_lvglHomeRoot, lvglSetObjXAnim);
  lv_anim_del(g_lvglAuxRoot, lvglSetObjXAnim);
  lv_anim_del(g_lvglWikiRoot, lvglSetObjXAnim);
  g_lvglPageAnimUntilMs = 0;
  g_lvglPageDragActive = true;

  lv_obj_clear_flag(g_lvglInfoRoot, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_lvglHomeRoot, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_lvglAuxRoot, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_lvglWikiRoot, LV_OBJ_FLAG_HIDDEN);

  lv_obj_set_pos(g_lvglInfoRoot, (lv_coord_t)(((0 - cur) * w) + dx), 0);
  lv_obj_set_pos(g_lvglHomeRoot, (lv_coord_t)(((1 - cur) * w) + dx), 0);
  lv_obj_set_pos(g_lvglAuxRoot, (lv_coord_t)(((2 - cur) * w) + dx), 0);
  lv_obj_set_pos(g_lvglWikiRoot, (lv_coord_t)(((3 - cur) * w) + dx), 0);
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
  if (!g_lvglInfoRoot || !g_lvglHomeRoot || !g_lvglAuxRoot || !g_lvglWikiRoot) return;

  if (g_lvglDoomRoot) {
    if (g_uiPageMode == UI_PAGE_DOOM) lv_obj_clear_flag(g_lvglDoomRoot, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(g_lvglDoomRoot, LV_OBJ_FLAG_HIDDEN);
  }

  if (g_uiPageMode == UI_PAGE_DOOM) {
    lv_obj_add_flag(g_lvglInfoRoot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_lvglHomeRoot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_lvglAuxRoot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_lvglWikiRoot, LV_OBJ_FLAG_HIDDEN);
    g_lvglPageDragActive = false;
    g_lvglPageAnimUntilMs = 0;
    return;
  }

  const int16_t w = canvasWidth();
  const int8_t cur = uiPageOrdinal(g_uiPageMode);
  if (cur < 0) return;
  const int32_t infoTargetX = (0 - cur) * w;
  const int32_t homeTargetX = (1 - cur) * w;
  const int32_t auxTargetX  = (2 - cur) * w;
  const int32_t wikiTargetX = (3 - cur) * w;
  const uint32_t now = millis();

  lv_obj_clear_flag(g_lvglInfoRoot, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_lvglHomeRoot, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_lvglAuxRoot, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_lvglWikiRoot, LV_OBJ_FLAG_HIDDEN);

  if (!animate) {
    if (g_lvglPageDragActive) return;
    if (now < g_lvglPageAnimUntilMs) return;
    // Only update positions if they actually changed — avoids constant LVGL invalidation.
    const bool infoOk = (lv_obj_get_x(g_lvglInfoRoot) == (lv_coord_t)infoTargetX);
    const bool homeOk = (lv_obj_get_x(g_lvglHomeRoot) == (lv_coord_t)homeTargetX);
    const bool auxOk  = (lv_obj_get_x(g_lvglAuxRoot)  == (lv_coord_t)auxTargetX);
    const bool wikiOk = (lv_obj_get_x(g_lvglWikiRoot) == (lv_coord_t)wikiTargetX);
    if (infoOk && homeOk && auxOk && wikiOk) return;
    lv_anim_del(g_lvglInfoRoot, lvglSetObjXAnim);
    lv_anim_del(g_lvglHomeRoot, lvglSetObjXAnim);
    lv_anim_del(g_lvglAuxRoot, lvglSetObjXAnim);
    lv_anim_del(g_lvglWikiRoot, lvglSetObjXAnim);
    if (!infoOk) lv_obj_set_pos(g_lvglInfoRoot, (lv_coord_t)infoTargetX, 0);
    if (!homeOk) lv_obj_set_pos(g_lvglHomeRoot, (lv_coord_t)homeTargetX, 0);
    if (!auxOk)  lv_obj_set_pos(g_lvglAuxRoot,  (lv_coord_t)auxTargetX, 0);
    if (!wikiOk) lv_obj_set_pos(g_lvglWikiRoot, (lv_coord_t)wikiTargetX, 0);
    if (abs(infoTargetX) >= w) lv_obj_add_flag(g_lvglInfoRoot, LV_OBJ_FLAG_HIDDEN);
    if (abs(homeTargetX) >= w) lv_obj_add_flag(g_lvglHomeRoot, LV_OBJ_FLAG_HIDDEN);
    if (abs(auxTargetX)  >= w) lv_obj_add_flag(g_lvglAuxRoot,  LV_OBJ_FLAG_HIDDEN);
    if (abs(wikiTargetX) >= w) lv_obj_add_flag(g_lvglWikiRoot, LV_OBJ_FLAG_HIDDEN);
    g_lvglPageDragActive = false;
    return;
  }

  constexpr uint16_t kSlideMs = 250;
  const int32_t infoFromX = lv_obj_get_x(g_lvglInfoRoot);
  const int32_t homeFromX = lv_obj_get_x(g_lvglHomeRoot);
  const int32_t auxFromX  = lv_obj_get_x(g_lvglAuxRoot);
  const int32_t wikiFromX = lv_obj_get_x(g_lvglWikiRoot);
  lvglStartSlideAnim(g_lvglInfoRoot, infoFromX, infoTargetX, kSlideMs);
  lvglStartSlideAnim(g_lvglHomeRoot, homeFromX, homeTargetX, kSlideMs);
  lvglStartSlideAnim(g_lvglAuxRoot,  auxFromX,  auxTargetX,  kSlideMs);
  lvglStartSlideAnim(g_lvglWikiRoot, wikiFromX, wikiTargetX, kSlideMs);
  g_lvglPageDragActive = false;
  g_lvglPageAnimUntilMs = now + kSlideMs + 30;
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

static void formatDateActive(const tm &timeinfo, char *out, size_t outLen) {
  if      (strcmp(g_wordClockLang, "en")   == 0) formatDateEn  (timeinfo, out, outLen);
  else if (strcmp(g_wordClockLang, "fr")   == 0) formatDateFr  (timeinfo, out, outLen);
  else if (strcmp(g_wordClockLang, "de")   == 0) formatDateDe  (timeinfo, out, outLen);
  else if (strcmp(g_wordClockLang, "es")   == 0) formatDateEs  (timeinfo, out, outLen);
  else if (strcmp(g_wordClockLang, "pt")   == 0) formatDatePt  (timeinfo, out, outLen);
  else if (strcmp(g_wordClockLang, "la")   == 0) formatDateLa  (timeinfo, out, outLen);
  else if (strcmp(g_wordClockLang, "eo")   == 0) formatDateEo  (timeinfo, out, outLen);

  else if (strcmp(g_wordClockLang, "tlh")  == 0) formatDateTlh (timeinfo, out, outLen);
  else if (strcmp(g_wordClockLang, "l33t") == 0) formatDateL33t(timeinfo, out, outLen);
  else if (strcmp(g_wordClockLang, "sha")  == 0) formatDateSha (timeinfo, out, outLen);
  else if (strcmp(g_wordClockLang, "val")  == 0) formatDateVal (timeinfo, out, outLen);
  else if (strcmp(g_wordClockLang, "bellazio") == 0) formatDateBellazio(timeinfo, out, outLen);
  else                                            formatDateIt  (timeinfo, out, outLen);
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
  if (!g_lvglCity) return;

  char shortCity[32];
  char fullCity[48];
  formatCityLabel(rawCity, shortCity, sizeof(shortCity));
  formatCityLabelFull(rawCity, fullCity, sizeof(fullCity));

  const uint32_t now = millis();
  const bool cityChanged = strncmp(g_lvglCityRawLast, rawCity ? rawCity : "", sizeof(g_lvglCityRawLast) - 1) != 0;
  if (cityChanged) {
    copyStringSafe(g_lvglCityRawLast, sizeof(g_lvglCityRawLast), rawCity ? rawCity : "");
    g_lvglCityTickerScroll = false;
    g_lvglCityTickerEndMs = 0;
    g_lvglCityTickerNextMs = now + 10000UL;
    force = true;
  }

  if (g_lvglCityTickerScroll) {
    if (now >= g_lvglCityTickerEndMs) {
      lv_label_set_long_mode(g_lvglCity, LV_LABEL_LONG_DOT);
      lv_label_set_text(g_lvglCity, shortCity);
      lvglForceLabelVisible(g_lvglCity);
      g_lvglCityTickerScroll = false;
      g_lvglCityTickerNextMs = now + 10000UL;
      return;
    }
    if (force) {
      lv_label_set_long_mode(g_lvglCity, LV_LABEL_LONG_SCROLL_CIRCULAR);
      lv_label_set_text(g_lvglCity, fullCity);
      lvglForceLabelVisible(g_lvglCity);
    }
    return;
  }

  if (force) {
    lv_label_set_long_mode(g_lvglCity, LV_LABEL_LONG_DOT);
    lv_label_set_text(g_lvglCity, shortCity);
    lvglForceLabelVisible(g_lvglCity);
  }

  if (now < g_lvglCityTickerNextMs) return;
  if (strcmp(fullCity, shortCity) == 0) {
    g_lvglCityTickerNextMs = now + 10000UL;
    return;
  }

  lv_label_set_long_mode(g_lvglCity, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_label_set_text(g_lvglCity, fullCity);
  lvglForceLabelVisible(g_lvglCity);

  uint32_t showMs = 2600UL + (uint32_t)strlen(fullCity) * 220UL;
  if (showMs > 6500UL) showMs = 6500UL;
  g_lvglCityTickerScroll = true;
  g_lvglCityTickerEndMs = now + showMs;
}

static void lvglUpdateInfoPanel(bool force) {
  (void)force;
  if (!g_lvglInfoTitle || !g_lvglInfoEndpoint || !g_lvglInfoBodyLeft || !g_lvglInfoBodyRight) return;

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
  const bool wifiOk = (st == WL_CONNECTED) && g_wifiConnected;
  const bool setupApOk = g_wifiSetupApActive;
  snprintf(wifiBuf, sizeof(wifiBuf), "%s", wlStatusToStr(st));
  if (wifiOk) {
    snprintf(ipBuf, sizeof(ipBuf), "%s", WiFi.localIP().toString().c_str());
    snprintf(macBuf, sizeof(macBuf), "%s", WiFi.macAddress().c_str());
    snprintf(wifiBuf, sizeof(wifiBuf), "OK %ddBm", WiFi.RSSI());
    copyStringSafe(ssidBuf, sizeof(ssidBuf), WiFi.SSID().c_str());
  } else if (g_lastWifiDiscReason >= 0) {
    snprintf(wifiBuf, sizeof(wifiBuf), "DISC %d", g_lastWifiDiscReason);
  }
#if WEB_CONFIG_ENABLED
  if (wifiOk && g_webConfigServerStarted) {
    snprintf(webUrlBuf, sizeof(webUrlBuf), "http://%s:%u", ipBuf, (unsigned)WEB_CONFIG_PORT);
    snprintf(endpointBuf, sizeof(endpointBuf), "%s:%u", ipBuf, (unsigned)WEB_CONFIG_PORT);
  } else if (setupApOk) {
    wifiBuildSetupPortalUrl(webUrlBuf, sizeof(webUrlBuf));
    IPAddress apIp = WiFi.softAPIP();
    if ((uint32_t)apIp == 0U) apIp = IPAddress(192, 168, 4, 1);
    snprintf(ipBuf, sizeof(ipBuf), "%s", apIp.toString().c_str());
    snprintf(endpointBuf, sizeof(endpointBuf), "%s:%u", ipBuf, (unsigned)WEB_CONFIG_PORT);
    snprintf(wifiBuf, sizeof(wifiBuf), "AP SETUP");
    if (g_wifiSetupApSsid[0]) copyStringSafe(ssidBuf, sizeof(ssidBuf), g_wifiSetupApSsid);
  }
#endif
#endif

#if TEST_BATTERY
  if (g_battHasSample) {
    char barBuf[16];
    batteryBarsForPercent(g_battPercent, barBuf, sizeof(barBuf));
    const char *levelColor = batteryLevelColorHex(g_battPercent);
    snprintf(pwrBuf, sizeof(pwrBuf), "%s %d%%", batteryPowerModeText(), g_battPercent);
    snprintf(pwrSourceBuf, sizeof(pwrSourceBuf), "%s", batteryPowerSourceText(millis()));
    if (g_battChargingLikely) {
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
  snprintf(ntpBuf, sizeof(ntpBuf), "%s", g_ntpSynced ? "SYNCED" : "WAIT");
#endif

  if (strcmp(endpointBuf, "--:8080") == 0) {
    snprintf(endpointBuf, sizeof(endpointBuf), "%s:%u", ipBuf, (unsigned)WEB_CONFIG_PORT);
  }

  char leftCol[512];
  snprintf(leftCol, sizeof(leftCol),
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
  lv_label_set_text(g_lvglInfoTitle, infoTitleBuf);
  lv_label_set_text(g_lvglInfoEndpoint, endpointBuf);
  lv_label_set_text(g_lvglInfoBodyLeft, leftCol);
#if defined(LV_USE_QRCODE) && LV_USE_QRCODE
  if (g_lvglInfoWebQr) {
    if (strncmp(g_lvglInfoLastQrPayload, webUrlBuf, sizeof(g_lvglInfoLastQrPayload) - 1) != 0) {
      copyStringSafe(g_lvglInfoLastQrPayload, sizeof(g_lvglInfoLastQrPayload), webUrlBuf);
      lv_qrcode_update(g_lvglInfoWebQr, g_lvglInfoLastQrPayload, strlen(g_lvglInfoLastQrPayload));
    }
    lv_obj_invalidate(g_lvglInfoWebQr);
  }
#endif
  lvglForceLabelVisible(g_lvglInfoTitle);
  lvglForceLabelVisible(g_lvglInfoEndpoint);
  lvglForceLabelVisible(g_lvglInfoBodyLeft);
}

static void lvglDisplayFlushCb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
  (void)drv;
  if (!g_canvasBuf) {
    lv_disp_flush_ready(drv);
    return;
  }

  const int32_t w = area->x2 - area->x1 + 1;
  const int16_t cW = canvasWidth();
  const int16_t cH = canvasHeight();
  const lv_color_t *src = color_p;

  for (int32_t y = area->y1; y <= area->y2; ++y) {
    if (y < 0 || y >= cH) {
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
    const int32_t over = (area->x1 + ex) - (cW - 1);
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

  g_canvasDirty = true;
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

  d.card = lv_obj_create(root);
  lv_obj_set_size(d.card, cW, cH);
  lv_obj_set_pos(d.card, 0, 0);
  lv_obj_set_style_radius(d.card, kCardRadius, LV_PART_MAIN);
  lv_obj_set_style_bg_color(d.card, kPanelBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(d.card, kPanelBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(d.card, LV_GRAD_DIR_NONE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(d.card, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(d.card, 0, LV_PART_MAIN);
  lv_obj_set_style_clip_corner(d.card, false, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(d.card, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(d.card, 0, LV_PART_MAIN);
  lv_obj_clear_flag(d.card, LV_OBJ_FLAG_SCROLLABLE);

  constexpr int16_t kDeckHeaderH = 30;
  const int16_t cardW = cW;
  const int16_t cardH = cH;

  d.header = lv_obj_create(d.card);
  lv_obj_set_size(d.header, cardW, kDeckHeaderH);
  lv_obj_set_pos(d.header, 0, 0);
  lv_obj_set_style_bg_color(d.header, kHeaderBlue, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(d.header, kHeaderBlue, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(d.header, LV_GRAD_DIR_NONE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(d.header, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(d.header, kCardRadius, LV_PART_MAIN);
  lv_obj_set_style_clip_corner(d.header, false, LV_PART_MAIN);
  lv_obj_set_style_border_width(d.header, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(d.header, 0, LV_PART_MAIN);
  lv_obj_clear_flag(d.header, LV_OBJ_FLAG_SCROLLABLE);

  d.headerFill = lv_obj_create(d.header);
  lv_obj_set_size(d.headerFill, cardW, 10);
  lv_obj_set_pos(d.headerFill, 0, kDeckHeaderH - 10);
  lv_obj_set_style_bg_color(d.headerFill, kHeaderBlue, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(d.headerFill, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(d.headerFill, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(d.headerFill, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(d.headerFill, 0, LV_PART_MAIN);
  lv_obj_clear_flag(d.headerFill, LV_OBJ_FLAG_SCROLLABLE);

  const int16_t btnW = 44;
  const int16_t btnH = 26;
  const int16_t btnGap = 6;
  const int16_t qrBtnX      = cardW - btnW - 8;
  const int16_t qrBtnY      = cardH - btnH - 8;
  const int16_t refreshBtnX = qrBtnX - btnW - btnGap;
  const int16_t nextFeedBtnX = refreshBtnX - btnW - btnGap;

  d.nextFeedBtn = lv_obj_create(d.card);
  lv_obj_set_size(d.nextFeedBtn, btnW, btnH);
  lv_obj_set_pos(d.nextFeedBtn, nextFeedBtnX, qrBtnY);
  lv_obj_set_style_bg_color(d.nextFeedBtn, lv_color_hex(0x7B63FF), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(d.nextFeedBtn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(d.nextFeedBtn, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(d.nextFeedBtn, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(d.nextFeedBtn, kButtonRadius, LV_PART_MAIN);
  lv_obj_set_style_pad_all(d.nextFeedBtn, 0, LV_PART_MAIN);
  lv_obj_clear_flag(d.nextFeedBtn, LV_OBJ_FLAG_SCROLLABLE);
  d.nextFeedBtnText = lv_label_create(d.nextFeedBtn);
  lv_obj_set_style_text_font(d.nextFeedBtnText, lvglFontTiny(), 0);
  lv_obj_set_style_text_color(d.nextFeedBtnText, lv_color_hex(0xF7F2FF), 0);
  lv_label_set_text(d.nextFeedBtnText, "NXT");
  lv_obj_center(d.nextFeedBtnText);
  lvglForceLabelVisible(d.nextFeedBtnText);

  d.refreshBtn = lv_obj_create(d.card);
  lv_obj_set_size(d.refreshBtn, btnW, btnH);
  lv_obj_set_pos(d.refreshBtn, refreshBtnX, qrBtnY);
  lv_obj_set_style_bg_color(d.refreshBtn, lv_color_hex(0x6FD8FF), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(d.refreshBtn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(d.refreshBtn, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(d.refreshBtn, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(d.refreshBtn, kButtonRadius, LV_PART_MAIN);
  lv_obj_set_style_pad_all(d.refreshBtn, 0, LV_PART_MAIN);
  lv_obj_clear_flag(d.refreshBtn, LV_OBJ_FLAG_SCROLLABLE);
  d.refreshBtnText = lv_label_create(d.refreshBtn);
  lv_obj_set_style_text_font(d.refreshBtnText, lvglFontTiny(), 0);
  lv_obj_set_style_text_color(d.refreshBtnText, lv_color_hex(0x113063), 0);
  lv_label_set_text(d.refreshBtnText, "SKIP");
  lv_obj_center(d.refreshBtnText);
  lvglForceLabelVisible(d.refreshBtnText);

  d.qrBtn = lv_obj_create(d.card);
  lv_obj_set_size(d.qrBtn, btnW, btnH);
  lv_obj_set_pos(d.qrBtn, qrBtnX, qrBtnY);
  lv_obj_set_style_bg_color(d.qrBtn, lv_color_hex(0xFFD34D), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(d.qrBtn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(d.qrBtn, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(d.qrBtn, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(d.qrBtn, kButtonRadius, LV_PART_MAIN);
  lv_obj_set_style_pad_all(d.qrBtn, 0, LV_PART_MAIN);
  lv_obj_clear_flag(d.qrBtn, LV_OBJ_FLAG_SCROLLABLE);
  d.qrBtnText = lv_label_create(d.qrBtn);
  lv_obj_set_style_text_font(d.qrBtnText, lvglFontTiny(), 0);
  lv_obj_set_style_text_color(d.qrBtnText, lv_color_hex(0x1E2F63), 0);
  lv_label_set_text(d.qrBtnText, "QR");
  lv_obj_center(d.qrBtnText);
  lvglForceLabelVisible(d.qrBtnText);

  d.title = lv_label_create(d.header);
  lv_obj_set_style_text_font(d.title, lvglFontSmall(), 0);
  lv_obj_set_style_text_color(d.title, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(d.title, LV_ALIGN_LEFT_MID, 12, -1);
  lv_label_set_text(d.title, isWiki ? "ScryBar Wiki" : "ScryBar RSS");
  lvglForceLabelVisible(d.title);

  if (!isWiki) {
    d.feedIcon = lv_label_create(d.header);
    lv_obj_add_flag(d.feedIcon, LV_OBJ_FLAG_HIDDEN);
  }

  d.status = lv_label_create(d.header);
  lv_obj_set_style_text_font(d.status, lvglFontTiny(), 0);
  lv_obj_set_style_text_color(d.status, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(d.status, LV_ALIGN_RIGHT_MID, -5, 0);
  lv_label_set_text(d.status, "SYNC");
  lvglForceLabelVisible(d.status);

  const int16_t leftPaneX      = 8;
  const int16_t leftPaneW      = cardW - (leftPaneX * 2);
  const int16_t sourceRowY     = kDeckHeaderH + 6;
  const int16_t sourceTextY    = kDeckHeaderH + 15;
  const int16_t sourceGap      = 2;
  const int16_t sourceBadgeSize = 35;
  const int16_t sourceRightEdge = cardW - 5;
  const int16_t sourceIconX =
      ((sourceRightEdge - sourceBadgeSize) < leftPaneX) ? leftPaneX : (sourceRightEdge - sourceBadgeSize);
  const int16_t sourceSiteX = leftPaneX;
  const int16_t sourceTextTotalW = sourceIconX - sourceSiteX - sourceGap;
  int16_t sourceSiteW = sourceTextTotalW;
  if (sourceSiteW < 40) sourceSiteW = 40;

  d.sourceBadge = lv_obj_create(d.card);
  lv_obj_set_size(d.sourceBadge, sourceBadgeSize, sourceBadgeSize);
  lv_obj_set_pos(d.sourceBadge, sourceIconX, sourceRowY);
  lv_obj_set_style_bg_color(d.sourceBadge, lv_color_hex(0x2B468E), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(d.sourceBadge, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(d.sourceBadge, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(d.sourceBadge, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(d.sourceBadge, kBadgeRadius, LV_PART_MAIN);
  lv_obj_set_style_pad_all(d.sourceBadge, 0, LV_PART_MAIN);
  lv_obj_clear_flag(d.sourceBadge, LV_OBJ_FLAG_SCROLLABLE);

  d.sourceBadgeText = lv_label_create(d.sourceBadge);
  lv_obj_set_style_text_font(d.sourceBadgeText, lvglFontTiny(), 0);
  lv_obj_set_style_text_color(d.sourceBadgeText, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text(d.sourceBadgeText, isWiki ? "W" : "WEB");
  lv_obj_center(d.sourceBadgeText);
  lvglForceLabelVisible(d.sourceBadgeText);

  d.sourceSite = lv_label_create(d.card);
  lv_obj_set_style_text_font(d.sourceSite, lvglFontMeta(), 0);
  lv_obj_set_style_text_color(d.sourceSite, lv_color_hex(0xEAF0FF), 0);
  lv_label_set_long_mode(d.sourceSite, LV_LABEL_LONG_DOT);
  lv_obj_set_size(d.sourceSite, sourceSiteW, 26);
  lv_obj_set_pos(d.sourceSite, sourceSiteX, sourceTextY);
  lv_obj_set_style_text_align(d.sourceSite, LV_TEXT_ALIGN_LEFT, 0);
  lv_label_set_text(d.sourceSite, isWiki ? "Wiki  --/-- --:--" : "RSS  --/-- --:--");
  lvglForceLabelVisible(d.sourceSite);

  d.news = lv_label_create(d.card);
  lv_obj_set_style_text_font(d.news, lvglFontRssNews(), 0);
  lv_obj_set_style_text_color(d.news, lv_color_hex(0xEAF0FF), 0);
  lv_obj_set_style_text_line_space(d.news, 3, 0);
  lv_label_set_long_mode(d.news, LV_LABEL_LONG_WRAP);
  const int16_t newsY      = sourceRowY + sourceBadgeSize - 1;
  const int16_t newsBottom = qrBtnY - 14;
  int16_t newsH = newsBottom - newsY;
  if (newsH < 44) newsH = 44;
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
  lv_obj_set_pos(d.meta, 188, 4);
  lv_label_set_text(d.meta, "Fetch --/-- --:--");
  lvglForceLabelVisible(d.meta);

#if defined(LV_USE_QRCODE) && LV_USE_QRCODE
  const int16_t qrOverlayH = cardH;
  d.qrOverlay = lv_obj_create(d.card);
  lv_obj_set_size(d.qrOverlay, cardW, qrOverlayH);
  lv_obj_set_pos(d.qrOverlay, 0, 0);
  lv_obj_set_style_bg_color(d.qrOverlay, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(d.qrOverlay, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(d.qrOverlay, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(d.qrOverlay, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(d.qrOverlay, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(d.qrOverlay, 0, LV_PART_MAIN);
  lv_obj_clear_flag(d.qrOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(d.qrOverlay, LV_OBJ_FLAG_HIDDEN);

  int16_t qrSize = qrOverlayH - 4;
  if (qrSize > (cardW - 4)) qrSize = cardW - 4;
  if (qrSize < 90) qrSize = 90;
  const lv_color_t qrDark  = lv_color_hex(theme.auxQrDark);
  const lv_color_t qrLight = lv_color_hex(theme.auxQrLight);
  const char *qrFallback = isWiki ? "https://en.wikipedia.org" : "https://ansa.it";
  d.qr = lv_qrcode_create(d.qrOverlay, qrSize, qrDark, qrLight);
  lv_obj_center(d.qr);
  lv_obj_set_style_bg_color(d.qr, qrLight, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(d.qr, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(d.qr, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(d.qr, lv_color_hex(theme.auxQrHint), LV_PART_MAIN);
  lv_obj_set_style_border_opa(d.qr, LV_OPA_70, LV_PART_MAIN);
  lv_qrcode_update(d.qr, qrFallback, strlen(qrFallback));
  lv_obj_clear_flag(d.qr, LV_OBJ_FLAG_SCROLLABLE);

  d.qrHint = lv_label_create(d.qrOverlay);
  lv_obj_set_style_text_font(d.qrHint, lvglFontTiny(), 0);
  lv_obj_set_style_text_color(d.qrHint, lv_color_hex(theme.auxQrHint), 0);
  lv_label_set_text(d.qrHint, activeUiStrings()->touchToClose);
  lv_obj_align(d.qrHint, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_add_flag(d.qrHint, LV_OBJ_FLAG_HIDDEN);
#endif
}

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

  lv_disp_draw_buf_init(&g_lvglDrawBuf, g_lvglBuf1, nullptr, bufPx);
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

  const int16_t weatherW = (DISPLAY_WEATHER_PANEL_W > (cW / 2)) ? (cW / 3) : DISPLAY_WEATHER_PANEL_W;
  const int16_t cardsGapX = 10;
  const int16_t leftW = cW - weatherW - cardsGapX;
  const int16_t outerPadX = 0;
  const int16_t outerPadY = 0;
  const bool minimalTheme = lvglThemeIsMinimalBrutalistMono();
  const lv_coord_t kCardRadius = minimalTheme ? 0 : 10;
  const lv_coord_t kInfoRadius = minimalTheme ? 0 : 8;
  const lv_coord_t kButtonRadius = minimalTheme ? 0 : 4;
  const lv_coord_t kBadgeRadius = minimalTheme ? 0 : 6;
  const lv_coord_t kWifiBarRadius = minimalTheme ? 0 : 1;
  const int16_t innerPad = 18;
  const int16_t weatherHeaderH = 30;
  const int16_t clockHeaderH = weatherHeaderH;
  const uint32_t panelBgHex = lvglResolvedPanelBg(theme);
  const uint32_t headerBgHex = lvglResolvedHeaderBg(theme);
  const uint32_t weatherBgHex = lvglResolvedWeatherBg(theme);
  const uint32_t weatherTextPrimaryHex = lvglResolvedWeatherPrimary(theme, weatherBgHex);
  const uint32_t weatherTextSecondaryHex = lvglResolvedWeatherSecondary(theme, weatherBgHex, weatherTextPrimaryHex);
  const uint32_t weatherForecastTextHex = lvglResolvedForecastText(theme, weatherBgHex, weatherTextPrimaryHex);
  const uint32_t weatherGlyphOnlineHex = lvglResolvedWeatherGlyphOnline(theme, weatherBgHex, weatherTextPrimaryHex);
  const lv_color_t kPanelBg = lv_color_hex(panelBgHex);
  const lv_color_t kHeaderBlue = lv_color_hex(headerBgHex);
  const int16_t weatherCardW = weatherW - (outerPadX * 2);
  const int16_t weatherCardH = cH - (outerPadY * 2);
  const int16_t weatherBodyH = weatherCardH - weatherHeaderH;
  const int16_t weatherIconW = 60;
  const int16_t weatherTextW = weatherCardW - 24;
  const int16_t weatherTopTextW = weatherCardW - (weatherIconW + 48);
  const int16_t clockBlockW = leftW - (outerPadX * 2);
  const int16_t clockBlockH = cH - (outerPadY * 2);
  const lv_color_t kWeatherCardBg = lv_color_hex(weatherBgHex);
  const lv_color_t kWeatherTextDark = lv_color_hex(weatherTextPrimaryHex);
  const lv_color_t kWeatherTextMid = lv_color_hex(weatherTextSecondaryHex);
  const lv_color_t kWeatherForecastText = lv_color_hex(weatherForecastTextHex);
  const lv_color_t kWeatherGlyphOnline = lv_color_hex(weatherGlyphOnlineHex);
  const lv_color_t kInfoBg = lv_color_hex(theme.infoBg);
  const lv_color_t kInfoHeaderBg = lv_color_hex(theme.infoHeaderBg);

  g_lvglInfoRoot = lv_obj_create(scr);
  lv_obj_set_size(g_lvglInfoRoot, cW, cH);
  lv_obj_set_pos(g_lvglInfoRoot, 0, 0);
  lv_obj_set_style_radius(g_lvglInfoRoot, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglInfoRoot, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglInfoRoot, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_lvglInfoRoot, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_lvglInfoRoot, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_lvglInfoRoot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(g_lvglInfoRoot, LV_SCROLLBAR_MODE_OFF);

  g_lvglInfoCard = lv_obj_create(g_lvglInfoRoot);
  lv_obj_set_size(g_lvglInfoCard, cW, cH);
  lv_obj_set_pos(g_lvglInfoCard, 0, 0);
  lv_obj_set_style_radius(g_lvglInfoCard, kInfoRadius, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_lvglInfoCard, kInfoBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(g_lvglInfoCard, kInfoBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(g_lvglInfoCard, LV_GRAD_DIR_NONE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglInfoCard, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglInfoCard, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_lvglInfoCard, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_lvglInfoCard, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_lvglInfoCard, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(g_lvglInfoCard, LV_SCROLLBAR_MODE_OFF);

  constexpr int16_t infoHeaderH = 30;
  g_lvglInfoHeader = lv_obj_create(g_lvglInfoCard);
  lv_obj_set_size(g_lvglInfoHeader, cW, infoHeaderH);
  lv_obj_set_pos(g_lvglInfoHeader, 0, 0);
  lv_obj_set_style_bg_color(g_lvglInfoHeader, kInfoHeaderBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(g_lvglInfoHeader, kInfoHeaderBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(g_lvglInfoHeader, LV_GRAD_DIR_NONE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglInfoHeader, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(g_lvglInfoHeader, kInfoRadius, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_lvglInfoHeader, lv_color_hex(theme.infoHeaderBorder), LV_PART_MAIN);
  lv_obj_set_style_border_opa(g_lvglInfoHeader, LV_OPA_60, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglInfoHeader, 1, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_lvglInfoHeader, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_lvglInfoHeader, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(g_lvglInfoHeader, LV_SCROLLBAR_MODE_OFF);
  g_lvglInfoHeaderFill = lv_obj_create(g_lvglInfoHeader);
  lv_obj_set_size(g_lvglInfoHeaderFill, cW, 10);
  lv_obj_set_pos(g_lvglInfoHeaderFill, 0, infoHeaderH - 10);
  lv_obj_set_style_bg_color(g_lvglInfoHeaderFill, kInfoHeaderBg, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglInfoHeaderFill, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglInfoHeaderFill, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_lvglInfoHeaderFill, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_lvglInfoHeaderFill, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_lvglInfoHeaderFill, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(g_lvglInfoHeaderFill, LV_SCROLLBAR_MODE_OFF);

  g_lvglInfoTitle = lv_label_create(g_lvglInfoHeader);
  lv_obj_set_style_text_font(g_lvglInfoTitle, lvglFontSmall(), 0);
  lv_obj_set_style_text_color(g_lvglInfoTitle, lv_color_hex(theme.infoText), 0);
  lv_label_set_long_mode(g_lvglInfoTitle, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(g_lvglInfoTitle, cW * 3 / 5);
  lv_obj_align(g_lvglInfoTitle, LV_ALIGN_LEFT_MID, 12, -1);
  lv_label_set_text(g_lvglInfoTitle, "ScryBar Stats  " FW_BUILD_TAG);
  lvglForceLabelVisible(g_lvglInfoTitle);

  g_lvglInfoEndpoint = lv_label_create(g_lvglInfoHeader);
  lv_obj_set_style_text_font(g_lvglInfoEndpoint, lvglFontSmall(), 0);
  lv_obj_set_style_text_color(g_lvglInfoEndpoint, lv_color_hex(theme.infoText), 0);
  lv_label_set_long_mode(g_lvglInfoEndpoint, LV_LABEL_LONG_DOT);
  lv_obj_set_size(g_lvglInfoEndpoint, (cW / 2) - 16, 20);
  lv_obj_align(g_lvglInfoEndpoint, LV_ALIGN_RIGHT_MID, -10, -1);
  lv_obj_set_style_text_align(g_lvglInfoEndpoint, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_text(g_lvglInfoEndpoint, "--:8080");
  lvglForceLabelVisible(g_lvglInfoEndpoint);

  const int16_t infoColsY = infoHeaderH + 4;
  const int16_t infoColsH = cH - infoColsY - 4;
  const int16_t infoQrPad = 5;
  // QR: fill the right column height, maximising the code size
  const int16_t infoQrSize = infoColsH - infoQrPad * 2;
  const int16_t infoQrAreaW = infoQrSize + infoQrPad * 2;
  const int16_t infoTextColW = cW - infoQrAreaW - 16;

  lv_obj_t *infoColLeft = lv_obj_create(g_lvglInfoCard);
  lv_obj_set_size(infoColLeft, infoTextColW, infoColsH);
  lv_obj_set_pos(infoColLeft, 8, infoColsY);
  lv_obj_set_style_bg_color(infoColLeft, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(infoColLeft, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(infoColLeft, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(infoColLeft, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(infoColLeft, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(infoColLeft, 0, LV_PART_MAIN);
  lv_obj_clear_flag(infoColLeft, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(infoColLeft, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *infoColRight = lv_obj_create(g_lvglInfoCard);
  lv_obj_set_size(infoColRight, infoQrAreaW, infoColsH);
  lv_obj_set_pos(infoColRight, cW - infoQrAreaW - 8, infoColsY);
  lv_obj_set_style_bg_color(infoColRight, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(infoColRight, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(infoColRight, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(infoColRight, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(infoColRight, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(infoColRight, 0, LV_PART_MAIN);
  lv_obj_clear_flag(infoColRight, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(infoColRight, LV_SCROLLBAR_MODE_OFF);

  g_lvglInfoBodyLeft = lv_label_create(infoColLeft);
  lv_obj_set_style_text_font(g_lvglInfoBodyLeft, lvglFontInfoBody(), 0);
  lv_obj_set_style_text_color(g_lvglInfoBodyLeft, lv_color_hex(theme.infoText), 0);
  lv_obj_set_style_text_line_space(g_lvglInfoBodyLeft, 1, 0);
  lv_label_set_recolor(g_lvglInfoBodyLeft, true);
  lv_label_set_long_mode(g_lvglInfoBodyLeft, LV_LABEL_LONG_WRAP);
  lv_obj_set_size(g_lvglInfoBodyLeft, infoTextColW - 8, infoColsH - 8);
  lv_obj_set_pos(g_lvglInfoBodyLeft, 4, 4);
  lv_obj_set_style_text_align(g_lvglInfoBodyLeft, LV_TEXT_ALIGN_LEFT, 0);
  lv_label_set_text(g_lvglInfoBodyLeft, "...");
  lvglForceLabelVisible(g_lvglInfoBodyLeft);

  // Right column is QR-only; keep the body label as a hidden placeholder
  g_lvglInfoBodyRight = lv_label_create(infoColRight);
  lv_obj_add_flag(g_lvglInfoBodyRight, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(g_lvglInfoBodyRight, "");

#if defined(LV_USE_QRCODE) && LV_USE_QRCODE
  // QR: centred in the right column
  const lv_color_t infoQrDark = lv_color_hex(theme.infoQrDark);
  const lv_color_t infoQrLight = lv_color_hex(theme.infoQrLight);
  g_lvglInfoWebQr = lv_qrcode_create(infoColRight, infoQrSize, infoQrDark, infoQrLight);
  lv_obj_align(g_lvglInfoWebQr, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(g_lvglInfoWebQr, infoQrLight, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglInfoWebQr, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglInfoWebQr, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_lvglInfoWebQr, lv_color_hex(theme.infoHeaderBorder), LV_PART_MAIN);
  lv_obj_set_style_border_opa(g_lvglInfoWebQr, LV_OPA_80, LV_PART_MAIN);
  lv_qrcode_update(g_lvglInfoWebQr, "http://--:8080", strlen("http://--:8080"));
#endif

  g_lvglHomeRoot = lv_obj_create(scr);
  lv_obj_set_size(g_lvglHomeRoot, cW, cH);
  lv_obj_set_pos(g_lvglHomeRoot, 0, 0);
  lv_obj_set_style_radius(g_lvglHomeRoot, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglHomeRoot, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglHomeRoot, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_lvglHomeRoot, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_lvglHomeRoot, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_lvglHomeRoot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(g_lvglHomeRoot, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *left = lv_obj_create(g_lvglHomeRoot);
  lv_obj_set_size(left, leftW, cH);
  lv_obj_set_pos(left, 0, 0);
  lv_obj_set_style_radius(left, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(left, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(left, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(left, 0, LV_PART_MAIN);
  lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);

  g_lvglClockBlock = lv_obj_create(left);
  lv_obj_set_size(g_lvglClockBlock, clockBlockW, clockBlockH);
  lv_obj_set_pos(g_lvglClockBlock, outerPadX, outerPadY);
  lv_obj_set_style_bg_color(g_lvglClockBlock, kPanelBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(g_lvglClockBlock, kPanelBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(g_lvglClockBlock, LV_GRAD_DIR_NONE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglClockBlock, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglClockBlock, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_lvglClockBlock, kCardRadius, LV_PART_MAIN);
  lv_obj_set_style_clip_corner(g_lvglClockBlock, false, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_lvglClockBlock, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_lvglClockBlock, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_lvglClockBlock, LV_OBJ_FLAG_SCROLLABLE);

  g_lvglClockHeader = lv_obj_create(g_lvglClockBlock);
  lv_obj_set_size(g_lvglClockHeader, clockBlockW, clockHeaderH);
  lv_obj_set_pos(g_lvglClockHeader, 0, 0);
  lv_obj_set_style_bg_color(g_lvglClockHeader, kHeaderBlue, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(g_lvglClockHeader, kHeaderBlue, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(g_lvglClockHeader, LV_GRAD_DIR_NONE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglClockHeader, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(g_lvglClockHeader, kCardRadius, LV_PART_MAIN);
  lv_obj_set_style_clip_corner(g_lvglClockHeader, false, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglClockHeader, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_lvglClockHeader, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_lvglClockHeader, LV_OBJ_FLAG_SCROLLABLE);

  g_lvglClockHeaderFill = lv_obj_create(g_lvglClockHeader);
  lv_obj_set_size(g_lvglClockHeaderFill, clockBlockW, 10);
  lv_obj_set_pos(g_lvglClockHeaderFill, 0, clockHeaderH - 10);
  lv_obj_set_style_bg_color(g_lvglClockHeaderFill, kHeaderBlue, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglClockHeaderFill, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglClockHeaderFill, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_lvglClockHeaderFill, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_lvglClockHeaderFill, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_lvglClockHeaderFill, LV_OBJ_FLAG_SCROLLABLE);

  constexpr int16_t kWifiBarW = 4;
  constexpr int16_t kWifiBarGap = 3;
  constexpr int16_t kWifiBarCount = 4;
  const int16_t clockWiFiTotalW = (kWifiBarW * kWifiBarCount) + (kWifiBarGap * (kWifiBarCount - 1));
  const int16_t clockWiFiStartX = clockBlockW - 12 - clockWiFiTotalW;
  const int16_t clockWiFiBaseY = clockHeaderH - 6;
  const int16_t clockWiFiHeights[4] = {5, 8, 11, 14};
  for (uint8_t i = 0; i < 4; ++i) {
    g_lvglClockWiFiBars[i] = lv_obj_create(g_lvglClockHeader);
    lv_obj_set_size(g_lvglClockWiFiBars[i], kWifiBarW, clockWiFiHeights[i]);
    lv_obj_set_pos(g_lvglClockWiFiBars[i],
                   clockWiFiStartX + (int16_t)i * (kWifiBarW + kWifiBarGap),
                   clockWiFiBaseY - clockWiFiHeights[i]);
    lv_obj_set_style_bg_color(g_lvglClockWiFiBars[i], lv_color_hex(theme.wifiBarOff), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_lvglClockWiFiBars[i], (lv_opa_t)190, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_lvglClockWiFiBars[i], 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(g_lvglClockWiFiBars[i], 0, LV_PART_MAIN);
    lv_obj_set_style_radius(g_lvglClockWiFiBars[i], kWifiBarRadius, LV_PART_MAIN);
    lv_obj_clear_flag(g_lvglClockWiFiBars[i], LV_OBJ_FLAG_SCROLLABLE);
  }

  lv_obj_t *right = lv_obj_create(g_lvglHomeRoot);
  lv_obj_set_size(right, weatherW, cH);
  lv_obj_set_pos(right, leftW + cardsGapX, 0);
  lv_obj_set_style_radius(right, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(right, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(right, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(right, 0, LV_PART_MAIN);
  lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);

  g_lvglWeatherCard = lv_obj_create(right);
  lv_obj_set_size(g_lvglWeatherCard, weatherCardW, weatherCardH);
  lv_obj_set_pos(g_lvglWeatherCard, outerPadX, outerPadY);
  lv_obj_set_style_radius(g_lvglWeatherCard, kCardRadius, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_lvglWeatherCard, kWeatherCardBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(g_lvglWeatherCard, kWeatherCardBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(g_lvglWeatherCard, LV_GRAD_DIR_NONE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglWeatherCard, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglWeatherCard, 0, LV_PART_MAIN);
  lv_obj_set_style_clip_corner(g_lvglWeatherCard, false, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_lvglWeatherCard, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_lvglWeatherCard, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_lvglWeatherCard, LV_OBJ_FLAG_SCROLLABLE);

  g_lvglWeatherHeader = lv_obj_create(g_lvglWeatherCard);
  lv_obj_set_size(g_lvglWeatherHeader, weatherCardW, weatherHeaderH);
  lv_obj_set_pos(g_lvglWeatherHeader, 0, 0);
  lv_obj_set_style_bg_color(g_lvglWeatherHeader, kHeaderBlue, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(g_lvglWeatherHeader, kHeaderBlue, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(g_lvglWeatherHeader, LV_GRAD_DIR_NONE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglWeatherHeader, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(g_lvglWeatherHeader, kCardRadius, LV_PART_MAIN);
  lv_obj_set_style_clip_corner(g_lvglWeatherHeader, false, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglWeatherHeader, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_lvglWeatherHeader, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_lvglWeatherHeader, LV_OBJ_FLAG_SCROLLABLE);
  g_lvglWeatherHeaderFill = lv_obj_create(g_lvglWeatherHeader);
  lv_obj_set_size(g_lvglWeatherHeaderFill, weatherCardW, 10);
  lv_obj_set_pos(g_lvglWeatherHeaderFill, 0, weatherHeaderH - 10);
  lv_obj_set_style_bg_color(g_lvglWeatherHeaderFill, kHeaderBlue, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglWeatherHeaderFill, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglWeatherHeaderFill, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_lvglWeatherHeaderFill, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_lvglWeatherHeaderFill, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_lvglWeatherHeaderFill, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *headerDivider = lv_obj_create(g_lvglWeatherCard);
  lv_obj_set_size(headerDivider, weatherCardW - 16, 2);
  lv_obj_set_pos(headerDivider, 8, weatherHeaderH - 1);
  lv_obj_set_style_bg_color(headerDivider, lv_color_hex(0x90A3DE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(headerDivider, LV_OPA_0, LV_PART_MAIN);
  lv_obj_set_style_border_width(headerDivider, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(headerDivider, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(headerDivider, 0, LV_PART_MAIN);
  lv_obj_clear_flag(headerDivider, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(headerDivider, LV_OBJ_FLAG_HIDDEN);

  g_lvglWeatherBody = lv_obj_create(g_lvglWeatherCard);
  lv_obj_set_size(g_lvglWeatherBody, weatherCardW, weatherCardH - weatherHeaderH);
  lv_obj_set_pos(g_lvglWeatherBody, 0, weatherHeaderH);
  lv_obj_set_style_bg_opa(g_lvglWeatherBody, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_radius(g_lvglWeatherBody, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglWeatherBody, 0, LV_PART_MAIN);
  lv_obj_set_style_clip_corner(g_lvglWeatherBody, false, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_lvglWeatherBody, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_left(g_lvglWeatherBody, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_right(g_lvglWeatherBody, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_top(g_lvglWeatherBody, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(g_lvglWeatherBody, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_lvglWeatherBody, LV_OBJ_FLAG_SCROLLABLE);

  g_lvglClockDate = lv_label_create(g_lvglClockHeader);
  lv_obj_set_style_text_font(g_lvglClockDate, lvglFontSmall(), 0);
  lv_obj_set_style_text_color(g_lvglClockDate, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(g_lvglClockDate, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_label_set_long_mode(g_lvglClockDate, LV_LABEL_LONG_DOT);
  const int16_t clockDateW = (clockWiFiStartX > 28) ? (clockWiFiStartX - 20) : (clockBlockW - 24);
  lv_obj_set_width(g_lvglClockDate, clockDateW);
  lv_obj_align(g_lvglClockDate, LV_ALIGN_LEFT_MID, 12, -1);
  lv_label_set_text(g_lvglClockDate, "...");
  lvglForceLabelVisible(g_lvglClockDate);

  g_lvglClockL1 = lv_label_create(g_lvglClockBlock);
  g_lvglClockL2 = lv_label_create(g_lvglClockBlock);
  g_lvglClockL3 = lv_label_create(g_lvglClockBlock);
  lv_obj_set_style_text_font(g_lvglClockL1, lvglFontClock(), 0);
  lv_obj_set_style_text_font(g_lvglClockL2, lvglFontTitle(), 0);
  lv_obj_set_style_text_font(g_lvglClockL3, lvglFontTitle(), 0);
  lv_obj_set_style_text_color(g_lvglClockL1, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_color(g_lvglClockL2, lv_color_hex(0xEAF0FF), 0);
  lv_obj_set_style_text_color(g_lvglClockL3, lv_color_hex(0xD8E3FF), 0);
  lv_obj_set_style_text_line_space(g_lvglClockL1, 4, 0);
  lv_obj_set_style_text_line_space(g_lvglClockL2, 2, 0);
  lv_obj_set_style_text_line_space(g_lvglClockL3, 2, 0);
  lv_obj_set_style_text_align(g_lvglClockL1, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_bg_opa(g_lvglClockL1, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglClockL1, 0, LV_PART_MAIN);
  lv_label_set_long_mode(g_lvglClockL1, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(g_lvglClockL1, clockBlockW - 16);
  lv_obj_set_pos(g_lvglClockL1, 8, 38);
  lvglApplyClockSentenceAutoFit("Clock...");
  lvglForceLabelVisible(g_lvglClockL1);
  lv_obj_add_flag(g_lvglClockL2, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(g_lvglClockL3, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(g_lvglClockL2, "");
  lv_label_set_text(g_lvglClockL3, "");

  g_lvglClockDivider = lv_obj_create(g_lvglClockBlock);
  lv_obj_set_size(g_lvglClockDivider, clockBlockW - (innerPad * 2), 1);
  lv_obj_set_pos(g_lvglClockDivider, innerPad, cH - 58);
  lv_obj_set_style_bg_color(g_lvglClockDivider, lv_color_hex(0x9FB5EE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglClockDivider, LV_OPA_0, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglClockDivider, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_lvglClockDivider, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_lvglClockDivider, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_lvglClockDivider, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(g_lvglClockDivider, LV_OBJ_FLAG_HIDDEN);

  g_lvglCity = lv_label_create(g_lvglWeatherHeader);
  lv_obj_set_style_text_font(g_lvglCity, lvglFontSmall(), 0);
  lv_obj_set_style_text_color(g_lvglCity, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_width(g_lvglCity, weatherCardW - 112);
  lv_label_set_long_mode(g_lvglCity, LV_LABEL_LONG_DOT);
  lv_obj_align(g_lvglCity, LV_ALIGN_LEFT_MID, 12, -1);
  lv_label_set_text(g_lvglCity, "Luino");
  lvglForceLabelVisible(g_lvglCity);

  g_lvglSun = lv_label_create(g_lvglWeatherHeader);
  lv_obj_set_style_text_font(g_lvglSun, lvglFontSmall(), 0);
  lv_obj_set_style_text_color(g_lvglSun, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_opa(g_lvglSun, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_align(g_lvglSun, LV_ALIGN_RIGHT_MID, -10, 0);
  lv_label_set_text(g_lvglSun, "--:-- | --:--");
  lvglForceLabelVisible(g_lvglSun);

  g_lvglTemp = lv_label_create(g_lvglWeatherBody);
  lv_obj_set_style_text_font(g_lvglTemp, lvglFontTemp(), 0);
  lv_obj_set_style_text_color(g_lvglTemp, kWeatherTextDark, 0);
  lv_obj_set_width(g_lvglTemp, weatherTopTextW);
  lv_obj_align(g_lvglTemp, LV_ALIGN_TOP_LEFT, 12, 8);
  lv_label_set_text(g_lvglTemp, "--\xC2\xB0, --%");
  lvglForceLabelVisible(g_lvglTemp);

  g_lvglIcon = lv_img_create(g_lvglWeatherBody);
  constexpr uint16_t kMainIconZoom = 336;  // ~63px rendered from 48px source
  const int16_t mainIconPx = (int16_t)(((int32_t)weatherIconW * kMainIconZoom + 128) / 256);
  const int16_t mainIconY = ((weatherBodyH - mainIconPx) / 2) + 2;  // lower icon a bit for vertical centering
  lv_obj_align(g_lvglIcon, LV_ALIGN_TOP_RIGHT, -21, mainIconY);     // move icon 7px right
  const lv_img_dsc_t *bootIcon = weatherImageFromCode(2, true);
  if (bootIcon) {
    lv_img_set_src(g_lvglIcon, bootIcon);
    lv_obj_clear_flag(g_lvglIcon, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(g_lvglIcon, LV_OBJ_FLAG_HIDDEN);
  }
  lv_img_set_zoom(g_lvglIcon, kMainIconZoom);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, g_lvglIcon);
  lv_anim_set_values(&a, -2, 2);
  lv_anim_set_time(&a, 2600);
  lv_anim_set_playback_time(&a, 2600);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_exec_cb(&a, lvglIconFloatAnimCb);
  lv_anim_start(&a);

  g_lvglGlyph = lv_label_create(g_lvglWeatherBody);
  lv_obj_set_style_text_font(g_lvglGlyph, lvglFontBig(), 0);
  lv_obj_set_style_text_color(g_lvglGlyph, kWeatherGlyphOnline, 0);
  lv_obj_align(g_lvglGlyph, LV_ALIGN_TOP_LEFT, 12, 4);
  lv_label_set_text(g_lvglGlyph, "*");
  lv_obj_add_flag(g_lvglGlyph, LV_OBJ_FLAG_HIDDEN);

  g_lvglWeatherSep = lv_obj_create(g_lvglWeatherBody);
  const int16_t weatherSepW = weatherTopTextW - 26;  // leave extra space near large icon
  lv_obj_set_size(g_lvglWeatherSep, weatherSepW, 1);
  lv_obj_set_pos(g_lvglWeatherSep, 12, 44);
  lv_obj_set_style_bg_color(g_lvglWeatherSep, kWeatherTextMid, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglWeatherSep, LV_OPA_70, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglWeatherSep, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_lvglWeatherSep, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_lvglWeatherSep, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_lvglWeatherSep, LV_OBJ_FLAG_SCROLLABLE);

  g_lvglDesc = lv_label_create(g_lvglWeatherBody);
  lv_obj_set_style_text_font(g_lvglDesc, lvglFontMeta(), 0);
  lv_obj_set_style_text_color(g_lvglDesc, kWeatherTextDark, 0);
  lv_obj_set_style_anim_speed(g_lvglDesc, 28, 0);
  lv_label_set_long_mode(g_lvglDesc, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_width(g_lvglDesc, weatherTopTextW - 10);
  lv_obj_align(g_lvglDesc, LV_ALIGN_TOP_LEFT, 12, 50);
  lv_label_set_text(g_lvglDesc, "Meteo in arrivo");
  lvglForceLabelVisible(g_lvglDesc);

  g_lvglHumidity = lv_label_create(g_lvglWeatherBody);
  lv_obj_set_style_text_font(g_lvglHumidity, lvglFontMini(), 0);
  lv_obj_set_style_text_color(g_lvglHumidity, kWeatherTextDark, 0);
  lv_obj_set_width(g_lvglHumidity, weatherTopTextW);
  lv_obj_align(g_lvglHumidity, LV_ALIGN_TOP_LEFT, 12, 77);
  lv_label_set_text(g_lvglHumidity, activeUiStrings()->windNa);
  lvglForceLabelVisible(g_lvglHumidity);

  g_lvglWind = lv_label_create(g_lvglWeatherBody);
  lv_obj_set_style_text_font(g_lvglWind, lvglFontTiny(), 0);
  lv_obj_set_style_text_color(g_lvglWind, kWeatherTextMid, 0);
  lv_obj_set_width(g_lvglWind, weatherTextW);
  lv_obj_align(g_lvglWind, LV_ALIGN_TOP_LEFT, 12, 90);
  lv_label_set_text(g_lvglWind, "");
  lv_obj_add_flag(g_lvglWind, LV_OBJ_FLAG_HIDDEN);

  constexpr int16_t forecastBarH = 34;
  const int16_t forecastBarY = weatherCardH - forecastBarH;
  g_lvglForecastBar = lv_obj_create(g_lvglWeatherCard);
  lv_obj_set_size(g_lvglForecastBar, weatherCardW, forecastBarH);
  lv_obj_set_pos(g_lvglForecastBar, 0, forecastBarY);
  lv_obj_set_style_bg_color(g_lvglForecastBar, kWeatherCardBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(g_lvglForecastBar, kWeatherCardBg, LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(g_lvglForecastBar, LV_GRAD_DIR_NONE, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglForecastBar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(g_lvglForecastBar, kCardRadius, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglForecastBar, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_lvglForecastBar, 0, LV_PART_MAIN);
  lv_obj_set_style_clip_corner(g_lvglForecastBar, false, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_lvglForecastBar, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_lvglForecastBar, LV_OBJ_FLAG_SCROLLABLE);
  g_lvglForecastBarFill = lv_obj_create(g_lvglForecastBar);
  lv_obj_set_size(g_lvglForecastBarFill, weatherCardW, 10);
  lv_obj_set_pos(g_lvglForecastBarFill, 0, 0);
  lv_obj_set_style_bg_color(g_lvglForecastBarFill, kWeatherCardBg, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglForecastBarFill, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglForecastBarFill, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_lvglForecastBarFill, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_lvglForecastBarFill, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_lvglForecastBarFill, LV_OBJ_FLAG_SCROLLABLE);

  g_lvglForecastIcon = lv_img_create(g_lvglForecastBar);
  lv_obj_set_pos(g_lvglForecastIcon, 5, -11);  // keep center while growing icon (+6px)
  if (bootIcon) {
    lv_img_set_src(g_lvglForecastIcon, bootIcon);
    lv_obj_clear_flag(g_lvglForecastIcon, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(g_lvglForecastIcon, LV_OBJ_FLAG_HIDDEN);
  }
  lv_img_set_zoom(g_lvglForecastIcon, 123);  // ~23px rendered from 48px source

  g_lvglForecastNow = lv_label_create(g_lvglForecastBar);
  lv_obj_set_style_text_font(g_lvglForecastNow, lvglFontSmall(), 0);
  lv_obj_set_style_text_color(g_lvglForecastNow, kWeatherForecastText, 0);
  lv_obj_set_width(g_lvglForecastNow, weatherCardW - 65);
  lv_obj_set_pos(g_lvglForecastNow, 52, 6);
  lv_label_set_long_mode(g_lvglForecastNow, LV_LABEL_LONG_DOT);
  lv_label_set_text(g_lvglForecastNow, activeUiStrings()->forecastNa);
  lvglForceLabelVisible(g_lvglForecastNow);

  g_lvglForecastTomorrow = lv_label_create(g_lvglWeatherBody);
  lv_obj_set_style_text_font(g_lvglForecastTomorrow, lvglFontTiny(), 0);
  lv_obj_set_style_text_color(g_lvglForecastTomorrow, kWeatherTextMid, 0);
  lv_label_set_long_mode(g_lvglForecastTomorrow, LV_LABEL_LONG_DOT);
  lv_obj_set_width(g_lvglForecastTomorrow, weatherTextW);
  lv_obj_align(g_lvglForecastTomorrow, LV_ALIGN_TOP_LEFT, 12, 102);
  lv_label_set_text(g_lvglForecastTomorrow, "");
  lv_obj_add_flag(g_lvglForecastTomorrow, LV_OBJ_FLAG_HIDDEN);

  g_lvglAuxRoot = lv_obj_create(scr);
  lv_obj_set_size(g_lvglAuxRoot, cW, cH);
  lv_obj_set_pos(g_lvglAuxRoot, 0, 0);
  lv_obj_set_style_radius(g_lvglAuxRoot, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglAuxRoot, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglAuxRoot, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_lvglAuxRoot, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_lvglAuxRoot, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_lvglAuxRoot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(g_lvglAuxRoot, LV_SCROLLBAR_MODE_OFF);

  // Wiki page root — positioned at ordinal 3 (one screen-width to the right of AUX)
  g_lvglWikiRoot = lv_obj_create(scr);
  lv_obj_set_size(g_lvglWikiRoot, cW, cH);
  lv_obj_set_pos(g_lvglWikiRoot, cW, 0);  // starts offscreen right
  lv_obj_set_style_radius(g_lvglWikiRoot, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglWikiRoot, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglWikiRoot, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_lvglWikiRoot, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_lvglWikiRoot, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_lvglWikiRoot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(g_lvglWikiRoot, LV_SCROLLBAR_MODE_OFF);

  lvglInitFeedDeck(g_wikiDeck, g_lvglWikiRoot, true);

  lvglInitFeedDeck(g_auxDeck, g_lvglAuxRoot, false);

  g_lvglDoomRoot = lv_obj_create(scr);
  lv_obj_set_size(g_lvglDoomRoot, cW, cH);
  lv_obj_set_pos(g_lvglDoomRoot, 0, 0);
  lv_obj_set_style_radius(g_lvglDoomRoot, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglDoomRoot, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_lvglDoomRoot, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglDoomRoot, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_lvglDoomRoot, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_lvglDoomRoot, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_lvglDoomRoot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(g_lvglDoomRoot, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_flag(g_lvglDoomRoot, LV_OBJ_FLAG_HIDDEN);

#if SCREENSAVER_ENABLED
  const uint32_t saverReadableText = lvglResolvedSaverReadableText(theme);
  g_lvglScreenSaverRoot = lv_obj_create(scr);
  lv_obj_set_size(g_lvglScreenSaverRoot, cW, cH);
  lv_obj_set_pos(g_lvglScreenSaverRoot, 0, 0);
  lv_obj_set_style_radius(g_lvglScreenSaverRoot, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_lvglScreenSaverRoot, lv_color_hex(theme.screenBg), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_lvglScreenSaverRoot, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_lvglScreenSaverRoot, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(g_lvglScreenSaverRoot, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_lvglScreenSaverRoot, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_lvglScreenSaverRoot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(g_lvglScreenSaverRoot, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_flag(g_lvglScreenSaverRoot, LV_OBJ_FLAG_HIDDEN);

  g_lvglScreenSaverSky = lv_label_create(g_lvglScreenSaverRoot);
  lv_obj_set_style_text_font(g_lvglScreenSaverSky, lvglFontMono(), 0);
  lv_obj_set_style_text_color(g_lvglScreenSaverSky, lv_color_hex(theme.saverSky), 0);
  lv_label_set_long_mode(g_lvglScreenSaverSky, LV_LABEL_LONG_WRAP);
  lv_obj_set_size(g_lvglScreenSaverSky, cW - 8, cH - 68);
  lv_obj_set_pos(g_lvglScreenSaverSky, 4, 4);
  lv_label_set_text(g_lvglScreenSaverSky, "");
  lvglForceLabelVisible(g_lvglScreenSaverSky);
  lv_obj_add_flag(g_lvglScreenSaverSky, LV_OBJ_FLAG_HIDDEN);

  for (uint8_t r = 0; r < kSaverSkyRowsMax; ++r) {
    for (uint8_t s = 0; s < kSaverStarsPerRow; ++s) {
      g_lvglScreenSaverStarObj[r][s] = lv_label_create(g_lvglScreenSaverRoot);
      lv_obj_set_style_text_font(g_lvglScreenSaverStarObj[r][s], lvglFontMonoTiny(), 0);
      lv_obj_set_style_text_color(g_lvglScreenSaverStarObj[r][s], lv_color_hex(theme.saverStarLow), 0);
      lv_label_set_text(g_lvglScreenSaverStarObj[r][s], ".");
      lv_obj_set_pos(g_lvglScreenSaverStarObj[r][s], 8, 8);
      lv_obj_add_flag(g_lvglScreenSaverStarObj[r][s], LV_OBJ_FLAG_HIDDEN);
      lvglForceLabelVisible(g_lvglScreenSaverStarObj[r][s]);
    }
  }

  g_lvglScreenSaverField = lv_label_create(g_lvglScreenSaverRoot);
  lv_obj_set_style_text_font(g_lvglScreenSaverField, lvglFontMonoTiny(), 0);
  lv_obj_set_style_text_color(g_lvglScreenSaverField, lv_color_hex(theme.saverField), 0);
  lv_label_set_long_mode(g_lvglScreenSaverField, LV_LABEL_LONG_WRAP);
  lv_obj_set_size(g_lvglScreenSaverField, cW - 8, 12);
  lv_obj_set_pos(g_lvglScreenSaverField, 4, cH - 24);
  lv_label_set_text(g_lvglScreenSaverField, "");
  lvglForceLabelVisible(g_lvglScreenSaverField);

  g_lvglScreenSaverCow = lv_label_create(g_lvglScreenSaverRoot);
  lv_obj_set_style_text_font(g_lvglScreenSaverCow, lvglFontMonoTiny(), 0);
  lv_obj_set_style_text_color(g_lvglScreenSaverCow, lv_color_hex(theme.saverCow), 0);
  lv_obj_set_style_text_letter_space(g_lvglScreenSaverCow, 0, 0);
  lv_obj_set_style_text_line_space(g_lvglScreenSaverCow, 0, 0);
  lvglScreenSaverSetCowArt(1);
  lv_obj_set_pos(g_lvglScreenSaverCow, 20, cH - 90);
  lvglForceLabelVisible(g_lvglScreenSaverCow);

  g_lvglScreenSaverBalloon = lv_label_create(g_lvglScreenSaverRoot);
  lv_obj_set_style_text_font(g_lvglScreenSaverBalloon, lvglFontScreenSaverBalloonText(), 0);
  lv_obj_set_style_text_color(g_lvglScreenSaverBalloon, lv_color_hex(saverReadableText), 0);
  lv_obj_set_style_bg_opa(g_lvglScreenSaverBalloon, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_lvglScreenSaverBalloon, 0, 0);
  lv_obj_set_style_pad_hor(g_lvglScreenSaverBalloon, 0, 0);
  lv_obj_set_style_pad_ver(g_lvglScreenSaverBalloon, 0, 0);
  lv_obj_set_style_text_letter_space(g_lvglScreenSaverBalloon, 0, 0);
  lv_obj_set_style_text_line_space(g_lvglScreenSaverBalloon, 0, 0);
  lv_label_set_long_mode(g_lvglScreenSaverBalloon, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(g_lvglScreenSaverBalloon, (cW * 56) / 100, LV_SIZE_CONTENT);
  lv_obj_set_pos(g_lvglScreenSaverBalloon, 166, cH - 126);
  lv_label_set_text(g_lvglScreenSaverBalloon, "");
  lvglForceLabelVisible(g_lvglScreenSaverBalloon);
  lv_obj_add_flag(g_lvglScreenSaverBalloon, LV_OBJ_FLAG_HIDDEN);

  g_lvglScreenSaverBalloonTail = lv_label_create(g_lvglScreenSaverRoot);
  lv_obj_set_style_text_font(g_lvglScreenSaverBalloonTail, lvglFontScreenSaverTail(), 0);
  lv_obj_set_style_text_color(g_lvglScreenSaverBalloonTail, lv_color_hex(saverReadableText), 0);
  lv_label_set_text(g_lvglScreenSaverBalloonTail, "- - - - -");
  lv_obj_set_pos(g_lvglScreenSaverBalloonTail, 200, cH - 64);
  lvglForceLabelVisible(g_lvglScreenSaverBalloonTail);
  lv_obj_add_flag(g_lvglScreenSaverBalloonTail, LV_OBJ_FLAG_HIDDEN);

  g_lvglScreenSaverFooter = lv_label_create(g_lvglScreenSaverRoot);
  lv_obj_set_style_text_font(g_lvglScreenSaverFooter, lvglFontScreenSaverFooterText(), 0);
  lv_obj_set_style_text_color(g_lvglScreenSaverFooter, lv_color_hex(saverReadableText), 0);
  lv_label_set_text(g_lvglScreenSaverFooter, "--:--  --/--");
  lv_obj_align(g_lvglScreenSaverFooter, LV_ALIGN_BOTTOM_RIGHT, -10, -4);
  lvglForceLabelVisible(g_lvglScreenSaverFooter);
#endif

  lvglApplyPageVisibility(false);

  g_lvglReady = true;
  g_lvglLastTickMs = millis();
#if SCREENSAVER_ENABLED
  g_lastUserInteractionMs = millis();
#endif
  g_lvglClockWiFiMask = 0xFFFF;
  lvglApplyThemeStyles(true);
  Serial.printf("[LVGL] widgets date=%p clock=%p city=%p temp=%p desc=%p hum=%p sun=%p wind=%p f0=%p f1=%p\n",
                (void*)g_lvglClockDate,
                (void*)g_lvglClockL1,
                (void*)g_lvglCity,
                (void*)g_lvglIcon,
                (void*)g_lvglTemp,
                (void*)g_lvglDesc,
                (void*)g_lvglHumidity,
                (void*)g_lvglSun,
                (void*)g_lvglWind,
                (void*)g_lvglForecastNow,
                (void*)g_lvglForecastTomorrow);
  Serial.printf("[LVGL] init ok ui=%dx%d split=%d/%d color_depth=%d color_size=%u icons=%s\n",
                cW, cH, leftW, weatherW, LV_COLOR_DEPTH, (unsigned)sizeof(lv_color_t), DB_LVGL_WEATHER_ICON_SET);
  return true;
}

static void updateLvglUi(bool force) {
  if (!g_lvglReady || !g_ntpSynced) return;
#if SCREENSAVER_ENABLED
  if (g_lvglScreenSaverActive) return;
#endif
  const UiThemeLvglTokens &theme = activeUiTheme().lvgl;
  const uint32_t weatherBg = lvglResolvedWeatherBg(theme);
  const uint32_t weatherPrimary = lvglResolvedWeatherPrimary(theme, weatherBg);
  const uint32_t weatherSecondary = lvglResolvedWeatherSecondary(theme, weatherBg, weatherPrimary);
  const uint32_t weatherGlyphOnline = lvglResolvedWeatherGlyphOnline(theme, weatherBg, weatherPrimary);
  const uint32_t weatherGlyphOffline = lvglResolvedWeatherGlyphOffline(theme, weatherBg, weatherSecondary);
  (void)weatherBg;
  (void)weatherPrimary;
  (void)weatherSecondary;
  (void)weatherGlyphOnline;
  (void)weatherGlyphOffline;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 50)) return;
  const int dateKey = ((timeinfo.tm_year + 1900) * 10000) + ((timeinfo.tm_mon + 1) * 100) + timeinfo.tm_mday;
  if (!force && !g_uiNeedsRedraw && timeinfo.tm_sec == g_lastClockSecond && dateKey == g_lastDateKey) return;

  lvglApplyPageVisibility(false);
  lvglUpdateWiFiBars(force);
  if (g_uiPageMode == UI_PAGE_DOOM) {
    g_uiNeedsRedraw = false;
    return;
  }
  if (g_uiPageMode == UI_PAGE_INFO) {
    lvglUpdateInfoPanel(force);
    g_lastClockSecond = timeinfo.tm_sec;
    g_lastDateKey = dateKey;
    g_uiNeedsRedraw = false;
    return;
  }
  if (g_uiPageMode == UI_PAGE_AUX) {
    lvglUpdateFeedDeck(g_auxDeck, g_rss, false, force);
    g_lastClockSecond = timeinfo.tm_sec;
    g_lastDateKey = dateKey;
    g_uiNeedsRedraw = false;
    return;
  }
  if (g_uiPageMode == UI_PAGE_WIKI) {
    lvglUpdateFeedDeck(g_wikiDeck, g_wiki, true, force);
    g_lastClockSecond = timeinfo.tm_sec;
    g_lastDateKey = dateKey;
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
  lvglForceLabelVisible(g_lvglClockL1);
  lv_label_set_text(g_lvglClockL2, "");
  lv_label_set_text(g_lvglClockL3, "");
  formatDateActive(timeinfo, d1, sizeof(d1));
  sanitizeAsciiBuffer(d1, sizeof(d1));
  lv_label_set_text(g_lvglClockDate, d1);
  lvglForceLabelVisible(g_lvglClockDate);
#if TEST_WIFI
  lvglUpdateCityTicker(runtimeWeatherCityLabel(), force);
#else
  lvglUpdateCityTicker(WEATHER_CITY_LABEL, force);
#endif

#if TEST_WIFI
  if (g_weather.valid) {
    char temp[24];
    snprintf(temp, sizeof(temp), "%d%s, %d%%", (int)lroundf(g_weather.tempC), utf8Degree(), g_weather.humidity);
    lv_label_set_text(g_lvglTemp, temp);
    lvglForceLabelVisible(g_lvglTemp);

    const lv_img_dsc_t *iconDsc = weatherImageFromCode(g_weather.weatherCode, g_weather.isDay);
    if (iconDsc && g_lvglIcon) {
      lv_img_set_src(g_lvglIcon, iconDsc);
      lv_obj_clear_flag(g_lvglIcon, LV_OBJ_FLAG_HIDDEN);
      if (g_lvglGlyph) lv_obj_add_flag(g_lvglGlyph, LV_OBJ_FLAG_HIDDEN);
    } else if (g_lvglGlyph) {
      if (g_lvglIcon) lv_obj_add_flag(g_lvglIcon, LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(g_lvglGlyph, weatherGlyphText(g_weather.weatherCode, g_weather.isDay));
      lvglForceLabelVisible(g_lvglGlyph);
    }
    lv_label_set_text(g_lvglDesc, weatherCodeUiLabel(g_weather.weatherCode));
    lvglForceLabelVisible(g_lvglDesc);

    char wind[28];
    snprintf(wind, sizeof(wind), activeUiStrings()->windFmt, g_weather.windKmh);
    lv_label_set_text(g_lvglHumidity, wind);
    lvglForceLabelVisible(g_lvglHumidity);

    char sun[40];
    snprintf(sun, sizeof(sun), "%s / %s", g_weather.sunrise, g_weather.sunset);
    lv_label_set_text(g_lvglSun, sun);
    lvglForceLabelVisible(g_lvglSun);

    const int fIdx = g_weather.nextValid[1] ? 1 : (g_weather.nextValid[0] ? 0 : -1);
    const int fCode = (fIdx >= 0) ? g_weather.nextCode[fIdx] : g_weather.weatherCode;
    const bool fIsDay = g_weather.isDay;
    const lv_img_dsc_t *fIconDsc = weatherForecastImageFromCode(fCode, fIsDay);
    if (g_lvglForecastIcon) {
      if (fIconDsc) {
        lv_img_set_src(g_lvglForecastIcon, fIconDsc);
        lv_obj_clear_flag(g_lvglForecastIcon, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(g_lvglForecastIcon, LV_OBJ_FLAG_HIDDEN);
      }
    }

    char forecast[64];
    if (fIdx == 1) {
      snprintf(forecast, sizeof(forecast), activeUiStrings()->forecast3h, weatherCodeShort(fCode));
    } else if (fIdx == 0) {
      snprintf(forecast, sizeof(forecast), activeUiStrings()->forecastNow, weatherCodeShort(fCode));
    } else {
      snprintf(forecast, sizeof(forecast), "%s", activeUiStrings()->forecastNa);
    }
    lv_label_set_text(g_lvglForecastNow, forecast);
    lvglForceLabelVisible(g_lvglForecastNow);

    lv_obj_add_flag(g_lvglForecastTomorrow, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_lvglWind, LV_OBJ_FLAG_HIDDEN);
    if (g_lvglGlyph) lv_obj_set_style_text_color(g_lvglGlyph, lv_color_hex(weatherGlyphOnline), 0);
  } else {
    lv_label_set_text(g_lvglTemp, "--\xC2\xB0, --%");
    lvglForceLabelVisible(g_lvglTemp);
    lv_label_set_text(g_lvglDesc, activeUiStrings()->weatherOffline);
    lvglForceLabelVisible(g_lvglDesc);
    lv_label_set_text(g_lvglHumidity, activeUiStrings()->windNa);
    lvglForceLabelVisible(g_lvglHumidity);
    lv_label_set_text(g_lvglSun, "--:-- / --:--");
    lvglForceLabelVisible(g_lvglSun);

    lv_label_set_text(g_lvglForecastNow, activeUiStrings()->forecastNa);
    lvglForceLabelVisible(g_lvglForecastNow);

    const lv_img_dsc_t *iconDsc = weatherImageFromCode(2, true);
    if (iconDsc && g_lvglIcon) {
      lv_img_set_src(g_lvglIcon, iconDsc);
      lv_obj_clear_flag(g_lvglIcon, LV_OBJ_FLAG_HIDDEN);
      if (g_lvglGlyph) lv_obj_add_flag(g_lvglGlyph, LV_OBJ_FLAG_HIDDEN);
    } else if (g_lvglGlyph) {
      if (g_lvglIcon) lv_obj_add_flag(g_lvglIcon, LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(g_lvglGlyph, LV_SYMBOL_REFRESH);
      lvglForceLabelVisible(g_lvglGlyph);
    }
    const lv_img_dsc_t *fIconDsc = weatherForecastImageFromCode(2, true);
    if (g_lvglForecastIcon) {
      if (fIconDsc) {
        lv_img_set_src(g_lvglForecastIcon, fIconDsc);
        lv_obj_clear_flag(g_lvglForecastIcon, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(g_lvglForecastIcon, LV_OBJ_FLAG_HIDDEN);
      }
    }
    lv_obj_add_flag(g_lvglForecastTomorrow, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_lvglWind, LV_OBJ_FLAG_HIDDEN);
    if (g_lvglGlyph) lv_obj_set_style_text_color(g_lvglGlyph, lv_color_hex(weatherGlyphOffline), 0);
  }
#else
  lv_label_set_text(g_lvglTemp, "--\xC2\xB0, --%");
  lvglForceLabelVisible(g_lvglTemp);
  lv_label_set_text(g_lvglDesc, activeUiStrings()->wifiOff);
  lvglForceLabelVisible(g_lvglDesc);
  lv_label_set_text(g_lvglHumidity, activeUiStrings()->windNa);
  lvglForceLabelVisible(g_lvglHumidity);
  lv_label_set_text(g_lvglSun, "--:-- / --:--");
  lvglForceLabelVisible(g_lvglSun);
  lv_label_set_text(g_lvglForecastNow, activeUiStrings()->forecastNa);
  lvglForceLabelVisible(g_lvglForecastNow);

  const lv_img_dsc_t *iconDsc = weatherImageFromCode(2, true);
  if (iconDsc && g_lvglIcon) {
    lv_img_set_src(g_lvglIcon, iconDsc);
    lv_obj_clear_flag(g_lvglIcon, LV_OBJ_FLAG_HIDDEN);
    if (g_lvglGlyph) lv_obj_add_flag(g_lvglGlyph, LV_OBJ_FLAG_HIDDEN);
  } else if (g_lvglGlyph) {
    if (g_lvglIcon) lv_obj_add_flag(g_lvglIcon, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(g_lvglGlyph, LV_SYMBOL_WIFI);
    lvglForceLabelVisible(g_lvglGlyph);
  }
  const lv_img_dsc_t *fIconDsc = weatherForecastImageFromCode(2, true);
  if (g_lvglForecastIcon) {
    if (fIconDsc) {
      lv_img_set_src(g_lvglForecastIcon, fIconDsc);
      lv_obj_clear_flag(g_lvglForecastIcon, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(g_lvglForecastIcon, LV_OBJ_FLAG_HIDDEN);
    }
  }
  lv_obj_add_flag(g_lvglForecastTomorrow, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(g_lvglWind, LV_OBJ_FLAG_HIDDEN);
#endif

  g_lastClockSecond = timeinfo.tm_sec;
  g_lastDateKey = dateKey;
  g_uiNeedsRedraw = false;

  // No manual invalidation needed — lv_label_set_text / lv_img_set_src
  // already invalidate their objects when content actually changes.
}

static void runLvglLoop() {
  if (!g_lvglReady) return;
  const uint32_t now = millis();
  // Cap LVGL handler cadence to reduce frame thrash on this panel pipeline.
  if (g_lvglLastRunMs != 0 && (now - g_lvglLastRunMs) < 12) return;
  g_lvglLastRunMs = now;
  uint32_t elapsed = now - g_lvglLastTickMs;
  if (elapsed > 50) elapsed = 50;
  if (elapsed == 0) elapsed = 1;
  g_lvglLastTickMs = now;
  lv_tick_inc(elapsed);
  if (g_uiPageMode == UI_PAGE_DOOM) {
    g_canvasDirty = false;
    return;
  }
  lvglApplyPageVisibility(false);
  lvglUpdateWiFiBars(false);

  const uint32_t t0 = micros();
  lv_timer_handler();
  if (g_canvasDirty) {
    dispFlush();
    g_canvasDirty = false;
  }
  const uint32_t dt = micros() - t0;
  g_perfLvglFrameCount++;
  g_perfLvglTotalUs += dt;
  if (dt > g_perfLvglMaxUs) g_perfLvglMaxUs = dt;
}
#endif

static void updateDisplayClock(bool force) {
  if (g_uiPageMode == UI_PAGE_DOOM) return; // direct view owns the display
  if (!g_ntpSynced) return;
  if (!initDisplay()) return;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 50)) return;

  const int dateKey = ((timeinfo.tm_year + 1900) * 10000) + ((timeinfo.tm_mon + 1) * 100) + timeinfo.tm_mday;
  if (!force && !g_uiNeedsRedraw && timeinfo.tm_sec == g_lastClockSecond && dateKey == g_lastDateKey) return;

  setBacklightPercent(100);
  const int16_t canvasW = canvasWidth();
  const int16_t canvasH = canvasHeight();
  const int16_t weatherW = (DISPLAY_WEATHER_PANEL_W > (canvasW / 2)) ? (canvasW / 3) : DISPLAY_WEATHER_PANEL_W;
  const int16_t leftW = canvasW - weatherW - 2;
  const int16_t weatherX = leftW + 2;

  if (force || !g_clockStaticDrawn) {
    fillRectCanvas(0, 0, canvasW, canvasH, DB_COLOR_BLACK);
    Serial.printf("[CLOCK] canvas=%dx%d mode=%s left=%d weather=%d\n",
                  canvasW, canvasH,
                  uiClockModeName(g_uiClockMode),
                  leftW, weatherW);
    g_clockStaticDrawn = true;
    g_lastDateKey = -1;
  }

  drawWordClockInRect(0, 0, leftW, canvasH, timeinfo);
  drawWeatherPanel(weatherX, 0, weatherW, canvasH, timeinfo);

#if DISPLAY_BACKEND_ESP_LCD
  dispFlush();
#endif

  g_lastDateKey = dateKey;
  g_uiNeedsRedraw = false;

  g_lastClockSecond = timeinfo.tm_sec;
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
  if (!g_canvasBuf) {
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

  const uint8_t *src = reinterpret_cast<const uint8_t *>(g_canvasBuf);
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

static void handleSerialCommand(const char *line) {
  if (!line || !*line) return;
  String raw(line);
  raw.trim();
  if (raw.length() == 0) return;
  String cmd(raw);
  cmd.toUpperCase();

  if (cmd == "HELP") {
    Serial.println("[CMD] Commands: HELP, SNAP, VIEW, VIEWFIRST, VIEWLAST, VIEW0, VIEW1, VIEW2, VIEW3, VIEW4, VIEWDOOM, DOOM, VIEWINFO, VIEWHOME, VIEWAUX, VIEWRSS, VIEWWIKI, RSSSTAT, WIKISTAT, RSSRELOAD, WIKIRELOAD, RELOAD, THEME, THEME <id>, LANG, LANG <code>, QRON, QROFF, QRTOGGLE, SAVERON, SAVEROFF, SAVERSTAT, PWRSTAT, NAVSTAT, PWROFF, PWROFFHARD, BATSTAT, RSSDIAG, WEBCFG, WIFIDIRECT, WIFIDIRECT <off|auto|on>");
    return;
  }

  if (cmd == "SNAP" || cmd == "SCREENSHOT") {
#if TEST_DISPLAY && DISPLAY_BACKEND_ESP_LCD
    emitSnapshotOverSerial();
#else
    Serial.println("[SNAP][ERR] non disponibile su questo backend display");
#endif
    return;
  }

  if (cmd == "VIEW" || cmd == "VIEWTOGGLE") {
    (void)stepUiPage(1, true);
    Serial.printf("[UI] page=%s\n", uiPageName(g_uiPageMode));
    return;
  }

  if (cmd == "VIEWFIRST" || cmd == "VIEWHOMEFIRST") {
    jumpToFirstMainView();
    Serial.printf("[UI] page=%s\n", uiPageName(g_uiPageMode));
    return;
  }

  if (cmd == "VIEWLAST" || cmd == "VIEWAUXLAST" || cmd == "VIEWRSSLAST") {
    jumpToLastMainView();
    Serial.printf("[UI] page=%s\n", uiPageName(g_uiPageMode));
    return;
  }

  if (cmd == "VIEW0" || cmd == "VIEWINFO") {
    setUiPage(UI_PAGE_INFO);
    Serial.printf("[UI] page=%s\n", uiPageName(g_uiPageMode));
    return;
  }

  if (cmd == "VIEW1" || cmd == "VIEWHOME") {
    setUiPage(UI_PAGE_HOME);
    Serial.printf("[UI] page=%s\n", uiPageName(g_uiPageMode));
    return;
  }

  if (cmd == "VIEW2" || cmd == "VIEWAUX" || cmd == "VIEWRSS") {
    if (!uiPageEnabled(UI_PAGE_AUX)) {
      Serial.println("[UI] AUX disabled");
      return;
    }
    setUiPage(UI_PAGE_AUX);
    Serial.printf("[UI] page=%s\n", uiPageName(g_uiPageMode));
    return;
  }

  if (cmd == "VIEW3" || cmd == "VIEWWIKI") {
    if (!uiPageEnabled(UI_PAGE_WIKI)) {
      Serial.println("[UI] WIKI disabled");
      return;
    }
    setUiPage(UI_PAGE_WIKI);
    Serial.printf("[UI] page=%s\n", uiPageName(g_uiPageMode));
    return;
  }

  if (cmd == "VIEW4" || cmd == "VIEWDOOM" || cmd == "DOOM") {
    if (!uiPageEnabled(UI_PAGE_DOOM)) {
      Serial.println("[UI] DOOM disabled");
      return;
    }
    setUiPage(UI_PAGE_DOOM);
    Serial.printf("[UI] page=%s\n", uiPageName(g_uiPageMode));
    return;
  }

  if (cmd == "THEME") {
    Serial.printf("[UI] theme='%s' (%s)\n", runtimeUiThemeId(), runtimeUiThemeLabel());
    Serial.print("[UI] themes:");
    for (size_t i = 0; i < UI_THEME_COUNT; ++i) {
      Serial.print(' ');
      Serial.print(kUiThemes[i].id);
    }
    Serial.println();
    return;
  }

  if (cmd == "LANG" || cmd == "LANGSTAT") {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 50)) {
      Serial.printf("[LANG] wc_lang='%s' sentence=<ntp-unavailable>\n", g_wordClockLang);
      return;
    }
    char sentence[96];
    composeWordClockSentenceActive(timeinfo, sentence, sizeof(sentence));
    Serial.printf("[LANG] wc_lang='%s' sentence=\"%s\"\n", g_wordClockLang, sentence);
    return;
  }

  if (cmd.startsWith("LANG ")) {
    String langArg = raw.substring(5);
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
    return;
  }

  if (cmd.startsWith("THEME ")) {
    String themeArg = raw.substring(6);
    themeArg.trim();
    themeArg.toLowerCase();
    if (themeArg.length() == 0) {
      Serial.println("[UI][ERR] THEME richiede un id");
      return;
    }
    const int8_t idx = findUiThemeIndexById(themeArg.c_str());
    if (idx < 0) {
      Serial.printf("[UI][ERR] theme id non valido: '%s'\n", themeArg.c_str());
      return;
    }
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
    return;
  }
  if (cmd == "QRON") {
    setUiPage(UI_PAGE_AUX);
    lvglSetDeckQrModalOpen(g_auxDeck, true);
    Serial.println("[UI] qr=ON");
    return;
  }
  if (cmd == "QROFF") {
    lvglSetDeckQrModalOpen(g_auxDeck, false);
    Serial.println("[UI] qr=OFF");
    return;
  }
  if (cmd == "QRTOGGLE") {
    setUiPage(UI_PAGE_AUX);
    lvglSetDeckQrModalOpen(g_auxDeck, !g_auxDeck.qrModalOpen);
    Serial.printf("[UI] qr=%s\n", g_auxDeck.qrModalOpen ? "ON" : "OFF");
    return;
  }

#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI && SCREENSAVER_ENABLED
  if (cmd == "SAVERON") {
    lvglSetScreenSaverActive(true);
    return;
  }
  if (cmd == "SAVEROFF") {
    lvglSetScreenSaverActive(false);
    markUserInteraction(millis());
    return;
  }
  if (cmd == "SAVERSTAT") {
    const uint32_t now = millis();
    const uint32_t idleTargetMs = lvglScreenSaverIdleTargetMs(now);
    Serial.printf("[SCRNSVR] active=%d idle_ms=%lu last_input_ago=%lu\n",
                  g_lvglScreenSaverActive ? 1 : 0,
                  (unsigned long)idleTargetMs,
                  (unsigned long)(now - g_lastUserInteractionMs));
    return;
  }
#endif

  if (cmd == "PWRSTAT") {
    const int raw = gpio_get_level((gpio_num_t)PWR_BUTTON_PIN);
    Serial.printf("[PWR] stat pin=%d raw=%d pressed=%d hold_ms=%lu\n",
                  PWR_BUTTON_PIN,
                  raw,
                  isPwrButtonPressed() ? 1 : 0,
                  g_pwrButtonDown ? (unsigned long)(millis() - g_pwrButtonDownMs) : 0UL);
    return;
  }
  if (cmd == "NAVSTAT") {
    const int raw = gpio_get_level((gpio_num_t)NAV_FIRST_BUTTON_PIN);
    Serial.printf("[NAV] stat pin=%d raw=%d pressed=%d hold_ms=%lu\n",
                  NAV_FIRST_BUTTON_PIN,
                  raw,
                  isNavFirstButtonPressed() ? 1 : 0,
                  g_navFirstButtonDown ? (unsigned long)(millis() - g_navFirstButtonDownMs) : 0UL);
    return;
  }

  if (cmd == "PWROFF") {
    Serial.println("[PWR] PWROFF command received.");
    shutdownFromPowerButton(false);
    return;
  }

  if (cmd == "PWROFFHARD") {
    Serial.println("[PWR] PWROFFHARD command received.");
    shutdownFromPowerButton(true);
    return;
  }

  if (cmd == "BATSTAT") {
#if TEST_BATTERY
    sampleBatteryNow(millis(), true);
    if (g_battHasSample) {
      Serial.printf("[BATT] stat raw=%d vbat=%.3fV soc=%d%%\n", g_battRaw, g_battVoltage, g_battPercent);
    } else {
      Serial.println("[BATT] stat unavailable");
    }
#else
    Serial.println("[BATT] monitor disabled");
#endif
    return;
  }

  if (cmd == "WEBCFG" || cmd == "WEB") {
#if TEST_WIFI
    ensureRuntimeNetConfig();
#if WEB_CONFIG_ENABLED
    if (g_wifiConnected && g_webConfigServerStarted) {
      Serial.printf("[WEB] url=http://%s:%u\n", WiFi.localIP().toString().c_str(), (unsigned)WEB_CONFIG_PORT);
    } else if (g_wifiConnected) {
      Serial.printf("[WEB] WiFi OK, server not started yet (port=%u)\n", (unsigned)WEB_CONFIG_PORT);
    } else if (g_wifiSetupApActive) {
      Serial.printf("[WEB] setup-ap ssid='%s' url=http://%s:%u\n",
                    g_wifiSetupApSsid,
                    WiFi.softAPIP().toString().c_str(),
                    (unsigned)WEB_CONFIG_PORT);
    } else {
      Serial.println("[WEB] WiFi non connesso");
    }
    Serial.printf("[WEB] city='%s' lat=%.4f lon=%.4f\n",
                  runtimeWeatherCityLabel(),
                  runtimeWeatherLat(),
                  runtimeWeatherLon());
    Serial.printf("[WEB] rss_active='%s' feeds=%u\n",
                  runtimeRssFeedUrl(),
                  (unsigned)runtimeRssConfiguredFeedCount());
    for (uint8_t i = 0; i < RSS_FEED_SLOT_COUNT; ++i) {
      const RuntimeRssFeedConfig *feed = runtimeRssFeedBySlot(i);
      if (!feed || !feed->url[0]) continue;
      Serial.printf("[WEB] rss%u name='%s' max=%u url='%s'\n",
                    (unsigned)(i + 1),
                    feed->name,
                    (unsigned)clampRssFeedMaxItems(feed->maxItems),
                    feed->url);
    }
    if (runtimeLogoUrl()[0]) Serial.printf("[WEB] logo='%s'\n", runtimeLogoUrl());
    else Serial.println("[WEB] logo=''");
    Serial.printf("[WEB] theme='%s' (%s)\n", runtimeUiThemeId(), runtimeUiThemeLabel());
    Serial.printf("[WEB] views info=%d home=1 aux=%d wiki=%d doom=%d\n",
                  (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_INFO) ? 1 : 0,
                  (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_AUX) ? 1 : 0,
                  (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_WIKI) ? 1 : 0,
                  (g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_DOOM) ? 1 : 0);
    Serial.printf("[WEB] lang='%s'\n", g_wordClockLang);
    Serial.printf("[WEB] wifi_setup_mode='%s' setup_ap=%d runtime_known=%u\n",
                  g_wifiSetupMode,
                  g_wifiSetupApActive ? 1 : 0,
                  (unsigned)g_wifiRuntimeCredCount);
#else
    Serial.println("[WEB] config UI disabled (WEB_CONFIG_ENABLED=0)");
#endif
#else
    Serial.println("[WEB] unavailable (TEST_WIFI=0)");
#endif
    return;
  }

  if (cmd == "WIFIDIRECT" || cmd == "WIFISETUP") {
    Serial.printf("[WIFI][DIRECT] mode='%s' ap_active=%d ap_ssid='%s' ap_ip=%s\n",
                  g_wifiSetupMode,
                  g_wifiSetupApActive ? 1 : 0,
                  g_wifiSetupApSsid[0] ? g_wifiSetupApSsid : "-",
                  WiFi.softAPIP().toString().c_str());
    return;
  }

  if (cmd.startsWith("WIFIDIRECT ") || cmd.startsWith("WIFISETUP ")) {
    String mode = raw.substring(raw.indexOf(' ') + 1);
    mode.trim();
    mode.toLowerCase();
    if (!(mode == "off" || mode == "auto" || mode == "on")) {
      Serial.println("[WIFI][DIRECT][ERR] usa: WIFIDIRECT off|auto|on");
      return;
    }
    copyStringSafe(g_wifiSetupMode, sizeof(g_wifiSetupMode), mode.c_str());
    normalizeWifiSetupMode();
    ensureRuntimeNetConfig();
    (void)saveRuntimeNetConfigToNvs();
    wifiHandleSetupModeLoop(millis());
    Serial.printf("[WIFI][DIRECT] mode set -> '%s'\n", g_wifiSetupMode);
    return;
  }

  if (cmd == "RSSDIAG") {
#if TEST_WIFI
    runRssShortenerDiag();
#else
    Serial.println("[CMD] RSSDIAG unavailable (TEST_WIFI=0)");
#endif
    return;
  }

  if (cmd == "RSSSTAT") {
#if TEST_WIFI && RSS_ENABLED
    Serial.printf("[RSSSTAT] valid=%d items=%u idx=%u http=%d fetched='%s' last_fetch=%lu last_attempt=%lu heap=%u psram=%u\n",
                  g_rss.valid ? 1 : 0,
                  (unsigned)g_rss.itemCount,
                  (unsigned)g_rss.currentIndex,
                  g_rss.lastHttpCode,
                  g_rss.fetchedAt,
                  (unsigned long)g_rss.lastFetchMs,
                  (unsigned long)g_rss.lastAttemptMs,
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getFreePsram());
#else
    Serial.println("[RSSSTAT] unavailable");
#endif
    return;
  }

  if (cmd == "WIKISTAT") {
#if TEST_WIFI && RSS_ENABLED
    Serial.printf("[WIKISTAT] valid=%d items=%u idx=%u http=%d fetched='%s' last_fetch=%lu last_attempt=%lu heap=%u psram=%u\n",
                  g_wiki.valid ? 1 : 0,
                  (unsigned)g_wiki.itemCount,
                  (unsigned)g_wiki.currentIndex,
                  g_wiki.lastHttpCode,
                  g_wiki.fetchedAt,
                  (unsigned long)g_wiki.lastFetchMs,
                  (unsigned long)g_wiki.lastAttemptMs,
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getFreePsram());
#else
    Serial.println("[WIKISTAT] unavailable");
#endif
    return;
  }

  if (cmd == "RSSRELOAD") {
#if TEST_WIFI && RSS_ENABLED
    const bool ok = updateRssFromFeed(true);
    Serial.printf("[RSSRELOAD] ok=%d valid=%d items=%u http=%d fetched='%s'\n",
                  ok ? 1 : 0,
                  g_rss.valid ? 1 : 0,
                  (unsigned)g_rss.itemCount,
                  g_rss.lastHttpCode,
                  g_rss.fetchedAt);
#else
    Serial.println("[RSSRELOAD] unavailable");
#endif
    return;
  }

  if (cmd == "WIKIRELOAD") {
#if TEST_WIFI && RSS_ENABLED
    const bool ok = updateWikiFromFeed(true);
    Serial.printf("[WIKIRELOAD] ok=%d valid=%d items=%u http=%d fetched='%s'\n",
                  ok ? 1 : 0,
                  g_wiki.valid ? 1 : 0,
                  (unsigned)g_wiki.itemCount,
                  g_wiki.lastHttpCode,
                  g_wiki.fetchedAt);
#else
    Serial.println("[WIKIRELOAD] unavailable");
#endif
    return;
  }

  if (cmd == "RELOAD") {
#if TEST_WIFI
    const bool weatherOk = updateWeatherFromApi(true);
#if RSS_ENABLED
    const bool rssOk = updateRssFromFeed(true);
    const bool wikiOk = updateWikiFromFeed(true);
    Serial.printf("[RELOAD] weather=%d rss=%d wiki=%d\n", weatherOk ? 1 : 0, rssOk ? 1 : 0, wikiOk ? 1 : 0);
#else
    Serial.printf("[RELOAD] weather=%d rss=0 wiki=0\n", weatherOk ? 1 : 0);
#endif
#else
    Serial.println("[RELOAD] unavailable");
#endif
    return;
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
  g_doomLastTiltSampleMs = 0;
  g_doomTiltFilterReady = false;
#endif
}

static void setImuSensorsActive(bool active) {
  if (!g_imuReady) return;
  if (g_imuSensorsActive == active) return;
  if (active) {
    g_qmi.enableAccelerometer();
    g_qmi.enableGyroscope();
    g_lastAccelMag = 1.0f;
    g_lastImuPrintMs = millis();
    imuResetDoomTiltState();
  } else {
    g_qmi.disableGyroscope();
    g_qmi.disableAccelerometer();
    g_lastShakeMs = 0;
    imuResetDoomTiltState();
  }
  g_imuSensorsActive = active;
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
  g_imuReady = false;

  for (uint8_t i = 0; i < (sizeof(imuCandidates) / sizeof(imuCandidates[0])); ++i) {
    const uint8_t addr = imuCandidates[i];
    if (g_qmi.begin(I2C_MAIN, addr, I2C_SDA_PIN, I2C_SCL_PIN)) {
      g_imuReady = true;
      g_imuAddr = addr;
      break;
    }
  }

  if (!g_imuReady) {
    Serial.println("[FAIL] QMI8658 init fallita su 0x6B/0x6A.");
    Serial.println("[HINT] Verifica I2C MAIN (SDA=47, SCL=48) e alimentazione sensori.");
    return;
  }

  Serial.printf("[OK] QMI8658 trovato su 0x%02X, chipId=0x%02X\n", g_imuAddr, g_qmi.getChipID());

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

  g_lastAccelMag = 1.0f;
  g_lastShakeMs = 0;
  g_lastImuPrintMs = millis();
  g_imuSensorsActive = true;
  setImuSensorsActive(false);

  Serial.printf("[CFG] shake_jerk=%.2fg, shake_gyro=%.1f dps, debounce=%u ms\n",
                IMU_SHAKE_JERK_G, IMU_SHAKE_GYRO_DPS, IMU_SHAKE_DEBOUNCE_MS);
  Serial.println("[IMU] standby until DOOM view is active.");
}

static void runImuLoop() {
  if (!g_imuReady) return;
  if (!g_imuSensorsActive) return;
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
  if (g_doomLastTiltSampleMs != 0 && sampleNow > g_doomLastTiltSampleMs) {
    dt = (float)(sampleNow - g_doomLastTiltSampleMs) / 1000.0f;
    if (dt > 0.05f) dt = 0.05f;
  }
  g_doomLastTiltSampleMs = sampleNow;

  const float moveAccelDeg = atan2f(ay, az) * kDoomRadToDeg;
  const float turnAccelDeg = atan2f(-ax, sqrtf((ay * ay) + (az * az))) * kDoomRadToDeg;

  if (!g_doomTiltFilterReady || dt <= 0.0f) {
    g_doomMoveTiltDeg = moveAccelDeg;
    g_doomTurnTiltDeg = turnAccelDeg;
    g_doomTiltFilterReady = true;
  } else {
    g_doomMoveTiltDeg = (kDoomTiltComplementaryAlpha * (g_doomMoveTiltDeg + (gx * dt))) +
                        ((1.0f - kDoomTiltComplementaryAlpha) * moveAccelDeg);
    g_doomTurnTiltDeg = (kDoomTiltComplementaryAlpha * (g_doomTurnTiltDeg + (gy * dt))) +
                        ((1.0f - kDoomTiltComplementaryAlpha) * turnAccelDeg);
  }

  const bool touchBusy = g_touchDown || g_touchAwaitRelease;
  if (g_doomNeutralPending) {
    const bool canSampleNeutral =
        g_doomTiltFilterReady &&
        sampleNow >= g_doomNeutralArmAtMs &&
        !touchBusy &&
        gyroPeak <= kDoomNeutralCaptureGyroMaxDps;
    if (!canSampleNeutral) {
      g_doomNeutralStableSinceMs = 0;
      g_doomNeutralStableSamples = 0;
      g_doomNeutralAccumMoveDeg = 0.0f;
      g_doomNeutralAccumTurnDeg = 0.0f;
    } else {
      if (g_doomNeutralStableSinceMs == 0) {
        g_doomNeutralStableSinceMs = sampleNow;
        g_doomNeutralStableSamples = 0;
        g_doomNeutralAccumMoveDeg = 0.0f;
        g_doomNeutralAccumTurnDeg = 0.0f;
      }
      g_doomNeutralAccumMoveDeg += g_doomMoveTiltDeg;
      g_doomNeutralAccumTurnDeg += g_doomTurnTiltDeg;
      if (g_doomNeutralStableSamples < UINT16_MAX) ++g_doomNeutralStableSamples;

      if ((sampleNow - g_doomNeutralStableSinceMs) >= kDoomNeutralStableWindowMs &&
          g_doomNeutralStableSamples >= kDoomNeutralStableMinSamples) {
        const float sampleCount = (float)g_doomNeutralStableSamples;
        g_doomNeutralMoveTiltDeg = g_doomNeutralAccumMoveDeg / sampleCount;
        g_doomNeutralTurnTiltDeg = g_doomNeutralAccumTurnDeg / sampleCount;
        g_doomNeutralPending = false;
        g_doomNeutralReady = true;
        g_doomAxisFilterReady = false;
        g_doomMoveDeltaFilteredDeg = 0.0f;
        g_doomTurnDeltaFilteredDeg = 0.0f;
        g_doomMoveBin = 0;
        g_doomTurnBin = 0;
        g_doomFrameDirty = true;
        Serial.printf("[DOOM][IMU] neutral move=%.1f turn=%.1f samples=%u\n",
                      g_doomNeutralMoveTiltDeg,
                      g_doomNeutralTurnTiltDeg,
                      (unsigned)g_doomNeutralStableSamples);
      }
    }
  }

  int8_t moveBin = 0;
  int8_t turnBin = 0;
  if (g_doomNeutralReady) {
    const float moveDeltaDegRaw = (g_doomMoveTiltDeg - g_doomNeutralMoveTiltDeg) * (float)kDoomMoveTiltSign;
    const float turnDeltaDegRaw = (g_doomTurnTiltDeg - g_doomNeutralTurnTiltDeg) * (float)kDoomTurnTiltSign;
    if (!g_doomAxisFilterReady) {
      g_doomMoveDeltaFilteredDeg = moveDeltaDegRaw;
      g_doomTurnDeltaFilteredDeg = turnDeltaDegRaw;
      g_doomAxisFilterReady = true;
    } else {
      g_doomMoveDeltaFilteredDeg += (moveDeltaDegRaw - g_doomMoveDeltaFilteredDeg) * kDoomAxisResponseAlpha;
      g_doomTurnDeltaFilteredDeg += (turnDeltaDegRaw - g_doomTurnDeltaFilteredDeg) * kDoomAxisResponseAlpha;
    }
    moveBin = doomQuantizeAxis(g_doomMoveDeltaFilteredDeg, kDoomMoveDeadbandDeg, kDoomMoveBinDeg,
                               kDoomMoveBinMin, kDoomMoveBinMax);
    turnBin = doomQuantizeAxis(g_doomTurnDeltaFilteredDeg, kDoomTurnDeadbandDeg, kDoomTurnBinDeg,
                               kDoomTurnBinMin, kDoomTurnBinMax);
  } else {
    g_doomAxisFilterReady = false;
    g_doomMoveDeltaFilteredDeg = 0.0f;
    g_doomTurnDeltaFilteredDeg = 0.0f;
  }
  if (moveBin != g_doomMoveBin || turnBin != g_doomTurnBin) {
    g_doomMoveBin = moveBin;
    g_doomTurnBin = turnBin;
    if (g_uiPageMode == UI_PAGE_DOOM) {
      g_doomFrameDirty = true;
#if IMU_VERBOSE_SERIAL
      Serial.printf("[DOOM][IMU] move=%d turn=%d tilt=(%.1f,%.1f) neutral=(%.1f,%.1f)\n",
                    (int)g_doomMoveBin,
                    (int)g_doomTurnBin,
                    g_doomMoveTiltDeg,
                    g_doomTurnTiltDeg,
                    g_doomNeutralMoveTiltDeg,
                    g_doomNeutralTurnTiltDeg);
#endif
    }
  }
#endif

  const float accMag = sqrtf((ax * ax) + (ay * ay) + (az * az));
  const float jerk = fabsf(accMag - g_lastAccelMag);
  g_lastAccelMag = accMag;

  const uint32_t now = millis();
  const bool shake = (jerk >= IMU_SHAKE_JERK_G) || (gyroPeak >= IMU_SHAKE_GYRO_DPS);
  if (shake && ((now - g_lastShakeMs) >= IMU_SHAKE_DEBOUNCE_MS)) {
    g_lastShakeMs = now;
    Serial.printf("[SHAKE] jerk=%.2fg gyro_peak=%.1f dps\n", jerk, gyroPeak);
  }

#if IMU_VERBOSE_SERIAL
  if ((now - g_lastImuPrintMs) >= IMU_PRINT_INTERVAL_MS) {
    g_lastImuPrintMs = now;
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

#if TEST_NTP
  if (wifiIsConnectedNow()) {
    runNtpTimeTest();
    updateWeatherFromApi(true);
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
  if (!g_ntpSynced && wifiIsConnectedNow()) {
    if (g_lastNtpAttemptMs == 0 || (now - g_lastNtpAttemptMs) >= 10000UL) {
      g_lastNtpAttemptMs = now;
      runNtpTimeTest();
    }
  }
#endif

  if ((now - lastHeartbeat) >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeat = now;
    Serial.printf("[HEARTBEAT] uptime=%lu ms, free_heap=%u, free_psram=%u\n",
                  now,
                  ESP.getFreeHeap(),
                  ESP.getFreePsram());
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
#if TEST_WIFI && RSS_ENABLED
#if TEST_LVGL_UI && DISPLAY_BACKEND_ESP_LCD
  if (!uiPageIsFeedDeck(g_uiPageMode)) {
    updateRssFromFeed(false);   // background preload/refresh outside AUX/WIKI to avoid swipe stalls
    updateWikiFromFeed(false);  // background preload/refresh outside AUX/WIKI to avoid swipe stalls
    wikiPreloadMetaStep();
  } else if (g_uiPageMode == UI_PAGE_AUX) {
    // Keep RSS alive even if user stays on AUX for long periods.
    updateRssFromFeed(false);
  } else if (g_uiPageMode == UI_PAGE_WIKI) {
    // Keep Wiki feed refreshing even while user is pinned to WIKI view.
    updateWikiFromFeed(false);
    wikiPreloadVisibleItemStep();  // keep current Wiki card (text summary) enriched while staying on WIKI
  }
#else
  updateRssFromFeed(false);
  updateWikiFromFeed(false);
  wikiPreloadMetaStep();
#endif
#endif
  handleTouchSwipeInput();
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
