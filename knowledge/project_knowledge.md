# ScryBar Project Knowledge

Stable technical context for all AI assistants.

## Repository Layout

- `scrybar.ino`: main firmware sketch in repository root.
- `config.h`: runtime feature toggles, hardware profile constants, firmware build tag/date.
- `src/`: firmware support modules and generated assets (including LVGL fonts in `src/fonts/`).
- `vendor/`: parked third-party modules not in active compile path (for reference/reuse).
- `assets/`: static resources (logos, design system, README previews, source TTFs).
- `assets/scrybar_design_system/`: standalone HTML/CSS design system with runtime theme selector.
- `tools/`: operational scripts (notably screenshot capture via serial framebuffer dump).
- `archive/ansi/`: archived ANSI art viewer (removed in r183 — low replay value; see `archive/ansi/README.md` for restoration).
- `knowledge/`: shared, sanitized cross-assistant knowledge (public, versioned).
- `memory/`: local assistant memory (project-local, not canonical public documentation).

## Core Stack

- Firmware: Arduino sketch (`scrybar.ino`)
- MCU: ESP32-S3
- Display/touch target: Waveshare ESP32-S3-Touch-LCD-3.49
- UI: LVGL 8.x
- Local config UI: embedded HTTP server (`/`, `/config`, `/api/config`, `/api/reload`)
- Companion: macOS SwiftUI app under `companion/mac/ScryBarCompanion/`

## Canonical Board Profile

Use these compile/upload parameters as baseline:

- board: `esp32:esp32:esp32s3`
- CPU: `240MHz`
- Flash: `16MB`
- PSRAM: `OPI`
- Flash mode: `QIO`
- Partition: `custom` via checked-in `partitions.csv` (`app0=0xA00000`, `fatfs=0x5F0000`)
- Upload speed: `921600`
- USB mode: `hwcdc`, CDC on boot enabled

## Build Commands (Template)

Compile:

```bash
arduino-cli compile --clean \
  --build-path /tmp/arduino-build-scrybar \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  .
```

Upload:

```bash
arduino-cli upload -p <PORT> \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  --input-dir /tmp/arduino-build-scrybar \
  .
```

## Configuration Boundaries

- Runtime secrets must stay in local non-versioned `secrets.h`.
- `secrets.h.example` is versioned and must contain placeholders only.
- `config.h` and `knowledge/` docs must remain secret-free.

## Firmware Versioning

Two constants in `config.h` must be updated for every firmware release:

```cpp
#define FW_BUILD_TAG     "DB-M0-rNNN"
#define FW_RELEASE_DATE  "YYYY-MM-DD"
```

Both are surfaced on:

- Web control surface hero card (`version` + `last release`)
- Device INFO panel (`ScryBar Stats`, `fw:` line)

Use them to confirm a flash landed (`[FW] Build=...` at boot).

## Runtime Config Model (NVS-backed)

`RuntimeNetConfig` is the runtime/persisted state nucleus. Key user-facing fields:

- Theme id: `uiTheme` (`ui_theme` in web/API payloads)
- System language: `g_wordClockLang` (`wc_lang` in web/API payloads)
- Wikipedia language: `g_wikiLang` (`wiki_lang` in web/API payloads, independent from system language)
- Preferred Wi-Fi SSID: `g_wifiPreferredSsid` (`wifi_pref_ssid` in web/API payloads)
- Wi-Fi direct mode: `g_wifiSetupMode` (`wifi_setup_mode` in web/API payloads, values `off|auto|on`)
- Weather city/lat/lon
- RSS feed slots (multi-feed)
- Dedicated Wiki deck (fixed 3-source rotation, separate from RSS runtime slots)
- Branding logo URL

Apply paths:

- Web form POST `/config`
- API POST `/api/config`
- Serial command `THEME <id>` (theme only)

## Now Playing Companion

Current production direction:

- Firmware advertises `_scrybar._tcp` and accepts `GET/POST /api/now-playing`.
- Firmware `Now Playing` UI is tuned on real hardware and already consumes live payloads from the companion.
- Companion default provider is the macOS system now-playing feed via JXA/osascript (see below).
- Fallback providers: `TidalNowPlayingProvider` (TIDAL local cache parsing), `MusicNowPlayingProvider` (Music.app AppleScript).
- Firmware sync badge is freshness-based: keep content visible, but flip sync state when payload refresh stops.

### macOS 15.4+ MediaRemote Entitlement Restriction

**Critical:** macOS 15.4 blocks unsigned apps from using `MediaRemote.framework` C function APIs (`MRMediaRemoteGetNowPlayingInfo`, etc.). Calls return `Operation not permitted` (error code 3). The notification registration API (`MRMediaRemoteRegisterForNowPlayingNotifications`) is silently accepted but notifications never fire.

**Solution:** Use JXA (JavaScript for Automation) via `/usr/bin/osascript`, which is an entitled system binary. The JXA script accesses `MRNowPlayingRequest` (Objective-C class) through the JXA bridge:

```javascript
var Req = $.NSClassFromString("MRNowPlayingRequest");
var dict = Req.localNowPlayingItem.nowPlayingInfo;  // all metadata
var isPlaying = Req.localIsPlaying;                  // playback state
var client = Req.localNowPlayingPlayerPath.client;   // bundle ID
```

Key classes: `MRNowPlayingRequest` (static), `MRContentItem` (returned by `.localNowPlayingItem`).

### Artwork Resolution by Source

`kMRMediaRemoteNowPlayingInfoArtworkIdentifier` format varies by player app:

| Source | Format | Action |
|--------|--------|--------|
| Apple Music | Direct HTTPS URL | Use as-is |
| Apple Podcasts | Template URL (`{w}x{h}bb.{f}`) | Expand to `600x600bb.jpg` |
| TIDAL | Opaque hex hash | Fallback to iTunes Search API |
| Others | Unknown | Fallback to iTunes Search API |

iTunes Search API fallback: `https://itunes.apple.com/search?term=ARTIST+TITLE&media=music&limit=1` → `artworkUrl100` → replace `100x100` with `600x600`.

`kMRMediaRemoteNowPlayingInfoArtworkData` exists in the dictionary but is NOT extractable via JXA (binary blob type not bridged properly — `base64EncodedStringWithOptions` fails).

### Companion Build Commands

```bash
cd companion/mac/ScryBarCompanion
xcodegen generate
swift build
# or open ScryBarCompanion.xcodeproj in Xcode → Cmd+R
```

Bump `CURRENT_PROJECT_VERSION` in `project.yml` for every release.

### Companion DMG Export (CLI)

Build Release + create drag-to-install DMG without Xcode GUI:

```bash
cd companion/mac/ScryBarCompanion

# 1. Build Release (ad-hoc signed)
xcodebuild -project ScryBarCompanion.xcodeproj \
  -scheme ScryBarCompanion -configuration Release \
  -derivedDataPath build \
  CODE_SIGN_IDENTITY="-" CODE_SIGNING_REQUIRED=NO

# 2. Create DMG with Applications symlink
APP=build/Build/Products/Release/ScryBarCompanion.app
STAGING=/tmp/scrybar-dmg-staging
rm -rf "$STAGING" && mkdir -p "$STAGING"
cp -R "$APP" "$STAGING/"
ln -s /Applications "$STAGING/Applications"
hdiutil create -volname "ScryBar Companion" \
  -srcfolder "$STAGING" -ov -format UDZO \
  ../ScryBarCompanion-<VERSION>.dmg
rm -rf "$STAGING"
```

Output lands in `companion/mac/ScryBarCompanion-<VERSION>.dmg`.

Signing notes:

- Ad-hoc signing (`-`) is sufficient for local/GitHub distribution.
- First launch on another Mac triggers Gatekeeper: right-click → Open, or System Settings → Privacy & Security → "Open Anyway".
- To eliminate Gatekeeper warnings: sign with a Developer ID Application certificate ($99/yr Apple Developer Program) + notarize via `xcrun notarytool submit`.

Wi-Fi preference behavior:

- `wifi_pref_ssid=""` means automatic rotation across known credentials.
- `wifi_pref_ssid="<known ssid>"` pins reconnect priority to that SSID.
- Value is validated against provisioned credentials only (no runtime password entry).
- NVS key: `wifi_pref`.
- On change, firmware schedules immediate reconnect and disconnects current STA only if needed.

Wi-Fi direct/provisioning behavior:

- Firmware keeps cycling all known SSIDs (from `secrets.h` + runtime NVS list).
- When mode is `auto`, setup AP is started after prolonged disconnect (`WIFI_SETUP_AP_AUTOSTART_MS`) or immediately if no known SSID exists.
- Setup AP mode is `2.4 GHz` only by hardware/network stack constraints.
- New credentials can be provisioned from web UI scan (`GET /api/wifi/scan`) and saved to runtime known list in NVS.
- Runtime credentials persist across reboot/reflash, and are cleared only when NVS is erased.
- Setup URL in AP mode is canonical: `http://192.168.4.1:8080` (also exposed via QR endpoint `GET /api/wifi/setup-qr.svg`).
- Captive portal probes are redirected, but popup behavior is OS-dependent; docs and operators should treat QR/direct URL as authoritative.
- Wi-Fi scan API is timeout-bounded and returns quickly even in AP setup mode (`scan_timeout` / `scan_failed` message path), to avoid hanging UI.

## UI Theming System (Unified)

The theming architecture is token-driven and unified across firmware UI and web UI.

Core structure in firmware:

- `UiThemeDefinition` = `{ id, label, web tokens, lvgl tokens }`
- `kUiThemes[]` contains all runtime themes
- `activeUiTheme()` + `setActiveUiThemeById()` drive runtime switching

Current theme ids:

- `scrybar-default`
- `cyberpunk-2077`
- `toxic-candy`
- `tokyo-transit`
- `minimal-brutalist-mono`
- `mint-protocol` *(LIGHT — sage green, Vibemilk DS v3, added r217)*
- `cathode-ray` *(DARK — phosphor green #33FF33 + amber #FFAA00 on near-black, CRT aesthetic, added r217)*

Theme switch inputs:

- Web UI `<select name="ui_theme">`
- API `ui_theme=<id>`
- Serial `THEME <id>` (plus `THEME` for list/status)

Where tokens are applied:

- Device LVGL palette/styling (`UiThemeLvglTokens`)
- Embedded web control surface CSS vars (`appendWebThemeCssVars`)
- Standalone design system (`assets/scrybar_design_system/`, HTML `data-theme` selector)

Important behavior rules:

- Weather panel is forced to light-background/dark-text fallback when needed, to preserve readability of transparent weather icons across themes.
- Light/dark detection: `lvglColorLuma(screenBg) >= 128` — automatic from palette, no per-theme flag needed.
- Sharp corners: enabled for `minimal-brutalist-mono` and `cathode-ray` via `lvglThemeIsCathodeRay()` helper + luma check; all other themes use rounded corners.
- **GOTCHA (Mint Protocol)**: `screenBg=#DBE8DB` (luma 220) and `panelBg=#FFFFFF` (luma 255) are close but distinguishable. `weatherCardBg=#EEF7EF` (tinted) instead of pure white — without the tint, the weather card was invisible against the near-white bg.
- **GOTCHA (Cathode Ray)**: phosphor green `#33FF33` is extremely saturated — web CSS `--text-muted` must stay at `#2D5A2D` (dark) not a light green, or it washes out on near-black bg.
- **Adding a new theme**: add `UiThemeDefinition` entry to `kUiThemes[]` in `scrybar.ino`, CSS vars block in `web/src/styles/tokens.css`, pill in `web/src/components/ThemeSelector.astro`. No other files need touching.

## Font System (Web + LVGL)

### Display typeface (LVGL)

**Funnel Display** (Google Fonts, SIL OFL) — unified across ALL themes since r215.
- Condensed display face, optimized for narrow 640×172 display
- 12 sizes generated: 12, 14, 16, 18, 20, 22, 23, 24, 25, 30, 32, 38 px
- ASCII range (0x20-0x7E) with `--lv-fallback lv_font_montserrat_XX` for extended chars
- Source TTF: `assets/fonts/FunnelDisplay-Regular.ttf` (24KB)
- Generated files: `src/fonts/scry_font_funnel_display_*.c`
- No per-theme font dispatch — all `lvglFont*()` functions are simple one-liners

### Web UI typography (per-theme, unchanged)

- `scrybar-default`: Montserrat web stack
- `cyberpunk-2077`: Space Mono terminal style
- `toxic-candy`: Delius Unicase (web), Chakra fallback
- `tokyo-transit`: Chakra Petch (web), Space Mono fallback
- `minimal-brutalist-mono`: IBM Plex Mono stack

Web Wi-Fi provisioning input policy:

- Password field (`#wifi_new_password`) is forced monospaced independent of active theme.
- Password field has show/hide eye toggle (`#wifi_pwd_toggle`) to avoid ambiguity under all-uppercase/stylized themes.

### Generated LVGL fonts

Sources:

- `assets/fonts/SpaceMono-Regular.ttf`
- `assets/fonts/DeliusUnicase-Regular.ttf`

Generated outputs:

- `src/fonts/scry_font_space_mono_{12,16,18,20,24,28,32}.c`
- `src/fonts/scry_font_delius_unicase_{12,16,18,20,24,28,32}.c`

Generation constraints:

- Use static TTFs (non-variable fonts)
- `lv_font_conv` with `--no-compress`
- Glyph range: `0x20-0x7E`
- Montserrat fallback enabled per size

## Build-Scope Hygiene

Arduino sketch behavior:

- Sources under `src/` are part of the active compile scope.
- Parked modules should stay under `vendor/` to avoid implicit compile.

Audio stack policy:

- Legacy audio/codec tree was moved from `src/audio` to `vendor/audio`.
- It is retained for future work but intentionally excluded from default firmware builds.
- Reactivation should be explicit and partial (import only required modules), not full-tree restore.

Reference:

- `src/README.md`
- `vendor/audio/README.md`

### Clock sentence auto-fit

Clock line `g_lvglClockL1` uses runtime auto-fit:

- Collect Funnel Display font cascade (38→32→30→24→22→20→18 px)
- Apply candidate, measure label height against available clock body space
- Select largest fitting font and re-center

This keeps clock text visually full across resolutions/themes without clipping.

## Language System (Word Clock + Full Display UI)

`g_wordClockLang` is the single language pivot for display content.

Supported 13 language codes:

- Standard: `it`, `en`, `fr`, `de`, `es`, `pt`, `la`, `eo`, `tlh`
- Creative/fun: `l33t`, `sha`, `val`, `bellazio`

Bellazio sentence style rule:

- `bellazio` keeps slang always-on with rotating lead/closer expressions (for variety), while preserving correct Italian grammar (`all'` / `alle`) and writing minute values in words (not digits).

Character safety rule:

- Displayed text is normalized to printable ASCII before rendering (clock/date/RSS/weather metadata), with UTF-8 folding + HTML entity decode + whitespace normalization.
- Goal: avoid unsupported glyph boxes on LVGL fonts constrained to `0x20-0x7E`.

Screensaver thought localization rule:

- Cow thought balloon quotes are language-aware and follow `wc_lang`.
- Supported with dedicated quote packs for all configured language codes; unknown codes fallback to Italian.

Localization architecture:

- `src/ui_strings.h`: `UiStrings` struct + one instance per language
- `activeUiStrings()` dispatcher at runtime
- Weather text dispatchers (`weatherCodeUiLabel*`, `weatherCodeShort*`)
- Date formatting dispatchers per language

Persistence + runtime apply:

- Web form/API payload key: `wc_lang`
- Saved in NVS key `wc_lang`
- UI re-renders with selected language

Web config selector groups languages in two `<optgroup>` blocks:

- `Creative & Constructed`
- `Modern Languages`

## Design System (`assets/scrybar_design_system`)

Purpose:

- Canonical token/documentation playground for ScryBar UI
- Theme selector in top bar writes `<html data-theme="...">`
- Mirrors real product theme ids/nomenclature

Notable features:

- Runtime theme selector with contrast-safe dropdown states
- Cyberpunk geometric chamfers and clipped corners via `clip-path` + pseudo-layer borders
- Theme-reactive FX grid animation section
- Responsive layout behavior across theme presets

Main files:

- `index.html`
- `scrybar.css`
- `SCRYBAR_DESIGN_SYSTEM.md`

Theme ideation backlog:

- `knowledge/theme_proposals_catalog.md` tracks candidate themes, palettes, font pairings, and external style references before implementation.

## README Preview Assets

Theme preview screenshots used in `README.md` are tracked in:

- `assets/readme_previews/home_weather_scrybar-default.png`
- `assets/readme_previews/home_weather_cyberpunk-2077.png`
- `assets/readme_previews/home_weather_toxic-candy.png`
- `assets/readme_previews/home_weather_tokyo-transit.png`
- `assets/readme_previews/home_weather_minimal-brutalist-mono.png`

This location is intentionally outside ignored `screenshots/` paths.

## LVGL UI Init Architecture (M1, r200)

`initLvglUi()` is a ~94-line orchestrator that delegates to focused sub-functions:

- `lvglCreatePageRoot()` — reusable transparent page container helper (13 lines)
- `initLvglInfoPanel()` — INFO page: stats header, body text, QR code (132 lines)
- `initLvglClockPanel()` — HOME clock block: header, WiFi bars, date, word clock labels (131 lines)
- `initLvglWeatherPanel()` — HOME weather card: structure, header, city/sun labels (119 lines)
- `initLvglWeatherBodyWidgets()` — weather body: temp, icon, glyph, forecast bar (153 lines)
- `initLvglScreensaverUi()` — cow screensaver: sky, stars, cow, balloon, footer (90 lines)
- `lvglInitFeedDeck()` — AUX/WIKI feed deck widgets (161 lines, uses shared helpers)
- `lvglInitNowPlayingUi()` — Now Playing UI (148 lines, uses shared helpers)

Each sub-function locally computes its layout dimensions and theme colors from `canvasWidth()`, `canvasHeight()`, and `activeUiTheme()`, matching the self-contained pattern of `lvglInitFeedDeck()`.

## LVGL Style Helper Library (M9, r213)

Shared helper functions that eliminate repeated LVGL boilerplate across all `initLvgl*` and `lvglApplyTheme*` functions:

| Helper | Purpose | Replaces |
|---|---|---|
| `lvglSetBgFlat(obj, hex)` | Flat bg_color + bg_grad_color from hex | 2-line bg pair pattern |
| `lvglSetBgFlatR(obj, hex, radius)` | Flat bg + radius | 3-line bg+radius pattern |
| `lvglSetTextHex(obj, hex)` | Null-guarded text color from hex | `if (obj) lv_obj_set_style_text_color(...)` |
| `lvglSetHeaderBorder(obj, show, hex)` | Conditional header border (bordered themes) | 3-line border_width/color/opa pattern |
| `lvglSetBtnBorder(obj, hex)` | Button accent border (1px, 80% opacity) | 3-line border pattern |
| `lvglCreatePanel(parent, w, h, x, y, bg, radius)` | Opaque container: flat bg, no border/shadow/scroll | 10-line object init boilerplate |
| `lvglCreateDeckButton(parent, w, h, x, y, bgHex, radius, label, textHex, outText)` | Deck action button with centered label | 16-line button init pattern |

`lvglApplyThemeStyles()` (156 lines) delegates feed deck theming to `lvglApplyThemeStylesFeedDecks()` (67 lines).

### Weather Display Helpers (M10, r214)

Weather icon/label update extracted from `updateLvglUi()`:

| Helper | Purpose |
|---|---|
| `lvglShowWeatherMainIcon(code, isDay, glyphFallback)` | Show bitmap icon or glyph fallback for current weather |
| `lvglShowWeatherForecastIcon(code, isDay)` | Show/hide forecast bitmap icon |
| `lvglSetWeatherOfflineLabels(desc, glyph, color, setColor)` | Set all weather labels to offline/placeholder state |
| `lvglUpdateWeatherDisplay(glyphOnline, glyphOffline)` | Full weather section (online + offline branches) |

`kWmoFallbackCode = 2` ("partly cloudy") — named constant for offline weather icon fallback.

### NVS Load Helpers (M10, r214)

| Helper | Purpose |
|---|---|
| `nvsLoadRssFeeds(prefs, loadedAny)` | Legacy single-URL key + multi-slot feed loop |
| `nvsLoadLanguageConfig(prefs, langNeedsPersist)` | Language load + genz→bellazio + bellazi→bellazio migration |

**When creating new LVGL containers:** Use `lvglCreatePanel()` as the default starting point. Override individual properties (gradient, opacity, border) after the call. Only use raw `lv_obj_create()` if the panel pattern doesn't fit.

## Language Dispatch Architecture (M2, r201)

Table-driven dispatch via `LangVtable` in `src/lang_types.h`:

- `kLangTable[13]` in `scrybar.ino` — one entry per language, maps code to:
  - `wordClock` function pointer (per-language compose logic)
  - `weatherShort` → `WeatherShortLabels` struct (8 category strings)
  - `weatherUi` → `const char*[WMO_UI_COUNT]` array (24 detailed WMO labels)
  - `formatDate` function pointer (per-language date formatting)
  - `uiStrings` → `UiStrings*` (from `src/ui_strings.h`)
- `findLangVtable()` — single lookup by `g_wordClockLang`, returns Italian default
- Dispatcher wrappers (`weatherCodeShort()`, `weatherCodeUiLabel()`, `composeWordClockSentenceActive()`, `activeUiStrings()`, `formatDateActive()`) are 1-2 line pass-throughs

**To add a new language:** 1 `kLangTable` entry + 4 functions (wordClock, weatherShort data, weatherUi data, formatDate) + 1 UiStrings in `ui_strings.h` + add code to `kAllowedLangs[]`. Zero dispatcher changes needed.

## Transit Departure Board (r240+, layout r248)

Live departure board using the [Transitous](https://transitous.org) community GTFS API (Motis backend). Free, global, no API key required.

### Key endpoints

| Endpoint | Purpose |
|---|---|
| `GET /api/v1/geocode?text=X&limit=12` | Station search — returns plain JSON array; filter `type === 'STOP'` only (excludes city POIs) |
| `GET /api/v1/stoptimes?stopId=X&n=8&time={utcISO}` | Next departures from a stop |
| `GET /api/v1/trip?tripId=X` | Full trip legs — used to get destination arrival time (`legs[0].to.arrival`) |

The `stopId` must be **fully percent-encoded** (colons, slashes, etc.) — use `urlEncodeParam()`.

### GTFS feed field variations — critical gotchas

Different national operators export different JSON field shapes for the same API endpoint:

| Operator / Feed | `headsign` | Destination source | Notes |
|---|---|---|---|
| **Trenord** (Lombardia IT) | ✅ populated | `headsign` | `routeShortName` = "S30", "R21" |
| **Trenitalia** (IT national) | `""` empty | `tripTo.name` | NeTEx format gap — all headsigns blank |
| **UK rail** (TransXChange) | ✅ populated | `headsign` | `routeShortName` can be long ("Greater Anglia") → badge scroll |
| **Emilia-Romagna** (IT regional) | ✅ populated | `headsign` | Confirmed working (Fidenza) |
| **French RER/Transilien** (IDFM) | 4-letter mission code | `tripTo.name` | e.g., "ROPO" → "Corbeil-Essonnes". Firmware detects `^[A-Z]{4}$` and prefers `tripTo.name` |
| **French SNCF** (CDG, TGV) | train number (all digits) | `tripTo.name` | e.g., "5470" → "Rennes". Firmware detects all-digit headsigns and prefers `tripTo.name` |
| **Trenitalia France** (Lyon) | ✅ populated | `headsign` | `routeShortName` can be "---/---" (placeholder) or full route "Origin/Dest" |
| **Renfe AVE** (Barcelona) | ✅ populated | `headsign` | `mode` = "HIGHSPEED_RAIL"; `routeColor` near-white `#F2F5F5` (luma guard) |
| **DB local** (M\u00fcnchen DELFI) | ✅ populated | `headsign` | Geocode may return tram/bus stop, not main station platforms |
| **SBB** (CH, opentransportdata) | ✅ populated | `headsign` | `routeShortName` = "IC5", "RE48"; `realTime` = true; `mode` = "LONG_DISTANCE" |
| **Renfe** (ES) | ✅ populated | `headsign` | `routeShortName` very long ("PROXIMDAD 17192"); `routeColor` = near-white `#F2F5F5` (luma guard skips it) |
| **LIRR** (US, NY) | ✅ populated | `headsign` | `routeShortName` = train number ("1510"), not line name; `routeColor` populated per branch |
| **UK coach** (National Express) | route name | `headsign` | Format: "Origin - Destination" (e.g., "Belgravia, Victoria - Poole"); `mode` = "COACH" |
| **FlixBus** (EU) | ✅ populated | `headsign` | `routeShortName` = "FlixBus 075"; `mode` = "COACH"; `routeColor` = FlixBus green `#73D700` |
| **UK tube** (TfL) | ✅ populated | `headsign` | `routeShortName` = full line name ("Piccadilly", "Elizabeth line"); `mode` = "SUBWAY" or "METRO" |

**Rule:** use `headsign` unless it looks like a code (4 uppercase ASCII letters = mission code, OR all digits = train number, both with `tripTo.name` available) or is empty (Trenitalia). Firmware: `const char *dest = (headsign[0] && !looksLikeCode) ? headsign : tripToName`.

### Destination filter

Web UI field "FILTER BY DESTINATION" — stored in NVS as `transit_arr`. Implemented as case-insensitive **substring** match (`transitHeadsignContains()`) against the effective destination. Empty = show all departures. **GOTCHA:** if the filter contains the departure station name and you change the departure station, the filter will drop all results. Clear the filter field when switching stations.

### Badge line name

`routeShortName` is displayed in the left badge. Length varies hugely by network (3 chars for "S30" vs 14 chars for "Greater Anglia"). Firmware uses adaptive font shrink (20→18→16→14px by char count) + `LV_LABEL_LONG_SCROLL` at 15 px/s for overflow — gives ~5-8 second reveal cycle.

### Row layout fonts (r248)
All transit row labels use `lvglFontMeta()` = **20px Funnel Display** for visual consistency:
- **Destination**: 20px (`lvglFontMeta`), left-aligned, truncated with `…`
- **Departure time** ("HH:MM"): 20px (`lvglFontMeta`), right-aligned
- **Arrival time** (">HH:MM"): 20px (`lvglFontMeta`), left-aligned, muted color
- **Platform / LIVE**: 20px (`lvglFontMeta`), right-aligned
- **Delay** ("+Xm"/"-Xm"): 16px (`lvglFontMini`), center-aligned, colored
- **Badge line name**: adaptive 20→18→16→14px (see Badge line name section above)
- All labels: +2px vertical offset for Funnel Display ascender optical centering
- **GOTCHA**: theme-override block (around line 12301) re-applies fonts after creation — must stay in sync with creation-time fonts.

### Stop ID selection

Geocode returns both `type=STOP` and `type=PLACE` results — filter to `type === 'STOP'`. For stations with multiple stops at the same name, the first result (highest Transitous relevance score) is used. Major junctions may have separate stop IDs per operator (Trenord vs Trenitalia at Gallarate) — the user should verify via the Transitous web map if results are unexpected.

### Known working stations (tested r247)
Porto Ceresio (IT, Trenord), Luino (IT, Trenord), Fidenza (IT, Emilia-Romagna), Gallarate (IT, Trenitalia), Ancona (IT, Trenitalia), London Liverpool Street (UK, TransXChange), Frankfurt Hbf (DE, nl-OpenOV cross-border ICE), Berlin Hbf (DE, nl-OpenOV cross-border EC/ES), Paris Gare de Lyon (FR, IDFM RER+bus+metro), Zürich HB (CH, SBB IC/RE), Wien Hbf (AT, tram via PTA-Eastern), Madrid Atocha (ES, Renfe Cercanías/MD/REG), NY Penn Station (US, LIRR+NJTransit), Heathrow T5 (UK, Piccadilly+Heathrow Express+Elizabeth), Amsterdam Schiphol (NL, FlixBus COACH), Victoria Coach Station (UK, National Express COACH+bus), München Hbf Süd (DE, DELFI tram+bus), Barcelona Sants (ES, Renfe AVE+Cercanías), Lyon Part-Dieu (FR, Trenitalia France), CDG Terminal 2 (FR, SNCF regional).

### Known mode values (from global testing r247)
`REGIONAL_RAIL`, `LONG_DISTANCE`, `HIGHSPEED_RAIL`, `NIGHT_RAIL`, `SUBURBAN_RAILWAY`, `SUBWAY`, `METRO`, `TRAM`, `BUS`, `COACH`, `FERRY`. Badge shape: `BUS`/`TRAM`/`COACH` → pill (radius=14); all others → rect (radius=6).

### Geocode relevance gotchas
- **Wien Hbf**: first STOP result is Ukrainian Railways feed (`ua-ukrzaliznytsya_8101003`) — shows only 1 daily international train. Use Transitous web map to find the ÖBB stop ID.
- **Frankfurt/Berlin Hbf**: first STOP result is `nl-OpenOV` (Netherlands feed) — shows only cross-border ICE/EC services, not local DB/S-Bahn. A DE-DELFI feed stop ID is needed for full coverage.
- **München Hbf**: first STOP result is a tram/bus stop ("Hauptbahnhof Süd"), not the main train platforms.
- **SF Caltrain**: empty stopTimes (GTFS schedule data gap for queried date range).
- **Tokyo Station**: NO STOP results at all — Japanese rail data is NOT in Transitous (only PLACE results returned).

### Airports — coverage and limitations
Transitous covers **ground transport only** (trains, metro, trams, buses, coaches). No flight data. Airport stops show rail/metro connections departing from the airport, not flight departures. Examples: Heathrow T5 shows Piccadilly/Elizabeth/Heathrow Express; CDG T2 shows SNCF regional trains; Schiphol shows FlixBus only (geocode points to coach terminal, not NS rail).

### Testing targets (not yet verified)
JFK AirTrain, NJ Transit, BART (geocode returned bus stop, not rail).

---

## Global State Struct Architecture (M7, 2026-03-24)

Firmware globals are grouped into 16 typed structs. Each struct instance keeps the `g_` prefix for grep-ability. Struct definitions sit inside the same `#if` guards as the original variables.

Key struct instances and their subsystems:

| Instance | Struct | Subsystem | Fields |
|---|---|---|---|
| `g_batt` | `BatteryState` | Battery + energy saver | 14 |
| `g_pwrBtn` | `PwrButtonState` | Power button debounce | 7 |
| `g_navBtn` | `NavButtonState` | Nav/BOOT button | 4 |
| `g_wifiSt` | `WifiState` | WiFi + credentials + reconnect | 28 |
| `g_touch` | `TouchState` | Touch input + gestures | 15 |
| `g_doom` | `DoomState` | DOOM tilt/neutral/palette | 24 |
| `g_imu` | `ImuState` | IMU sensor state | 6 |
| `g_clock` | `ClockState` | NTP + clock rendering | 8 |
| `g_saver` | `ScreensaverState` | Cow screensaver | 34 |
| `g_perf` | `PerfCounters` | Frame perf counters | 7 |
| `g_webCfg` | `WebConfigState` | Web server + mDNS + QR | 10 |
| `g_clockUi` | `LvglClockUi` | Clock LVGL widgets | 10 |
| `g_weatherUi` | `LvglWeatherUi` | Weather LVGL widgets | 22 |
| `g_infoUi` | `LvglInfoUi` | INFO page LVGL widgets | 10 |
| `g_dispHw` | `DisplayHwState` | Display HW handles + DMA | 7 |
| `g_pageAnim` | `LvglPageAnimState` | Page drag/animation | 3 |

Access pattern: `g_batt.voltage`, `g_wifiSt.connected`, `g_doom.moveBin`, etc.

Remaining independent `g_` globals (35) are pre-existing struct instances (`g_weather`, `g_rss`, `g_wiki`, `g_auxDeck`, `g_wikiDeck`, `g_nowPlayingUi`, `g_liveNowPlaying`, etc.), LVGL page roots, hardware objects (`g_qmi`, `g_gfx`), and standalone config state.

## LVGL Configuration Baselines

Key settings:

- `LV_COLOR_16_SWAP=1` in `lv_conf.h` (AXS15231B byte order)
- `full_refresh=1` in display driver (rotation stability)

## Display Orientation

Default: `DISPLAY_FLIP_180=1` in `config.h` (USB-C left, speaker top, mic bottom).
Touch mapping is direct (`x=rawX`, `y=rawY`) with no axis swap in current baseline.

## Touch Anti-Ghost Filtering (AXS15231B)

Discard touch frames where:

- `point_count == 0` or `point_count > 5`
- Any coordinate ≥ `0x0FFF`
- Raw coords outside panel bounds

## View Model and Navigation

- Runtime pages: `INFO`, `HOME`, `AUX` (RSS), `WIKI`, `DOOM`.
- Swipe graph:
  - `INFO <-> HOME <-> AUX <-> WIKI <-> DOOM`
  - `AUX/WIKI` share the same content deck widgets and controls.
  - `DOOM` is a direct-render page and bypasses LVGL page dragging while active.
- AUX/WIKI controls:
  - `SKIP` = next item
  - `NXT` = next feed
  - `QR` = modal on-demand (not always visible)
- DOOM control baseline:
  - `TITLEPIC` welcome screen first; `FIRE` boots the live core
  - centered 4:3 frame (`230x172`) inside the `640x172` display
  - left/right pillarbox bands are gameplay HUD + touch buttons
  - IMU is active only in `UI_PAGE_DOOM`
  - neutral is captured only after a short stable settle window
  - left band = `USE`, right band = `FIRE`, center tap = recenter, swipe = exit
- WIKI deck model:
  - 3 feed families: `Featured`, `On this day`, `Random Article`
  - up to 3 items per family (total max 9 items/cycle)
  - Wikipedia language (`wiki_lang`) is independent from system language — supports `en`, `it`, `fr`, `de`, `es`, `pt`, `la`, `eo`
  - Random Article fetched via REST API (`/api/rest_v1/page/random/summary`) in selected wiki language
  - refresh cadence uses `RSS_REFRESH_MS` / `RSS_RETRY_MS` (defaults 15m / 2m)
- Physical buttons (current mapping):
  - `PWR` short press: screensaver toggle (debounced)
  - `PWR` long press `1.5s`: hard-off (TCA9554 SYS_EN cut on battery) / soft-off fallback on USB
  - `PWR` long press `1.5s` in soft-off: wake to `HOME`
  - `BOOT` short press: jump to `HOME`
  - `RST`: hardware reset
- Power button GOTCHAs:
  - Pin: GPIO16, active-low, INPUT_PULLUP. Confirmed via `[PWR] raw level change` serial events.
  - 3 seconds without visual feedback = users release early → threshold was halved to 1.5s.
  - Hard-off (`shutdownFromPowerButton(true)`): tries TCA9554 EXIO6=LOW first; if USB holds chip alive, falls back to soft-off (display off, MCU busy-wait). This is expected USB behavior, not a bug.
  - Short press toggles screensaver — can confuse shutdown attempt if user taps before holding.

## DOOM Integration Baseline

- Donor source: `ducalex/retro-go`, but only `prboom-go` is vendored into `src/doom/prboom/`.
- ScryBar glue:
  - `src/doom/scrybar_prboom.cpp`
  - `src/doom/scrybar_prboom_runtime.h`
  - `build_opt.h`
- Current embedded DOOM assets:
  - `src/doom/doom1.wad`
  - `src/doom/doom_iwad.S`
  - `src/doom/doom_titlepic.h`
- Direct framebuffer path:
  - DOOM writes an 8-bit `320x200` frame
  - `doomScrybarBlitIndexedFrame()` maps it into the centered `230x172` live frame
  - DOOM owns the display and flushes directly through `dispFlush()`
- Memory/runtime gotchas now part of the stable baseline:
  - keep `DB_CHUNK_ROWS=32`, not `64`, to preserve TLS heap headroom
  - DOOM task stack/framebuffer must be allowed to use PSRAM
  - IMU must stay DOOM-only to avoid wasted CPU/log spam
  - neutral calibration must reset filter state and wait for a stable window
- Supporting deep note:
  - `knowledge/doom_integration_gotchas.md`

## Power Policy

- Long-press 3s: soft-off (wakeup via long-press, resume on `HOME`)
- Hard-off only by serial `PWROFFHARD`
- Boot always re-asserts `SYS_EN` via TCA9554
- Battery monitor: ADC1 CH3, 12dB attenuation, ×3 divider
- Screensaver idle target: `2h` on USB and `2h` on battery
- Power short-press hardening:
  - press debounce window: `45ms`
  - minimum short-press duration: `70ms`
  - sub-threshold pulses are treated as bounce/glitch and ignored

## FAT Filesystem

- FAT partition: 5.9 MB at offset `0xA10000` (see `partitions.csv`).
- Currently unused after ANSI viewer removal (r183). Available for future features.
- `FFat.begin(false)` is called at boot (no format-on-fail) for readiness.

## RSS Favicon System (r215+)

- Source: Google Favicon API — `https://www.google.com/s2/favicons?domain={host}&sz=32`
- Decode: `pngle` library (streaming PNG decoder, ~10KB flash)
- Output: RGB565 in PSRAM, alpha-blended onto white background
- Cache: 8 slots × 2KB = 16KB PSRAM; LRU eviction when full
- Prefetch: triggered after each RSS fetch; skips already-cached hosts
- Display: `lv_img` inside source badge panel; hides text badge when favicon available
- Fallback: text badge ("NYT", "BBC", etc.) shown if favicon download fails
- GOTCHA: `LV_COLOR_16_SWAP=1` — store RGB565 big-endian (high byte first)
- GOTCHA: PNG transparent pixels must be alpha-blended (not ignored) — favicon PNGs have non-zero RGB in transparent areas

## RSS/WIKI Data Pipeline Notes

- Text sanitization:
  - feed text passes through HTML entity decode + tag stripping + whitespace collapse
  - goal: avoid raw HTML leakage in article body and keep ASCII-safe render path
- Thumbnail compatibility:
  - if image host is Wikimedia thumb JPG, firmware first tries a PNG thumb variant
  - JPEG payloads are normalized with JFIF APP0 injection when missing, to maximize LVGL decoder compatibility
  - thumbnail budget is tuned for richer cards (`RSS_THUMB_MAX_BYTES=65536`)
- QR policy in AUX/WIKI:
  - QR can open immediately with canonical long URL
  - short URL replacement happens opportunistically when ready (non-blocking UX)

## Web UI — Architecture & Rules

**Design system:** Vibemilk DS v3 subset (Netmilk Studio's token-driven CSS). All CSS uses `vm-*` prefixed classes. No external CSS framework, no Font Awesome, no animations.

**Fixed theme:** Web config page is always rendered with scrybar-default styling. CSS `:root{}` block in `kWebCssCore` contains hardcoded token values (no bridge layer, no runtime injection). Component classes (`vm-card`, `vm-btn`, `vm-input`, `vm-select`, `vm-label`, `vm-badge`) reference final variable names directly. Theme selector dropdown still exists in the form (drives the ESP32 display theme), but does not change web page styling.

**Layout rules (r198+):**
- **One visible container per section.** Each config section is a single `vm-card`. No `vm-card--inner` wrappers — content sits directly inside the card.
- **No box-in-box nesting.** View items use `border-bottom` separators, not individual bordered cards. RSS rows use left accent bar + bottom separator. System Info columns are plain `<div>` inside `vm-grid`, not inner cards.
- **Flat hero.** Logo + release row + lede text flow directly in `section.hero` (no `hero-top-card` wrapper, no `hero-copy` wrapper). Hero has a subtle bottom border as separator.
- Single responsive breakpoint at 768px. Mobile tightens padding: `vm-wrap` 12px/10px, `vm-card` 14px/12px, grid gap 8px.
- `vm-actions` (Save/Reload buttons) have 40px bottom margin to visually separate from System Info.

**Emoji in HTML:** Always use HTML numeric entities (`&#x1F3A8;`, `&#x1F310;`, etc.) — never raw UTF-8 emoji in `F()` strings. Raw multi-byte emoji risk double-encoding through the Arduino toolchain, producing mojibake/tofu on mobile browsers.

**Font loading:** Google Fonts (Montserrat + Space Mono) loaded async with system-font fallback stack. Page must be fully functional offline (AP mode) with only system fonts.

**Zero CDN dependencies** for functionality. Google Fonts is the only external resource and is loaded with `display=swap` for graceful degradation.

**Key functions:**
- `buildWebConfigPage()` — generates complete inline HTML page (hardcoded scrybar-default CSS, no runtime theme injection)
- `buildWebCssBlock(String&)` — emits `kWebCssCore` PROGMEM (single call, no parameters)
- `runtimeLogoUrl()` — returns user-configured or default logo URL for hero
- `appendWebThemeCssVars(String&, const UiThemeWebTokens&)` — retained for design system use, not called by web config page

**Output budget:** ~20KB target (was ~35KB pre-vibemilk). No keyframe animations, no backdrop-filter, no FX grid divs.

**Responsiveness:**
- Web control page keeps critical style inline and loads external font CSS asynchronously.
- During long network I/O (RSS/WIKI fetch/download), firmware periodically pumps the web server loop to reduce UI stalls.
- RSS HTTP timeout baseline is intentionally short (`RSS_HTTP_TIMEOUT_MS=3000`) for quicker recovery on bad links.

## Serial Command Reference (Operational)

| Command | Effect |
|---|---|
| `HELP` | Print available commands |
| `THEME` | Print current theme + list |
| `THEME <id>` | Switch theme at runtime (and persist) |
| `VIEW` | Toggle HOME ↔ AUX |
| `VIEWFIRST` | Jump to first main view (`HOME`, INFO excluded) |
| `VIEWLAST` | Jump to last main view (`DOOM`) |
| `VIEW0` / `VIEWINFO` | Force INFO page |
| `VIEW1` / `VIEWHOME` | Force HOME page |
| `VIEW2` / `VIEWAUX` / `VIEWRSS` | Force AUX/RSS page |
| `VIEW3` / `VIEWWIKI` | Force WIKI page |
| `VIEW4` / `VIEWDOOM` / `DOOM` | Force DOOM page |
| `SNAP` | Emit framebuffer snapshot protocol |
| `BATSTAT` | Print battery status |
| `SAVERON` / `SAVEROFF` | Toggle screensaver |
| `PWRSTAT` | Print power button status |
| `PWROFFHARD` | Hard power-off |
| `WEBCFG` | Print active web config summary |

Runtime summary (`[SUMMARY]`) is emitted every 30s.

## Screenshot Workflow

Base capture:

```bash
python3 tools/capture_snapshot.py --port <PORT> --out-dir screenshots
```

Wire format with current LVGL settings is `rgb565be`.
For deterministic theme captures:

1. Send serial theme/view command(s) first.
2. Wait a short settle delay (`~1-2s`).
3. Capture frame.

## Recurring Gotchas

- Do not compile with default bare board profile: wrong partition/flash settings can fail with "Sketch too big".
- Keep `TEST_TOUCH=1` for swipe navigation.
- `arduino-cli monitor` can drop on USB re-enumeration after reset/upload.
- Use `lv_color_hex(0xRRGGBB)` (not RGB565 literals).
- Degree symbol in LVGL labels must be UTF-8 `"\xC2\xB0"`.
- Prefer non-variable font sources for deterministic LVGL conversion.
- If Wiki hero image appears missing on first draw, stay on WIKI briefly: visible-item progressive preload now fills summary/thumb/icon without requiring page changes.
- **Web UI emoji:** Never put raw multi-byte emoji (🎨🌐📖 etc.) in Arduino `F()` strings — they get double-encoded through the toolchain (each UTF-8 byte re-encoded as UTF-8, producing 8 bytes of mojibake). Use HTML numeric entities instead: `&#x1F3A8;` for 🎨, `&#x1F310;` for 🌐, etc.
- **Web UI box nesting:** Never wrap form content in inner containers (`vm-card--inner` was removed). One `vm-card` per section is the visual boundary — content sits directly inside, no wrapper divs. See "Web UI — Architecture & Rules" for the full pattern.
- **MediaRemote artist field:** macOS MediaRemote only exposes the primary artist, not collaborators. "Rihanna, Kanye West, Paul McCartney" shows as just "Rihanna". This is a system-level limitation, not a companion bug.
- **Now Playing LVGL labels:** Use `LV_LABEL_LONG_DOT` for single-line truncation (artist). Title uses `LV_LABEL_LONG_WRAP` with max 2 lines + manual clipping. LVGL 8 has no native multi-line ellipsis.
- **Struct grouping enum ordering:** When grouping globals into structs, any enum types used as struct members (e.g. `TouchAuxButton`, `UiClockMode`) must be defined *before* the struct definition. The Arduino/GCC single-pass compilation model requires forward declarations for types used in in-class member initializers.
- **LVGL panel helper vs raw create:** Use `lvglCreatePanel()` for new containers — it handles the 10-line boilerplate (opaque bg, no border/shadow/scroll, pad zero). Override specific properties after. Only use raw `lv_obj_create()` when the panel pattern doesn't fit (e.g. labels, images, canvases).
- **PSRAM heap for ephemeral large buffers:** Use `heap_caps_malloc(sz, MALLOC_CAP_SPIRAM)` + `heap_caps_free()` for any stack buffer >256B that is ephemeral (used within one function call). LVGL `lv_label_set_text()` copies the string, so the source buffer can be freed immediately after. Always check the allocation return for null.
- **Web flasher first install — bootloader mode:** Factory Waveshare firmware does not yield USB-CDC control to the browser via DTR/RTS. The `esp-web-tools` button will show "No device found". Fix: hold `BOOT`, tap `RST` once, release `BOOT` — the chip enters the ROM bootloader (shows as a bare CDC-JTAG device, no driver needed). This is a one-time step; subsequent OTA-style flashes work without it once ScryBar firmware is installed.
- **lvglCreatePanel sets bg_grad_color = bg_color:** This means no visible gradient by default. If you need a gradient, call `lv_obj_set_style_bg_grad_color()` and `lv_obj_set_style_bg_grad_dir()` after `lvglCreatePanel()`. The panel's bg_opa is `LV_OPA_COVER` — override to `LV_OPA_TRANSP` or `LV_OPA_40` etc. if transparency is needed.
- **Adding a new `UI_PAGE_*` — checklist of 9 matching sites:** When you add a new entry to the `UiPageMode` enum, the web-UI toggle, NVS persistence, and swipe carousel will silently lie until you touch EVERY site. Missing any one creates either a dead toggle (checkbox writes a flag nobody reads — happened with Transit in r247 through r261) or a zero-toggle page (user can't disable — happened with Launch in r252 through r261). The full checklist: (1) declare `UI_VIEW_FLAG_* = 0xNN`; (2) include the flag in `UI_VIEW_MASK_DEFAULT`; (3) add the `case` to `uiViewFlagForPage()`; (4) gate it in `uiPageEnabledNoEnsure()` as `flag AND any_functional_gate` (not one or the other); (5) add an `id='view_X_cb'` checkbox in `buildWebViewToggles()`; (6) add the parser entry to `parseViewsConfig()`'s `kViewArgs[]`; (7) add `hasArg("view_X")` to `webRequestHasConfigParams()`; (8) add the field to the `/api/config` JSON response; (9) add the pair to the JS `[[view_X, view_X_cb]]` list near `kWebJsCorePost`. If you also add the page to the swipe carousel, touch `kSwipePageOrder[]` and `uiPageInSwipeCarousel()`. Missing any of 1/2/3/5/6 means the checkbox exists but does nothing. Missing 4 means the flag persists to NVS but the page is always/never enabled. Grep anchor: search for `UI_VIEW_FLAG_WIKI` — every touch site reads that flag, so the list of files the grep returns IS the set of sites you need to update.
- **uint32_t `millis()` underflow (CRITICAL — cost us a full session r256→r259):** `loop()` captures `const uint32_t now = millis();` once at the top and passes the same `now` to many handlers. Handlers called *later* in the same iteration (`handleTouchSwipeInput`, `runImuLoop`, web/event callbacks, and the async `onWiFiEvent` task) read their **own fresh `millis()`** and stamp timestamp fields (`lastUserInteractionMs`, `lastShakeMs`, `lastDisconnectMs`, `noLinkSinceMs`, `receivedAtMs`, …). When a later handler — e.g. `handleScreenSaverLoop(now)` — computes `(now - storedMs) >= threshold`, `storedMs` can be a handful of ms *ahead* of the stale `now`. The `uint32_t` subtraction wraps to ≈4.29×10⁹, which is ≥ any sane threshold, so the check *fires falsely*. Concrete symptom: rapid-tapping WIKI would trigger the 2-hour-idle screensaver within seconds. **Rule: any handler called from `loop()` with a passed `nowMs` that compares it against a field stampable elsewhere MUST refresh `nowMs = millis();` at entry, OR clamp every subtraction as `(now >= stored) ? (now - stored) : 0`.** Never trust the stale loop snapshot for cross-handler time deltas. Prefer the entry-refresh pattern (one line, covers every subtraction in the function). Reserve the clamp for single-call-site subtractions against async-stamped fields (WiFi event handler). Fixed sites: `handleScreenSaverLoop` (idle + shake-wake), `wifiHandleSetupModeLoop`, `wifiSetupApActiveRecently`. See `knowledge/decisions.md` (r259) for the full incident writeup.

## Toolchain Setup (macOS — reference install)

Everything needed to compile, flash, and operate ScryBar from a macOS machine.

> **Quick start**: run `tools/bootstrap.sh` from the repository root.
> It installs, checks, and validates everything below — safe to re-run.

### 1. arduino-cli

```bash
# Install via Homebrew
brew install arduino-cli

# Initialize config (creates ~/.arduino15/arduino-cli.yaml)
arduino-cli config init

# Update index
arduino-cli core update-index
```

Verify:
```bash
arduino-cli version
# arduino-cli  Version: 1.x.x  ...
```

### 2. ESP32 Arduino Core

```bash
# Add Espressif board index (only if not already present)
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

# Update index again and install
arduino-cli core update-index
arduino-cli core install esp32:esp32

# Verify
arduino-cli core list | grep esp32
# esp32:esp32   x.x.x  ...
```

Tested core: `esp32:esp32` version 3.x (LTS branch). The FQBN must always include the full parameter string — do not use the bare `esp32:esp32:esp32s3`.

### 3. Required Arduino Libraries

Install via arduino-cli before first compile:

```bash
arduino-cli lib install "ArduinoJson"        # config/API serialization
arduino-cli lib install "lvgl"               # LVGL UI framework (8.x branch)
```

Check installed:
```bash
arduino-cli lib list
```

> LVGL config lives in `src/lv_conf.h` (not the library default). Ensure the library lookup finds this file via the sketch's `src/` path.

### 4. Python (screenshot tool)

The capture script (`tools/capture_snapshot.py`) requires Python 3 and `pyserial`:

```bash
pip3 install pyserial Pillow
```

Usage:
```bash
python3 tools/capture_snapshot.py --port <PORT> --out-dir screenshots
```

### 5. Identify Device Port

```bash
ls /dev/cu.usbmodem*
# Typical: /dev/cu.usbmodem14101
```

If the device is not found, check:
- USB cable supports data (not charge-only)
- Board booted with `USBMode=hwcdc` (correct FQBN)
- If upload hangs: hold BOOT, press+release RST, release BOOT

### 6. Recommended shell aliases (optional)

```bash
alias scry-build='arduino-cli compile --clean \
  --build-path /tmp/arduino-build-scrybar \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  .'

alias scry-flash='arduino-cli upload \
  -p $(ls /dev/cu.usbmodem* | head -1) \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  --input-dir /tmp/arduino-build-scrybar \
  .'

alias scry-monitor='arduino-cli monitor -p $(ls /dev/cu.usbmodem* | head -1) --config baudrate=115200'
```

### 7. lv_font_conv (LVGL font generation — optional)

Only needed when adding/regenerating custom LVGL fonts:

```bash
npm install -g lv_font_conv
```

Usage example (Funnel Display, 20px):
```bash
lv_font_conv \
  --font assets/fonts/FunnelDisplay-Regular.ttf \
  --size 20 --bpp 4 --no-compress \
  --range 0x20-0x7E \
  --lv-fallback lv_font_montserrat_20 \
  --lv-font-name scry_font_funnel_display_20 \
  --format lvgl -o src/fonts/scry_font_funnel_display_20.c \
  --lv-include lvgl.h
```

### 8. git (standard macOS git or Homebrew)

```bash
xcode-select --install    # includes git
# or
brew install git
```

Repository uses no LFS. Assets are tracked directly.

## Public Logging Rule

If you add session notes to versioned docs:

- keep only generalized lessons
- avoid local identifiers
- do not copy raw serial logs with personal/network metadata
