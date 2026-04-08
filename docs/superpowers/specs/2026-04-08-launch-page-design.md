# LAUNCH Page — Rocket Launch Departure Board

**Date:** 2026-04-08
**Status:** Draft
**Page index:** 7 (UI_PAGE_LAUNCH), after TRANSIT in carousel

## Overview

A new ScryBar page showing upcoming rocket launches sourced from the
RocketLaunch.Live free API. The page follows the Transit departure-board
pattern with a hero row featuring a live T-minus countdown, three compact
rows for subsequent launches, a QR code linking to the hero launch, and a
tap-to-detail overlay modal.

## Data Source

**Endpoint:** `GET https://fdo.rocketlaunch.live/json/launches/next/5`

- No API key required (free tier, public).
- Returns JSON array of the next 5 launches.
- Fields used: `id`, `name`, `provider.name`, `provider.slug`,
  `vehicle.name`, `pad.name`, `location.name`, `location.country`,
  `t0` (ISO 8601 UTC), `win_open`, `win_close`, `est_date`, `date_str`,
  `missions[].description`, `launch_description`, `tags[].text`,
  `weather_summary`, `weather_temp`, `weather_condition`,
  `weather_icon`, `result`, `sort_date`.
- Rate limit: not documented; poll conservatively.

## Layout (640x172 canvas)

```
|<---------- 556px ---------->|<- 84px ->|
+-----------------------------+----------+
| HEADER (30px)               |          |
| "LAUNCHES"        last-fetch|          |
+-----------------------------+   QR     |
| PROGRESS BAR (3px)          |  80x80   |
+-----------------------------+          |
| HERO ROW (~48px)            |          |
| [Provider]  Mission Name    |          |
| Vehicle . Pad      T-HH:MM:SS         |
+-----------------------------+          |
| ROW 2 (30px)                |          |
| [Prov] Mission   Apr 12 08:30         |
+-----------------------------+          |
| ROW 3 (30px)                |          |
+-----------------------------+          |
| ROW 4 (30px)                |          |
+-----------------------------+----------+
```

### Content area (556px wide)

**Header (30px):**
- Title label "LAUNCHES" left-aligned.
- Last-fetch timestamp right-aligned (e.g. "14:32").
- Uses `activeUiTheme().lvgl.headerBg` / `.headerText`.

**Progress bar (3px):**
- `lv_bar` spanning 556px, sits directly below header.
- Fills over the last 24 h before hero T-0.
- Color follows escalation tiers (see below).

**Hero row (~48px, 2 display rows):**
- Provider badge: pill background (`lv_obj` with rounded corners),
  colored per provider (SpaceX blue, RocketLab dark, ULA navy, etc.),
  or fallback to theme accent.
- Line 1: mission name (scrolling `LV_LABEL_LONG_SCROLL_CIRCULAR` if
  too long).
- Line 2: vehicle name, dot separator, pad name, right-aligned
  countdown `T-HH:MM:SS`.
- Countdown updates every 1 s via `millis()` delta against cached
  `t0_epoch` (no network call).

**Compact rows (30px each, launches 2-4):**
- Smaller provider badge.
- Mission name (truncate with `LV_LABEL_LONG_DOT`).
- Right-aligned: static date/time string from `date_str` + `t0` hour
  (e.g. "Apr 12 08:30") or "TBD" if `t0` is null.
- Alternate row tinting for readability (Transit pattern).

### QR column (84px wide)

- 80x80 QR code vertically centered in the 172px height.
- Encodes URL: `https://rocketlaunch.live/launch/<provider.slug>-<vehicle.slug>-...`
  (derived from launch `slug` or fallback to provider+vehicle slug).
- Regenerated when hero launch changes.

## Countdown Escalation

| Condition        | Text color       | Badge        | Progress bar  |
|------------------|------------------|--------------|---------------|
| T > 24 h         | theme default    | —            | empty         |
| T < 24 h         | theme default    | —            | filling       |
| T < 1 h          | yellow (#FFD600) | "SOON" pulse | yellow        |
| T < 10 min       | red (#FF1744)    | "IMMINENT" blink | red       |
| T <= 0           | white flash      | "LIFTOFF"    | full, green   |

- "SOON" pulsing: `lv_anim` opacity 100-255 cycle, 800 ms period.
- "IMMINENT" blink: `lv_anim` opacity 0-255 cycle, 400 ms period.
- "LIFTOFF" flash: header bg flashes white for 1 s, then reverts.

## Tap-to-Detail Overlay

Tapping any launch row opens a full-canvas (640x172) modal overlay:

```
+------------------------------------------------+
| X  MISSION NAME                    T-HH:MM:SS  |
|                                                 |
| Provider . Vehicle                              |
| Pad, Location, Country                          |
| Window: HH:MM - HH:MM UTC  .  Condition  Temp  |
| launch_description (scrollable)                 |
|                                     [QR] [CLOSE]|
+------------------------------------------------+
```

- Close via X button (top-left) or tap outside.
- QR in detail links to the same launch URL.
- `launch_description` uses `LV_LABEL_LONG_WRAP` with vertical scroll
  if text exceeds available height.
- Weather fields: `weather_condition`, `weather_temp` (Fahrenheit from
  API, display as-is or convert based on locale setting if available).
- Tags shown as small pills below description (e.g. "ISS Cargo",
  "B1094").

## Data Structures

```cpp
#define LAUNCH_MAX_ITEMS       4
#define LAUNCH_NAME_LEN        80
#define LAUNCH_PROVIDER_LEN    32
#define LAUNCH_VEHICLE_LEN     40
#define LAUNCH_PAD_LEN         32
#define LAUNCH_LOCATION_LEN    48
#define LAUNCH_DESC_LEN        256
#define LAUNCH_SLUG_LEN        96
#define LAUNCH_WEATHER_LEN     32
#define LAUNCH_TAG_LEN         24
#define LAUNCH_MAX_TAGS        4
#define LAUNCH_REFRESH_MS      300000UL   // 5 min
#define LAUNCH_RETRY_MS        30000UL    // 30 s on error
#define LAUNCH_HTTP_TIMEOUT_MS 8000
```

```cpp
struct LaunchItem {
    char name[LAUNCH_NAME_LEN];
    char provider[LAUNCH_PROVIDER_LEN];
    char providerSlug[LAUNCH_SLUG_LEN];
    char vehicle[LAUNCH_VEHICLE_LEN];
    char pad[LAUNCH_PAD_LEN];
    char location[LAUNCH_LOCATION_LEN];
    char country[LAUNCH_PROVIDER_LEN];
    char description[LAUNCH_DESC_LEN];
    char weatherCondition[LAUNCH_WEATHER_LEN];
    char weatherTemp[8];
    char tags[LAUNCH_MAX_TAGS][LAUNCH_TAG_LEN];
    uint8_t tagCount;
    time_t t0Epoch;           // 0 if TBD
    time_t winOpen, winClose; // 0 if absent
    int8_t result;            // -1 pending, 0 unknown, 1 success, 2 failure
    bool hasT0;
};

struct LaunchState {
    LaunchItem items[LAUNCH_MAX_ITEMS];
    uint8_t count;
    bool valid, dirty;
    uint32_t lastFetchMs, lastHttpCode;
};

struct LaunchUi {
    lv_obj_t *root;
    // Header
    lv_obj_t *header, *headerFill, *title, *fetchTime, *status;
    // Progress bar
    lv_obj_t *progressBar;
    // Hero row
    lv_obj_t *heroBg, *heroBadge, *heroBadgeLabel;
    lv_obj_t *heroName, *heroVehiclePad, *heroCountdown;
    // Compact rows [3]
    lv_obj_t *rowBg[3], *rowBadge[3], *rowBadgeLabel[3];
    lv_obj_t *rowName[3], *rowDate[3];
    // QR
    lv_obj_t *qrCode;
    // Detail overlay
    lv_obj_t *detailOverlay;
    lv_obj_t *detailTitle, *detailCountdown, *detailClose;
    lv_obj_t *detailProvider, *detailPadLocation;
    lv_obj_t *detailWindow, *detailWeather;
    lv_obj_t *detailDesc;
    lv_obj_t *detailTags[LAUNCH_MAX_TAGS];
    lv_obj_t *detailQr;
    int8_t detailIndex;       // -1 = closed
};
```

## Page Registration

1. Add `UI_PAGE_LAUNCH = 7` to `UiPageMode` enum.
2. Add `UI_VIEW_FLAG_LAUNCH = 0x80` flag.
3. Add to `UI_VIEW_FLAGS_ALL` bitmask.
4. Always enabled when `TEST_WIFI` is set (no user config needed).
5. Add `g_lvglLaunchRoot` to carousel in `lvglApplyPageVisibility()`.
6. Add to ordinal/name helpers.

## Polling & Fetch Logic

- `updateLaunchFromApi(bool force)` — called from main loop.
- Polls every `LAUNCH_REFRESH_MS` (5 min) when page is visible.
- On error, retries every `LAUNCH_RETRY_MS` (30 s).
- HTTP timeout: `LAUNCH_HTTP_TIMEOUT_MS` (8 s).
- User-Agent: `ScryBar/<FW_BUILD_TAG>`.
- Parse JSON response: iterate `result[]` array, fill up to
  `LAUNCH_MAX_ITEMS` items, skip launches with `result == 1`
  (already launched successfully).
- Convert `t0` ISO 8601 to `time_t` epoch for countdown math.

## Countdown Timer

- `lvglTickLaunchCountdown()` — called every 1 s from main loop
  (only when LAUNCH page is visible).
- Computes `delta = hero.t0Epoch - now()`.
- Formats as `T-HH:MM:SS` (or `T-DDd HH:MM` if > 24 h).
- Updates `heroCountdown` label text and color per escalation table.
- Updates progress bar value.
- Also updates detail overlay countdown if open.

## Theming

Uses standard theme tokens from `activeUiTheme().lvgl`:
- `.panelBg`, `.headerBg`, `.headerText`, `.infoText`, `.auxMeta`,
  `.divider`.
- Provider badge colors: hardcoded map for known providers (SpaceX,
  RocketLab, ULA, ISRO, Arianespace, CASC, Roscosmos), fallback to
  theme accent.

## Future Work (out of scope)

- **Page reordering via drag-and-drop in web config UI.** Store custom
  page order in NVS, update carousel positioning logic. Separate spec.
- **Provider logo favicons** on badges (requires bitmap assets).
- **Push notification** on T-10 min (companion app integration).

## Config Constants (config.h)

```cpp
// --- Launch page --------------------------------------------------------
#define LAUNCH_MAX_ITEMS        4
#define LAUNCH_REFRESH_MS       300000UL
#define LAUNCH_RETRY_MS         30000UL
#define LAUNCH_HTTP_TIMEOUT_MS  8000
#define LAUNCH_NAME_LEN         80
#define LAUNCH_PROVIDER_LEN     32
#define LAUNCH_VEHICLE_LEN      40
#define LAUNCH_PAD_LEN          32
#define LAUNCH_LOCATION_LEN     48
#define LAUNCH_DESC_LEN         256
#define LAUNCH_SLUG_LEN         96
#define LAUNCH_WEATHER_LEN      32
#define LAUNCH_TAG_LEN          24
#define LAUNCH_MAX_TAGS         4
```
