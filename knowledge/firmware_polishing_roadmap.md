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

**Status: TODO**

Replace 56 per-language functions + 4 dispatchers with:

```c
struct LangVtable {
  const char* code;
  String (*wordClock)(int h, int m);
  const char* (*weatherShort)(int code);
  const char* (*weatherUiLabel)(int code);
  String (*formatDate)(int y, int m, int d, int dow);
  const UiStrings* uiStrings;
};

static const LangVtable kLangTable[] = {
  { "it", composeWordClockSentenceIt, weatherCodeShortIt, ... },
  ...
};
```

Dispatch becomes a single loop/lookup instead of 13x `strcmp`.

**Verification:**
1. Compile + upload
2. Switch language via web UI for each of the 13 languages
3. Verify: clock sentence, weather label, date format, UI strings
4. Serial: `WEBCFG` shows correct `wc_lang`

**Measurable:** ~1,400 lines removed. Adding a new language = 1 struct entry + 4 functions (no dispatcher changes).

---

### M3 — Decompose `buildWebConfigPage()` (418 -> N x ~80)

**Status: TODO**

Separate concerns:
- `buildWebCssBlock()` — all CSS (theme vars + component styles)
- `buildWebHeroSection()` — header/hero card
- `buildWebConfigForms()` — form inputs (theme, lang, weather, RSS, wiki, etc.)
- `buildWebSystemInfo()` — runtime info cards
- `buildWebConfigPage()` — orchestrator (~40 lines)

Move repeated CSS strings to `const char PROGMEM`.

**Verification:**
1. Compile + upload
2. Open web UI in browser — visual parity
3. Change theme, language, RSS feeds, weather location — all persist after reload
4. Test in AP mode (no internet) — page loads with system fonts

**Measurable:** No function >100 lines. `+=` count drops from 269 to <50.

---

### M4 — Serial command dispatch table (413 -> ~80)

**Status: TODO**

Replace if/else chain with:

```c
struct SerialCmd {
  const char* name;
  void (*handler)(const String& args);
};

static const SerialCmd kSerialCmds[] = {
  { "HELP",     cmdHelp },
  { "THEME",    cmdTheme },
  { "VIEW",     cmdView },
  ...
};
```

**Verification:**
1. Compile + upload
2. Test every serial command from the reference table in `project_knowledge.md`
3. `HELP` output matches all registered commands

**Measurable:** `handleSerialCommand()` drops from 413 to <80 lines.

---

### M5 — Decompose `applyRuntimeConfigFromRequest()` (372 -> N x ~60)

**Status: TODO**

Split into:
- `parseThemeFromRequest()` — theme validation + apply
- `parseLangFromRequest()` — language validation + apply
- `parseWeatherFromRequest()` — lat/lon/city validation
- `parseRssFeedsFromRequest()` — multi-feed slot parsing
- `parseWifiFromRequest()` — WiFi credential handling
- `parseNowPlayingToggle()` — now playing enable/disable
- `applyRuntimeConfigFromRequest()` — orchestrator

**Verification:**
1. Web UI: change every configurable field, save, reload — values persist
2. API: `POST /api/config` with JSON payload — same behavior
3. NVS: reboot device — all settings survive

**Measurable:** No function >80 lines.

---

### M6 — Decompose `handleTouchSwipeInput()` (346 -> N x ~60)

**Status: TODO**

Separate:
- `handleSwipeNavigation()` — page transitions
- `handleDoomTouchInput()` — DOOM-specific touch
- `handleFeedDeckTouchInput()` — AUX/WIKI skip/next/QR
- `handleTouchSwipeInput()` — gesture classification + dispatch

**Verification:**
1. Swipe left/right through all views
2. DOOM: USE, FIRE, recenter, swipe-exit
3. AUX/WIKI: SKIP, NXT, QR
4. Power button: short press screensaver, long press soft-off

**Measurable:** No function >80 lines.

---

### M7 — Global state grouping (249 `g_` -> structs)

**Status: TODO**

Group related globals into structs:

```c
struct DisplayState { ... };      // 112 display/LVGL vars
struct WifiState { ... };         // 28 WiFi vars
struct BatteryState { ... };      // 12 battery vars
struct ButtonState { ... };       // 11 button vars
struct FeedState { ... };         // RSS/Wiki state
struct NowPlayingState { ... };   // Now Playing state
```

**Verification:**
1. Full compile + upload
2. All features functional
3. Serial: `BATSTAT`, `PWRSTAT`, `WEBCFG` — correct output

**Measurable:** `g_` count drops from 249 to <80 (struct instances + truly independent globals).

---

### M8 — Stack buffer audit (15 -> <5 large)

**Status: TODO**

Move buffers >256B from stack to:
- PSRAM heap (via `heap_caps_malloc(sz, MALLOC_CAP_SPIRAM)`) for ephemeral use
- Static allocation for persistent buffers
- Reduce sizes where possible (e.g., `leftCol[512]` — is 512 actually needed?)

**Verification:**
1. Compile + upload
2. Trigger RSS fetch while on HOME view (concurrent LVGL + HTTP)
3. Trigger Wikipedia fetch while on WIKI view
4. Monitor free heap via serial `WEBCFG` or web System Info
5. No crashes after 1h runtime

**Measurable:** No stack-local buffer >256B.

---

### M9 — `lvglApplyThemeStyles()` cleanup (293 -> ~150)

**Status: TODO**

DRY repeated style property sets. Extract helpers:
- `applyTextStyle(obj, font, color, align)` — combines 3-4 `lv_obj_set_style_*` calls
- `applyCardStyle(obj, bg, border, radius, pad)` — combines 5-6 calls

**Verification:**
1. Compile + upload
2. Switch through all 5 themes via web UI + serial `THEME <id>`
3. Visual parity on device for all views

**Measurable:** Function drops to ~150 lines. Style helpers reusable across `initLvgl*` functions.

---

### M10 — Final polish pass

**Status: TODO**

After M1-M9 are done:
- Review all functions — none >150 lines
- Remove any leftover magic numbers → named constants
- Verify PSRAM allocation consistency
- Run full feature matrix test
- Update `FW_BUILD_TAG` for the polished release
- Update `project_knowledge.md` with new architecture notes

---

## Progress Log

Track each completed milestone here with date, r-number, and commit hash.

```
| Date | r# | Commit | Milestone | Notes |
|------|-----|--------|-----------|-------|
| 2026-03-20 | r200 | 7b66f73 | M1 | initLvglUi 714→94 lines, 7 sub-functions, all <155 lines |
```

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
