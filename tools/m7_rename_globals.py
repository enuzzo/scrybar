#!/usr/bin/env python3
"""M7 — Global state grouping: rename g_ globals to struct members.

This script performs two passes:
1. Replace declaration blocks with struct definitions + instances
2. Rename all g_oldName → g_struct.member across the entire file

Run: python3 tools/m7_rename_globals.py
"""

import re
import sys
import os

FILENAME = "scrybar.ino"

# ── Rename mappings: old_name → new_name ──────────────────────────────────────
# Each group becomes a struct. The struct instance keeps a short g_ prefix.

RENAMES = {
    # ── BatteryState g_batt ───────────────────────────────────────────────────
    "g_battAdcHandle":               "g_batt.adcHandle",
    "g_battReady":                   "g_batt.ready",
    "g_battHasSample":               "g_batt.hasSample",
    "g_battRaw":                     "g_batt.raw",
    "g_battVoltage":                 "g_batt.voltage",
    "g_battPercent":                 "g_batt.percent",
    "g_battLastSampleMs":            "g_batt.lastSampleMs",
    "g_battChargingLikely":          "g_batt.chargingLikely",
    "g_battTrendMs":                 "g_batt.trendMs",
    "g_battTrendVoltage":            "g_batt.trendVoltage",
    "g_battExternalPowerLikely":     "g_batt.externalPowerLikely",
    "g_battExternalPowerHoldUntilMs":"g_batt.externalPowerHoldUntilMs",
    "g_energyBatteryMode":           "g_batt.energySaverActive",
    "g_energyLastEvalMs":            "g_batt.energyLastEvalMs",

    # ── PwrButtonState g_pwrBtn ───────────────────────────────────────────────
    "g_pwrButtonDown":               "g_pwrBtn.down",
    "g_pwrButtonDownMs":             "g_pwrBtn.downMs",
    "g_pwrHoldReported":             "g_pwrBtn.holdReported",
    "g_pwrIgnoreUntilRelease":       "g_pwrBtn.ignoreUntilRelease",
    "g_pwrLastRawLevel":             "g_pwrBtn.lastRawLevel",
    "g_pwrPressCandidateMs":         "g_pwrBtn.pressCandidateMs",
    "g_pwrReleaseCandidateMs":       "g_pwrBtn.releaseCandidateMs",

    # ── NavButtonState g_navBtn ───────────────────────────────────────────────
    "g_navFirstButtonDown":          "g_navBtn.down",
    "g_navFirstButtonDownMs":        "g_navBtn.downMs",
    "g_navFirstLastRawLevel":        "g_navBtn.lastRawLevel",
    "g_navFirstReleaseCandidateMs":  "g_navBtn.releaseCandidateMs",

    # ── WifiState g_wifiSt ────────────────────────────────────────────────────
    "g_wifiConnected":               "g_wifiSt.connected",
    "g_lastWifiDiscReason":          "g_wifiSt.lastDiscReason",
    "g_wifiEventRegistered":         "g_wifiSt.eventRegistered",
    "g_wifiEverConnected":           "g_wifiSt.everConnected",
    "g_wifiLastConnectMs":           "g_wifiSt.lastConnectMs",
    "g_wifiLastDisconnectMs":        "g_wifiSt.lastDisconnectMs",
    "g_wifiStaticSsids":            "g_wifiSt.staticSsids",
    "g_wifiStaticPasswords":        "g_wifiSt.staticPasswords",
    "g_wifiStaticCredCount":        "g_wifiSt.staticCredCount",
    "g_wifiRuntimeCreds":           "g_wifiSt.runtimeCreds",
    "g_wifiRuntimeCredCount":       "g_wifiSt.runtimeCredCount",
    "g_wifiCredSsids":              "g_wifiSt.credSsids",
    "g_wifiCredPasswords":          "g_wifiSt.credPasswords",
    "g_wifiCredCount":              "g_wifiSt.credCount",
    "g_wifiPreferredSsid":          "g_wifiSt.preferredSsid",
    "g_wifiSetupMode":              "g_wifiSt.setupMode",
    "g_wifiSetupApActive":          "g_wifiSt.setupApActive",
    "g_wifiSetupApAutoStarted":     "g_wifiSt.setupApAutoStarted",
    "g_wifiNoLinkSinceMs":          "g_wifiSt.noLinkSinceMs",
    "g_wifiSetupApSsid":            "g_wifiSt.setupApSsid",
    "g_wifiReconnectAttemptActive":  "g_wifiSt.reconnectAttemptActive",
    "g_wifiReconnectIdx":           "g_wifiSt.reconnectIdx",
    "g_wifiReconnectAttemptStartMs": "g_wifiSt.reconnectAttemptStartMs",
    "g_wifiReconnectNextAttemptMs":  "g_wifiSt.reconnectNextAttemptMs",
    "g_wifiConsecutiveFailCount":    "g_wifiSt.consecutiveFailCount",
    "g_wifiLastRadioResetMs":       "g_wifiSt.lastRadioResetMs",
    "g_wifiInternalDisconnect":     "g_wifiSt.internalDisconnect",
    "g_lastNtpAttemptMs":           "g_wifiSt.lastNtpAttemptMs",

    # ── TouchState g_touch ────────────────────────────────────────────────────
    "g_touchReady":                  "g_touch.ready",
    "g_touchUseAltBus":              "g_touch.useAltBus",
    "g_touchDown":                   "g_touch.down",
    "g_touchMissCount":              "g_touch.missCount",
    "g_touchRawPresenceCount":       "g_touch.rawPresenceCount",
    "g_touchPageDragging":           "g_touch.pageDragging",
    "g_touchAuxBtnDown":             "g_touch.auxBtnDown",
    "g_lastSwipeToggleMs":           "g_touch.lastSwipeToggleMs",
    "g_touchAwaitRelease":           "g_touch.awaitRelease",
    "g_touchReleaseStartMs":         "g_touch.releaseStartMs",
    "g_touchStartX":                 "g_touch.startX",
    "g_touchStartY":                 "g_touch.startY",
    "g_touchLastX":                  "g_touch.lastX",
    "g_touchLastY":                  "g_touch.lastY",
    "g_touchStartMs":                "g_touch.startMs",

    # ── DoomState g_doom ──────────────────────────────────────────────────────
    "g_doomPaletteReady":            "g_doom.paletteReady",
    "g_doomFrameDirty":              "g_doom.frameDirty",
    "g_doomLaunchRequested":         "g_doom.launchRequested",
    "g_doomPalette565":              "g_doom.palette565",
    "g_doomLastRenderLogMs":         "g_doom.lastRenderLogMs",
    "g_doomTouchZone":               "g_doom.touchZone",
    "g_doomTiltFilterReady":         "g_doom.tiltFilterReady",
    "g_doomNeutralPending":          "g_doom.neutralPending",
    "g_doomNeutralReady":            "g_doom.neutralReady",
    "g_doomNeutralArmAtMs":          "g_doom.neutralArmAtMs",
    "g_doomNeutralStableSinceMs":    "g_doom.neutralStableSinceMs",
    "g_doomLastTiltSampleMs":        "g_doom.lastTiltSampleMs",
    "g_doomMoveTiltDeg":             "g_doom.moveTiltDeg",
    "g_doomTurnTiltDeg":             "g_doom.turnTiltDeg",
    "g_doomNeutralMoveTiltDeg":      "g_doom.neutralMoveTiltDeg",
    "g_doomNeutralTurnTiltDeg":      "g_doom.neutralTurnTiltDeg",
    "g_doomNeutralAccumMoveDeg":     "g_doom.neutralAccumMoveDeg",
    "g_doomNeutralAccumTurnDeg":     "g_doom.neutralAccumTurnDeg",
    "g_doomNeutralStableSamples":    "g_doom.neutralStableSamples",
    "g_doomAxisFilterReady":         "g_doom.axisFilterReady",
    "g_doomMoveDeltaFilteredDeg":    "g_doom.moveDeltaFilteredDeg",
    "g_doomTurnDeltaFilteredDeg":    "g_doom.turnDeltaFilteredDeg",
    "g_doomMoveBin":                 "g_doom.moveBin",
    "g_doomTurnBin":                 "g_doom.turnBin",

    # ── ImuState g_imu ────────────────────────────────────────────────────────
    "g_imuReady":                    "g_imu.ready",
    "g_imuSensorsActive":            "g_imu.sensorsActive",
    "g_imuAddr":                     "g_imu.addr",
    "g_lastImuPrintMs":              "g_imu.lastPrintMs",
    "g_lastShakeMs":                 "g_imu.lastShakeMs",
    "g_lastAccelMag":                "g_imu.lastAccelMag",

    # ── ClockState g_clock ────────────────────────────────────────────────────
    "g_ntpSynced":                   "g_clock.ntpSynced",
    "g_lastClockSecond":             "g_clock.lastSecond",
    "g_lastDateKey":                 "g_clock.lastDateKey",
    "g_clockStaticDrawn":            "g_clock.staticDrawn",
    "g_bellazioLastMinuteKey":       "g_clock.bellazioLastMinuteKey",
    "g_bellazioLastLeadIdx":         "g_clock.bellazioLastLeadIdx",
    "g_bellazioLastCloserIdx":       "g_clock.bellazioLastCloserIdx",
    "g_uiClockMode":                 "g_clock.mode",

    # ── ScreensaverState g_saver ──────────────────────────────────────────────
    "g_lvglScreenSaverRoot":           "g_saver.root",
    "g_lvglScreenSaverSky":            "g_saver.sky",
    "g_lvglScreenSaverStarObj":        "g_saver.starObj",
    "g_lvglScreenSaverField":          "g_saver.field",
    "g_lvglScreenSaverCow":            "g_saver.cow",
    "g_lvglScreenSaverBalloon":        "g_saver.balloon",
    "g_lvglScreenSaverBalloonTail":    "g_saver.balloonTail",
    "g_lvglScreenSaverFooter":         "g_saver.footer",
    "g_lvglScreenSaverActive":         "g_saver.active",
    "g_lastUserInteractionMs":         "g_saver.lastUserInteractionMs",
    "g_lvglScreenSaverLastStepMs":     "g_saver.lastStepMs",
    "g_lvglScreenSaverRand":           "g_saver.rand",
    "g_lvglScreenSaverX":              "g_saver.x",
    "g_lvglScreenSaverY":              "g_saver.y",
    "g_lvglScreenSaverColorIdx":       "g_saver.colorIdx",
    "g_lvglScreenSaverWakeGuardUntilMs":"g_saver.wakeGuardUntilMs",
    "g_lvglScreenSaverCowNextMoveMs":  "g_saver.cowNextMoveMs",
    "g_lvglScreenSaverCowStepsLeft":   "g_saver.cowStepsLeft",
    "g_lvglScreenSaverCowDir":         "g_saver.cowDir",
    "g_lvglScreenSaverCols":           "g_saver.cols",
    "g_lvglScreenSaverRows":           "g_saver.rows",
    "g_lvglScreenSaverStarX":          "g_saver.starX",
    "g_lvglScreenSaverStarLevel":      "g_saver.starLevel",
    "g_lvglScreenSaverStarDir":        "g_saver.starDir",
    "g_lvglScreenSaverStarNextMs":     "g_saver.starNextMs",
    "g_lvglScreenSaverBalloonIdx":     "g_saver.balloonIdx",
    "g_lvglScreenSaverBalloonNextMs":  "g_saver.balloonNextMs",
    "g_lvglScreenSaverBalloonVisible": "g_saver.balloonVisible",
    "g_lvglScreenSaverFooterNextMs":   "g_saver.footerNextMs",
    "g_lvglScreenSaverFooterJitterNextMs":"g_saver.footerJitterNextMs",
    "g_lvglScreenSaverFooterJitterIdx":"g_saver.footerJitterIdx",
    "g_lvglScreenSaverFieldNextMs":    "g_saver.fieldNextMs",
    "g_lvglScreenSaverFieldScroll":    "g_saver.fieldScroll",
    "g_lvglScreenSaverFieldBuf":       "g_saver.fieldBuf",

    # ── PerfCounters g_perf ───────────────────────────────────────────────────
    "g_perfFlushCount":              "g_perf.flushCount",
    "g_perfFlushTotalUs":            "g_perf.flushTotalUs",
    "g_perfFlushMaxUs":              "g_perf.flushMaxUs",
    "g_perfLvglFrameCount":          "g_perf.lvglFrameCount",
    "g_perfLvglTotalUs":             "g_perf.lvglTotalUs",
    "g_perfLvglMaxUs":               "g_perf.lvglMaxUs",
    "g_perfLastResetMs":             "g_perf.lastResetMs",

    # ── WebConfigState g_webCfg ───────────────────────────────────────────────
    "g_webConfigServer":             "g_webCfg.server",
    "g_webConfigServerStarted":      "g_webCfg.serverStarted",
    "g_webConfigRoutesRegistered":   "g_webCfg.routesRegistered",
    "g_webConfigDnsServer":          "g_webCfg.dnsServer",
    "g_webConfigDnsStarted":         "g_webCfg.dnsStarted",
    "g_scrybarMdnsStarted":         "g_webCfg.mdnsStarted",
    "g_scrybarMdnsHost":            "g_webCfg.mdnsHost",
    "g_scrybarMdnsInstanceName":    "g_webCfg.mdnsInstanceName",
    "g_webQrTempBuf":               "g_webCfg.qrTempBuf",
    "g_webQrDataBuf":               "g_webCfg.qrDataBuf",

    # ── LvglClockUi g_clockUi ────────────────────────────────────────────────
    "g_lvglClockL1":                 "g_clockUi.l1",
    "g_lvglClockL2":                 "g_clockUi.l2",
    "g_lvglClockL3":                 "g_clockUi.l3",
    "g_lvglClockDate":               "g_clockUi.date",
    "g_lvglClockHeader":             "g_clockUi.header",
    "g_lvglClockHeaderFill":         "g_clockUi.headerFill",
    "g_lvglClockWiFiBars":           "g_clockUi.wifiBars",
    "g_lvglClockWiFiMask":           "g_clockUi.wifiMask",
    "g_lvglClockDivider":            "g_clockUi.divider",
    "g_lvglClockBlock":              "g_clockUi.block",

    # ── LvglWeatherUi g_weatherUi ────────────────────────────────────────────
    "g_lvglWeatherCard":             "g_weatherUi.card",
    "g_lvglWeatherHeader":           "g_weatherUi.header",
    "g_lvglWeatherHeaderFill":       "g_weatherUi.headerFill",
    "g_lvglWeatherBody":             "g_weatherUi.body",
    "g_lvglCity":                    "g_weatherUi.city",
    "g_lvglCityTickerScroll":        "g_weatherUi.cityTickerScroll",
    "g_lvglCityTickerNextMs":        "g_weatherUi.cityTickerNextMs",
    "g_lvglCityTickerEndMs":         "g_weatherUi.cityTickerEndMs",
    "g_lvglCityRawLast":             "g_weatherUi.cityRawLast",
    "g_lvglTemp":                    "g_weatherUi.temp",
    "g_lvglIcon":                    "g_weatherUi.icon",
    "g_lvglGlyph":                   "g_weatherUi.glyph",
    "g_lvglDesc":                    "g_weatherUi.desc",
    "g_lvglHumidity":                "g_weatherUi.humidity",
    "g_lvglSun":                     "g_weatherUi.sun",
    "g_lvglWind":                    "g_weatherUi.wind",
    "g_lvglWeatherSep":              "g_weatherUi.sep",
    "g_lvglForecastBar":             "g_weatherUi.forecastBar",
    "g_lvglForecastBarFill":         "g_weatherUi.forecastBarFill",
    "g_lvglForecastIcon":            "g_weatherUi.forecastIcon",
    "g_lvglForecastNow":             "g_weatherUi.forecastNow",
    "g_lvglForecastTomorrow":        "g_weatherUi.forecastTomorrow",

    # ── LvglInfoUi g_infoUi ──────────────────────────────────────────────────
    "g_lvglInfoRoot":                "g_infoUi.root",
    "g_lvglInfoCard":                "g_infoUi.card",
    "g_lvglInfoHeader":              "g_infoUi.header",
    "g_lvglInfoHeaderFill":          "g_infoUi.headerFill",
    "g_lvglInfoTitle":               "g_infoUi.title",
    "g_lvglInfoEndpoint":            "g_infoUi.endpoint",
    "g_lvglInfoBodyLeft":            "g_infoUi.bodyLeft",
    "g_lvglInfoBodyRight":           "g_infoUi.bodyRight",
    "g_lvglInfoWebQr":               "g_infoUi.webQr",
    "g_lvglInfoLastQrPayload":       "g_infoUi.lastQrPayload",

    # ── DisplayHwState g_dispHw ───────────────────────────────────────────────
    "g_panelIo":                     "g_dispHw.panelIo",
    "g_panel":                       "g_dispHw.panel",
    "g_dispFlushSem":                "g_dispHw.flushSem",
    "g_canvasBuf":                   "g_dispHw.canvasBuf",
    "g_canvasDirty":                 "g_dispHw.canvasDirty",
    "g_dmaBuf":                      "g_dispHw.dmaBuf",
    "g_dmaBuf2":                     "g_dispHw.dmaBuf2",

    # ── LvglPageState g_pageAnim ──────────────────────────────────────────────
    "g_lvglPageAnimUntilMs":         "g_pageAnim.untilMs",
    "g_lvglLastRunMs":               "g_pageAnim.lastRunMs",
    "g_lvglPageDragActive":          "g_pageAnim.dragActive",
}

def apply_renames(text):
    """Apply all renames using word-boundary-aware replacement.

    Process longer names first to avoid partial matches.
    """
    # Sort by length descending so longer names match first
    sorted_renames = sorted(RENAMES.items(), key=lambda x: -len(x[0]))

    for old, new in sorted_renames:
        # Use word boundary to avoid partial matches
        pattern = r'\b' + re.escape(old) + r'\b'
        text = re.sub(pattern, new, text)

    return text


# ── Struct definitions to insert ──────────────────────────────────────────────
# These replace the individual variable declarations.

# Map: marker comment → struct definition + instance
# We'll insert struct defs by replacing specific declaration blocks.

STRUCT_BATTERY = """\
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
static BatteryState g_batt;"""

STRUCT_PWR_BUTTON = """\
struct PwrButtonState {
  bool down = false;
  uint32_t downMs = 0;
  bool holdReported = false;
  bool ignoreUntilRelease = false;
  int lastRawLevel = -1;
  uint32_t pressCandidateMs = 0;
  uint32_t releaseCandidateMs = 0;
};
static PwrButtonState g_pwrBtn;"""

STRUCT_NAV_BUTTON = """\
struct NavButtonState {
  bool down = false;
  uint32_t downMs = 0;
  int lastRawLevel = -1;
  uint32_t releaseCandidateMs = 0;
};
static NavButtonState g_navBtn;"""

STRUCT_WIFI = """\
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
static WifiState g_wifiSt;"""

STRUCT_TOUCH = """\
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
static TouchState g_touch;"""

STRUCT_DOOM = """\
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
static DoomState g_doom;"""

STRUCT_IMU = """\
struct ImuState {
  bool ready = false;
  bool sensorsActive = false;
  uint8_t addr = 0;
  uint32_t lastPrintMs = 0;
  uint32_t lastShakeMs = 0;
  float lastAccelMag = 1.0f;
};
static ImuState g_imu;"""

STRUCT_CLOCK = """\
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
static ClockState g_clock;"""

STRUCT_SCREENSAVER = """\
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
};
static ScreensaverState g_saver;"""

STRUCT_PERF = """\
struct PerfCounters {
  uint32_t flushCount = 0;
  uint32_t flushTotalUs = 0;
  uint32_t flushMaxUs = 0;
  uint32_t lvglFrameCount = 0;
  uint32_t lvglTotalUs = 0;
  uint32_t lvglMaxUs = 0;
  uint32_t lastResetMs = 0;
};
static PerfCounters g_perf;"""

STRUCT_WEBCFG = """\
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
static WebConfigState g_webCfg;"""

STRUCT_CLOCK_UI = """\
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
static LvglClockUi g_clockUi;"""

STRUCT_WEATHER_UI = """\
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
static LvglWeatherUi g_weatherUi;"""

STRUCT_INFO_UI = """\
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
static LvglInfoUi g_infoUi;"""

STRUCT_DISP_HW = """\
struct DisplayHwState {
  esp_lcd_panel_io_handle_t panelIo = nullptr;
  esp_lcd_panel_handle_t panel = nullptr;
  SemaphoreHandle_t flushSem = nullptr;
  uint16_t *canvasBuf = nullptr;
  bool canvasDirty = false;
  uint16_t *dmaBuf = nullptr;
  uint16_t *dmaBuf2 = nullptr;
};
static DisplayHwState g_dispHw;"""

STRUCT_PAGE_ANIM = """\
struct LvglPageAnimState {
  uint32_t untilMs = 0;
  uint32_t lastRunMs = 0;
  bool dragActive = false;
};
static LvglPageAnimState g_pageAnim;"""


def replace_declaration_blocks(text):
    """Replace groups of individual global declarations with struct defs + instances."""

    # ── Power button (lines ~176-182) ─────────────────────────────────────────
    text = text.replace(
        "static bool g_pwrButtonDown = false;\n"
        "static uint32_t g_pwrButtonDownMs = 0;\n"
        "static bool g_pwrHoldReported = false;\n"
        "static bool g_pwrIgnoreUntilRelease = false;\n"
        "static int g_pwrLastRawLevel = -1;\n"
        "static uint32_t g_pwrPressCandidateMs = 0;\n"
        "static uint32_t g_pwrReleaseCandidateMs = 0;",
        STRUCT_PWR_BUTTON, 1)

    # ── Nav button (lines ~185-188) ───────────────────────────────────────────
    text = text.replace(
        "static bool g_navFirstButtonDown = false;\n"
        "static uint32_t g_navFirstButtonDownMs = 0;\n"
        "static int g_navFirstLastRawLevel = -1;\n"
        "static uint32_t g_navFirstReleaseCandidateMs = 0;",
        STRUCT_NAV_BUTTON, 1)

    # ── Battery (lines ~191-202) ──────────────────────────────────────────────
    text = text.replace(
        "static adc_oneshot_unit_handle_t g_battAdcHandle = nullptr;\n"
        "static bool g_battReady = false;\n"
        "static bool g_battHasSample = false;\n"
        "static int g_battRaw = 0;\n"
        "static float g_battVoltage = 0.0f;\n"
        "static int g_battPercent = -1;\n"
        "static uint32_t g_battLastSampleMs = 0;\n"
        "static bool g_battChargingLikely = false;\n"
        "static uint32_t g_battTrendMs = 0;\n"
        "static float g_battTrendVoltage = 0.0f;\n"
        "static bool g_battExternalPowerLikely = false;\n"
        "static uint32_t g_battExternalPowerHoldUntilMs = 0;",
        STRUCT_BATTERY, 1)

    # ── Energy (lines ~206-207) — folded into BatteryState ────────────────────
    text = text.replace(
        "#if TEST_BATTERY && ENERGY_SAVER_ENABLED\n"
        "static bool g_energyBatteryMode = false;\n"
        "static uint32_t g_energyLastEvalMs = 0;\n"
        "#endif",
        "// Energy saver fields folded into BatteryState (g_batt)", 1)

    # ── WiFi (lines ~218-245) ─────────────────────────────────────────────────
    text = text.replace(
        "static bool g_wifiConnected = false;\n"
        "static int g_lastWifiDiscReason = -1;\n"
        "static bool g_wifiEventRegistered = false;\n"
        "static bool g_wifiEverConnected = false;\n"
        "static uint32_t g_wifiLastConnectMs = 0;\n"
        "static uint32_t g_wifiLastDisconnectMs = 0;\n"
        "static const char *g_wifiStaticSsids[WIFI_STATIC_CREDENTIALS_MAX] = {nullptr};\n"
        "static const char *g_wifiStaticPasswords[WIFI_STATIC_CREDENTIALS_MAX] = {nullptr};\n"
        "static size_t g_wifiStaticCredCount = 0;\n"
        "static RuntimeWiFiCredential g_wifiRuntimeCreds[WIFI_RUNTIME_CREDENTIALS_MAX];\n"
        "static uint8_t g_wifiRuntimeCredCount = 0;\n"
        "static const char *g_wifiCredSsids[WIFI_TOTAL_CREDENTIALS_MAX] = {nullptr};\n"
        "static const char *g_wifiCredPasswords[WIFI_TOTAL_CREDENTIALS_MAX] = {nullptr};\n"
        "static size_t g_wifiCredCount = 0;\n"
        "static char g_wifiPreferredSsid[33] = \"\";\n"
        "static char g_wifiSetupMode[8] = WIFI_SETUP_MODE_DEFAULT;  // off | auto | on\n"
        "static bool g_wifiSetupApActive = false;\n"
        "static bool g_wifiSetupApAutoStarted = false;\n"
        "static uint32_t g_wifiNoLinkSinceMs = 0;\n"
        "static char g_wifiSetupApSsid[33] = \"\";\n"
        "static bool g_wifiReconnectAttemptActive = false;\n"
        "static uint8_t g_wifiReconnectIdx = 0;\n"
        "static uint32_t g_wifiReconnectAttemptStartMs = 0;\n"
        "static uint32_t g_wifiReconnectNextAttemptMs = 0;\n"
        "static uint16_t g_wifiConsecutiveFailCount = 0;\n"
        "static uint32_t g_wifiLastRadioResetMs = 0;\n"
        "static bool g_wifiInternalDisconnect = false;\n"
        "static uint32_t g_lastNtpAttemptMs = 0;",
        STRUCT_WIFI, 1)

    # ── Web config (lines ~676-691) ───────────────────────────────────────────
    text = text.replace(
        "#if WEB_CONFIG_ENABLED\n"
        "static WebServer g_webConfigServer(WEB_CONFIG_PORT);\n"
        "static bool g_webConfigServerStarted = false;\n"
        "static bool g_webConfigRoutesRegistered = false;\n"
        "static DNSServer g_webConfigDnsServer;\n"
        "static bool g_webConfigDnsStarted = false;\n"
        "#if DB_HAS_MDNS\n"
        "static bool g_scrybarMdnsStarted = false;\n"
        "static char g_scrybarMdnsHost[32] = {0};\n"
        "static char g_scrybarMdnsInstanceName[40] = {0};\n"
        "#endif\n"
        "#if DB_HAS_QRCODEGEN\n"
        "// Allocate QR work buffers lazily in PSRAM to keep internal heap free for TLS.\n"
        "static uint8_t *g_webQrTempBuf = nullptr;\n"
        "static uint8_t *g_webQrDataBuf = nullptr;\n"
        "#endif\n"
        "#endif",
        "#if WEB_CONFIG_ENABLED\n" + STRUCT_WEBCFG + "\n#endif", 1)

    # ── Clock (lines ~775-786) ────────────────────────────────────────────────
    text = text.replace(
        "static bool g_ntpSynced = false;\n"
        "static int g_lastClockSecond = -1;\n"
        "static int g_lastDateKey = -1;\n"
        "static bool g_clockStaticDrawn = false;\n"
        "static uint32_t g_bellazioLastMinuteKey = 0xFFFFFFFFu;\n"
        "static uint8_t g_bellazioLastLeadIdx = 0;\n"
        "static uint8_t g_bellazioLastCloserIdx = 0;",
        "// ClockState: UiClockMode enum must be defined first (below)", 1)

    # The UiClockMode and g_uiClockMode are declared just after, so we handle it there
    text = text.replace(
        "static UiClockMode g_uiClockMode = UI_CLOCK_MODE_WORDCLOCK;",
        STRUCT_CLOCK, 1)

    # Remove the placeholder comment we just inserted
    text = text.replace(
        "// ClockState: UiClockMode enum must be defined first (below)\n",
        "", 1)

    # ── LVGL widget declarations ──────────────────────────────────────────────
    # Clock UI widgets (lines ~805-813)
    text = text.replace(
        "static lv_obj_t *g_lvglClockL1 = nullptr;\n"
        "static lv_obj_t *g_lvglClockL2 = nullptr;\n"
        "static lv_obj_t *g_lvglClockL3 = nullptr;\n"
        "static lv_obj_t *g_lvglClockDate = nullptr;\n"
        "static lv_obj_t *g_lvglClockHeader = nullptr;\n"
        "static lv_obj_t *g_lvglClockHeaderFill = nullptr;\n"
        "static lv_obj_t *g_lvglClockWiFiBars[4] = {nullptr, nullptr, nullptr, nullptr};\n"
        "static uint16_t g_lvglClockWiFiMask = 0xFFFF;\n"
        "static lv_obj_t *g_lvglClockDivider = nullptr;",
        STRUCT_CLOCK_UI, 1)

    # Info UI widgets (lines ~814-825)
    text = text.replace(
        "static lv_obj_t *g_lvglInfoRoot = nullptr;\n"
        "static lv_obj_t *g_lvglInfoCard = nullptr;\n"
        "static lv_obj_t *g_lvglInfoHeader = nullptr;\n"
        "static lv_obj_t *g_lvglInfoHeaderFill = nullptr;\n"
        "static lv_obj_t *g_lvglInfoTitle = nullptr;\n"
        "static lv_obj_t *g_lvglInfoEndpoint = nullptr;\n"
        "static lv_obj_t *g_lvglInfoBodyLeft = nullptr;\n"
        "static lv_obj_t *g_lvglInfoBodyRight = nullptr;\n"
        "#if defined(LV_USE_QRCODE) && LV_USE_QRCODE\n"
        "static lv_obj_t *g_lvglInfoWebQr = nullptr;\n"
        "#endif\n"
        "static char g_lvglInfoLastQrPayload[96] = {0};",
        STRUCT_INFO_UI, 1)

    # Weather UI + Home root + clock block (lines ~826-849)
    text = text.replace(
        "static lv_obj_t *g_lvglHomeRoot = nullptr;\n"
        "static lv_obj_t *g_lvglClockBlock = nullptr;\n"
        "static lv_obj_t *g_lvglWeatherCard = nullptr;\n"
        "static lv_obj_t *g_lvglWeatherHeader = nullptr;\n"
        "static lv_obj_t *g_lvglWeatherHeaderFill = nullptr;\n"
        "static lv_obj_t *g_lvglWeatherBody = nullptr;\n"
        "static lv_obj_t *g_lvglCity = nullptr;\n"
        "static bool g_lvglCityTickerScroll = false;\n"
        "static uint32_t g_lvglCityTickerNextMs = 0;\n"
        "static uint32_t g_lvglCityTickerEndMs = 0;\n"
        "static char g_lvglCityRawLast[48] = {0};\n"
        "static lv_obj_t *g_lvglTemp = nullptr;\n"
        "static lv_obj_t *g_lvglIcon = nullptr;\n"
        "static lv_obj_t *g_lvglGlyph = nullptr;\n"
        "static lv_obj_t *g_lvglDesc = nullptr;\n"
        "static lv_obj_t *g_lvglHumidity = nullptr;\n"
        "static lv_obj_t *g_lvglSun = nullptr;\n"
        "static lv_obj_t *g_lvglWind = nullptr;\n"
        "static lv_obj_t *g_lvglWeatherSep = nullptr;\n"
        "static lv_obj_t *g_lvglForecastBar = nullptr;\n"
        "static lv_obj_t *g_lvglForecastBarFill = nullptr;\n"
        "static lv_obj_t *g_lvglForecastIcon = nullptr;\n"
        "static lv_obj_t *g_lvglForecastNow = nullptr;\n"
        "static lv_obj_t *g_lvglForecastTomorrow = nullptr;",
        "static lv_obj_t *g_lvglHomeRoot = nullptr;\n" + STRUCT_WEATHER_UI, 1)

    # ── Doom state (lines ~1062-1110) ─────────────────────────────────────────
    text = text.replace(
        "static bool      g_doomPaletteReady = false;\n"
        "static bool      g_doomFrameDirty   = true;\n"
        "static bool      g_doomLaunchRequested = false;\n"
        "static uint16_t  g_doomPalette565[256] = {0};\n"
        "static uint32_t  g_doomLastRenderLogMs = 0;",
        "// DoomState: basic fields", 1)

    text = text.replace(
        "static uint8_t g_doomTouchZone = DOOM_TOUCH_NONE;\n"
        "static bool g_doomTiltFilterReady = false;\n"
        "static bool g_doomNeutralPending = false;\n"
        "static bool g_doomNeutralReady = false;\n"
        "static uint32_t g_doomNeutralArmAtMs = 0;\n"
        "static uint32_t g_doomNeutralStableSinceMs = 0;\n"
        "static uint32_t g_doomLastTiltSampleMs = 0;\n"
        "static float g_doomMoveTiltDeg = 0.0f;\n"
        "static float g_doomTurnTiltDeg = 0.0f;\n"
        "static float g_doomNeutralMoveTiltDeg = 0.0f;\n"
        "static float g_doomNeutralTurnTiltDeg = 0.0f;\n"
        "static float g_doomNeutralAccumMoveDeg = 0.0f;\n"
        "static float g_doomNeutralAccumTurnDeg = 0.0f;\n"
        "static uint16_t g_doomNeutralStableSamples = 0;\n"
        "static bool g_doomAxisFilterReady = false;\n"
        "static float g_doomMoveDeltaFilteredDeg = 0.0f;\n"
        "static float g_doomTurnDeltaFilteredDeg = 0.0f;\n"
        "static int8_t g_doomMoveBin = 0;\n"
        "static int8_t g_doomTurnBin = 0;",
        "// DoomState: tilt/neutral fields", 1)

    # Now replace both markers with the full struct
    text = text.replace(
        "// DoomState: basic fields",
        STRUCT_DOOM, 1)
    text = text.replace(
        "// DoomState: tilt/neutral fields",
        "// (tilt/neutral fields folded into DoomState above)", 1)

    # ── Page anim state (lines ~1112-1114) ────────────────────────────────────
    text = text.replace(
        "static uint32_t g_lvglPageAnimUntilMs = 0;\n"
        "static uint32_t g_lvglLastRunMs = 0;\n"
        "static bool g_lvglPageDragActive = false;",
        STRUCT_PAGE_ANIM, 1)

    # ── Screensaver (lines ~1119-1152) ────────────────────────────────────────
    text = text.replace(
        "static lv_obj_t *g_lvglScreenSaverRoot = nullptr;\n"
        "static lv_obj_t *g_lvglScreenSaverSky = nullptr;\n"
        "static lv_obj_t *g_lvglScreenSaverStarObj[kSaverSkyRowsMax][kSaverStarsPerRow] = {};\n"
        "static lv_obj_t *g_lvglScreenSaverField = nullptr;\n"
        "static lv_obj_t *g_lvglScreenSaverCow = nullptr;\n"
        "static lv_obj_t *g_lvglScreenSaverBalloon = nullptr;\n"
        "static lv_obj_t *g_lvglScreenSaverBalloonTail = nullptr;\n"
        "static lv_obj_t *g_lvglScreenSaverFooter = nullptr;\n"
        "static bool g_lvglScreenSaverActive = false;\n"
        "static uint32_t g_lastUserInteractionMs = 0;\n"
        "static uint32_t g_lvglScreenSaverLastStepMs = 0;\n"
        "static uint32_t g_lvglScreenSaverRand = 0x1A2B3C4Du;\n"
        "static int16_t g_lvglScreenSaverX = -80;\n"
        "static int16_t g_lvglScreenSaverY = 0;\n"
        "static int8_t g_lvglScreenSaverColorIdx = 0;\n"
        "static uint32_t g_lvglScreenSaverWakeGuardUntilMs = 0;\n"
        "static uint32_t g_lvglScreenSaverCowNextMoveMs = 0;\n"
        "static uint8_t g_lvglScreenSaverCowStepsLeft = 0;\n"
        "static int8_t g_lvglScreenSaverCowDir = 1;\n"
        "static uint8_t g_lvglScreenSaverCols = 60;\n"
        "static uint8_t g_lvglScreenSaverRows = 6;\n"
        "static uint8_t g_lvglScreenSaverStarX[kSaverSkyRowsMax][kSaverStarsPerRow] = {};\n"
        "static uint8_t g_lvglScreenSaverStarLevel[kSaverSkyRowsMax][kSaverStarsPerRow] = {};\n"
        "static int8_t g_lvglScreenSaverStarDir[kSaverSkyRowsMax][kSaverStarsPerRow] = {};\n"
        "static uint32_t g_lvglScreenSaverStarNextMs[kSaverSkyRowsMax][kSaverStarsPerRow] = {};\n"
        "static uint8_t g_lvglScreenSaverBalloonIdx = 0;\n"
        "static uint32_t g_lvglScreenSaverBalloonNextMs = 0;\n"
        "static bool g_lvglScreenSaverBalloonVisible = false;\n"
        "static uint32_t g_lvglScreenSaverFooterNextMs = 0;\n"
        "static uint32_t g_lvglScreenSaverFooterJitterNextMs = 0;\n"
        "static uint8_t g_lvglScreenSaverFooterJitterIdx = 0;\n"
        "static uint32_t g_lvglScreenSaverFieldNextMs = 0;\n"
        "static uint8_t g_lvglScreenSaverFieldScroll = 0;\n"
        "static char g_lvglScreenSaverFieldBuf[256] = {0};",
        STRUCT_SCREENSAVER, 1)

    # ── Touch (lines ~1157-1177) ──────────────────────────────────────────────
    text = text.replace(
        "static bool g_touchReady = false;\n"
        "static bool g_touchUseAltBus = true;\n"
        "static bool g_touchDown = false;\n"
        "static uint8_t g_touchMissCount = 0;  // consecutive \"no touch\" frames since last detection\n"
        "static uint8_t g_touchRawPresenceCount = 0;\n"
        "static bool g_touchPageDragging = false;",
        "// TouchState: first block", 1)

    text = text.replace(
        "static TouchAuxButton g_touchAuxBtnDown = TOUCH_AUX_BTN_NONE;\n"
        "static uint32_t g_lastSwipeToggleMs = 0;\n"
        "static bool g_touchAwaitRelease = false;\n"
        "static uint32_t g_touchReleaseStartMs = 0;\n"
        "static int16_t g_touchStartX = 0;\n"
        "static int16_t g_touchStartY = 0;\n"
        "static int16_t g_touchLastX = 0;\n"
        "static int16_t g_touchLastY = 0;\n"
        "static uint32_t g_touchStartMs = 0;",
        "// TouchState: second block", 1)

    text = text.replace("// TouchState: first block", STRUCT_TOUCH, 1)
    text = text.replace("// TouchState: second block",
                        "// (remaining touch fields folded into TouchState above)", 1)

    # ── IMU (lines ~1181-1191) ────────────────────────────────────────────────
    text = text.replace(
        "static bool g_imuReady = false;\n"
        "static bool g_imuSensorsActive = false;\n"
        "static uint8_t g_imuAddr = 0;\n"
        "static uint32_t g_lastImuPrintMs = 0;\n"
        "static uint32_t g_lastShakeMs = 0;\n"
        "static float g_lastAccelMag = 1.0f;",
        STRUCT_IMU, 1)

    # ── Display HW (lines ~1209-1215,1231) ────────────────────────────────────
    text = text.replace(
        "static esp_lcd_panel_io_handle_t g_panelIo = nullptr;\n"
        "static esp_lcd_panel_handle_t g_panel = nullptr;\n"
        "static SemaphoreHandle_t g_dispFlushSem = nullptr;\n"
        "static uint16_t *g_canvasBuf = nullptr;  // logical 640x172\n"
        "// g_rotBuf eliminated — rotation now done directly into DMA chunks\n"
        "static uint16_t *g_dmaBuf = nullptr;     // native chunk (172x32) — ping\n"
        "static uint16_t *g_dmaBuf2 = nullptr;    // native chunk (172x32) — pong",
        STRUCT_DISP_HW, 1)

    # canvasDirty is separate, after perf counters
    text = text.replace(
        "static bool g_canvasDirty = false;  // set by flush callback, cleared after dispFlush",
        "// g_canvasDirty folded into DisplayHwState (g_dispHw)", 1)

    # ── Perf counters (lines ~1224-1230) ──────────────────────────────────────
    text = text.replace(
        "// --- Frame performance counters (lightweight, no per-frame logging) ---\n"
        "static uint32_t g_perfFlushCount = 0;\n"
        "static uint32_t g_perfFlushTotalUs = 0;\n"
        "static uint32_t g_perfFlushMaxUs = 0;\n"
        "static uint32_t g_perfLvglFrameCount = 0;\n"
        "static uint32_t g_perfLvglTotalUs = 0;\n"
        "static uint32_t g_perfLvglMaxUs = 0;\n"
        "static uint32_t g_perfLastResetMs = 0;",
        "// --- Frame performance counters (lightweight, no per-frame logging) ---\n" + STRUCT_PERF, 1)

    return text


def main():
    filepath = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), FILENAME)
    if not os.path.exists(filepath):
        print(f"Error: {filepath} not found")
        sys.exit(1)

    print(f"Reading {filepath}...")
    with open(filepath, 'r') as f:
        text = f.read()

    original_len = len(text.splitlines())

    # Phase 1: Replace declaration blocks with struct defs
    print("Phase 1: Replacing declaration blocks with struct definitions...")
    text = replace_declaration_blocks(text)

    # Phase 2: Rename all g_ references
    print("Phase 2: Applying global renames...")
    text = apply_renames(text)

    # Write result
    print(f"Writing {filepath}...")
    with open(filepath, 'w') as f:
        f.write(text)

    new_len = len(text.splitlines())
    print(f"Done. Lines: {original_len} -> {new_len}")

    # Count remaining g_ globals
    remaining = set()
    for line in text.splitlines():
        m = re.match(r'^static\s+.*\b(g_\w+)', line)
        if m:
            name = m.group(1)
            # Skip struct member access patterns (they have dots)
            if '.' not in name:
                remaining.add(name)

    print(f"\nRemaining g_ globals: {len(remaining)}")
    for name in sorted(remaining):
        print(f"  {name}")


if __name__ == "__main__":
    main()
