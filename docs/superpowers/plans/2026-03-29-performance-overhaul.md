# Performance Overhaul Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate UI hitches during network activity, maximize frame rate during swipe animations, reduce heap fragmentation and network overhead.

**Architecture:** Move all HTTP I/O to a dedicated FreeRTOS task on Core 1 (Phase 1), optimize the LVGL→display pipeline with double-buffering and cache-aligned rotation (Phase 2), then improve network efficiency with HTTP/1.1 keep-alive and String elimination (Phase 3).

**Tech Stack:** ESP32-S3 (Arduino + ESP-IDF), LVGL 8.4.0, FreeRTOS, WiFiClientSecure, AXS15231B QSPI display

**Spec:** `docs/superpowers/specs/2026-03-29-performance-overhaul-design.md`

**Build/Flash workflow (every verification step):**
```bash
# Compile
arduino-cli compile --clean \
  --build-path /tmp/arduino-build-scrybar \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  <REPO_ROOT>

# Upload (verify port with: arduino-cli board list)
arduino-cli upload -p /dev/cu.usbmodem83201 \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  --input-dir /tmp/arduino-build-scrybar \
  <REPO_ROOT>

# Monitor serial
python3 -c "import serial,sys; s=serial.Serial('/dev/cu.usbmodem83201',115200,timeout=1); [print(s.readline().decode('utf-8','replace'),end='') for _ in iter(int,1)]"
```

**IMPORTANT:** Bump `FW_BUILD_TAG` (r-number) and `FW_RELEASE_DATE` in `config.h` at every flash.

---

## File Map

| File | Phase | Changes |
|------|-------|---------|
| `config.h` | 1,2 | Add `NET_TASK_*` defines, change `DB_CHUNK_ROWS` to 64 |
| `scrybar.ino` ~L789-833 | 1 | Add `dirty` flag + `netBusy` to `WeatherState`, `RssState` |
| `scrybar.ino` ~L1314 | 2 | `DB_CHUNK_ROWS` = 64 (driven by config.h) |
| `scrybar.ino` ~L2649-2701 | 2 | `dispRotateChunk` tile 8→16, `dispFlush` chunk count comment |
| `scrybar.ino` ~L5112-5148 | 1 | `handleWebReloadForm/Api` → queue sends |
| `scrybar.ino` ~L6377-6380 | 1 | `ScopedPsramTls` — used once in netTask |
| `scrybar.ino` ~L6726-6824 | 3 | `faviconFetchAndCache` — String cleanup |
| `scrybar.ino` ~L6854-7105 | 1,3 | `fetchRssItemsFromUrl/updateRssFromFeed` — extract to Core 1 + String cleanup |
| `scrybar.ino` ~L7129-7482 | 1,3 | Wiki functions — extract to Core 1 + String cleanup |
| `scrybar.ino` ~L7574-7692 | 1,3 | `updateWeatherFromApi` — extract to Core 1 + snprintf |
| `scrybar.ino` ~L12299-12341 | 2 | `lvglDisplayFlushCb` — memcpy optimization |
| `scrybar.ino` ~L13484-13498 | 2 | LVGL init — double-buffer allocation |
| `scrybar.ino` ~L13754-13782 | 2 | `runLvglLoop` — adaptive cadence |
| `scrybar.ino` ~L14844-14923 | 1 | `loop()` — replace direct network calls with queue sends |

---

## Phase 1: Network Isolation

### Task 1.1: Add config defines and data structures

**Files:**
- Modify: `config.h`
- Modify: `scrybar.ino` ~L789-833

- [ ] **Step 1: Add network task config to `config.h`**

At end of file (after line 220, `PWR_USE_TCA9554_SYS_EN`), add:

```c
// --- Network background task (Phase 1 perf overhaul) ---
#define NET_TASK_STACK_SIZE 16384
#define NET_TASK_PRIORITY 1
#define NET_QUEUE_DEPTH 8
#define NET_STACK_MONITOR_MS 30000
```

- [ ] **Step 2: Add dirty flags to WeatherState**

In `scrybar.ino`, modify the `WeatherState` struct (~L789) — add a `dirty` field at the end:

```c
struct WeatherState {
  // ... existing fields ...
  uint32_t lastFetchMs = 0;
  bool dirty = false;  // NEW: set by netTask, cleared by UI update
};
```

- [ ] **Step 3: Add dirty flag to RssState**

Modify `RssState` (~L817) — add `dirty`:

```c
struct RssState {
  // ... existing fields ...
  int lastHttpCode = 0;
  bool dirty = false;  // NEW: set by netTask, cleared by UI update
};
```

- [ ] **Step 4: Add netTask globals after the RssState definitions**

After `static uint32_t g_wikiVisiblePreloadLastMs = 0;` (~L832), add:

```c
// --- Network background task (Core 1) ---
enum NetRequestType : uint8_t {
  NET_REQ_WEATHER = 0,
  NET_REQ_RSS,
  NET_REQ_WIKI,
  NET_REQ_FAVICON,
  NET_REQ_WIKI_META,
};
struct NetRequest { NetRequestType type; };

static QueueHandle_t    g_netQueue  = nullptr;
static SemaphoreHandle_t g_netMutex = nullptr;
static TaskHandle_t     g_netTaskHandle = nullptr;
static bool             g_netTaskReady = false;
```

- [ ] **Step 5: Compile to verify no syntax errors**

Run compile command. Expected: clean compile, no errors.

- [ ] **Step 6: Commit**

```bash
git add config.h scrybar.ino
git commit -m "perf(phase1): add netTask config defines, dirty flags, queue/mutex globals"
```

---

### Task 1.2: Create netTask function and startup

**Files:**
- Modify: `scrybar.ino`

- [ ] **Step 1: Add the netTask main function**

Place this **before** `setup()` (around line ~13400, after all the network helper functions but before LVGL init). Find a good insertion point after `wikiPreloadMetaStep()` (~L7482) and before the LVGL section.

```c
// ── Network background task (Core 1) ──────────────────────────────────────────
static void netTaskMain(void *param) {
  (void)param;

  // Activate ScopedPsramTls for this task's lifetime — redirect mbedtls to PSRAM.
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
          // Fetch weather into local copy, then mutex-copy to shared
          WeatherState local = {};
          const bool ok = netFetchWeather(local);
          if (ok) {
            xSemaphoreTake(g_netMutex, portMAX_DELAY);
            // Preserve lastAttemptMs/lastFetchMs gating fields
            const uint32_t prevAttempt = g_weather.lastFetchMs;
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
          Serial.printf("[NET] wiki_meta done dt=%lu ms\n", millis() - t0);
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
```

Note: `netFetchWeather`, `netFetchRss`, `netFetchWiki`, `netFetchFavicons`, `netFetchWikiMeta` are wrapper functions we'll create in the next task. They contain the HTTP logic extracted from the existing functions.

- [ ] **Step 2: Add netTask startup to `setup()`**

In `setup()`, after WiFi initialization and before the first `updateWeatherFromApi()` call, add:

```c
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
```

- [ ] **Step 3: Add a helper to enqueue network requests**

Place near the netTaskMain function:

```c
static bool netEnqueue(NetRequestType type) {
  if (!g_netQueue) return false;
  NetRequest req = { type };
  if (xQueueSend(g_netQueue, &req, 0) != pdTRUE) {
    Serial.printf("[NET] queue full, dropping %d\n", (int)type);
    return false;
  }
  return true;
}
```

- [ ] **Step 4: Compile to verify no errors**

Expected: compile succeeds. The `netFetch*` wrapper functions don't exist yet — they'll be forward-declared or stubbed.

- [ ] **Step 5: Commit**

```bash
git add scrybar.ino
git commit -m "perf(phase1): add netTask skeleton, queue/mutex init, enqueue helper"
```

---

### Task 1.3: Extract weather fetch to Core 1

**Files:**
- Modify: `scrybar.ino` ~L7574-7692

- [ ] **Step 1: Create `netFetchWeather()` wrapper**

This function contains the HTTP-fetching body of `updateWeatherFromApi()`, but writes results to a local `WeatherState` instead of the global. Place it near the existing `updateWeatherFromApi()`.

Extract the HTTP body from `updateWeatherFromApi()` (lines ~7590-7680) into a new function.

**Extraction checklist — transformations required:**
1. Copy lines ~7590-7680 (from WiFiClientSecure setup through JSON parsing) into `netFetchWeather(WeatherState &out)`
2. Replace every `g_weather.tempC` → `out.tempC`, `g_weather.humidity` → `out.humidity`, `g_weather.weatherCode` → `out.weatherCode`, `g_weather.isDay` → `out.isDay`, `g_weather.windKmh` → `out.windKmh` (all ~15 field writes)
3. Replace `g_weather.sunrise`/`g_weather.sunset` → `out.sunrise`/`out.sunset` (strncpy targets)
4. Replace `g_weather.nextTemp[]`, `g_weather.nextCode[]`, `g_weather.nextValid[]` → `out.xxx`
5. Replace `g_weather.tomorrowTemp`, `g_weather.tomorrowCode`, `g_weather.tomorrowValid` → `out.xxx`
6. **Remove** the time-gating check at top (`if (!force) { ... return g_weather.valid; }`) — stays in dispatcher
7. **Remove** `g_weather.valid = true;` / `g_weather.lastFetchMs = millis();` — set by caller under mutex
8. **Remove** all `pumpWebUiDuringIo()` calls (if any in this function)
9. **Remove** any `g_uiNeedsRedraw = true;` — UI update happens via dirty flag on Core 0
10. **Do NOT touch** any LVGL objects, `g_dispHw`, or display functions

```c
static bool netFetchWeather(WeatherState &out) {
  if (!wifiIsConnectedNow()) return false;
  // [Apply transformations 1-10 above to copied code]
  // Return true if parse succeeded, false on HTTP/parse error
}
```

- [ ] **Step 2: Gut `updateWeatherFromApi()` to become a queue dispatcher**

Replace the function body with:

```c
static bool updateWeatherFromApi(bool force) {
  if (!force) {
    const uint32_t now = millis();
    const uint32_t wait = g_weather.valid ? WEATHER_REFRESH_MS : WEATHER_RETRY_MS;
    if (g_weather.lastFetchMs != 0 && (now - g_weather.lastFetchMs) < wait) return g_weather.valid;
  }
  // If netTask is running, enqueue; otherwise fall back to inline (safety)
  if (g_netTaskReady) {
    netEnqueue(NET_REQ_WEATHER);
    return g_weather.valid;  // return current state, result arrives async
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
```

- [ ] **Step 3: Add weather dirty-flag consumption in `updateLvglUi()`**

Find `updateLvglUi()` and add near the top:

```c
// Consume network results (Phase 1)
if (g_weather.dirty) {
  xSemaphoreTake(g_netMutex, portMAX_DELAY);
  // Weather labels are updated from g_weather fields — already reads the struct
  g_weather.dirty = false;
  xSemaphoreGive(g_netMutex);
  g_uiNeedsRedraw = true;
}
```

- [ ] **Step 4: Compile + flash + verify**

Bump `FW_BUILD_TAG` to `r219`. Flash to device. Open serial monitor.

Expected:
- `[NET] task started on core 1`
- `[NET] weather done ok=1 dt=XXX ms`
- Weather data updates on display
- **Swipe should NOT freeze during weather fetch**

- [ ] **Step 5: Commit**

```bash
git add scrybar.ino config.h
git commit -m "perf(phase1): move weather fetch to Core 1 netTask"
```

---

### Task 1.4: Extract RSS fetch to Core 1

**Files:**
- Modify: `scrybar.ino` ~L6854-7105

- [ ] **Step 1: Create `netFetchRss()` wrapper**

Extract the HTTP body of `updateRssFromFeed()` into `netFetchRss()`. The function fetches all RSS feeds and writes results to a local buffer, then mutex-copies to `g_rss`.

**IMPORTANT: Feed URL thread safety.** Feed URLs in `g_runtimeNetConfig.rssFeeds[]` can be written by the web config handler on Core 0. Read them under mutex into local copies before starting the fetch loop:

```c
static void netFetchRss() {
  if (!wifiIsConnectedNow()) return;

  // Copy feed URLs under mutex (web UI can change them on Core 0)
  char feedUrls[RSS_FEED_SLOT_COUNT][RSS_FEED_URL_LEN];
  xSemaphoreTake(g_netMutex, portMAX_DELAY);
  for (int i = 0; i < RSS_FEED_SLOT_COUNT; ++i) {
    strncpy(feedUrls[i], g_runtimeNetConfig.rssFeeds[i].url, RSS_FEED_URL_LEN - 1);
    feedUrls[i][RSS_FEED_URL_LEN - 1] = '\0';
  }
  xSemaphoreGive(g_netMutex);

  // Allocate local parse buffer (or reuse g_rssParseBuf — it's only accessed from one task now)
  // [Copy HTTP fetch loop from updateRssFromFeed lines ~6965-7076]
  // Remove all pumpWebUiDuringIo() calls
  // After parsing all feeds:
  xSemaphoreTake(g_netMutex, portMAX_DELAY);
  // Copy parsed items to g_rss.items[]
  g_rss.itemCount = localCount;
  g_rss.valid = true;
  g_rss.lastFetchMs = millis();
  g_rss.dirty = true;
  xSemaphoreGive(g_netMutex);

  // Trigger favicon prefetch
  netEnqueue(NET_REQ_FAVICON);
}
```

- [ ] **Step 2: Gut `updateRssFromFeed()` to queue dispatcher**

Same pattern as weather:

```c
static bool updateRssFromFeed(bool force) {
  if (!force) {
    const uint32_t now = millis();
    const uint32_t wait = g_rss.valid ? RSS_REFRESH_MS : RSS_RETRY_MS;
    if (g_rss.lastAttemptMs != 0 && (now - g_rss.lastAttemptMs) < wait) return g_rss.valid;
    g_rss.lastAttemptMs = now;
  }
  if (g_netTaskReady) {
    netEnqueue(NET_REQ_RSS);
    return g_rss.valid;
  }
  // Fallback inline
  netFetchRss();
  return g_rss.valid;
}
```

- [ ] **Step 3: Add RSS dirty-flag consumption in `updateLvglUi()`**

```c
if (g_rss.dirty) {
  xSemaphoreTake(g_netMutex, portMAX_DELAY);
  g_rss.dirty = false;
  xSemaphoreGive(g_netMutex);
  g_uiNeedsRedraw = true;
}
```

- [ ] **Step 4: Compile + flash + verify**

Bump `FW_BUILD_TAG`. Expected: `[NET] rss done dt=XXX ms`. RSS cards appear. Swipe stays smooth during fetch.

- [ ] **Step 5: Commit**

```bash
git add scrybar.ino
git commit -m "perf(phase1): move RSS fetch to Core 1 netTask"
```

---

### Task 1.5: Extract Wiki + Favicon + WikiMeta to Core 1

**Files:**
- Modify: `scrybar.ino` ~L7129-7482, ~L6726-6824

- [ ] **Step 1: Create `netFetchWiki()` wrapper**

Same pattern — extract HTTP body from `updateWikiFromFeed()`, write to local, mutex-copy to `g_wiki`.

- [ ] **Step 2: Create `netFetchFavicons()` wrapper**

Call `faviconFetchAndCache()` for each unique host in `g_rss.items[]` and `g_wiki.items[]`. Read the item links under mutex to get host list, then fetch favicons without holding the mutex.

**Favicon cache access safety:** The favicon cache (`g_faviconCache[]`) is a separate PSRAM array with `uint16_t* pixels` pointers. LVGL reads the pixel data on Core 0 during rendering. The favicon fetch on Core 1 writes to a newly allocated buffer and then atomically sets the pointer. Since pointer writes on ESP32-S3 are atomic (32-bit aligned), no mutex is needed for the favicon cache itself. No `g_faviconDirty` flag needed — LVGL will see updated favicons on the next render cycle automatically.

- [ ] **Step 3: Create `netFetchWikiMeta()` wrapper**

Extract body of `wikiPreloadMetaStep()`. Same mutex pattern.

- [ ] **Step 4: Convert `wikiPreloadVisibleItemStep()` (~L7485) to queue dispatch**

This function makes HTTP calls on Core 0 when user is on WIKI page (line 14902 in loop). It must also be moved to Core 1. Convert it to enqueue `NET_REQ_WIKI_META` like `wikiPreloadMetaStep()`.

- [ ] **Step 5: Gut `updateWikiFromFeed()`, `wikiPreloadMetaStep()`, and `wikiPreloadVisibleItemStep()` to queue dispatchers**

- [ ] **Step 6: Add wiki dirty-flag consumption in `updateLvglUi()`**

```c
if (g_wiki.dirty) {
  xSemaphoreTake(g_netMutex, portMAX_DELAY);
  g_wiki.dirty = false;
  xSemaphoreGive(g_netMutex);
  g_uiNeedsRedraw = true;
}
```

- [ ] **Step 7: Disable `runRssDiag()` TLS usage on Core 0**

`runRssDiag()` (~L7521) uses `WiFiClientSecure` + `ScopedPsramTls` directly on Core 0 as a debug serial command. This would corrupt the global mbedtls allocator if netTask is running. Either:
- (a) Gate with `if (g_netTaskReady) { Serial.println("[DIAG] disabled — use web UI"); return; }`
- (b) Remove the TLS test section from the diagnostic

Option (a) is simplest and preserves the diagnostic for pre-netTask debugging.

- [ ] **Step 8: Compile + flash + verify**

Bump `FW_BUILD_TAG`. Expected: all `[NET]` logs show weather/rss/wiki/favicons/wiki_meta completing on Core 1.

- [ ] **Step 7: Commit**

```bash
git add scrybar.ino
git commit -m "perf(phase1): move wiki, favicon, wiki-meta fetch to Core 1"
```

---

### Task 1.6: Convert web UI force-refresh to queue sends

**Files:**
- Modify: `scrybar.ino` ~L5112-5148

- [ ] **Step 1: Update `handleWebReloadForm()`**

Replace direct fetch calls with queue sends:

```c
static void handleWebReloadForm() {
  if (webRequestHasConfigParams()) {
    String err;
    if (!applyRuntimeConfigFromRequest(err)) {
      webConfigRedirect("invalid");
      return;
    }
  }
  // Queue network refreshes instead of blocking inline
  netEnqueue(NET_REQ_WEATHER);
  netEnqueue(NET_REQ_RSS);
  netEnqueue(NET_REQ_WIKI);
  g_uiNeedsRedraw = true;
  Serial.println("[WEB] reload queued (weather+rss+wiki)");
  webConfigRedirect("reloaded");
}
```

- [ ] **Step 2: Update `handleWebReloadApi()` similarly**

```c
static void handleWebReloadApi() {
  if (webRequestHasConfigParams()) {
    String err;
    if (!applyRuntimeConfigFromRequest(err)) {
      sendWebConfigJson(400, false, err.c_str());
      return;
    }
  }
  netEnqueue(NET_REQ_WEATHER);
  netEnqueue(NET_REQ_RSS);
  netEnqueue(NET_REQ_WIKI);
  sendWebConfigJson(200, true, "reload queued");
}
```

- [ ] **Step 3: Update `cmdReload` serial command (~L14359)**

The serial `RELOAD` command also calls `updateWeatherFromApi(true)` etc. These become async dispatchers after refactoring, so `cmdReload` should print "reload queued" instead of reporting sync ok/fail results that would now be stale:

```c
// In handleSerialCommand, case "RELOAD":
netEnqueue(NET_REQ_WEATHER);
netEnqueue(NET_REQ_RSS);
netEnqueue(NET_REQ_WIKI);
Serial.println("[CMD] reload queued");
```

- [ ] **Step 5: Remove `pumpWebUiDuringIo()` calls from all network functions**

Search for `pumpWebUiDuringIo()` calls inside `fetchRssItemsFromUrl`, `updateRssFromFeed`, `fetchWikiRandomArticle`, `updateWikiFromFeed`, `faviconFetchAndCache`. Remove them all — they're no longer needed since network doesn't block the web server anymore.

- [ ] **Step 6: Compile + flash + verify**

Bump `FW_BUILD_TAG`. Test: use web UI to trigger reload. Should return instantly. Serial shows `[WEB] reload queued` followed by `[NET] weather/rss/wiki done`.

- [ ] **Step 7: Commit**

```bash
git add scrybar.ino
git commit -m "perf(phase1): convert web UI reload to async queue sends, remove pumpWebUiDuringIo"
```

---

### Task 1.7: Clean up loop() network calls

**Files:**
- Modify: `scrybar.ino` ~L14888-14908

- [ ] **Step 1: Simplify the network section of `loop()`**

The time-gating still happens in the dispatcher functions (`updateWeatherFromApi(false)`, etc.), which now just enqueue if due. The conditional branches for page mode can be simplified:

```c
#if TEST_DISPLAY && TEST_NTP
  updateWeatherFromApi(false);
#if TEST_WIFI && RSS_ENABLED
  updateRssFromFeed(false);
  updateWikiFromFeed(false);
  wikiPreloadMetaStep();
#endif
  handleTouchSwipeInput();
  // ...
```

The page-mode guards (`uiPageIsFeedDeck`) were there to avoid blocking the UI during fetch. With network on Core 1, they're no longer needed — the fetches are async.

- [ ] **Step 2: Compile + flash + full integration test**

Bump `FW_BUILD_TAG`. This is the Phase 1 integration checkpoint.

**Human test protocol:**
1. Boot device, let it connect to WiFi. Note: initial boot will queue multiple requests at once (`[NET]` logs show weather/rss/wiki queued rapidly) — device may show no data for a few seconds while the queue drains. This is expected.
2. Swipe between all pages while serial shows `[NET]` fetches happening
3. **Verify: zero UI freezes during network activity**
4. Check `[PERF]` output — FPS should be stable
5. Check `[NET] stack high-water` — should be >2KB remaining
6. Test web UI reload button — should respond instantly
7. Test web UI config change (e.g., change weather city) — should respond instantly, fetch queued in background
8. Test serial `RELOAD` command — should print "reload queued"
9. Test DOOM — should still work (Core 1 coexistence)

- [ ] **Step 3: Commit**

```bash
git add scrybar.ino
git commit -m "perf(phase1): simplify loop() network calls, Phase 1 complete"
```

---

## Phase 2: Display Pipeline

### Task 2.1: LVGL double-buffering

**Files:**
- Modify: `scrybar.ino` ~L13484-13498

- [ ] **Step 1: Add second LVGL buffer allocation**

In the LVGL init section (~L13484), after `g_lvglBuf1` allocation:

```c
g_lvglBuf1 = (lv_color_t*)heap_caps_malloc(bufPx * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
if (!g_lvglBuf1) g_lvglBuf1 = (lv_color_t*)malloc(bufPx * sizeof(lv_color_t));
if (!g_lvglBuf1) {
  Serial.println("[LVGL][ERR] alloc draw buffer fallita");
  return false;
}

// Phase 2: double-buffering — second buffer for render/flush overlap
static lv_color_t *g_lvglBuf2 = nullptr;
g_lvglBuf2 = (lv_color_t*)heap_caps_malloc(bufPx * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
if (g_lvglBuf2) {
  Serial.printf("[LVGL] double-buffer enabled (%u + %u KB PSRAM)\n",
                (unsigned)(bufPx * sizeof(lv_color_t) / 1024),
                (unsigned)(bufPx * sizeof(lv_color_t) / 1024));
} else {
  Serial.println("[LVGL][WARN] second buffer alloc failed — single-buffer fallback");
}

lv_disp_draw_buf_init(&g_lvglDrawBuf, g_lvglBuf1, g_lvglBuf2, bufPx);  // g_lvglBuf2 may be nullptr (single-buffer fallback)
```

- [ ] **Step 2: Compile + flash + verify**

Bump `FW_BUILD_TAG`. Serial should show `[LVGL] double-buffer enabled (215 + 215 KB PSRAM)`.

- [ ] **Step 3: Commit**

```bash
git add scrybar.ino
git commit -m "perf(phase2): enable LVGL double-buffering (+215KB PSRAM)"
```

---

### Task 2.2: Bigger DMA chunks

**Files:**
- Modify: `scrybar.ino` ~L1314, ~L2668

- [ ] **Step 1: Change `DB_CHUNK_ROWS` from 32 to 64**

At line ~1314:

```c
static constexpr int16_t DB_CHUNK_ROWS = 64;  // was 32 — halves semaphore overhead per frame
```

- [ ] **Step 2: Fix the stale comment in `dispFlush()`**

At line ~2668, update the comment:

```c
const int chunks = DB_NATIVE_H / DB_CHUNK_ROWS;  // 640/64 = 10
```

- [ ] **Step 3: Compile + flash + verify**

Bump `FW_BUILD_TAG`. Check `[PERF]` output — `flush_avg_us` should decrease compared to Phase 1 baseline.

Expected: DMA buffer allocation log shows 22KB per buffer (was 11KB).

- [ ] **Step 4: Commit**

```bash
git add scrybar.ino
git commit -m "perf(phase2): double DMA chunk size to 64 rows (10 chunks, was 20)"
```

---

### Task 2.3: Cache-aligned tile rotation

**Files:**
- Modify: `scrybar.ino` ~L2649-2662

- [ ] **Step 1: Change tile size from 8 to 16 in `dispRotateChunk()`**

Replace the function:

```c
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
```

- [ ] **Step 2: Compile + flash + verify**

Bump `FW_BUILD_TAG`. Compare `[PERF] flush_avg_us` — rotation portion should be faster.

- [ ] **Step 3: Commit**

```bash
git add scrybar.ino
git commit -m "perf(phase2): 16x16 tile rotation aligned to PSRAM cache line"
```

---

### Task 2.4: memcpy flush callback

**Files:**
- Modify: `scrybar.ino` ~L12299-12341

- [ ] **Step 1: Replace `lvglDisplayFlushCb` with memcpy version**

```c
static void lvglDisplayFlushCb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
  if (!g_dispHw.canvasBuf) { lv_disp_flush_ready(drv); return; }  // null guard (preserve from original)
  const int32_t w = area->x2 - area->x1 + 1;
  const lv_color_t *src = color_p;
  for (int32_t y = area->y1; y <= area->y2; ++y) {
    if (y < 0 || y >= DB_CANVAS_H) { src += w; continue; }
    const int32_t x1 = max((int32_t)0, (int32_t)area->x1);
    const int32_t x2 = min((int32_t)(DB_CANVAS_W - 1), (int32_t)area->x2);
    if (x1 > x2) { src += w; continue; }
    const int32_t copyW = x2 - x1 + 1;
    uint16_t *dst = &g_dispHw.canvasBuf[(size_t)y * DB_CANVAS_W + x1];
    memcpy(dst, src + (x1 - area->x1), copyW * sizeof(uint16_t));
    src += w;
  }
  g_dispHw.canvasDirty = true;
  lv_disp_flush_ready(drv);
}
```

- [ ] **Step 2: Compile + flash + verify**

Bump `FW_BUILD_TAG`. Display should render correctly with no visual artifacts. Check `[PERF]` output.

- [ ] **Step 3: Commit**

```bash
git add scrybar.ino
git commit -m "perf(phase2): memcpy flush callback replaces pixel loop"
```

---

### Task 2.5: Adaptive LVGL cadence

**Files:**
- Modify: `scrybar.ino` ~L13754-13782

- [ ] **Step 1: Replace fixed 12ms cadence with adaptive logic in `runLvglLoop()`**

Find the cadence check (~L13758):

```c
if (g_pageAnim.lastRunMs != 0 && (now - g_pageAnim.lastRunMs) < 12) return;
```

Replace with:

```c
const bool animating = (g_pageAnim.untilMs > now) || (lv_anim_count_running() > 0);
const uint16_t cadenceMs = animating ? 8 : 20;
if (g_pageAnim.lastRunMs != 0 && (now - g_pageAnim.lastRunMs) < cadenceMs) return;
```

- [ ] **Step 2: Compile + flash + full Phase 2 integration test**

Bump `FW_BUILD_TAG`. This is the Phase 2 checkpoint.

**Human test protocol:**
1. Swipe between pages — should feel noticeably smoother than Phase 1
2. Check `[PERF]` output: FPS during animation should be higher (8ms cadence = ~125 FPS ceiling)
3. FPS when idle should be lower (~50 FPS) — that's intentional
4. Check serial for any `[LVGL][WARN]` about buffer allocation failure
5. Verify all views render correctly: HOME, RSS, WIKI, INFO, DOOM

- [ ] **Step 3: Commit**

```bash
git add scrybar.ino
git commit -m "perf(phase2): adaptive LVGL cadence (8ms anim, 20ms idle), Phase 2 complete"
```

---

## Phase 3: Network Efficiency

### Task 3.1: Replace String with snprintf in weather URL

**Files:**
- Modify: `scrybar.ino` ~L7580-7610 (inside `netFetchWeather`)

- [ ] **Step 1: Replace String concatenation with snprintf**

Find the weather URL construction (originally in `updateWeatherFromApi`, now in `netFetchWeather`). Replace:

```c
String url = "http://api.open-meteo.com/v1/forecast?latitude=";
url += String(lat, 4);
// ... etc
```

With:

```c
char url[400];
snprintf(url, sizeof(url),
  "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
  "&current=temperature_2m,relative_humidity_2m,weather_code,is_day,wind_speed_10m"
  "&hourly=temperature_2m,weather_code"
  "&daily=sunrise,sunset"
  "&timezone=Europe%%2FRome&forecast_hours=36&forecast_days=2",
  lat, lon);
```

- [ ] **Step 2: Replace `http.getString()` with stream read**

Find where the weather response is read. Replace:

```c
String payload = http.getString();
```

With a read into a stack buffer:

```c
WiFiClient *stream = http.getStreamPtr();
char payload[4096];  // weather response is typically <3KB
size_t payloadLen = 0;
while (stream->available() && payloadLen < sizeof(payload) - 1) {
  int n = stream->readBytes(payload + payloadLen, min((size_t)256, sizeof(payload) - 1 - payloadLen));
  if (n <= 0) break;
  payloadLen += n;
}
payload[payloadLen] = '\0';
```

Note: the existing `extractJsonNumberField()` etc. parsers take `const char*`, so they work with `char[]` unchanged.

- [ ] **Step 3: Compile + flash + verify weather still works**

- [ ] **Step 4: Commit**

```bash
git add scrybar.ino
git commit -m "perf(phase3): replace String with snprintf/stream-read in weather fetch"
```

---

### Task 3.2: Replace String in RSS/Wiki/Favicon paths

**Files:**
- Modify: `scrybar.ino` — all `netFetch*` functions

- [ ] **Step 1: Audit all remaining `String` usage in network functions**

Search for `String url`, `String payload`, `http.getString()`, `url +=`, `payload +=` inside:
- `netFetchRss()` / `fetchRssItemsFromUrl()`
- `netFetchWiki()` / `fetchWikiRandomArticle()`
- `netFetchFavicons()` / `faviconFetchAndCache()`

- [ ] **Step 2: Replace each with snprintf + stream-read**

Apply the same pattern: `snprintf` for URL construction, `stream->readBytes` for response reading.

For RSS feeds where the URL comes from config (already a `char[]`), just pass it directly to `http.begin()` — no String needed.

- [ ] **Step 3: Replace String in `handleWebReloadApi()` response**

At ~L5141-5147, replace:

```c
String msg = "weather=";
msg += (wOk ? "ok" : "fail");
// ...
```

With:

```c
char msg[64];
snprintf(msg, sizeof(msg), "reload queued");
sendWebConfigJson(200, true, msg);
```

(This function was already simplified in Task 1.6 to just queue, but clean up any remaining String usage.)

- [ ] **Step 4: Compile + flash + verify all feeds work**

- [ ] **Step 5: Commit**

```bash
git add scrybar.ino
git commit -m "perf(phase3): eliminate Arduino String from all network paths"
```

---

### Task 3.3: DNS cache

**Files:**
- Modify: `scrybar.ino`

- [ ] **Step 1: Add DNS cache data structure**

Near the network globals (~after the `g_netTaskReady` definition):

```c
// --- DNS cache (Phase 3) ---
struct DnsCacheEntry {
  char host[64];
  IPAddress ip;
  uint32_t resolvedMs;
  bool valid;
};
static DnsCacheEntry g_dnsCache[8] = {};

static bool resolveHostCached(const char* host, IPAddress& out) {
  const uint32_t now = millis();
  // Check cache
  for (auto& e : g_dnsCache) {
    if (e.valid && strcmp(e.host, host) == 0 && (now - e.resolvedMs) < 3600000UL) {
      out = e.ip;
      return true;
    }
  }
  // Miss: resolve via DNS
  if (!WiFi.hostByName(host, out)) return false;
  // Store in cache — evict first invalid slot, or oldest by resolvedMs
  int evictIdx = 0;
  for (int i = 0; i < 8; ++i) {
    if (!g_dnsCache[i].valid) { evictIdx = i; break; }
    if (g_dnsCache[i].resolvedMs < g_dnsCache[evictIdx].resolvedMs) evictIdx = i;
  }
  strncpy(g_dnsCache[evictIdx].host, host, sizeof(g_dnsCache[evictIdx].host) - 1);
  g_dnsCache[evictIdx].host[sizeof(g_dnsCache[evictIdx].host) - 1] = '\0';
  g_dnsCache[evictIdx].ip = out;
  g_dnsCache[evictIdx].resolvedMs = now;
  g_dnsCache[evictIdx].valid = true;
  return true;
}
```

- [ ] **Step 2: Use `resolveHostCached()` in network functions**

In each `netFetch*` function, before `http.begin(tls, url)`, extract the host from the URL and pre-resolve:

```c
// Extract host from URL for DNS cache
char host[64];
extractHostFromUrl(url, host, sizeof(host));  // helper: parse between "://" and "/"
IPAddress resolved;
if (resolveHostCached(host, resolved)) {
  tls.connect(resolved, 443);  // pre-connect with cached IP
}
```

Note: this is an optimization hint. `HTTPClient::begin()` will still do its own resolution, but the OS DNS cache will have the result warm.

Alternative simpler approach: just call `resolveHostCached()` to warm the OS cache. The ESP-IDF lwIP stack has its own DNS cache, so warming it is sufficient.

- [ ] **Step 3: Compile + flash + verify**

Check serial for DNS cache hits after the second fetch cycle.

- [ ] **Step 4: Commit**

```bash
git add scrybar.ino
git commit -m "perf(phase3): add 8-entry DNS cache with 1-hour TTL"
```

---

### Task 3.4: HTTP/1.1 keep-alive for same-host feeds

**Files:**
- Modify: `scrybar.ino` — `netFetchRss()`, `fetchRssItemsFromUrl()`

- [ ] **Step 1: Remove HTTP/1.0 forcing and Connection:close**

In `fetchRssItemsFromUrl()`, remove:

```c
http.useHTTP10(true);
// and
http.addHeader("Connection", "close");
```

Do the same in all other fetch functions (wiki, favicon).

- [ ] **Step 2: Reuse WiFiClientSecure across same-host RSS feeds**

In `netFetchRss()`, create one `WiFiClientSecure` before the feed loop and reuse it:

```c
WiFiClientSecure tls;
tls.setInsecure();
tls.setHandshakeTimeout((RSS_HTTP_TIMEOUT_MS + 999U) / 1000U);

for (int slot = 0; slot < RSS_FEED_SLOT_COUNT; ++slot) {
  // ... get feed URL ...
  if (!tls.connected()) {
    // New connection needed (first request or server closed)
  }
  HTTPClient http;
  http.setConnectTimeout(RSS_HTTP_TIMEOUT_MS);
  http.setTimeout(RSS_HTTP_TIMEOUT_MS);
  http.begin(tls, feedUrl);
  int code = http.GET();
  // ... parse ...
  // Don't call http.end() between same-host feeds — keep connection alive
}
tls.stop();  // Clean close after all feeds
```

- [ ] **Step 3: Compile + flash + verify**

Check `[NET] rss done dt=XXX ms` — total time should be significantly less if multiple feeds share a host.

- [ ] **Step 4: Commit**

```bash
git add scrybar.ino
git commit -m "perf(phase3): HTTP/1.1 keep-alive for same-host RSS feeds"
```

---

### Task 3.5: Final integration test + perf baseline

**Files:**
- Modify: `config.h` (bump build tag)

- [ ] **Step 1: Bump FW_BUILD_TAG for final Phase 3 build**

- [ ] **Step 2: Compile + flash + full integration test**

**Human test protocol — full regression:**
1. Boot device, watch serial for clean startup
2. `[NET] task started on core 1` — network task running
3. `[LVGL] double-buffer enabled` — double-buffer active
4. Swipe all 5 pages rapidly while network fetches happen — **zero freezes**
5. Check `[PERF]` output:
   - `fps` during swipe animation: should be 50-100+ range
   - `flush_avg_us`: lower than r218 baseline
   - `lvgl_avg_us`: stable
6. Check `[NET]` timing:
   - Weather fetch: <3s
   - RSS cycle (same-host): <5s (was ~20s)
   - Wiki fetch: <5s
7. `[NET] stack high-water`: >2KB remaining
8. Web UI reload: responds instantly, feeds refresh in background
9. DOOM: works, smooth gameplay, netTask doesn't interfere
10. Memory: `[HEARTBEAT] free_psram` should be >3MB
11. Long-running stability: leave device on for 10+ minutes, verify no crashes or heap fragmentation

- [ ] **Step 3: Record baseline performance numbers**

From serial output, capture:
- `[PERF]` averages (flush_avg_us, lvgl_avg_us, fps)
- `[NET]` fetch durations
- `[HEARTBEAT]` free_heap and free_psram after 5 minutes
- `[NET] stack high-water` mark

These become the new performance baseline for future work.

- [ ] **Step 4: Commit + update knowledge**

```bash
git add config.h scrybar.ino
git commit -m "perf: Phase 3 complete — all optimizations active, performance baseline recorded"
```

Update `knowledge/decisions.md` with the performance overhaul summary and baseline numbers.

**Note:** Spec section 3D (Streaming XML/JSON parsing — SAX-style) is intentionally deferred as a stretch goal. Implement only after Phases 1-3 are stable and if heap fragmentation from buffered responses is still a concern (unlikely after Phase 3A eliminates `http.getString()`).

- [ ] **Step 5: Update MEMORY.md**

Add performance overhaul section to session memory with key numbers and architecture changes.
