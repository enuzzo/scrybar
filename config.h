#pragma once
// SECURITY: keep this file secret-free. Put credentials only in secrets.h (git-ignored).

// Smoke test toggles (1 = enable, 0 = disable)
#define TEST_SERIAL_INFO 1
#define TEST_BACKLIGHT 0
#define TEST_DISPLAY 1
#define TEST_TOUCH 1
#define TEST_I2C_SCAN 0
#define TEST_IMU 0
#define TEST_WIFI 1
#define TEST_NTP 1
#define TEST_BATTERY 1
#define TEST_AUDIO 1

// Global runtime config
#define SERIAL_BAUDRATE 115200
#define HEARTBEAT_INTERVAL_MS 5000
#define SUMMARY_INTERVAL_MS 30000
#define BATTERY_SAMPLE_INTERVAL_MS 15000
#define ENERGY_SAVER_ENABLED 1
#define ENERGY_BACKLIGHT_ON_BATTERY 72
#define SCREENSAVER_ENABLED 1
#define SCREENSAVER_IDLE_USB_MS 10800000UL
#define SCREENSAVER_IDLE_BATTERY_MS 5400000UL
#define SCREENSAVER_STEP_MS 55UL
#define AUDIO_RECORD_SECONDS 20
#define AUDIO_SAMPLE_RATE 24000

// Increment this tag at every firmware edit to confirm Arduino IDE is flashing latest code.
#define FW_BUILD_TAG "DB-M0-r132"
#define FW_RELEASE_DATE "2026-02-13"

// --- M0.2 Backlight test config ---
// From Waveshare schematic:
// - LCD_BL appears on GPIO8
// - BL_EN appears on EXIO1 (TCA9554 IO expander)
#define LCD_BL_PIN 8

// Main I2C bus from Waveshare HW list:
// IO47 = IMU_SDA / RTC_SDA / Audio_SDA (shared bus, includes expander)
// IO48 = IMU_SCL / RTC_SCL / Audio_SCL
#define I2C_SDA_PIN 47
#define I2C_SCL_PIN 48

// Optional secondary bus scan:
// IO17 = TP_SDA, IO18 = TP_SCL
#define I2C_SDA_PIN_ALT 17
#define I2C_SCL_PIN_ALT 18

// Set 1 to use TCA9554 for BL_EN gate, 0 to skip expander.
#define BACKLIGHT_USE_TCA9554 0

// TCA9554 address: set -1 for auto-scan (0x20..0x27).
#define TCA9554_ADDR -1

// BL_EN is expected on EXIO1 (bit 1). Change only if needed.
#define TCA9554_BL_EN_BIT 1
#define TCA9554_BL_EN_ACTIVE_HIGH 1

// Optional global system enable through IO expander.
#define TCA9554_SYS_EN_BIT 6
#define TCA9554_SYS_EN_ACTIVE_HIGH 1
#define BACKLIGHT_FORCE_SYS_EN 1

// Probe EXIO bits 0..7 during backlight test.
// Set to 0 when BL_EN bit is confirmed.
#define BACKLIGHT_PROBE_EXIO_BITS 0

// Matrix test tries combinations of EXIO1/EXIO6 and LCD_BL polarity.
#define BACKLIGHT_MATRIX_TEST 0
#define BACKLIGHT_RAW_SWEEP_TEST 0

// Timing for visible blink test.
#define BACKLIGHT_STEP_DELAY_MS 1200

// --- M0.4 Display (official Waveshare Arduino demo pins) ---
#define LCD_QSPI_CS_PIN 9
#define LCD_QSPI_SCK_PIN 10
#define LCD_QSPI_D0_PIN 11
#define LCD_QSPI_D1_PIN 12
#define LCD_QSPI_D2_PIN 13
#define LCD_QSPI_D3_PIN 14
#define LCD_RST_PIN 21
#define LCD_WIDTH 172
#define LCD_HEIGHT 640
// Final rendering rotation for runtime clock UI (0..3).
#define DISPLAY_ROTATION 0
// Canonical physical orientation:
// - USB-C/power on the LEFT
// - speaker on TOP
// - microphone on BOTTOM
// Keep enabled unless you intentionally mount the board reversed.
#define DISPLAY_FLIP_180 1
// Debug sweep: during M0.4 test show geometry for rotations 0..3.
#define DISPLAY_ROTATION_PROBE 0
#define DISPLAY_ROTATION_PROBE_MS 1400
// Coordinate mapping:
// 0 = use library native coordinates directly
// 1 = use logical landscape canvas (640x172) mapped to native panel (recommended)
#define DISPLAY_COORD_MODE 1
// Display backend:
// 0 = Arduino_GFX
// 1 = Waveshare esp_lcd AXS15231B backend
#define DISPLAY_BACKEND_ESP_LCD 1
// Visual debug and UI tuning
#define DISPLAY_DEBUG_OVERLAY 0
#define DISPLAY_SECONDS_BAR 0
#define DISPLAY_SPLASH_MS 2200
#define DISPLAY_BOOT_COLOR_TEST 0
#define DISPLAY_WEATHER_PANEL_W 250
#define TEST_LVGL_UI 1
#define DISPLAY_TOUCH_SWIPE_MIN_PX 14
#define DISPLAY_TOUCH_TAP_MAX_PX 12
#define DISPLAY_TOUCH_TAP_MAX_MS 350

// Touch controller (AXS15231B) on secondary I2C bus for this board wiring.
#define TOUCH_I2C_ADDR 0x3B

// --- Weather (Open-Meteo / Luino) ---
#define WEATHER_CITY_LABEL "LUINO"
#define WEATHER_LAT 46.0010f
#define WEATHER_LON 8.7450f
#define WEATHER_REFRESH_MS 600000UL
#define WEATHER_RETRY_MS 60000UL

// --- RSS feed (AUX page) ---
#define RSS_ENABLED 1
#define RSS_FEED_URL "https://www.ansa.it/sito/notizie/topnews/topnews_rss.xml"
#define RSS_REFRESH_MS 900000UL
#define RSS_RETRY_MS 120000UL
#define RSS_HTTP_TIMEOUT_MS 8000
#define RSS_ROTATE_MS 15000UL
#define RSS_SHORTENER_RETRY_MS 900000UL
// In-memory cache of long->short URL pairs to avoid repeated API calls.
#define RSS_SHORTENER_CACHE_SIZE 24
// Runtime favicon cache (cleared on reboot).
#define RSS_FAVICON_CACHE_SIZE 12
#define RSS_FAVICON_MAX_BYTES 8192
#define RSS_FAVICON_RETRY_MS 300000UL

// --- Local Web Config (foundation) ---
// Lightweight HTTP server to configure weather/RSS at runtime from LAN.
#define WEB_CONFIG_ENABLED 1
#define WEB_CONFIG_PORT 8080
// Optional external logo URL shown on top of the web page.
#define WEB_CONFIG_LOGO_URL ""

// --- M0.5 IMU (QMI8658) ---
// Shake detection heuristics:
// - jerk threshold on accel magnitude delta (g)
// - or high angular speed on gyro (deg/s)
#define IMU_PRINT_INTERVAL_MS 300
#define IMU_SHAKE_JERK_G 0.45f
#define IMU_SHAKE_GYRO_DPS 120.0f
#define IMU_SHAKE_DEBOUNCE_MS 500

// --- M0.6 WiFi + NTP ---
// Per-SSID connect window used by non-blocking roaming loop.
#define WIFI_CONNECT_TIMEOUT_MS 10000
#define WIFI_POLL_INTERVAL_MS 250
// Force public DNS policy (never router DNS):
// - primary: Google
// - fallback: Cloudflare
#define WIFI_DNS_OVERRIDE_ENABLED 1
#define WIFI_DNS1_IP IPAddress(8, 8, 8, 8)
#define WIFI_DNS2_IP IPAddress(1, 1, 1, 1)

#define NTP_SYNC_TIMEOUT_MS 15000
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.google.com"
// Keep UTC by default; set to your local timezone later (e.g. Europe/Rome rules).
#define NTP_TZ_INFO "CET-1CEST,M3.5.0/2,M10.5.0/3"

// --- Power button / shutdown ---
// Board vendor example maps PWR key to GPIO16 (active low).
#define PWR_BUTTON_PIN 16
#define PWR_BUTTON_ACTIVE_LOW 1
#define PWR_HOLD_SHUTDOWN_MS 5000
#define PWR_HOLD_WAKE_MS 5000
#define PWR_RELEASE_DEBOUNCE_MS 120

// Battery ADC monitor (vendor ADC test uses ADC1 channel 3 with x3 divider).
#define BATTERY_ADC_CHANNEL 3
#define BATTERY_DIVIDER_RATIO 3.0f
#define BATTERY_EMPTY_V 3.30f
#define BATTERY_FULL_V 4.20f

// Try hard power cut through TCA9554 SYS_EN bit before deep sleep fallback.
#define PWR_USE_TCA9554_SYS_EN 1
