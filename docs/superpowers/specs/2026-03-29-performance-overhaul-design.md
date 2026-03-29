# ScryBar Performance Overhaul — Design Spec

**Date:** 2026-03-29
**Firmware:** r218 (baseline) → r219+ (target)
**Goal:** Eliminate UI hitches, maximize frame rate during swipe animations, reduce network overhead. Make interactions feel magical.

## Problem Statement

All firmware logic runs on Core 0: LVGL rendering, touch polling, serial commands, **and** blocking HTTP requests (weather, RSS, Wikipedia, favicons). Every network fetch freezes the display for 3-7 seconds — touch input and animations halt completely.

Secondary issues: single-buffer LVGL (can't overlap render + DMA), suboptimal DMA chunk size, heap fragmentation from Arduino `String`, no DNS caching, no HTTP keep-alive.

## Current Architecture (r218)

### Display Pipeline
- **Display**: AXS15231B, QSPI 40MHz, 640x172 RGB565 (native 172x640 portrait)
- **LVGL draw buffer**: 1x full-screen (430KB PSRAM), single-buffered
- **Flush callback**: pixel-by-pixel copy to canvas buffer (PSRAM, 215KB)
- **Rotation**: 8x8 tile transpose from canvas → DMA buffer
- **DMA**: 20 chunks (32 rows each, 11KB per buffer, double-buffered)
- **Cadence**: fixed 12ms throttle (~83 FPS ceiling)

### Task Architecture
- **Core 0**: loop() — LVGL, touch, network, serial, web server (everything)
- **Core 1**: DOOM only (idle when not playing)

### Network
- HTTP/1.0 + Connection:close (new TCP+TLS per request)
- Sequential feed fetching (5 RSS feeds one-by-one)
- No DNS caching (12-15 lookups per cycle)
- `String` concatenation for URLs/responses (heap fragmentation)
- Full response buffered before parsing

## Design — 3 Phases

### Phase 1: Network Isolation (eliminates hitches)

Move all HTTP I/O to a dedicated FreeRTOS task on Core 1.

#### New Components

**`netTask`** — FreeRTOS task, Core 1, priority 2, stack 16KB
- Receives requests from `g_netQueue` (FreeRTOS queue, depth 4)
- Executes HTTP operations (weather, RSS, wiki, favicons)
- Writes results to shared data structs under `g_netMutex`
- Sets dirty flags for LVGL to pick up

**`g_netQueue`** — `xQueueHandle`, 4 entries of `NetRequest` struct
```c
enum NetRequestType { NET_REQ_WEATHER, NET_REQ_RSS, NET_REQ_WIKI, NET_REQ_FAVICON };
struct NetRequest { NetRequestType type; };
```

**`g_netMutex`** — `SemaphoreHandle_t` (mutex), protects shared data

#### Data Flow

```
Core 0 (loop)                    Core 1 (netTask)
─────────────                    ────────────────
if (weatherDue)                  xQueueReceive(g_netQueue)
  xQueueSend(WEATHER)    ──►    fetchWeather() [blocking HTTP, up to 7s]
                                 xSemaphoreTake(g_netMutex)
                                 copy weather results to shared struct
                                 g_weather.dirty = true
                                 xSemaphoreGive(g_netMutex)

// In updateLvglUi:
if (g_weather.dirty)
  xSemaphoreTake(g_netMutex)
  update LVGL labels
  g_weather.dirty = false
  xSemaphoreGive(g_netMutex)
```

#### What Moves to Core 1
- `updateWeatherFromApi()` HTTP body
- `fetchRssItemsFromUrl()` + `updateRssFromFeed()` HTTP body
- `updateWikiFromFeed()` + `fetchWikiRandomArticle()` HTTP body
- `faviconFetchAndCache()` all calls
- `wikiPreloadMetaStep()` meta enrichment HTTP

#### What Stays on Core 0
- `handleWiFiReconnectLoop()` — WiFi state machine (protocol core)
- `handleWebConfigServerLoop()` — web UI serving
- Touch input, LVGL rendering, serial commands
- Time-gating logic (checking if fetch is due)
- `pumpWebUiDuringIo()` — removed from network functions (no longer needed)

#### DOOM Coexistence
DOOM runs on Core 1 at priority 1 (netTask is priority 2). When DOOM is active, it yields during `vTaskDelay()` in its game loop. netTask can use those gaps for background fetches. When DOOM is inactive, netTask has Core 1 to itself.

#### Mutex Granularity
One mutex for all shared network data. Mutex hold time is minimal (memcpy of result structs, ~10-50us). No risk of priority inversion with DOOM (DOOM doesn't touch network data).

---

### Phase 2: Display Pipeline (smoother frames)

#### 2A. LVGL Double-Buffering

Allocate second full-screen PSRAM buffer:
```c
g_lvglBuf2 = (lv_color_t*)heap_caps_malloc(bufPx * sizeof(lv_color_t),
                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
lv_disp_draw_buf_init(&g_lvglDrawBuf, g_lvglBuf1, g_lvglBuf2, bufPx);
```

PSRAM cost: +430KB (total ~1.1MB of 4MB = 27%).

Benefit: LVGL can render frame N+1 into buffer B while buffer A is being flushed to display. Eliminates render→flush serial bottleneck.

#### 2B. Larger DMA Chunks

```c
static constexpr int16_t DB_CHUNK_ROWS = 64;  // was 32
```

- DMA buffers: 172 x 64 x 2 = 22,016 bytes each (was 11,008)
- Two buffers: 44KB total internal SRAM (was 22KB)
- Chunks per frame: 10 (was 20)
- Half the semaphore wait/signal overhead per frame

Safe because: with network on Core 1, TLS buffers (~40-50KB internal SRAM) don't compete with DMA on the same thread. ESP32-S3 has ~300KB free internal SRAM — 44KB DMA + 50KB TLS = 94KB, well within budget.

#### 2C. Cache-Aligned Tile Rotation

Increase tile size from 8x8 to 16x16 to match PSRAM cache line (32 bytes = 16 pixels):

```c
static inline void dispRotateChunk(uint16_t *dst, int16_t colBase) {
  constexpr int T = 16;  // was 8
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

Expected improvement: ~20-40% faster rotation due to fewer cache misses.

Note: DB_CANVAS_H = 172 is not divisible by 16 (172 = 10*16 + 12). The `diEnd` clamp handles the remainder tile correctly (last tile is 12 rows instead of 16).

#### 2D. memcpy in Flush Callback

Replace pixel-by-pixel copy with memcpy for contiguous row segments:

```c
static void lvglDisplayFlushCb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
  const int32_t w = area->x2 - area->x1 + 1;
  const lv_color_t *src = color_p;
  for (int32_t y = area->y1; y <= area->y2; ++y) {
    if (y < 0 || y >= DB_CANVAS_H) { src += w; continue; }
    int32_t x1 = max((int32_t)0, (int32_t)area->x1);
    int32_t x2 = min((int32_t)(DB_CANVAS_W - 1), (int32_t)area->x2);
    if (x1 > x2) { src += w; continue; }
    const int32_t copyW = x2 - x1 + 1;
    uint16_t *dst = &g_dispHw.canvasBuf[(size_t)y * DB_CANVAS_W + x1];
    memcpy(dst, &src[x1 - area->x1].full, copyW * sizeof(uint16_t));
    src += w;
  }
  g_dispHw.canvasDirty = true;
  lv_disp_flush_ready(drv);
}
```

#### 2E. Adaptive LVGL Cadence

```c
const bool animating = (g_pageAnim.untilMs > now);
const uint16_t cadenceMs = animating ? 8 : 20;
if (g_pageAnim.lastRunMs != 0 && (now - g_pageAnim.lastRunMs) < cadenceMs) return;
```

- During page slide animations: 8ms cadence (~125 FPS ceiling)
- When static: 20ms cadence (~50 FPS, saves CPU for background work)

---

### Phase 3: Network Efficiency (faster fetches)

#### 3A. Eliminate Arduino String from Network Paths

Replace all `String` concatenation in URL construction with `snprintf` into stack/static buffers:

```c
// Weather URL
char url[320];
snprintf(url, sizeof(url),
  "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
  "&current=temperature_2m,relative_humidity_2m,weather_code,is_day,wind_speed_10m"
  "&hourly=temperature_2m,weather_code&daily=sunrise,sunset"
  "&timezone=Europe%%2FRome&forecast_hours=36&forecast_days=2",
  lat, lon);
```

Replace `http.getString()` with streaming read into pre-allocated PSRAM buffer:

```c
static char* g_netResponseBuf = nullptr;  // allocated once in PSRAM, ~8KB
// ...
WiFiClient *stream = http.getStreamPtr();
size_t len = 0;
while (stream->available() && len < kNetBufSize - 1) {
  len += stream->readBytes(g_netResponseBuf + len, min((size_t)256, kNetBufSize - 1 - len));
}
g_netResponseBuf[len] = '\0';
```

#### 3B. HTTP/1.1 Keep-Alive

Remove forced HTTP/1.0 and Connection:close:

```c
// REMOVE these lines:
// http.useHTTP10(true);
// http.addHeader("Connection", "close");
```

For sequential same-host requests (e.g., 5 RSS feeds from ansa.it), reuse the `WiFiClientSecure` object across requests. One TCP+TLS handshake serves all feeds.

Expected saving: 5 feeds from same host goes from ~20s (5 handshakes) to ~5s (1 handshake + 5 GETs).

#### 3C. DNS Cache

8-entry LRU cache with 1-hour TTL:

```c
struct DnsCacheEntry {
  char host[64];
  IPAddress ip;
  uint32_t resolvedMs;
  bool valid;
};
static DnsCacheEntry g_dnsCache[8];
static SemaphoreHandle_t g_dnsMutex;  // if accessed from multiple tasks

bool resolveHostCached(const char* host, IPAddress& out) {
  for (auto& e : g_dnsCache) {
    if (e.valid && strcmp(e.host, host) == 0 && (millis() - e.resolvedMs) < 3600000UL) {
      out = e.ip;
      return true;
    }
  }
  if (WiFi.hostByName(host, out)) {
    // Evict oldest, store new entry
    return true;
  }
  return false;
}
```

Since all network runs on Core 1 (after Phase 1), the DNS cache doesn't need a mutex — single-threaded access. Saves ~50-200ms per cached hit.

#### 3D. Streaming XML/JSON Parsing (Stretch Goal)

Replace full-response buffering with incremental SAX-style XML parsing for RSS feeds. Parse `<item>`, `<title>`, `<link>` tags as they arrive from the stream, never holding the full response in memory.

Lower priority — implement only after Phases 1-3A/B/C are stable and tested.

---

## Memory Budget

| Resource | Before (r218) | After | Delta |
|----------|---------------|-------|-------|
| **PSRAM (persistent)** | ~645KB | ~1,085KB | +440KB |
| LVGL draw buffers | 430KB (1x) | 860KB (2x) | +430KB |
| Canvas buffer | 215KB | 215KB | 0 |
| Net response buffer | 0 (String) | ~8KB | +8KB |
| DNS cache | 0 | ~0.5KB | +0.5KB |
| **Internal SRAM (persistent)** | ~22KB | ~44KB | +22KB |
| DMA buffers | 22KB (2x 11K) | 44KB (2x 22K) | +22KB |
| **Internal SRAM (transient)** | ~50KB | ~50KB | 0 |
| TLS handshake buffers | ~50KB | ~50KB | 0 |
| **Stack (new task)** | 0 | 16KB | +16KB |
| netTask stack | 0 | 16KB | +16KB |
| **PSRAM free (of 4MB)** | ~3.3MB | ~2.9MB | -0.4MB |

All within safe margins. PSRAM usage goes from ~16% to ~27%.

## Risk Assessment

| Risk | Mitigation |
|------|-----------|
| Mutex contention between Core 0 (LVGL) and Core 1 (net) | Hold time is minimal (memcpy, <50us). No priority inversion risk. |
| DOOM + netTask both on Core 1 | Different priorities (DOOM=1, net=2). DOOM yields in its game loop. |
| Bigger DMA buffers competing with TLS for internal SRAM | TLS is transient, DMA is persistent. 44KB + 50KB = 94KB out of ~300KB available. |
| Double-buffer PSRAM allocation failure | Fallback to single-buffer with warning log. |
| HTTP/1.1 keep-alive connection stale | HTTPClient handles this — falls back to new connection on error. |

## Testing Strategy

Each phase is independently testable:

- **Phase 1**: Flash, open serial monitor, swipe while RSS/weather is fetching. Should see zero frame drops. `[PERF]` log should show consistent FPS.
- **Phase 2**: Compare `[PERF] flush_avg_us` and `fps` before/after. FPS during swipe animation should increase noticeably.
- **Phase 3**: Serial log shows `[NET]` timing for each fetch. Compare total cycle time before/after.

Human testing needed for:
- Swipe responsiveness during network activity (Phase 1)
- Animation smoothness comparison (Phase 2)
- Overall "feel" assessment (all phases)

## Files to Modify

### Phase 1
- `scrybar.ino`: Extract network functions, add netTask, queue, mutex, dirty flags
- `config.h`: Add `NET_TASK_STACK`, `NET_TASK_PRIORITY`, `NET_QUEUE_DEPTH`

### Phase 2
- `scrybar.ino`: Double-buffer init, bigger DMA chunks, rotation optimization, flush callback, adaptive cadence
- `config.h`: Update `DB_CHUNK_ROWS`

### Phase 3
- `scrybar.ino`: Replace String with snprintf, streaming response read, HTTP/1.1 keep-alive, DNS cache
- New: `src/net/dns_cache.h` (optional — could be inline in scrybar.ino)
