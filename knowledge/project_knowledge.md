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
- `knowledge/`: shared, sanitized cross-assistant knowledge (public, versioned).
- `memory/`: local assistant memory (project-local, not canonical public documentation).

## Core Stack

- Firmware: Arduino sketch (`scrybar.ino`)
- MCU: ESP32-S3
- Display/touch target: Waveshare ESP32-S3-Touch-LCD-3.49
- UI: LVGL 8.x
- Local config UI: embedded HTTP server (`/`, `/config`, `/api/config`, `/api/reload`)

## Canonical Board Profile

Use these compile/upload parameters as baseline:

- board: `esp32:esp32:esp32s3`
- CPU: `240MHz`
- Flash: `16MB`
- PSRAM: `OPI`
- Flash mode: `QIO`
- Partition: `noota_app15M_16MB` (custom single-app, no OTA, ~15.9MB app space)
- Upload speed: `921600`
- USB mode: `hwcdc`, CDC on boot enabled

## Build Commands (Template)

Compile:

```bash
arduino-cli compile --clean \
  --build-path /tmp/arduino-build-scrybar \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=noota_app15M_16MB,PSRAM=opi \
  .
```

Upload:

```bash
arduino-cli upload -p <PORT> \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=noota_app15M_16MB,PSRAM=opi \
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

Theme switch inputs:

- Web UI `<select name="ui_theme">`
- API `ui_theme=<id>`
- Serial `THEME <id>` (plus `THEME` for list/status)

Where tokens are applied:

- Device LVGL palette/styling (`UiThemeLvglTokens`)
- Embedded web control surface CSS vars (`appendWebThemeCssVars`)
- Standalone design system (`assets/scrybar_design_system/`, HTML `data-theme` selector)

Important behavior rule:

- Weather panel is forced to light-background/dark-text fallback when needed, to preserve readability of transparent weather icons across themes.

## Font System (Web + LVGL)

### Theme typography map

- `scrybar-default`
  - Main UI font: Montserrat (LVGL built-ins + web stack)
  - Monospace utility font: Space Mono stack
- `cyberpunk-2077`
  - Main + mono: Space Mono terminal style
  - LVGL custom font family: `scry_font_space_mono_*`
- `toxic-candy`
  - Main UI font: Delius Unicase (web), Chakra fallback
  - Mono: Space Mono stack
  - LVGL custom font family: `scry_font_delius_unicase_*`
- `tokyo-transit`
  - Main UI font: Chakra Petch (web), Space Mono fallback
  - Mono: Space Mono stack
  - LVGL custom font family: `scry_font_space_mono_*`
- `minimal-brutalist-mono`
  - Main + mono web font: IBM Plex Mono stack
  - LVGL custom font family: `scry_font_space_mono_*` (embedded fallback)

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

- Collect candidate font list per active theme (largest to smallest)
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

- Runtime pages: `INFO`, `HOME`, `AUX` (RSS), `WIKI`, `ANSI`.
- Swipe graph:
  - `INFO <-> HOME <-> AUX <-> WIKI <-> ANSI`
  - `AUX/WIKI` share the same content deck widgets and controls.
- AUX/WIKI controls:
  - `SKIP` = next item
  - `NXT` = next feed
  - `QR` = modal on-demand (not always visible)
- WIKI deck model:
  - 3 feed families: `Featured`, `On this day`, `Random Article`
  - up to 3 items per family (total max 9 items/cycle)
  - Wikipedia language (`wiki_lang`) is independent from system language — supports `en`, `it`, `fr`, `de`, `es`, `pt`, `la`, `eo`
  - Random Article fetched via REST API (`/api/rest_v1/page/random/summary`) in selected wiki language
  - refresh cadence uses `RSS_REFRESH_MS` / `RSS_RETRY_MS` (defaults 15m / 2m)
- Physical buttons (current mapping):
  - `PWR` short press: screensaver (debounced)
  - `BOOT` short press: jump to `HOME`
  - `RST`: hardware reset

## Power Policy

- Long-press 5s: soft-off (wakeup via long-press)
- Hard-off only by serial `PWROFFHARD`
- Boot always re-asserts `SYS_EN` via TCA9554
- Battery monitor: ADC1 CH3, 12dB attenuation, ×3 divider
- Screensaver idle target: `2h` on USB and `2h` on battery
- Power short-press hardening:
  - press debounce window: `45ms`
  - minimum short-press duration: `90ms`
  - sub-threshold pulses are treated as bounce/glitch and ignored

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

## Web UI Responsiveness

- Web control page keeps critical style inline and loads external font/icon CSS asynchronously.
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
| `VIEWLAST` | Jump to last main view (`WIKI`) |
| `VIEW0` / `VIEWINFO` | Force INFO page |
| `VIEW1` / `VIEWHOME` | Force HOME page |
| `VIEW2` / `VIEWAUX` / `VIEWRSS` | Force AUX/RSS page |
| `VIEW3` / `VIEWWIKI` | Force WIKI page |
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
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=noota_app15M_16MB,PSRAM=opi \
  .'

alias scry-flash='arduino-cli upload \
  -p $(ls /dev/cu.usbmodem* | head -1) \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=noota_app15M_16MB,PSRAM=opi \
  --input-dir /tmp/arduino-build-scrybar \
  .'

alias scry-monitor='arduino-cli monitor -p $(ls /dev/cu.usbmodem* | head -1) --config baudrate=115200'
```

### 7. lv_font_conv (LVGL font generation — optional)

Only needed when adding/regenerating custom LVGL fonts:

```bash
npm install -g lv_font_conv
```

Usage example (Space Mono, 16px):
```bash
lv_font_conv \
  --font assets/fonts/SpaceMono-Regular.ttf \
  --size 16 --bpp 4 --no-compress \
  --range 0x20-0x7E \
  --format lvgl -o src/fonts/scry_font_space_mono_16.c \
  --force-include lv_conf.h
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
