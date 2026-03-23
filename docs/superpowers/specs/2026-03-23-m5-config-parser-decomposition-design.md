# M5 — Decompose `applyRuntimeConfigFromRequest()` Design Spec

**Date:** 2026-03-23
**Milestone:** M5 (Firmware Polishing Roadmap)
**Scope:** `scrybar.ino` — `applyRuntimeConfigFromRequest()` (lines 4506–4877, 371 lines)

---

## Problem

`applyRuntimeConfigFromRequest()` is a 371-line monolithic function that handles parsing, validation, commit, and side-effect application for every configurable field in the web UI and API. It mixes 10 unrelated config domains (weather, RSS, logo, theme, views, WiFi setup mode, WiFi provisioning, WiFi preferred SSID, word clock language, wiki language), each with its own validation rules and error paths. Adding a new config field requires understanding the full 371-line flow.

4 callers (lines 5069, 5078, 5112, 5130) all use `static bool applyRuntimeConfigFromRequest(String &errorOut)`.

## Approach

**Three-phase decomposition:**
1. **Parse handlers** — 10 self-contained functions, one per config domain
2. **Commit phase** — diff detection + NVS save
3. **Side-effect phase** — cache invalidation, WiFi reconnect, live reload

Each parse handler follows a uniform contract:
- Reads `g_webConfigServer` args directly (self-contained)
- Writes validated values into `RuntimeNetConfig &next` or standalone globals
- Sets `bool &hasInput` to true if any arg was present
- Returns false + `errorOut` on validation failure

## Architecture

```
applyRuntimeConfigFromRequest()         ~45 lines (orchestrator)
├── Phase 1 — Parse (early-return on error)
│   ├── parseWeatherConfig()            ~25 lines
│   ├── parseRssFeedConfig()            ~55 lines
│   ├── parseLogoConfig()               ~12 lines
│   ├── parseThemeConfig()              ~16 lines
│   ├── parseViewsConfig()              ~28 lines
│   ├── parseWifiSetupModeConfig()      ~18 lines
│   ├── parseWifiCredentialConfig()     ~35 lines
│   ├── parseWifiPreferredConfig()      ~28 lines
│   ├── parseLangConfig()               ~18 lines
│   └── parseWikiLangConfig()           ~16 lines
├── Phase 2 — Commit
│   └── commitConfigToNvs()             ~35 lines
└── Phase 3 — Side effects
    └── applyConfigSideEffects()        ~65 lines
```

### Diff Flags Flow

A stack-local struct carries diff results from commit to side effects:

```c
struct ConfigDiffResult {
  bool weatherChanged;
  bool rssChanged;
  bool brandingChanged;
  bool themeChanged;
  bool viewsChanged;
};
```

Parse-time flags (`langChanged`, `wikiLangChanged`, `wifiPrefChanged`, `wifiSetupModeChanged`, `wifiProvisioned`, `wifiPrefIdx`) remain as local bools in the orchestrator, passed to side effects.

### Orchestrator: `applyRuntimeConfigFromRequest(String &errorOut)`

~45 lines. Signature unchanged — 4 callers unmodified.

1. `ensureRuntimeNetConfig()`
2. `RuntimeNetConfig next = g_runtimeNetConfig;` — working copy
3. Declare flags
4. Call 10 parse handlers in sequence, early return on false
5. `!hasInput` guard → error
6. `commitConfigToNvs(next, g_runtimeNetConfig)` → `ConfigDiffResult`
7. `applyConfigSideEffects(diff, langChanged, wikiLangChanged, wifiPrefChanged, wifiSetupModeChanged, wifiPrefIdx)`
8. `return true;`

### Parse Handler Contract

```c
static bool parseWeatherConfig(RuntimeNetConfig &next, String &errorOut, bool &hasInput) {
  if (g_webConfigServer.hasArg("weather_city")) {
    hasInput = true;
    // validate + copy to next
  }
  return true;  // no error
}
```

### Parse Handler Details

| Handler | Args parsed | Target | Lines | `#if` guard |
|---------|-----------|--------|-------|-------------|
| `parseWeatherConfig` | weather_city, weather_lat, weather_lon | `next` struct | ~25 | — |
| `parseRssFeedConfig` | rss_feed_url/name/items + rss_feed_url_N/name_N/items_N | `next` struct | ~55 | — |
| `parseLogoConfig` | logo_url | `next` struct | ~12 | — |
| `parseThemeConfig` | ui_theme | `next` struct | ~16 | — |
| `parseViewsConfig` | view_info/aux/wiki/now_playing/doom | `next` struct | ~28 | — |
| `parseWifiSetupModeConfig` | wifi_setup_mode | `g_wifiSetupMode` global | ~18 | — |
| `parseWifiCredentialConfig` | wifi_new_ssid, wifi_new_password | globals via `upsertRuntimeWiFiCredential()` | ~35 | — |
| `parseWifiPreferredConfig` | wifi_pref_ssid | `g_wifiPreferredSsid` global | ~28 | — |
| `parseLangConfig` | wc_lang | `g_wordClockLang` global | ~18 | — |
| `parseWikiLangConfig` | wiki_lang | `g_wikiLang` global | ~16 | — |

**Note:** WiFi/lang handlers write to standalone globals (not `next` struct) — matches current code exactly.

### `commitConfigToNvs(RuntimeNetConfig &next, const RuntimeNetConfig &prev)` → `ConfigDiffResult`

~35 lines:
1. `normalizeRuntimeUiTheme(next)`
2. Diff detection: `weatherChanged` (city strcmp + lat/lon fabsf 0.00005), `rssChanged` (loop `runtimeRssFeedEntriesEqual`), `brandingChanged` (logo strcmp), `themeChanged` (uiTheme strcmp), `viewsChanged` (bitmask)
3. `g_runtimeNetConfig = next;` — commit
4. `normalizeRuntimeViewMask()` + `syncActiveUiThemeFromRuntimeConfig()`
5. `g_runtimeNetConfig.ready = true;`
6. `saveRuntimeNetConfigToNvs()` + warning on failure

### `applyConfigSideEffects(...)`

~65 lines. Order preserved from current code:
1. Weather cache invalidation → `g_weather.valid = false`
2. RSS cache invalidation → zero `g_rss` state
3. Wiki cache invalidation → zero `g_wiki` state
4. Theme live-apply → `#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI`
5. UI redraw flag → `#if TEST_NTP`
6. WiFi reconnect → schedule + disconnect if needed
7. WiFi setup AP → normalize + start/stop
8. Live data reload → `updateWeatherFromApi()`, `updateRssFromFeed()`, `updateWikiFromFeed()`
9. View page correction → fallback to enabled view

## Conditional Compilation

| Guard | Function |
|-------|----------|
| `#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI` | `applyConfigSideEffects()` (theme live-apply) |
| `#if TEST_NTP` | `applyConfigSideEffects()` (UI redraw flag) |

All 10 parse handlers have **no `#if` guards**.

## Callers — Unchanged

| Caller | Line | Context |
|--------|------|---------|
| `handleWebConfigApplyApi()` | 5069 | POST `/api/config` |
| `handleWebConfigApplyForm()` | 5078 | POST form submit |
| `handleWebReloadForm()` | 5112 | POST reload form |
| `handleWebReloadApi()` | 5130 | POST `/api/reload` |

## Invariants

1. **Identical behavior for all 4 callers** — same boolean return, same `errorOut`, same side effects
2. **Same validation order** — weather → RSS → logo → theme → views → wifi setup → wifi cred → wifi pref → lang → wiki lang
3. **Same error messages** — identical Italian-language strings
4. **WiFi provisioning priority preserved** — `parseWifiPreferredConfig()` skipped when `wifiProvisioned`
5. **All `#if` guards preserved**
6. **No function >80 lines**
7. **No new files** — all in `scrybar.ino`
8. **Side-effect ordering preserved** — cache invalidation before live reload

## Measurable Targets

| Metric | Before | After |
|--------|--------|-------|
| `applyRuntimeConfigFromRequest()` lines | 371 | ~45 |
| Largest new function | — | ~65 lines (`applyConfigSideEffects`) |
| Functions >80 lines created | — | 0 |
| Config domains per function | 10 in 1 | 1 per function |

## Verification Steps

1. `arduino-cli compile --clean` — zero warnings
2. Upload to device, hard reset
3. Web UI: change every field, save — values persist after reload
4. API: `POST /api/config` — same behavior
5. WiFi provisioning: add new SSID+password — device connects
6. NVS: reboot — all settings survive
7. Error handling: invalid lat/theme/lang → 400 with correct error
8. Reload: verify weather/RSS/wiki fetch triggered
9. View toggle: disable view, verify page switches

## Build Tag

Bump `FW_BUILD_TAG` to `DB-M5-r204` and `FW_RELEASE_DATE` to `2026-03-23`.

## Commit

```
refactor(firmware): M5 — decompose applyRuntimeConfigFromRequest (371 → ~45 lines)
```
