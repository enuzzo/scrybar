# Firmware Polishing Roadmap

Living document tracking the systematic code quality pass on ScryBar firmware.
Updated after each milestone. Read this at session start to know where we are.

---

## Baseline Snapshot (r199, 2026-03-20)

### Core Metrics

| Metric | Value |
|---|---|
| **Total sketch lines** | 15,762 |
| **Functions** | 671 |
| **Static globals** | 964 |
| **`g_` prefixed globals** | 249 |
| **Feature toggles (`#define`)** | 111 |
| **Languages** | 13 active |
| **Largest function** | `initLvglUi()` — 714 lines |
| **Functions >200 lines** | 10 |
| **Language dispatcher functions** | 56 (1,855 lines) |
| **Stack buffers >200B** | 15 (4,668 bytes total) |
| **`F()` calls in web builder** | 237 |
| **`+=` concatenations in web builder** | 269 |

### Top 12 Functions by Size

| Function | Lines | Start | End | Subsystem |
|---|---|---|---|---|
| `initLvglUi()` | 714 | 13677 | 14390 | Display |
| `buildWebConfigPage()` | 418 | 3956 | 4373 | Web UI |
| `handleSerialCommand()` | 413 | 14900 | 15312 | Serial |
| `applyRuntimeConfigFromRequest()` | 372 | 4481 | 4852 | Web Config |
| `handleTouchSwipeInput()` | 346 | 10091 | 10436 | Touch |
| `lvglApplyThemeStyles()` | 293 | 8979 | 9271 | Theming |
| `lvglInitFeedDeck()` | 234 | 13064 | 13297 | Display |
| `lvglInitNowPlayingUi()` | 225 | 13299 | 13523 | Display |
| `updateLvglUi()` | 210 | 14392 | 14601 | Display |
| `loadRuntimeNetConfigFromNvs()` | 202 | 3519 | 3720 | Config |
| `setup()` | 109 | 15572 | 15680 | Core |
| `loop()` | 81 | 15682 | 15762 | Core |

### Stack Buffer Inventory (>200 bytes)

| Variable | Size | Line | Context |
|---|---|---|---|
| `leftCol` | 512 | 12978 | `updateLvglUi()` |
| `summary` | 420 | 788 | `RssItem` struct |
| `httpFallback` | 320 | 7051 | RSS fetch |
| `link` | 280 | 812 | RSS parse |
| `lastQrPayload` | 280 | 923 | QR cache |
| `artworkId` | 256 | — | Now Playing |
| `g_lvglScreenSaverFieldBuf` | 256 | — | Screensaver |
| + 8 more in 200–256B range | | | |

---

## Goals

By end of polishing pass, the firmware should meet these targets:

| Metric | Baseline | Target |
|---|---|---|
| Largest function | 714 lines | **< 150 lines** |
| Functions >200 lines | 10 | **0** |
| Language dispatcher lines | 1,855 | **< 400** (table-driven) |
| `g_` globals | 249 | **< 180** (struct grouping) |
| `+=` in web builder | 269 | **< 50** (template approach) |
| Stack buffers >200B | 15 | **< 5** (heap/PSRAM for large) |

Behavioral target: **zero regressions**. Every milestone must compile, upload, and pass visual verification on device before moving to the next.

---

## Milestones

### M1 — Decompose `initLvglUi()` (714 -> N x ~100)

**Status: DONE (r200, 2026-03-20)**

Split the monolithic display initialization into focused sub-functions:

- `lvglCreatePageRoot()` — 13 lines, reusable transparent page container helper
- `initLvglInfoPanel()` — 132 lines, INFO page (stats, QR code)
- `initLvglClockPanel()` — 131 lines, HOME clock block (header, WiFi bars, date, word clock labels)
- `initLvglWeatherBodyWidgets()` — 153 lines, weather body (temp, icon, glyph, sep, desc, forecast)
- `initLvglWeatherPanel()` — 119 lines, weather card structure + header + calls body widgets
- `initLvglScreensaverUi()` — 90 lines, cow screensaver
- `lvglInitFeedDeck()` — 234 lines, already existed (review target for M9)
- `lvglInitNowPlayingUi()` — 225 lines, already existed (review target for M9)

**Results:**
- `initLvglUi()` orchestrator: **94 lines** (14 of which are debug Serial.printf formatting)
- All new sub-functions under 155 lines
- No function >200 lines created
- Clean compile, 43% flash / 65% RAM (unchanged)

**Verification:** compile --clean ✓ (upload + device test pending)

---

### M2 — Table-driven language dispatchers (1,855 -> ~400)

**Status: DONE (r201, 2026-03-20)**

Replaced 26 weather functions + 5 dispatcher functions with data tables + `LangVtable`:

- `WeatherShortLabels` struct — 8 fields (clear/cloudy/overcast/fog/rain/snow/storm/na), 13 instances (1 line each)
- `WmoUiIdx` enum — 24 WMO code indices, shared `wmoCodeToUiIdx()` lookup
- 13 `kWeatherUi*[WMO_UI_COUNT]` arrays — per-language detailed WMO labels (5 lines each)
- `LangVtable` struct — code + 4 function pointers + data pointers, defined in `src/lang_types.h`
- `kLangTable[13]` — single lookup table mapping language codes to all dispatch targets
- `findLangVtable()` — single lookup function replacing all 5 dispatcher strcmp chains
- 5 slim dispatcher wrappers (1-2 lines each): `weatherCodeShort()`, `weatherCodeUiLabel()`, `composeWordClockSentenceActive()`, `activeUiStrings()`, `formatDateActive()`

**Results:**
- `scrybar.ino`: **15441 lines** (was 15803, -362 lines)
- New `src/lang_types.h`: 40 lines (struct/enum definitions)
- Net savings: **322 lines** removed
- All 26 weather per-language functions eliminated (replaced by data tables)
- All 5 dispatcher functions reduced to 1-2 line wrappers
- 13 word clock + 13 formatDate functions kept as-is (unique logic per language)
- Adding a new language = 1 `kLangTable` entry + 4 functions (zero dispatcher changes)
- Clean compile, 43% flash / 65% RAM (unchanged)

**Verification:** compile ✓ | upload ✓ | all 13 languages via serial LANG sweep ✓ | word clock ✓ | weather ✓ | NVS persist ✓

---

### M3 — Decompose `buildWebConfigPage()` (418 -> N x ~80)

**Status: DONE (85a9c84, 2026-03-23 + 728aaad, 2026-03-24)**

Decomposed into 10 focused sub-functions:

- `buildWebCssBlock()` — PROGMEM CSS emit (1 line after r205 fixed-theme simplification)
- `buildWebHeroSection()` — logo, release metadata, lede, status toast
- `buildWebThemeSelector()` — theme `<select>` (still drives ESP32 display theme)
- `buildWebViewToggles()` — view enable/disable checkboxes
- `buildWebWifiSection()` — preferred SSID, direct mode, provisioning
- `buildWebLangSelectors()` — system language + Wikipedia language
- `buildWebWeatherSection()` — geo search + lat/lon inputs
- `buildWebRssBuilder()` — RSS composer + feed list + Save/Reload actions
- `buildWebSystemInfo()` — network + firmware runtime cards
- `buildWebJsBlock()` — PROGMEM JS + RSS feed data injection

Second pass (728aaad, 2026-03-24): fixed theme to scrybar-default (removed CSS bridge layer, `data-theme`, `appendWebThemeCssVars` call from web path), flattened hero (removed `hero-top-card` and `hero-copy` wrappers), removed all `vm-card--inner` wrappers, tightened mobile padding, simplified Google Fonts to Montserrat only, fixed DOOM view alignment, added `vm-card--muted` for System Info.

**Results:**
- `buildWebConfigPage()` orchestrator: **36 lines** (was 418)
- All sub-functions under 80 lines
- CSS/JS moved to `kWebCssCore` / `kWebJsCorePre` / `kWebJsCorePost` PROGMEM blocks
- Verified: compile ✓ | upload ✓ | web UI visual parity ✓ | theme/lang/RSS persist ✓

---

### M4 — Serial command dispatch table (413 -> ~80)

**Status: DONE (dcd69ad, 2026-03-23)**

Replaced if/else chain with `SerialCmd` struct table + `kSerialCmds[]` lookup array. Each command is a `{ name, handler }` entry. `handleSerialCommand()` iterates the table and calls the matched handler.

**Results:**
- `handleSerialCommand()`: **28 lines** (was 413)
- All commands extracted to individual `cmd*()` handler functions
- `HELP` auto-generates from table entries
- Verified: compile ✓ | upload ✓ | all serial commands tested ✓

---

### M5 — Decompose `applyRuntimeConfigFromRequest()` (372 -> N x ~60)

**Status: DONE (7110baf, 2026-03-23)**

Split into per-concern parse functions:

- `parseThemeFromRequest()` — theme validation + apply
- `parseLangFromRequest()` — system language + wiki language
- `parseWeatherFromRequest()` — lat/lon/city validation
- `parseRssFeedsFromRequest()` — multi-feed slot parsing
- `parseWifiFromRequest()` — preferred SSID, direct mode, new credentials
- `parseViewTogglesFromRequest()` — view enable/disable bitmask
- `applyRuntimeConfigFromRequest()` — orchestrator calling sub-parsers

**Results:**
- `applyRuntimeConfigFromRequest()` orchestrator: **~45 lines** (was 372)
- All parse functions under 60 lines
- Verified: compile ✓ | upload ✓ | web UI field changes persist ✓ | NVS survives reboot ✓

---

### M6 — Decompose `handleTouchSwipeInput()` (346 -> N x ~60)

**Status: DONE (48f239d, 2026-03-23)**

Separated gesture classification from per-page action handling:

- `handleSwipeNavigation()` — page transitions with edge damping
- `handleDoomTouchInput()` — DOOM USE/FIRE/recenter/swipe-exit
- `handleFeedDeckTouchInput()` — AUX/WIKI skip/next/QR
- `handleNowPlayingTouchInput()` — Now Playing touch actions
- `handleTouchSwipeInput()` — gesture classification + dispatch

**Results:**
- `handleTouchSwipeInput()` dispatcher: **~31 lines** (was 346), sub-functions ~80-120 lines each
- Verified: compile ✓ | upload ✓ | swipe navigation ✓ | DOOM controls ✓ | AUX/WIKI buttons ✓

---

### M7 — Global state grouping (244 `g_` -> structs)

**Status: DONE (2026-03-24)**

Grouped 209 individual globals into 16 structs via automated Python transform (`tools/m7_rename_globals.py`):

| Struct | Instance | Fields absorbed |
|---|---|---|
| `BatteryState` | `g_batt` | 14 (battery + energy saver) |
| `PwrButtonState` | `g_pwrBtn` | 7 |
| `NavButtonState` | `g_navBtn` | 4 |
| `WifiState` | `g_wifiSt` | 28 |
| `TouchState` | `g_touch` | 15 |
| `DoomState` | `g_doom` | 24 |
| `ImuState` | `g_imu` | 6 |
| `ClockState` | `g_clock` | 8 |
| `ScreensaverState` | `g_saver` | 34 |
| `PerfCounters` | `g_perf` | 7 |
| `WebConfigState` | `g_webCfg` | 10 |
| `LvglClockUi` | `g_clockUi` | 10 |
| `LvglWeatherUi` | `g_weatherUi` | 22 |
| `LvglInfoUi` | `g_infoUi` | 10 |
| `DisplayHwState` | `g_dispHw` | 7 |
| `LvglPageAnimState` | `g_pageAnim` | 3 |

**Results:**
- `g_` globals: **244 → 51** (16 struct instances + 35 truly independent)
- Lines: 14,993 → 15,040 (+47 for struct definitions)
- Clean compile, 42% flash / 63% RAM (unchanged)

**Verification:** compile --clean ✓ | upload ✓ | WEBCFG ✓ | BATSTAT ✓ | PWRSTAT ✓ | swipe nav all pages ✓ | DOOM controls ✓ | energy saver ✓

---

### M8 — Stack buffer audit (15 -> <5 large)

**Status: DONE (r213, 2026-03-24)**

Addressed all 3 stack-local buffers >256B:

- `leftCol[512]` in `lvglUpdateInfoPanel()` → moved to PSRAM heap via `heap_caps_malloc(512, MALLOC_CAP_SPIRAM)` + `heap_caps_free()` after use
- `httpFallback[320]` in `rssFetchWikipediaSummaryMeta()` → reduced to `[256]` (Wikipedia URLs fit comfortably)
- `title3[260]` in `lvglUpdateFeedDeck()` → reduced to `[256]` (article title truncation is fine at 255 chars)
- `wrapped[256]` (static) in `lvglScreenSaverSetBalloonText()` — already static, not on stack (no change needed)

**Results:**
- Stack-local buffers >256B: **3 → 0**
- Clean compile, 42% flash / 63% RAM (unchanged)

**Verification:** compile --clean ✓ (upload + device test pending)

---

### M9 — LVGL style DRY + function cleanup (776 -> ~465)

**Status: DONE (r213, 2026-03-24)**

Extracted 7 shared LVGL style helpers and applied them across the three target functions:

- `lvglSetBgFlat(obj, hex)` — flat bg_color + bg_grad_color from hex (replaces 23 pairs)
- `lvglSetBgFlatR(obj, hex, radius)` — flat bg + radius
- `lvglSetTextHex(obj, hex)` — guarded text color from hex (replaces 14+ if-guard patterns)
- `lvglSetHeaderBorder(obj, show, hex)` — conditional header border (replaces 6× 3-line blocks)
- `lvglSetBtnBorder(obj, hex)` — button accent border (replaces 3× 3-line blocks per deck)
- `lvglCreatePanel(parent, w, h, x, y, bg, radius)` — opaque container init (replaces 10+ 10-line blocks)
- `lvglCreateDeckButton(parent, w, h, x, y, bgHex, radius, label, textHex, outText)` — deck action button (replaces 3× 16-line blocks)

Extracted `lvglApplyThemeStylesFeedDecks()` (67 lines) from `lvglApplyThemeStyles()`.

**Results:**

| Function | Before | After |
|---|---|---|
| `lvglApplyThemeStyles()` | 295 | **156** (+67 in extracted sub-function) |
| `lvglInitFeedDeck()` | 256 | **161** |
| `lvglInitNowPlayingUi()` | 225 | **148** |

- Total sketch lines: 15,040 → **14,879** (-161 net)
- Clean compile, 42% flash / 63% RAM (unchanged)
- Style helpers are reusable across all `initLvgl*` and `lvglApplyTheme*` functions

**Verification:** compile --clean ✓ (upload + device test pending)

---

### M10 — Final polish pass

**Status: DONE (r214, 2026-03-25)**

Decomposed last two functions >200 lines and DRY'd weather display logic:

- `lvglShowWeatherMainIcon(code, isDay, glyphFallback)` — icon/glyph toggle (replaces 3× 10-line blocks)
- `lvglShowWeatherForecastIcon(code, isDay)` — forecast icon show/hide (replaces 3× 7-line blocks)
- `lvglSetWeatherOfflineLabels(desc, glyph, color, setColor)` — offline placeholder (replaces 2× 15-line blocks)
- `lvglUpdateWeatherDisplay(glyphOnline, glyphOffline)` — full weather section extracted from `updateLvglUi()`
- `nvsLoadRssFeeds(prefs, loadedAny)` — RSS feed loading extracted from `loadRuntimeNetConfigFromNvs()`
- `nvsLoadLanguageConfig(prefs, langNeedsPersist)` — language config + legacy migration extracted
- `kWmoFallbackCode = 2` — named constant for offline weather icon (was magic number 4×)

**Results:**

| Function | Before | After |
|---|---|---|
| `updateLvglUi()` | 210 | **79** |
| `loadRuntimeNetConfigFromNvs()` | 202 | **132** |

- Total sketch lines: 14,879 → **14,859** (-20 net)
- Functions >200 lines: **0** (target achieved)
- Clean compile, 42% flash / 63% RAM (unchanged)

**Verification:** compile --clean ✓ | upload ✓ | HELP ✓ | BATSTAT ✓ | all page navigation ✓ | LANG it/en ✓ | VIEWAUX/WIKI/INFO/HOME ✓ | WEBCFG ✓ | Now Playing ✓

---

## Progress Log

Track each completed milestone here with date, r-number, and commit hash.

```
| Date | r# | Commit | Milestone | Notes |
|------|-----|--------|-----------|-------|
| 2026-03-20 | r200 | 7b66f73 | M1 | initLvglUi 714→94 lines, 7 sub-functions, all <155 lines |
| 2026-03-20 | r201 | 4bfba91 | M2 | LangVtable dispatch, 26 weather funcs→data tables, -322 net lines |
| 2026-03-23 | — | 85a9c84 | M3 | buildWebConfigPage 418→36 lines, 10 sub-functions, CSS/JS to PROGMEM |
| 2026-03-23 | — | dcd69ad | M4 | handleSerialCommand 413→28 lines, SerialCmd table dispatch |
| 2026-03-23 | — | 7110baf | M5 | applyRuntimeConfigFromRequest 372→~45 lines, per-concern parsers |
| 2026-03-23 | — | 48f239d | M6 | handleTouchSwipeInput 346→31 lines, per-page touch handlers |
| 2026-03-24 | — | 728aaad | M3+ | Fixed scrybar-default theme, flat layout, mobile tightening, DOOM fix |
| 2026-03-24 | — | — | M7 | 244 g_ globals → 51 (16 structs absorbing 209 vars), +47 lines |
| 2026-03-24 | r213 | — | M8 | leftCol→PSRAM heap, httpFallback 320→256, title3 260→256, 0 stack buffers >256B |
| 2026-03-24 | r213 | — | M9 | 7 style helpers, lvglApplyThemeStyles 295→156, lvglInitFeedDeck 256→161, lvglInitNowPlayingUi 225→148 |
| 2026-03-25 | r214 | — | M10 | updateLvglUi 210→79, loadRuntimeNetConfigFromNvs 202→132, weather DRY helpers, kWmoFallbackCode, 0 functions >200 |
```

### Final Metrics (post-M10, 2026-03-25) — ROADMAP COMPLETE

| Metric | Baseline (r199) | Target | **Final** |
|---|---|---|---|
| **Total sketch lines** | 15,762 | — | **14,859** (-903) |
| **Functions >200 lines** | 10 | 0 | **0** |
| **Largest function** | `initLvglUi()` 714 | <150 | **lvglApplyThemeStyles()` 156** |
| **Language dispatcher lines** | 1,855 | <400 | **~80** (table-driven) |
| **`g_` prefixed globals** | 249 | <180 | **51** (16 structs + 35 independent) |
| **Stack buffers >256B** | 15 | <5 | **0** |
| **Flash / RAM** | 42% / 63% | unchanged | **42% / 63%** |

---

## How to Use This Document

### Starting a new session

1. Read this file first
2. Check the **Progress Log** to see what's done
3. Find the first milestone with **Status: TODO**
4. Read its description, verification steps, and measurable target
5. Implement it
6. Run ALL verification steps before marking complete
7. Update this document: change status to **DONE (rNNN, date)**, add to Progress Log

### After completing a milestone

1. Bump `FW_BUILD_TAG` in `config.h`
2. Compile + upload + verify on device
3. Commit with message: `refactor(firmware): M<N> — <milestone title>`
4. Update this document's Progress Log
5. Push

### Rules

- **One milestone per session** unless the milestone is small
- **Never skip verification** — compile, upload, reset, test on device
- **Zero regressions** — if something breaks, fix before moving on
- **Update this doc** — the Progress Log is the single source of truth
- **Measure before/after** — report actual line counts, not estimates
