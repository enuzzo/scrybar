# LAUNCH Page Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a rocket launch departure board page (index 7) with live T-minus countdown, hero + compact rows, QR code, and tap-to-detail overlay.

**Architecture:** Single-file integration into scrybar.ino following the Transit page pattern. Data from free RocketLaunch.Live API (`/json/launches/next/5`, no key). Background fetch on Core 1 via `netEnqueue`, 1-second countdown tick on main loop, LVGL widgets for display.

**Tech Stack:** C++ (Arduino/ESP32-S3), LVGL 8.x, HTTPClient, manual JSON parsing (`strstr`/`memmem`).

**Spec:** `docs/superpowers/specs/2026-04-08-launch-page-design.md`

---

## File Structure

All changes in two files:
- **Modify:** `config.h` — add Launch constants and structs
- **Modify:** `scrybar.ino` — add page enum, state, UI, fetch, init, update, countdown

No new files. Follows existing monolithic pattern.

---

### Task 1: Config Constants and Data Structs (config.h)

**Files:**
- Modify: `config.h:86` (after Transit section)

- [ ] **Step 1: Add Launch constants to config.h**

Insert after the `TransitConfig` struct closing brace (line 85):

```cpp
// --- Launch Page (Rocket Launch Departure Board) ---
#define LAUNCH_MAX_ITEMS        4
#define LAUNCH_REFRESH_MS       300000UL   // 5 min
#define LAUNCH_RETRY_MS         30000UL    // 30 s on error
#define LAUNCH_HTTP_TIMEOUT_MS  8000
#define LAUNCH_NAME_LEN         80
#define LAUNCH_PROVIDER_LEN     32
#define LAUNCH_VEHICLE_LEN      40
#define LAUNCH_PAD_LEN          32
#define LAUNCH_LOCATION_LEN     48
#define LAUNCH_DESC_LEN         160
#define LAUNCH_SLUG_LEN         64
#define LAUNCH_WEATHER_LEN      32
#define LAUNCH_TAG_LEN          24
#define LAUNCH_MAX_TAGS         4

struct LaunchItem {
  char     name[LAUNCH_NAME_LEN];
  char     provider[LAUNCH_PROVIDER_LEN];
  char     providerSlug[LAUNCH_SLUG_LEN];
  char     vehicle[LAUNCH_VEHICLE_LEN];
  char     pad[LAUNCH_PAD_LEN];
  char     location[LAUNCH_LOCATION_LEN];
  char     country[32];
  char     description[LAUNCH_DESC_LEN];
  char     weatherCondition[LAUNCH_WEATHER_LEN];
  char     weatherTemp[8];
  char     tags[LAUNCH_MAX_TAGS][LAUNCH_TAG_LEN];
  uint8_t  tagCount;
  time_t   t0Epoch;            // 0 if TBD
  time_t   winOpen, winClose;  // 0 if absent
  int8_t   result;             // -1 pending, 0 unknown, 1 success, 2 failure
  bool     hasT0;
};
```

- [ ] **Step 2: Compile to verify no errors**

Run:
```bash
arduino-cli compile --clean \
  --build-path /tmp/arduino-build-scrybar \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  <REPO_ROOT>
```
Expected: compiles successfully.

- [ ] **Step 3: Commit**

```bash
git add config.h
git commit -m "r252: add Launch page constants and LaunchItem struct"
```

---

### Task 2: Page Registration (scrybar.ino — enums, flags, helpers)

**Files:**
- Modify: `scrybar.ino` — multiple locations

- [ ] **Step 1: Add UI_PAGE_LAUNCH to UiPageMode enum**

At line ~917, before the closing brace:
```cpp
  UI_PAGE_TRANSIT = 6,
  UI_PAGE_LAUNCH  = 7,
};
```

- [ ] **Step 2: Add LaunchState and LaunchUi structs**

After `g_transitUi` / `g_lvglTransitRoot` declarations (~line 1114), add:

```cpp
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
  lv_obj_t *root = nullptr;
  // Header
  lv_obj_t *header = nullptr;
  lv_obj_t *headerFill = nullptr;
  lv_obj_t *title = nullptr;
  lv_obj_t *fetchTime = nullptr;
  lv_obj_t *status = nullptr;
  // Progress bar
  lv_obj_t *progressBar = nullptr;
  // Hero row
  lv_obj_t *heroBg = nullptr;
  lv_obj_t *heroBadge = nullptr;
  lv_obj_t *heroBadgeLabel = nullptr;
  lv_obj_t *heroName = nullptr;
  lv_obj_t *heroVehiclePad = nullptr;
  lv_obj_t *heroCountdown = nullptr;
  // Compact rows [3]
  lv_obj_t *rowBg[3] = {};
  lv_obj_t *rowBadge[3] = {};
  lv_obj_t *rowBadgeLabel[3] = {};
  lv_obj_t *rowName[3] = {};
  lv_obj_t *rowDate[3] = {};
  // No-data
  lv_obj_t *noData = nullptr;
  // QR
  lv_obj_t *qrCode = nullptr;
  lv_obj_t *qrParent = nullptr;
  // Detail overlay
  lv_obj_t *detailOverlay = nullptr;
  lv_obj_t *detailTitle = nullptr;
  lv_obj_t *detailCountdown = nullptr;
  lv_obj_t *detailClose = nullptr;
  lv_obj_t *detailProvider = nullptr;
  lv_obj_t *detailPadLocation = nullptr;
  lv_obj_t *detailWindow = nullptr;
  lv_obj_t *detailWeather = nullptr;
  lv_obj_t *detailDesc = nullptr;
  lv_obj_t *detailTags[LAUNCH_MAX_TAGS] = {};
  lv_obj_t *detailQr = nullptr;
  int8_t    detailIndex = -1;  // -1 = closed
};
static LvglLaunchUi g_launchUi;
static lv_obj_t *g_lvglLaunchRoot = nullptr;
```

- [ ] **Step 3: Add NET_REQ_LAUNCH_POLL to NetRequestType enum**

At ~line 868, before the closing brace:
```cpp
  NET_REQ_TRANSIT_POLL,
  NET_REQ_LAUNCH_POLL,
};
```

- [ ] **Step 4: Update page helper functions**

In `uiPageName()` (~line 8847), add before `case UI_PAGE_HOME:`:
```cpp
    case UI_PAGE_LAUNCH:
      return "LAUNCH";
```

In `uiViewFlagForPage()` (~line 8867), add before `case UI_PAGE_HOME:`:
```cpp
    case UI_PAGE_LAUNCH: return 0;  // no bitmask — always on with WiFi
```

In `uiPageEnabledNoEnsure()` (~line 8881), add before the Transit check:
```cpp
  if (mode == UI_PAGE_LAUNCH) return g_wifiSt.connected;
```

In `uiPageInSwipeCarousel()` (~line 8900), add before `return true;`:
```cpp
    case UI_PAGE_LAUNCH:
```

In `kSwipePageOrder[]` (~line 8915), add before closing brace:
```cpp
    UI_PAGE_LAUNCH,
```

- [ ] **Step 5: Compile to verify**

Run the compile command. Expected: success.

- [ ] **Step 6: Commit**

```bash
git add scrybar.ino
git commit -m "r252: register LAUNCH page — enum, helpers, state structs"
```

---

### Task 3: API Fetch and JSON Parsing

**Files:**
- Modify: `scrybar.ino` — add after Transit fetch functions (~line 8646)

- [ ] **Step 1: Add ISO 8601 to epoch helper (if not already present)**

Check if `isoToEpoch()` or similar exists. If not, add after Transit parsing section:

```cpp
// --- Launch: ISO 8601 "2026-04-10T12:03Z" → time_t ----
static time_t launchIsoToEpoch(const char *iso) {
  if (!iso || strlen(iso) < 16) return 0;
  struct tm tm = {};
  // "YYYY-MM-DDTHH:MMZ" or "YYYY-MM-DDTHH:MM:SSZ"
  tm.tm_year = atoi(iso) - 1900;
  tm.tm_mon  = atoi(iso + 5) - 1;
  tm.tm_mday = atoi(iso + 8);
  tm.tm_hour = atoi(iso + 11);
  tm.tm_min  = atoi(iso + 14);
  if (strlen(iso) >= 19 && iso[16] == ':') tm.tm_sec = atoi(iso + 17);
  return mktime(&tm) - _timezone;  // mktime assumes local, adjust to UTC
}
```

- [ ] **Step 2: Add netFetchLaunchData()**

Insert after `netFetchTransitDepartures()` (~line 8629):

```cpp
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
  if (!arr) {
    Serial.println("[LAUNCH] no result array");
    return;
  }
  arr = strchr(arr, '[');
  if (!arr) return;
  arr++;  // skip '['

  uint8_t count = 0;
  const char *cur = arr;

  while (count < LAUNCH_MAX_ITEMS && cur < json + jsonLen) {
    // Find next object
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

    // Extract into temporary item
    LaunchItem item = {};

    // Helper lambda: extract string value for a key from object
    auto extractStr = [&](const char *key, char *dst, size_t maxLen) {
      char pattern[64];
      snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
      const char *p = strstr(objStart, pattern);
      if (!p || p > objEnd) { dst[0] = '\0'; return; }
      p += strlen(pattern);
      const char *end = strchr(p, '"');
      if (!end || end > objEnd) { dst[0] = '\0'; return; }
      size_t len = min((size_t)(end - p), maxLen - 1);
      memcpy(dst, p, len);
      dst[len] = '\0';
    };

    // Helper: extract nested object field  "outerKey":{"innerKey":"value"}
    auto extractNested = [&](const char *outerKey, const char *innerKey,
                             char *dst, size_t maxLen) {
      char pattern[64];
      snprintf(pattern, sizeof(pattern), "\"%s\":{", outerKey);
      const char *p = strstr(objStart, pattern);
      if (!p || p > objEnd) { dst[0] = '\0'; return; }
      const char *subEnd = strchr(p, '}');
      if (!subEnd || subEnd > objEnd) { dst[0] = '\0'; return; }
      char innerPattern[64];
      snprintf(innerPattern, sizeof(innerPattern), "\"%s\":\"", innerKey);
      const char *q = strstr(p, innerPattern);
      if (!q || q > subEnd) { dst[0] = '\0'; return; }
      q += strlen(innerPattern);
      const char *valEnd = strchr(q, '"');
      if (!valEnd || valEnd > subEnd) { dst[0] = '\0'; return; }
      size_t len = min((size_t)(valEnd - q), maxLen - 1);
      memcpy(dst, q, len);
      dst[len] = '\0';
    };

    // Flat fields
    extractStr("name", item.name, LAUNCH_NAME_LEN);
    extractStr("launch_description", item.description, LAUNCH_DESC_LEN);
    extractStr("weather_condition", item.weatherCondition, LAUNCH_WEATHER_LEN);
    extractStr("weather_temp", item.weatherTemp, sizeof(item.weatherTemp));

    // Nested fields
    extractNested("provider", "name", item.provider, LAUNCH_PROVIDER_LEN);
    extractNested("provider", "slug", item.providerSlug, LAUNCH_SLUG_LEN);
    extractNested("vehicle", "name", item.vehicle, LAUNCH_VEHICLE_LEN);
    extractNested("pad", "name", item.pad, LAUNCH_PAD_LEN);
    extractNested("location", "name", item.location, LAUNCH_LOCATION_LEN);
    extractNested("location", "country", item.country, 32);

    // t0 field — can be null
    {
      const char *t0p = strstr(objStart, "\"t0\":\"");
      if (t0p && t0p < objEnd) {
        t0p += 6;
        char t0buf[32] = {};
        const char *t0end = strchr(t0p, '"');
        if (t0end && t0end < objEnd) {
          size_t len = min((size_t)(t0end - t0p), sizeof(t0buf) - 1);
          memcpy(t0buf, t0p, len);
          t0buf[len] = '\0';
          item.t0Epoch = launchIsoToEpoch(t0buf);
          item.hasT0 = (item.t0Epoch > 0);
        }
      }
    }

    // win_open, win_close (same pattern, optional)
    {
      const char *wp = strstr(objStart, "\"win_open\":\"");
      if (wp && wp < objEnd) {
        wp += 12;
        char buf[32] = {};
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
        wp += 13;
        char buf[32] = {};
        const char *we = strchr(wp, '"');
        if (we && we < objEnd) {
          size_t len = min((size_t)(we - wp), sizeof(buf) - 1);
          memcpy(buf, wp, len); buf[len] = '\0';
          item.winClose = launchIsoToEpoch(buf);
        }
      }
    }

    // result field (integer, can be null → 0)
    {
      const char *rp = strstr(objStart, "\"result\":");
      if (rp && rp < objEnd) {
        rp += 9;
        if (*rp == 'n') item.result = 0;  // null
        else item.result = atoi(rp);
      }
    }

    // Tags: "tags":[{"id":N,"text":"..."},...]
    {
      const char *tp = strstr(objStart, "\"tags\":[");
      if (tp && tp < objEnd) {
        tp += 7; // skip past '['
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
    if (item.result == 1) {
      cur = objEnd + 1;
      continue;
    }

    g_launchState.items[count] = item;
    count++;
    cur = objEnd + 1;
  }

  g_launchState.count = count;
  g_launchState.valid = (count > 0);
  g_launchState.dirty = true;
  g_launchState.lastFetchMs = millis();

  // Update fetchedAt timestamp
  struct tm ti;
  time_t now = time(nullptr);
  localtime_r(&now, &ti);
  snprintf(g_launchState.fetchedAt, sizeof(g_launchState.fetchedAt),
           "%02d:%02d", ti.tm_hour, ti.tm_min);

  Serial.printf("[LAUNCH] parsed %u launches\n", count);
}
```

- [ ] **Step 3: Add updateLaunchFromApi()**

Insert right after `netFetchLaunchData()`:

```cpp
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
```

- [ ] **Step 4: Add dispatch case in net task handler**

At ~line 8079, before the closing brace of the switch, add:
```cpp
        case NET_REQ_LAUNCH_POLL: {
          netFetchLaunchData();
          Serial.printf("[NET] launch_poll done dt=%lu ms\n", millis() - t0);
          break;
        }
```

- [ ] **Step 5: Add updateLaunchFromApi() call in main loop**

At ~line 16975, after `updateTransitFromApi(false);`:
```cpp
  updateLaunchFromApi(false);
```

- [ ] **Step 6: Compile to verify**

Run the compile command. Expected: success (UI not wired yet, just data layer).

- [ ] **Step 7: Commit**

```bash
git add config.h scrybar.ino
git commit -m "r252: add Launch API fetch + JSON parsing + polling"
```

---

### Task 4: LVGL UI Initialization

**Files:**
- Modify: `scrybar.ino` — add after Transit UI init functions

- [ ] **Step 1: Add provider badge color helper**

Insert before `lvglInitTransitUi()` or after it:

```cpp
static uint32_t launchProviderColor(const char *slug) {
  if (strstr(slug, "spacex"))       return 0x005288;
  if (strstr(slug, "rocket-lab"))   return 0x1A1A2E;
  if (strstr(slug, "ula"))          return 0x0033A0;
  if (strstr(slug, "isro"))         return 0xFF6F00;
  if (strstr(slug, "arianespace"))  return 0x003399;
  if (strstr(slug, "casc"))         return 0xDE2910;
  if (strstr(slug, "roscosmos"))    return 0x1B3A6B;
  return activeUiTheme().lvgl.headerBg;  // fallback
}
```

- [ ] **Step 2: Add lvglInitLaunchUi()**

```cpp
static void lvglInitLaunchUi() {
  if (!g_lvglLaunchRoot) return;
  const int16_t cW = canvasWidth();   // 640
  const int16_t cH = canvasHeight();  // 172
  const int16_t contentW = cW - 84;   // 556
  const UiThemeLvglTokens &t = activeUiTheme().lvgl;

  // ---- Header (30px) ----
  g_launchUi.header = lv_obj_create(g_lvglLaunchRoot);
  lv_obj_remove_style_all(g_launchUi.header);
  lv_obj_set_size(g_launchUi.header, contentW, 30);
  lv_obj_set_pos(g_launchUi.header, 0, 0);
  lvglSetBgFlat(g_launchUi.header, t.headerBg);
  lv_obj_clear_flag(g_launchUi.header, LV_OBJ_FLAG_SCROLLABLE);

  g_launchUi.title = lv_label_create(g_launchUi.header);
  lv_label_set_text(g_launchUi.title, "LAUNCHES");
  lv_obj_set_style_text_font(g_launchUi.title, &lv_font_montserrat_14, 0);
  lvglSetTextHex(g_launchUi.title, t.headerText);
  lv_obj_set_pos(g_launchUi.title, 6, 7);

  g_launchUi.fetchTime = lv_label_create(g_launchUi.header);
  lv_label_set_text(g_launchUi.fetchTime, "--:--");
  lv_obj_set_style_text_font(g_launchUi.fetchTime, &lv_font_montserrat_12, 0);
  lvglSetTextHex(g_launchUi.fetchTime, t.auxMeta);
  lv_obj_align(g_launchUi.fetchTime, LV_ALIGN_RIGHT_MID, -6, 0);

  // ---- Progress bar (3px) ----
  g_launchUi.progressBar = lv_bar_create(g_lvglLaunchRoot);
  lv_obj_set_size(g_launchUi.progressBar, contentW, 3);
  lv_obj_set_pos(g_launchUi.progressBar, 0, 30);
  lv_bar_set_range(g_launchUi.progressBar, 0, 1000);
  lv_bar_set_value(g_launchUi.progressBar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(g_launchUi.progressBar, lv_color_hex(t.divider), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_launchUi.progressBar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_launchUi.progressBar, lv_color_hex(t.headerBg), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(g_launchUi.progressBar, LV_OPA_COVER, LV_PART_INDICATOR);

  // ---- Hero row (49px, y=33) ----
  const int16_t heroY = 33;
  g_launchUi.heroBg = lv_obj_create(g_lvglLaunchRoot);
  lv_obj_remove_style_all(g_launchUi.heroBg);
  lv_obj_set_size(g_launchUi.heroBg, contentW, 49);
  lv_obj_set_pos(g_launchUi.heroBg, 0, heroY);
  lvglSetBgFlat(g_launchUi.heroBg, t.panelBg);
  lv_obj_clear_flag(g_launchUi.heroBg, LV_OBJ_FLAG_SCROLLABLE);

  // Provider badge (pill)
  g_launchUi.heroBadge = lv_obj_create(g_launchUi.heroBg);
  lv_obj_remove_style_all(g_launchUi.heroBadge);
  lv_obj_set_size(g_launchUi.heroBadge, 82, 18);
  lv_obj_set_pos(g_launchUi.heroBadge, 4, 4);
  lv_obj_set_style_radius(g_launchUi.heroBadge, 9, 0);
  lvglSetBgFlat(g_launchUi.heroBadge, t.headerBg);
  lv_obj_clear_flag(g_launchUi.heroBadge, LV_OBJ_FLAG_SCROLLABLE);

  g_launchUi.heroBadgeLabel = lv_label_create(g_launchUi.heroBadge);
  lv_label_set_text(g_launchUi.heroBadgeLabel, "");
  lv_obj_set_style_text_font(g_launchUi.heroBadgeLabel, &lv_font_montserrat_10, 0);
  lvglSetTextHex(g_launchUi.heroBadgeLabel, 0xFFFFFF);
  lv_obj_center(g_launchUi.heroBadgeLabel);
  lv_label_set_long_mode(g_launchUi.heroBadgeLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_width(g_launchUi.heroBadgeLabel, 76);

  // Mission name (line 1)
  g_launchUi.heroName = lv_label_create(g_launchUi.heroBg);
  lv_label_set_text(g_launchUi.heroName, "");
  lv_obj_set_style_text_font(g_launchUi.heroName, &lv_font_montserrat_14, 0);
  lvglSetTextHex(g_launchUi.heroName, t.infoText);
  lv_obj_set_pos(g_launchUi.heroName, 92, 2);
  lv_obj_set_width(g_launchUi.heroName, contentW - 92 - 6);
  lv_label_set_long_mode(g_launchUi.heroName, LV_LABEL_LONG_SCROLL_CIRCULAR);

  // Vehicle · Pad (line 2)
  g_launchUi.heroVehiclePad = lv_label_create(g_launchUi.heroBg);
  lv_label_set_text(g_launchUi.heroVehiclePad, "");
  lv_obj_set_style_text_font(g_launchUi.heroVehiclePad, &lv_font_montserrat_12, 0);
  lvglSetTextHex(g_launchUi.heroVehiclePad, t.auxMeta);
  lv_obj_set_pos(g_launchUi.heroVehiclePad, 4, 28);
  lv_obj_set_width(g_launchUi.heroVehiclePad, contentW - 160);
  lv_label_set_long_mode(g_launchUi.heroVehiclePad, LV_LABEL_LONG_DOT);

  // Countdown (right-aligned, line 2)
  g_launchUi.heroCountdown = lv_label_create(g_launchUi.heroBg);
  lv_label_set_text(g_launchUi.heroCountdown, "T-00:00:00");
  lv_obj_set_style_text_font(g_launchUi.heroCountdown, &lv_font_montserrat_14, 0);
  lvglSetTextHex(g_launchUi.heroCountdown, t.infoText);
  lv_obj_set_style_text_align(g_launchUi.heroCountdown, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(g_launchUi.heroCountdown, contentW - 120, 26);
  lv_obj_set_width(g_launchUi.heroCountdown, 114);

  // ---- Compact rows (30px each, y=82, 112, 142) ----
  for (int i = 0; i < 3; i++) {
    const int16_t ry = 82 + i * 30;

    g_launchUi.rowBg[i] = lv_obj_create(g_lvglLaunchRoot);
    lv_obj_remove_style_all(g_launchUi.rowBg[i]);
    lv_obj_set_size(g_launchUi.rowBg[i], contentW, 30);
    lv_obj_set_pos(g_launchUi.rowBg[i], 0, ry);
    // Alternate row tinting
    lvglSetBgFlat(g_launchUi.rowBg[i], (i % 2 == 0) ? t.panelBg : t.divider);
    lv_obj_clear_flag(g_launchUi.rowBg[i], LV_OBJ_FLAG_SCROLLABLE);

    // Provider badge (smaller pill)
    g_launchUi.rowBadge[i] = lv_obj_create(g_launchUi.rowBg[i]);
    lv_obj_remove_style_all(g_launchUi.rowBadge[i]);
    lv_obj_set_size(g_launchUi.rowBadge[i], 70, 16);
    lv_obj_set_pos(g_launchUi.rowBadge[i], 4, 7);
    lv_obj_set_style_radius(g_launchUi.rowBadge[i], 8, 0);
    lvglSetBgFlat(g_launchUi.rowBadge[i], t.headerBg);
    lv_obj_clear_flag(g_launchUi.rowBadge[i], LV_OBJ_FLAG_SCROLLABLE);

    g_launchUi.rowBadgeLabel[i] = lv_label_create(g_launchUi.rowBadge[i]);
    lv_label_set_text(g_launchUi.rowBadgeLabel[i], "");
    lv_obj_set_style_text_font(g_launchUi.rowBadgeLabel[i], &lv_font_montserrat_10, 0);
    lvglSetTextHex(g_launchUi.rowBadgeLabel[i], 0xFFFFFF);
    lv_obj_center(g_launchUi.rowBadgeLabel[i]);
    lv_label_set_long_mode(g_launchUi.rowBadgeLabel[i], LV_LABEL_LONG_DOT);
    lv_obj_set_width(g_launchUi.rowBadgeLabel[i], 64);

    // Mission name
    g_launchUi.rowName[i] = lv_label_create(g_launchUi.rowBg[i]);
    lv_label_set_text(g_launchUi.rowName[i], "");
    lv_obj_set_style_text_font(g_launchUi.rowName[i], &lv_font_montserrat_12, 0);
    lvglSetTextHex(g_launchUi.rowName[i], t.infoText);
    lv_obj_set_pos(g_launchUi.rowName[i], 80, 8);
    lv_obj_set_width(g_launchUi.rowName[i], contentW - 80 - 110);
    lv_label_set_long_mode(g_launchUi.rowName[i], LV_LABEL_LONG_DOT);

    // Date/time (right-aligned)
    g_launchUi.rowDate[i] = lv_label_create(g_launchUi.rowBg[i]);
    lv_label_set_text(g_launchUi.rowDate[i], "");
    lv_obj_set_style_text_font(g_launchUi.rowDate[i], &lv_font_montserrat_12, 0);
    lvglSetTextHex(g_launchUi.rowDate[i], t.auxMeta);
    lv_obj_set_style_text_align(g_launchUi.rowDate[i], LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(g_launchUi.rowDate[i], contentW - 106, 8);
    lv_obj_set_width(g_launchUi.rowDate[i], 100);
  }

  // ---- No-data label ----
  g_launchUi.noData = lv_label_create(g_lvglLaunchRoot);
  lv_label_set_text(g_launchUi.noData, "No upcoming launches");
  lv_obj_set_style_text_font(g_launchUi.noData, &lv_font_montserrat_14, 0);
  lvglSetTextHex(g_launchUi.noData, t.auxMeta);
  lv_obj_set_pos(g_launchUi.noData, 0, 33);
  lv_obj_set_size(g_launchUi.noData, contentW, 139);
  lv_obj_set_style_text_align(g_launchUi.noData, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_pad_top(g_launchUi.noData, 50, 0);
  lv_obj_add_flag(g_launchUi.noData, LV_OBJ_FLAG_HIDDEN);

  // ---- QR column (84px) ----
  g_launchUi.qrParent = lv_obj_create(g_lvglLaunchRoot);
  lv_obj_remove_style_all(g_launchUi.qrParent);
  lv_obj_set_size(g_launchUi.qrParent, 84, cH);
  lv_obj_set_pos(g_launchUi.qrParent, contentW, 0);
  lvglSetBgFlat(g_launchUi.qrParent, t.panelBg);
  lv_obj_clear_flag(g_launchUi.qrParent, LV_OBJ_FLAG_SCROLLABLE);

  const lv_color_t qrDark = lv_color_hex(t.infoText);
  const lv_color_t qrLight = lv_color_hex(t.panelBg);
  g_launchUi.qrCode = lv_qrcode_create(g_launchUi.qrParent, 76, qrDark, qrLight);
  lv_obj_align(g_launchUi.qrCode, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_border_width(g_launchUi.qrCode, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_launchUi.qrCode, qrLight, LV_PART_MAIN);

  // Set a placeholder QR
  const char *defaultUrl = "https://rocketlaunch.live";
  lv_qrcode_update(g_launchUi.qrCode, defaultUrl, strlen(defaultUrl));

  // ---- Touch event for row taps ----
  lv_obj_add_flag(g_launchUi.heroBg, LV_OBJ_FLAG_CLICKABLE);
  for (int i = 0; i < 3; i++) {
    lv_obj_add_flag(g_launchUi.rowBg[i], LV_OBJ_FLAG_CLICKABLE);
  }
}
```

- [ ] **Step 3: Add page init in setup block**

After Transit init (~line 15556), add:
```cpp
  // Launch Page
  g_lvglLaunchRoot = lvglCreatePageRoot(scr, cW, cH);
  g_launchUi.root = g_lvglLaunchRoot;
  lv_obj_set_pos(g_lvglLaunchRoot, cW, 0);
  lvglInitLaunchUi();
```

- [ ] **Step 4: Add to carousel arrays**

In `lvglApplyPageVisibility()` pages array (~line 13784-13791), add:
```cpp
  {UI_PAGE_LAUNCH,     g_lvglLaunchRoot},
```

In `lvglHandleDragPages()` if it uses a separate array, add the same entry.

Add null check in `lvglApplyPageVisibility()` guard:
```cpp
if (!g_infoUi.root || !g_lvglHomeRoot || !g_lvglAuxRoot || !g_lvglWikiRoot || !g_lvglNowPlayingRoot || !g_lvglLaunchRoot) return;
```

- [ ] **Step 5: Compile to verify**

Run the compile command. Expected: success.

- [ ] **Step 6: Commit**

```bash
git add scrybar.ino
git commit -m "r252: add Launch page LVGL UI init — header, hero, rows, QR"
```

---

### Task 5: UI Update and Countdown Timer

**Files:**
- Modify: `scrybar.ino`

- [ ] **Step 1: Add lvglUpdateLaunchUi()**

```cpp
static void lvglUpdateLaunchUi(bool force) {
  if (!g_lvglLaunchRoot) return;
  const UiThemeLvglTokens &t = activeUiTheme().lvgl;

  // Update fetch time
  lv_label_set_text(g_launchUi.fetchTime, g_launchState.fetchedAt);

  if (!g_launchState.valid || g_launchState.count == 0) {
    // Show no-data, hide everything else
    lv_obj_clear_flag(g_launchUi.noData, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_launchUi.heroBg, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 3; i++) lv_obj_add_flag(g_launchUi.rowBg[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_launchUi.progressBar, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  lv_obj_add_flag(g_launchUi.noData, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_launchUi.heroBg, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_launchUi.progressBar, LV_OBJ_FLAG_HIDDEN);

  // ---- Hero row (item 0) ----
  const LaunchItem &hero = g_launchState.items[0];
  lv_label_set_text(g_launchUi.heroBadgeLabel, hero.provider);
  lv_label_set_text(g_launchUi.heroName, hero.name);

  uint32_t badgeColor = launchProviderColor(hero.providerSlug);
  lvglSetBgFlat(g_launchUi.heroBadge, badgeColor);

  char vpBuf[96];
  snprintf(vpBuf, sizeof(vpBuf), "%s \xC2\xB7 %s", hero.vehicle, hero.pad);
  lv_label_set_text(g_launchUi.heroVehiclePad, vpBuf);

  // QR update
  char qrUrl[128];
  snprintf(qrUrl, sizeof(qrUrl), "https://rocketlaunch.live/launch/%s",
           hero.providerSlug[0] ? hero.providerSlug : "upcoming");
  lv_qrcode_update(g_launchUi.qrCode, qrUrl, strlen(qrUrl));

  // ---- Compact rows ----
  for (int i = 0; i < 3; i++) {
    const int idx = i + 1;
    if (idx >= g_launchState.count) {
      lv_obj_add_flag(g_launchUi.rowBg[i], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_clear_flag(g_launchUi.rowBg[i], LV_OBJ_FLAG_HIDDEN);
    const LaunchItem &item = g_launchState.items[idx];

    lv_label_set_text(g_launchUi.rowBadgeLabel[i], item.provider);
    uint32_t rc = launchProviderColor(item.providerSlug);
    lvglSetBgFlat(g_launchUi.rowBadge[i], rc);

    lv_label_set_text(g_launchUi.rowName[i], item.name);

    // Date string
    if (item.hasT0) {
      struct tm ti;
      time_t t0 = item.t0Epoch;
      gmtime_r(&t0, &ti);
      char dateBuf[20];
      static const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                     "Jul","Aug","Sep","Oct","Nov","Dec"};
      snprintf(dateBuf, sizeof(dateBuf), "%s %02d %02d:%02d",
               months[ti.tm_mon], ti.tm_mday, ti.tm_hour, ti.tm_min);
      lv_label_set_text(g_launchUi.rowDate[i], dateBuf);
      lvglSetTextHex(g_launchUi.rowDate[i], t.auxMeta);
    } else {
      lv_label_set_text(g_launchUi.rowDate[i], "TBD");
      lvglSetTextHex(g_launchUi.rowDate[i], t.auxMeta);
    }
  }

  g_launchState.dirty = false;
}
```

- [ ] **Step 2: Add lvglTickLaunchCountdown()**

```cpp
static void lvglTickLaunchCountdown() {
  if (!g_lvglLaunchRoot || g_uiPageMode != UI_PAGE_LAUNCH) return;
  if (g_launchState.count == 0 || !g_launchState.items[0].hasT0) {
    lv_label_set_text(g_launchUi.heroCountdown, "TBD");
    lv_bar_set_value(g_launchUi.progressBar, 0, LV_ANIM_OFF);
    return;
  }

  const UiThemeLvglTokens &t = activeUiTheme().lvgl;
  const time_t now = time(nullptr);
  const time_t t0 = g_launchState.items[0].t0Epoch;
  const int32_t delta = (int32_t)(t0 - now);

  char buf[24];
  uint32_t textColor = t.infoText;
  uint32_t barColor = t.headerBg;

  if (delta <= 0) {
    // LIFTOFF
    snprintf(buf, sizeof(buf), "LIFTOFF!");
    textColor = 0x00E676;  // green
    barColor = 0x00E676;
    lv_bar_set_value(g_launchUi.progressBar, 1000, LV_ANIM_OFF);
  } else if (delta < 600) {
    // < 10 min — IMMINENT
    int m = delta / 60, s = delta % 60;
    snprintf(buf, sizeof(buf), "T-%02d:%02d", m, s);
    textColor = 0xFF1744;  // red
    barColor = 0xFF1744;
  } else if (delta < 3600) {
    // < 1 hour — SOON
    int m = delta / 60, s = delta % 60;
    snprintf(buf, sizeof(buf), "T-%02d:%02d", m, s);
    textColor = 0xFFD600;  // yellow
    barColor = 0xFFD600;
  } else if (delta < 86400) {
    // < 24 hours — show HH:MM:SS
    int h = delta / 3600, m = (delta % 3600) / 60, s = delta % 60;
    snprintf(buf, sizeof(buf), "T-%02d:%02d:%02d", h, m, s);
  } else {
    // > 24 hours — show days
    int d = delta / 86400, h = (delta % 86400) / 3600, m = (delta % 3600) / 60;
    if (d > 99) {
      snprintf(buf, sizeof(buf), "T-%dd", d);
    } else {
      snprintf(buf, sizeof(buf), "T-%dd %02d:%02d", d, h, m);
    }
  }

  lv_label_set_text(g_launchUi.heroCountdown, buf);
  lvglSetTextHex(g_launchUi.heroCountdown, textColor);

  // Progress bar: fills over last 24h
  if (delta > 0 && delta < 86400) {
    int32_t progress = 1000 - (delta * 1000 / 86400);
    lv_bar_set_value(g_launchUi.progressBar, progress, LV_ANIM_OFF);
  } else if (delta <= 0) {
    lv_bar_set_value(g_launchUi.progressBar, 1000, LV_ANIM_OFF);
  } else {
    lv_bar_set_value(g_launchUi.progressBar, 0, LV_ANIM_OFF);
  }
  lv_obj_set_style_bg_color(g_launchUi.progressBar, lv_color_hex(barColor), LV_PART_INDICATOR);
}
```

- [ ] **Step 3: Wire up updateLvglUi() dispatch**

In `updateLvglUi()`, add before the Transit dispatch (~line 15773):
```cpp
if (g_uiPageMode == UI_PAGE_LAUNCH) {
  if (g_launchState.dirty) lvglUpdateLaunchUi(force);
  lvglTickLaunchCountdown();
  g_clock.lastSecond = timeinfo.tm_sec;
  g_clock.lastDateKey = dateKey;
  g_uiNeedsRedraw = false;
  return;
}
```

- [ ] **Step 4: Add 1-second countdown tick in main loop**

In the main loop, after `handleTouchSwipeInput()` (~line 16988), add:
```cpp
  // Launch countdown — tick every second when visible
  {
    static uint32_t sLastLaunchTickMs = 0;
    const uint32_t nowMs = millis();
    if (g_uiPageMode == UI_PAGE_LAUNCH && (nowMs - sLastLaunchTickMs) >= 1000) {
      sLastLaunchTickMs = nowMs;
      lvglTickLaunchCountdown();
      g_uiNeedsRedraw = true;
    }
  }
```

- [ ] **Step 5: Compile to verify**

Run the compile command. Expected: success.

- [ ] **Step 6: Commit**

```bash
git add scrybar.ino
git commit -m "r252: add Launch page UI update + live countdown timer"
```

---

### Task 6: Detail Overlay (Tap-to-Detail)

**Files:**
- Modify: `scrybar.ino`

- [ ] **Step 1: Add lvglInitLaunchDetail()**

Call this at the end of `lvglInitLaunchUi()`:

```cpp
static void lvglInitLaunchDetail() {
  const int16_t cW = canvasWidth();
  const int16_t cH = canvasHeight();
  const UiThemeLvglTokens &t = activeUiTheme().lvgl;

  // Full-canvas overlay (hidden by default)
  g_launchUi.detailOverlay = lv_obj_create(g_lvglLaunchRoot);
  lv_obj_remove_style_all(g_launchUi.detailOverlay);
  lv_obj_set_size(g_launchUi.detailOverlay, cW, cH);
  lv_obj_set_pos(g_launchUi.detailOverlay, 0, 0);
  lvglSetBgFlat(g_launchUi.detailOverlay, t.panelBg);
  lv_obj_add_flag(g_launchUi.detailOverlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_launchUi.detailOverlay, LV_OBJ_FLAG_SCROLLABLE);

  // Header line: X + mission name + countdown
  g_launchUi.detailClose = lv_label_create(g_launchUi.detailOverlay);
  lv_label_set_text(g_launchUi.detailClose, LV_SYMBOL_CLOSE);
  lv_obj_set_style_text_font(g_launchUi.detailClose, &lv_font_montserrat_14, 0);
  lvglSetTextHex(g_launchUi.detailClose, t.headerText);
  lv_obj_set_pos(g_launchUi.detailClose, 6, 6);
  lv_obj_add_flag(g_launchUi.detailClose, LV_OBJ_FLAG_CLICKABLE);

  g_launchUi.detailTitle = lv_label_create(g_launchUi.detailOverlay);
  lv_label_set_text(g_launchUi.detailTitle, "");
  lv_obj_set_style_text_font(g_launchUi.detailTitle, &lv_font_montserrat_14, 0);
  lvglSetTextHex(g_launchUi.detailTitle, t.infoText);
  lv_obj_set_pos(g_launchUi.detailTitle, 28, 6);
  lv_obj_set_width(g_launchUi.detailTitle, cW - 160);
  lv_label_set_long_mode(g_launchUi.detailTitle, LV_LABEL_LONG_DOT);

  g_launchUi.detailCountdown = lv_label_create(g_launchUi.detailOverlay);
  lv_label_set_text(g_launchUi.detailCountdown, "");
  lv_obj_set_style_text_font(g_launchUi.detailCountdown, &lv_font_montserrat_14, 0);
  lvglSetTextHex(g_launchUi.detailCountdown, t.infoText);
  lv_obj_set_style_text_align(g_launchUi.detailCountdown, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(g_launchUi.detailCountdown, cW - 130, 6);
  lv_obj_set_width(g_launchUi.detailCountdown, 124);

  // Provider · Vehicle
  g_launchUi.detailProvider = lv_label_create(g_launchUi.detailOverlay);
  lv_label_set_text(g_launchUi.detailProvider, "");
  lv_obj_set_style_text_font(g_launchUi.detailProvider, &lv_font_montserrat_12, 0);
  lvglSetTextHex(g_launchUi.detailProvider, t.infoText);
  lv_obj_set_pos(g_launchUi.detailProvider, 6, 28);
  lv_obj_set_width(g_launchUi.detailProvider, cW - 12);

  // Pad, Location, Country
  g_launchUi.detailPadLocation = lv_label_create(g_launchUi.detailOverlay);
  lv_label_set_text(g_launchUi.detailPadLocation, "");
  lv_obj_set_style_text_font(g_launchUi.detailPadLocation, &lv_font_montserrat_12, 0);
  lvglSetTextHex(g_launchUi.detailPadLocation, t.auxMeta);
  lv_obj_set_pos(g_launchUi.detailPadLocation, 6, 46);
  lv_obj_set_width(g_launchUi.detailPadLocation, cW - 12);

  // Window + Weather
  g_launchUi.detailWindow = lv_label_create(g_launchUi.detailOverlay);
  lv_label_set_text(g_launchUi.detailWindow, "");
  lv_obj_set_style_text_font(g_launchUi.detailWindow, &lv_font_montserrat_12, 0);
  lvglSetTextHex(g_launchUi.detailWindow, t.auxMeta);
  lv_obj_set_pos(g_launchUi.detailWindow, 6, 64);
  lv_obj_set_width(g_launchUi.detailWindow, cW - 12);

  // Description (scrollable)
  g_launchUi.detailDesc = lv_label_create(g_launchUi.detailOverlay);
  lv_label_set_text(g_launchUi.detailDesc, "");
  lv_obj_set_style_text_font(g_launchUi.detailDesc, &lv_font_montserrat_10, 0);
  lvglSetTextHex(g_launchUi.detailDesc, t.infoText);
  lv_obj_set_pos(g_launchUi.detailDesc, 6, 82);
  lv_obj_set_size(g_launchUi.detailDesc, cW - 100, cH - 86);
  lv_label_set_long_mode(g_launchUi.detailDesc, LV_LABEL_LONG_WRAP);

  // Detail QR (bottom-right)
  const lv_color_t qrDark = lv_color_hex(t.infoText);
  const lv_color_t qrLight = lv_color_hex(t.panelBg);
  g_launchUi.detailQr = lv_qrcode_create(g_launchUi.detailOverlay, 64, qrDark, qrLight);
  lv_obj_set_pos(g_launchUi.detailQr, cW - 74, cH - 74);
  lv_obj_set_style_border_width(g_launchUi.detailQr, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(g_launchUi.detailQr, qrLight, LV_PART_MAIN);
}
```

- [ ] **Step 2: Add launch detail open/close functions**

```cpp
static void lvglOpenLaunchDetail(int8_t index) {
  if (index < 0 || index >= g_launchState.count) return;
  g_launchUi.detailIndex = index;
  const LaunchItem &item = g_launchState.items[index];

  lv_label_set_text(g_launchUi.detailTitle, item.name);

  char provBuf[96];
  snprintf(provBuf, sizeof(provBuf), "%s \xC2\xB7 %s", item.provider, item.vehicle);
  lv_label_set_text(g_launchUi.detailProvider, provBuf);

  char locBuf[128];
  snprintf(locBuf, sizeof(locBuf), "%s, %s, %s", item.pad, item.location, item.country);
  lv_label_set_text(g_launchUi.detailPadLocation, locBuf);

  // Window + weather
  char winBuf[96] = {};
  if (item.winOpen || item.winClose) {
    struct tm wo, wc;
    if (item.winOpen) { gmtime_r(&item.winOpen, &wo); }
    if (item.winClose) { gmtime_r(&item.winClose, &wc); }
    if (item.winOpen && item.winClose) {
      snprintf(winBuf, sizeof(winBuf), "Window: %02d:%02d - %02d:%02d UTC",
               wo.tm_hour, wo.tm_min, wc.tm_hour, wc.tm_min);
    } else if (item.winOpen) {
      snprintf(winBuf, sizeof(winBuf), "Window opens: %02d:%02d UTC",
               wo.tm_hour, wo.tm_min);
    }
  }
  if (item.weatherCondition[0]) {
    char weatherBuf[64];
    snprintf(weatherBuf, sizeof(weatherBuf), " \xC2\xB7 %s %s\xC2\xB0F",
             item.weatherCondition, item.weatherTemp);
    strncat(winBuf, weatherBuf, sizeof(winBuf) - strlen(winBuf) - 1);
  }
  lv_label_set_text(g_launchUi.detailWindow, winBuf);

  // Description
  lv_label_set_text(g_launchUi.detailDesc,
                    item.description[0] ? item.description : "No description available");

  // Detail QR
  char qrUrl[128];
  snprintf(qrUrl, sizeof(qrUrl), "https://rocketlaunch.live/launch/%s",
           item.providerSlug[0] ? item.providerSlug : "upcoming");
  lv_qrcode_update(g_launchUi.detailQr, qrUrl, strlen(qrUrl));

  lv_obj_clear_flag(g_launchUi.detailOverlay, LV_OBJ_FLAG_HIDDEN);
}

static void lvglCloseLaunchDetail() {
  g_launchUi.detailIndex = -1;
  lv_obj_add_flag(g_launchUi.detailOverlay, LV_OBJ_FLAG_HIDDEN);
}
```

- [ ] **Step 3: Wire touch events**

In the touch/tap handler (where feed deck taps are handled), add a case
for LAUNCH page. When `g_uiPageMode == UI_PAGE_LAUNCH` and a tap is
detected:

```cpp
// In handleTouchTap() or equivalent tap callback:
if (g_uiPageMode == UI_PAGE_LAUNCH) {
  if (g_launchUi.detailIndex >= 0) {
    // Detail is open — close on tap
    lvglCloseLaunchDetail();
    return;
  }
  // Check which row was tapped based on Y coordinate
  const int16_t ty = tapY;  // touch Y position
  if (ty >= 33 && ty < 82) {
    lvglOpenLaunchDetail(0);  // hero
  } else if (ty >= 82 && ty < 112 && g_launchState.count > 1) {
    lvglOpenLaunchDetail(1);
  } else if (ty >= 112 && ty < 142 && g_launchState.count > 2) {
    lvglOpenLaunchDetail(2);
  } else if (ty >= 142 && ty < 172 && g_launchState.count > 3) {
    lvglOpenLaunchDetail(3);
  }
  return;
}
```

- [ ] **Step 4: Update detail countdown in tick**

At the end of `lvglTickLaunchCountdown()`, add:
```cpp
  // Update detail overlay countdown if open
  if (g_launchUi.detailIndex >= 0 && g_launchUi.detailIndex < g_launchState.count) {
    const LaunchItem &di = g_launchState.items[g_launchUi.detailIndex];
    if (di.hasT0) {
      const int32_t dd = (int32_t)(di.t0Epoch - now);
      char dbuf[24];
      if (dd <= 0) snprintf(dbuf, sizeof(dbuf), "LIFTOFF!");
      else if (dd < 3600) snprintf(dbuf, sizeof(dbuf), "T-%02d:%02d", (int)(dd/60), (int)(dd%60));
      else if (dd < 86400) snprintf(dbuf, sizeof(dbuf), "T-%02d:%02d:%02d", (int)(dd/3600), (int)((dd%3600)/60), (int)(dd%60));
      else snprintf(dbuf, sizeof(dbuf), "T-%dd %02d:%02d", (int)(dd/86400), (int)((dd%86400)/3600), (int)((dd%3600)/60));
      lv_label_set_text(g_launchUi.detailCountdown, dbuf);
    } else {
      lv_label_set_text(g_launchUi.detailCountdown, "TBD");
    }
  }
```

- [ ] **Step 5: Call lvglInitLaunchDetail() at end of lvglInitLaunchUi()**

Add at the bottom of `lvglInitLaunchUi()`:
```cpp
  lvglInitLaunchDetail();
```

- [ ] **Step 6: Compile to verify**

Run the compile command. Expected: success.

- [ ] **Step 7: Commit**

```bash
git add scrybar.ino
git commit -m "r252: add Launch detail overlay with tap-to-open"
```

---

### Task 7: Theme Integration and Polish

**Files:**
- Modify: `scrybar.ino`

- [ ] **Step 1: Add Launch page to theme application**

In `lvglApplyThemeStyles()` (wherever Transit theme is applied), add
Launch page theme application:

```cpp
// Launch page theming
if (g_lvglLaunchRoot) {
  const UiThemeLvglTokens &t = activeUiTheme().lvgl;
  lvglSetBgFlat(g_launchUi.header, t.headerBg);
  lvglSetTextHex(g_launchUi.title, t.headerText);
  lvglSetTextHex(g_launchUi.fetchTime, t.auxMeta);
  lvglSetBgFlat(g_launchUi.heroBg, t.panelBg);
  lvglSetTextHex(g_launchUi.heroName, t.infoText);
  lvglSetTextHex(g_launchUi.heroVehiclePad, t.auxMeta);
  lvglSetTextHex(g_launchUi.heroCountdown, t.infoText);
  lvglSetTextHex(g_launchUi.noData, t.auxMeta);
  for (int i = 0; i < 3; i++) {
    lvglSetBgFlat(g_launchUi.rowBg[i], (i % 2 == 0) ? t.panelBg : t.divider);
    lvglSetTextHex(g_launchUi.rowName[i], t.infoText);
    lvglSetTextHex(g_launchUi.rowDate[i], t.auxMeta);
  }
  // QR parent
  lvglSetBgFlat(g_launchUi.qrParent, t.panelBg);
  // Detail overlay
  if (g_launchUi.detailOverlay) {
    lvglSetBgFlat(g_launchUi.detailOverlay, t.panelBg);
    lvglSetTextHex(g_launchUi.detailTitle, t.infoText);
    lvglSetTextHex(g_launchUi.detailProvider, t.infoText);
    lvglSetTextHex(g_launchUi.detailPadLocation, t.auxMeta);
    lvglSetTextHex(g_launchUi.detailWindow, t.auxMeta);
    lvglSetTextHex(g_launchUi.detailDesc, t.infoText);
  }
}
```

- [ ] **Step 2: Bump firmware version**

In `config.h`:
```cpp
#define FW_BUILD_TAG    "r252"
#define FW_RELEASE_DATE "2026-04-08"
```

- [ ] **Step 3: Compile clean**

Run the compile command with `--clean`. Expected: success.

- [ ] **Step 4: Commit**

```bash
git add config.h scrybar.ino
git commit -m "r252: Launch page — theme integration + version bump"
```

---

### Task 8: Upload, Test, and Screenshot

- [ ] **Step 1: Upload firmware**

```bash
arduino-cli upload -p /dev/cu.usbmodem83201 \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  --input-dir /tmp/arduino-build-scrybar \
  <REPO_ROOT>
```

- [ ] **Step 2: Verify page appears**

Swipe right past TRANSIT to reach LAUNCH page. Verify:
- Header shows "LAUNCHES" with fetch timestamp
- Hero row shows nearest launch with provider badge and countdown
- Compact rows show subsequent launches
- QR code visible on right side
- Countdown ticks every second

- [ ] **Step 3: Test tap-to-detail**

Tap hero row → verify detail overlay opens with mission info, weather, QR.
Tap again → overlay closes.

- [ ] **Step 4: Capture screenshot**

```bash
python3 tools/capture_snapshot.py --port /dev/cu.usbmodem83201 --out /tmp/launch-page.png
```

- [ ] **Step 5: Final commit with any fixes**

```bash
git add -A
git commit -m "r252: Launch page — verified on device"
```
