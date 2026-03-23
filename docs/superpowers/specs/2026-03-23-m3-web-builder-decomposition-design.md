# M3 — Decompose `buildWebConfigPage()` Design Spec

**Date:** 2026-03-23
**Milestone:** M3 (Firmware Polishing Roadmap)
**Scope:** `scrybar.ino` — `buildWebConfigPage()` (lines 3957–4374, 418 lines)

---

## Problem

`buildWebConfigPage()` is a 418-line monolithic function that generates the entire web config UI inline. It mixes CSS emission, HTML structure, form controls, system info cards, and JavaScript into a single function body with 244 `html +=` concatenations and 224 `F()` macro calls. This makes it difficult to navigate, modify individual sections, or reason about the output structure.

## Approach

Two complementary changes:

1. **Function decomposition** — identical pattern to M1 (`initLvglUi()`). The orchestrator becomes a ~35-40 line sequence of calls to focused sub-functions. Each sub-function receives `String &html` by reference and appends its block directly.

2. **PROGMEM consolidation** — static CSS and JS blocks (which have zero dynamic interpolation between adjacent `F()` calls) are consolidated into `static const char PROGMEM` raw string literals and appended via single `html += FPSTR(...)` calls. This fulfills the roadmap's "Move repeated CSS strings to `const char PROGMEM`" requirement and dramatically reduces `+=` count.

## Architecture

The forms section is split into per-section functions upfront (not as an escape clause), because actual line counts show the combined section is ~170 lines — well over the 100-line limit.

```
buildWebConfigPage()              ~35 lines (orchestrator)
├── buildWebCssBlock()            ~15 lines (PROGMEM append + theme vars call)
├── buildWebHeroSection()         ~25 lines (logo, release, branding, status)
├── buildWebThemeSelector()       ~15 lines (theme <select>)
├── buildWebViewToggles()         ~30 lines (view checkboxes + hidden inputs)
├── buildWebWifiSection()         ~55 lines (#if TEST_WIFI, SSID pref + provisioning)
├── buildWebLangSelectors()       ~50 lines (system lang optgroups + wiki lang)
├── buildWebWeatherSection()      ~10 lines (geo search + lat/lon inputs)
├── buildWebRssBuilder()          ~10 lines (RSS composer + feed list container)
├── buildWebSystemInfo()          ~70 lines (network + firmware cards, #if guards)
└── buildWebJsBlock()             ~15 lines (PROGMEM append + RSS feed data injection)
```

### Self-contained sub-functions

Following the M1 pattern, each sub-function computes its own locals from runtime accessors and globals. The orchestrator does **not** pre-compute values and pass them down. For example:

- `buildWebViewToggles()` reads `g_runtimeNetConfig.enabledViewsMask` directly
- `buildWebSystemInfo()` calls `WiFi.localIP()`, `ESP.getFreeHeap()`, etc. directly
- `buildWebLangSelectors()` reads `g_wordClockLang` and `g_wikiLang` directly

This eliminates parameter threading and keeps functions independently testable.

### Orchestrator: `buildWebConfigPage(const char *statusMsg)`

~35 lines. Responsibilities:
- `ensureRuntimeNetConfig()`
- `html.reserve(22000)` — unchanged allocation budget
- DOCTYPE + `<head>` open + meta tags + fonts preconnect
- `<style>` open → `buildWebCssBlock()` → `</style></head>`
- `<body>` open + `<main>` + `<form>` open
- Call each section function in order
- Form close + footer + `buildWebJsBlock()` + close tags
- Return `html`

### `buildWebCssBlock(String &html, const UiThemeDefinition &theme)`

~15 lines. The ~85 static CSS `F()` calls (lines 3992-4076) are consolidated into a single `static const char kWebCssCore[] PROGMEM = R"rawliteral(...)rawliteral";` constant. The function:
1. Calls `appendWebThemeCssVars(html, theme.web)` for dynamic theme vars
2. Appends `FPSTR(kWebCssCore)` — one operation for the entire static CSS block

This single change eliminates ~83 `+=` calls.

### `buildWebHeroSection(String &html, const char *statusMsg)`

~25 lines. Dynamic insertions: logo URL, release date, build tag, status message. Static HTML between insertions consolidated where possible.

### `buildWebThemeSelector(String &html)`

~15 lines. Reads `runtimeUiThemeId()` for current selection. Loops `kUiThemes[0..UI_THEME_COUNT]`.

### `buildWebViewToggles(String &html)`

~30 lines. Reads `g_runtimeNetConfig.enabledViewsMask` directly. Emits checkbox for each view with `UI_VIEW_FLAG_*` checks. Contains `#if TEST_DISPLAY && DOOM_SPIKE_ENABLED` guard for DOOM availability.

### `buildWebWifiSection(String &html)`

~55 lines. Entire function body wrapped in `#if TEST_WIFI ... #endif`. Reads WiFi state (`WiFi.status()`, `g_wifiConnected`, `g_wifiPreferredSsid`, `g_wifiCredSsids[]`, etc.) directly. Contains SSID preference dropdown, setup mode selector, network provisioning form, and conditional setup AP info.

### `buildWebLangSelectors(String &html)`

~50 lines. Two sections:
1. System language — local `kLangsFun[]` + `kLangsStd[]` arrays, reads `g_wordClockLang`
2. Wikipedia language — local `kWikiLangs[]` array, reads `g_wikiLang`

### `buildWebWeatherSection(String &html)`

~10 lines. Reads `runtimeWeatherLat()`, `runtimeWeatherLon()`, `runtimeWeatherCityLabel()`. Computes `latBuf`/`lonBuf` locally via `snprintf`.

### `buildWebRssBuilder(String &html)`

~10 lines. Reads `runtimeRssConfiguredFeedCount()`. Emits RSS composer form, feed list container, empty state, and action buttons (Save/Reload).

### `buildWebSystemInfo(String &html)`

~70 lines. Contains its own `#if` guards:
- `#if TEST_WIFI` — network info block (ip, ssid, rssi, mac, dns, preferred, direct mode, setup ap)
- `#if TEST_NTP` — NTP sync status
- `#if TEST_BATTERY` — battery percentage + charging state

Uses local `siBuf[48]` for `snprintf` formatting. Also emits the API note and footer.

### `buildWebJsBlock(String &html)`

~15 lines. The static JavaScript (~35 lines of minified JS at lines 4336-4371) is consolidated into a `static const char kWebJsCore[] PROGMEM` raw string literal, **split at the RSS feed data injection point**:

```cpp
static const char kWebJsCorePre[] PROGMEM = R"rawliteral(
<script>(function(){
... geo autocomplete, wifi scan, RSS management ...
const initialFeeds=[)rawliteral";

static const char kWebJsCorePost[] PROGMEM = R"rawliteral(];
... feed filtering, render, hidden inputs, event listeners ...
})();</script>)rawliteral";
```

The function:
1. Appends `FPSTR(kWebJsCorePre)`
2. Loops `RSS_FEED_SLOT_COUNT` to emit feed JSON objects (calls `runtimeRssFeedBySlot()`, `appendJsonEscaped()`, `clampRssFeedMaxItems()`)
3. Appends `FPSTR(kWebJsCorePost)`

This eliminates ~33 `+=` calls while preserving runtime data injection.

## PROGMEM Constants Summary

| Constant | Content | Approx Size |
|----------|---------|-------------|
| `kWebCssCore` | All `vm-*` CSS classes + bridge + responsive | ~5.5 KB |
| `kWebJsCorePre` | JS before `initialFeeds` array | ~4.5 KB |
| `kWebJsCorePost` | JS after `initialFeeds` array | ~3.5 KB |

Total: ~13.5 KB in flash PROGMEM. These were already in flash via `F()` — no change in memory class, just consolidation.

## `+=` Count Analysis

| Section | Before | After | Reduction |
|---------|--------|-------|-----------|
| CSS block | ~85 | ~2 (theme vars + FPSTR) | -83 |
| JS block | ~35 | ~3 (pre + feed loop + post) | -32 |
| Hero | ~15 | ~10 (dynamic insertions remain) | -5 |
| Form sections | ~80 | ~70 (some adjacent F() merges) | -10 |
| System info | ~30 | ~25 | -5 |
| **Total** | **244** | **~110** | **-134** |

The roadmap target of <50 requires a full template engine approach (beyond M3 scope). Updating roadmap to reflect realistic target of <120 after function split + PROGMEM consolidation.

## Conditional Compilation Placement

Each sub-function owns its own `#if` guards:

| Guard | Function |
|-------|----------|
| `#if TEST_DISPLAY && DOOM_SPIKE_ENABLED` | `buildWebViewToggles()` (DOOM checkbox) |
| `#if TEST_WIFI` | `buildWebWifiSection()` (entire function body) |
| `#if TEST_WIFI` | `buildWebSystemInfo()` (network info block) |
| `#if TEST_NTP` | `buildWebSystemInfo()` (NTP status line) |
| `#if TEST_BATTERY` | `buildWebSystemInfo()` (battery status line) |

## Existing Helpers — Unchanged

These functions remain at their current locations, unmodified:

- `appendWebThemeCssVars(String &html, const UiThemeWebTokens &tokens)` — line 3895
- `appendHtmlEscaped(String &out, const char *text)` — line 3869
- `appendJsonEscaped(String &out, const char *text)` — line 3882
- `runtimeLogoUrl()` — line 3814
- `statusMessageFromCode()` — called by `handleWebConfigRoot()`
- `runtimeRssFeedBySlot()`, `clampRssFeedMaxItems()` — called by `buildWebJsBlock()`
- `runtimeWeatherLat()`, `runtimeWeatherLon()`, `runtimeWeatherCityLabel()` — called by `buildWebWeatherSection()`
- `runtimeUiThemeId()`, `runtimeUiThemeLabel()` — called by `buildWebThemeSelector()` / `buildWebSystemInfo()`

## Invariants

1. **Byte-identical HTML output** — the browser receives exactly the same bytes before and after
2. **Same `html.reserve(22000)`** — no allocation change
3. **No function >100 lines** — enforced target
4. **`handleWebConfigRoot()` unchanged** — still calls `buildWebConfigPage()`, still returns the String
5. **All runtime data paths preserved** — theme, lang, RSS, weather, WiFi, views mask
6. **All `#if` conditionals preserved** — each sub-function contains its own guards as documented above
7. **No new files** — all functions and PROGMEM constants stay in `scrybar.ino`
8. **PROGMEM constants are same memory class as `F()`** — no RAM impact change

## Measurable Targets

| Metric | Before | After |
|--------|--------|-------|
| `buildWebConfigPage()` lines | 418 | ~35 |
| Largest new function | — | <70 lines (`buildWebSystemInfo`) |
| Functions >100 lines created | — | 0 |
| `html +=` count (web builder total) | 244 | ~110 |
| HTML output | X bytes | X bytes (identical) |

## Verification Steps

1. `arduino-cli compile --clean` — clean compile, no warnings from refactored code
2. Upload to device, hard reset
3. Open web UI in browser — **pixel-perfect visual parity** with pre-refactor
4. Change theme via web UI — persists after page reload
5. Change language (system + wiki) — persists after reload
6. Add/remove RSS feeds — persists after reload
7. Change weather location — persists after reload
8. Test in AP mode (no internet) — page loads with system fonts, all controls functional
9. Views toggle — enable/disable views, verify on device

## Roadmap Update

Update `firmware_polishing_roadmap.md` M3 section:
- Change `+=` target from `<50` to `<120` (realistic with PROGMEM consolidation; <50 requires template engine, out of scope)
- Add PROGMEM consolidation as explicit deliverable
- Reflect 10-function split (not 5)

## Build Tag

Bump `FW_BUILD_TAG` to `r202` and `FW_RELEASE_DATE` to `2026-03-23` after successful verification.

## Commit

```
refactor(firmware): M3 — decompose buildWebConfigPage (418 → ~35 lines)
```
