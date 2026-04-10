# LAUNCH Hero — "Cinematic Asymmetric" Redesign

**Status:** Design spec
**Date:** 2026-04-10
**Scope:** LAUNCH page **View 0 (hero)** only. View 1 (compact, rows 2-3) is out of scope for this spec — will be addressed in a follow-up.
**Previous work:** r255 fixed badge/row/font bugs from the visual-polish pass. This spec replaces the hero layout entirely.

---

## Goal

Rebuild the hero as a two-column asymmetric layout where the **countdown dominates** (cinematic, readable in a glance) and the **mission info** sits quietly to its left. The reference is the "Cinematic Asymmetric" mockup option C proposed during the r253 session and approved on 2026-04-10.

Success = when approaching a mission T-0 you can read the countdown from across the room, while still having all the context (provider, mission, vehicle, pad, location, weather) one glance away.

---

## Visual Design

### Canvas & grid

```
Canvas:      640 × 172
Header:      y=0..30   (existing — not touched)
Body:        y=30..172 (142px tall)
Body veil:   alt-tint ~8% (white on dark themes, black on light) — unchanged from r255
Gutter:      16px empty vertical strip centered horizontally between columns
Left col:    x=0..312
Right col:   x=328..640
Left col padding:  x=16 (inset from screen edge)
```

### Column 1 — Mission info (left, x=0..312)

All coordinates are in **canvas space** (y measured from top of display).

| # | Field           | Pos          | Size         | Font               | Color          | Notes |
|---|-----------------|--------------|--------------|--------------------|----------------|-------|
| 1 | Provider badge  | x=16, y=38   | auto × 30    | `lvglFontSmall()` 18 | white on provider color | pill radius 8, `bg_opa=COVER`, label padding 14px horizontal, auto width (scroll-clip if > 280) |
| 2 | Mission name    | x=16, y=72   | 296 × 32     | **NEW** `lvglFontLaunchName()` 25 (Regular, alias for `scry_font_funnel_display_25` — or reuse existing `lvglNowPlayingTitleFont()` which already points to the same file) | `t.infoText` | `LV_LABEL_LONG_DOT` for ellipsis |
| 3 | Vehicle \| Pad  | x=16, y=106  | 296 × 18     | `lvglFontMini()` 16 | `t.auxMeta`    | separator = ASCII `\|` (space-pipe-space). Never middle-dot. |
| 4 | Location        | x=16, y=126  | 296 × 22     | `lvglFontMeta()` 20 | `t.infoText`   | launch pad location (e.g. "Andøya Space") |
| 5 | Country         | x=16, y=150  | 296 × 16     | `lvglFontTiny()` 14 | `t.auxMeta`    | country name (bottom edge y=166, **6px clear of body bottom**) |

Vertical gap budget left column: 8 above badge, 4 between badge/mission, 2 between mission/vehicle, 2 between vehicle/location, 2 between location/country, 6 below country. Tight but breathable.

**Badge width:** auto-sized to text + 28px horizontal padding (14 left + 14 right), min 82, max 280. Provider color from `launchProviderColor()` (unchanged from r255). Radius 8 (slightly more rounded than r255's 6).

### Column 2 — Countdown (right, x=328..640)

| # | Field           | Pos          | Size         | Font               | Color         | Notes |
|---|-----------------|--------------|--------------|--------------------|---------------|-------|
| 1 | LIFTOFF IN label | x=328, y=42 | 312 × 16     | `lvglFontTiny()` 14 | `t.auxMeta`   | uppercase, `letter-spacing` via `lv_obj_set_style_text_letter_space` = 3, centered |
| 2 | Countdown value  | x=328, y=62 | 312 × 68     | `lvglFontCountdown()` **NEW 60** | `t.infoText` | tabular-nums-ish via `letter_space = -2`, centered, no "T-" prefix |
| 3 | Weather          | x=328, y=124 | 312 × 20    | `lvglFontMini()` 16 | `lvglLaunchWeatherAccent()` **NEW helper** | centered, format `<glyph> <condition> <temp>` |
| 4 | QR tap hint      | x=328, y=150 | 312 × 16    | `lvglFontMicro()` / `scry_font_funnel_display_12` | `t.auxMeta`, `LV_OPA_70` | uppercase, `letter_space = 2`, text `"tap · scan qr"` or `"TAP FOR QR"`, centered. **Bottom edge y=166, aligned with country bottom.** |

**QR tap behaviour (already implemented, unchanged):** in View 0, tap anywhere below the header (y ≥ 33, both columns) calls `lvglOpenLaunchQr(0)` which opens the full-screen QR overlay. The hint label is a purely visual affordance — no new touch handler needed. See `scrybar.ino:11812-11815`.

**Countdown value format:**
- When `d > 0`: `"%dd %02d:%02d"` → e.g. `"2d 18:42"` (8 chars)
- When `d == 0`: `"%02d:%02d:%02d"` → e.g. `"05:10:54"` (8 chars)
- **No `T-` prefix in the value.** The "LIFTOFF IN" label carries that semantics.

**Countdown states** (affects label + countdown color, not layout):

| State        | Label text     | Countdown text               | Color        | Trigger |
|--------------|---------------|-------------------------------|--------------|---------|
| Counting     | `LIFTOFF IN`  | `HH:MM:SS` / `Dd HH:MM`      | `t.infoText` | `t_until > 0` and no override |
| Imminent     | `LIFTOFF IN`  | same, countdown red tint     | `0xFF5252`   | `t_until ≤ 60s` and > 0 |
| Live / Go    | `LIFTOFF`     | `+HH:MM:SS` (elapsed)        | `0x22AA33`   | `t_until ≤ 0` and not scrubbed, within 30 min |
| Scrubbed    | `SCRUBBED`    | `—:—:—`                       | `t.auxMeta`  | item status indicates scrub |
| NET window  | `NET`         | date short (`Apr 15`)        | `t.auxMeta`  | no precise time, only window |
| Success     | `LAUNCHED`    | `+Dd HH:MM`                  | `t.auxMeta`  | > 30 min after liftoff |

For r256 first pass, implement **Counting + Imminent + NET** (the states we can detect from current `g_launchState` data). Scrubbed/Live/Success require data we don't yet fetch — flagged as future work.

---

## Font strategy

Two-phase font work.

### Phase 1 — New countdown font

Generate a **new Funnel Display font at 60px**, minimal glyph set:

```
Glyphs: 0 1 2 3 4 5 6 7 8 9 : d space + -
(15 glyphs total)
```

- Source: `assets/fonts/FunnelDisplay-Regular.ttf`
- Output: `src/fonts/scry_font_funnel_display_countdown_60.c`
- Command (template, to be refined in implementation plan):
  ```
  lv_font_conv --size 60 --bpp 4 --no-compress --format lvgl \
      --font assets/fonts/FunnelDisplay-Regular.ttf \
      -r 0x20,0x2B,0x2D,0x30-0x39,0x3A,0x64 \
      --lv-font-name scry_font_funnel_display_countdown_60 \
      -o src/fonts/scry_font_funnel_display_countdown_60.c
  ```
- Estimated file size: ~2-4 KB
- Expose via `lvglFontCountdown()` accessor in `scrybar.ino`

### Phase 2 — Charset extension for location/country fonts

Regenerate existing Funnel Display fonts used by location/country fields (and by extension anywhere text could include international place names) with **Latin-1 Supplement + Latin Extended-A**:

Fonts to regenerate:
- `scry_font_funnel_display_14` (country)
- `scry_font_funnel_display_16` (vehicle | pad, weather)
- `scry_font_funnel_display_18` (header + badge)
- `scry_font_funnel_display_20` (location)
- `scry_font_funnel_display_25` (mission name)

Glyph ranges:
```
0x0020-0x007E  Basic Latin (ASCII)              [existing]
0x00A0-0x00FF  Latin-1 Supplement               [NEW — ø, ñ, ç, ü, é, ö, ß, ...]
0x0100-0x017F  Latin Extended-A                 [NEW — ı, ğ, ş, č, ž, ł, ...]
```

- Estimated size increase per font: +8-15 KB
- Total flash cost Phase 2: ~40-75 KB
- **Cyrillic, Greek, CJK, Arabic: explicitly out of scope.** Place names in those scripts will continue to arrive romanized from `rocketlaunch.live` and render correctly (Byakonur, Tanegashima, Jiuquan).

### Tofu fallback

For any character outside the generated charset, the existing LVGL fallback to `lv_font_montserrat_24` applies. If a launch location hits a glyph gap, we get Montserrat's rendering instead of a tofu box (worst case). This matches current behavior.

---

## Weather accent helper

Introduce `lvglLaunchWeatherAccent(const UiThemeLvglTokens &t)` that returns a theme-dependent accent color for the weather readout. Default implementation:

```c
static inline uint32_t lvglLaunchWeatherAccent(const UiThemeLvglTokens &t) {
  // Warm accent for the cinematic hero. Theme-aware so cold themes don't clash.
  // Default: t.weatherGlyphSunny if defined, else hardcoded amber.
  return t.weatherGlyphSunny ? t.weatherGlyphSunny : 0xFFB74D;
}
```

Rationale: weather is the only non-critical hint on the hero. A warm amber reads as "ambient/friendly" without competing with the countdown.

---

## LVGL widget mapping

Existing `LvglLaunchUi` struct fields to keep (rename/repurpose where needed):

| Current field              | New role                                    | Action |
|----------------------------|---------------------------------------------|--------|
| `heroBg`                   | body container, full-width veil             | keep |
| `heroBadge` + `heroBadgeLabel` | provider badge                         | keep, retune size to auto-fit |
| `heroName`                 | mission name (25px, no quotes)              | keep, change font + pos |
| `heroVehiclePad`           | vehicle \| pad                              | keep, change pos |
| `heroLocation`             | launch site name                            | keep, change pos + font |
| `heroCountry`              | country                                     | keep, change pos + font |
| `heroCountdown`            | countdown value (new 60px font)             | keep, change font + pos + format |
| `heroWeather`              | weather (new accent color, centered)        | keep, change pos + align |
| **NEW** `heroCountdownLabel` | "LIFTOFF IN" label above countdown        | **add** |
| **NEW** `heroQrHint`       | "tap · scan qr" affordance below weather   | **add** |
| `heroWindow`               | (no longer used in hero)                   | **remove or hide** |

The existing separator line at y=48 is **removed** — the gutter does the job.

---

## Data flow

No changes to data layer. `lvglUpdateLaunchUi()` continues to read from `g_launchState.items[0]` and populates the same semantic fields. Only the label text + color + position assignments change. The `lvglTickLaunchCountdown()` tick handler updates `heroCountdown` text format (strip "T-") and `heroCountdownLabel` text based on state.

---

## Out of scope

- **View 1 (compact rows)** — explicitly deferred. The spec author (user) will brainstorm View 1 separately.
- **Detail overlay (QR lightbox)** — already reworked in r255, not re-touched here.
- **Theme-specific tweaks** — the design uses standard theme tokens; cyberpunk / toxic-candy variations will inherit automatically.
- **Bold font variant** — only Funnel Display Regular is in the repo. Visual hierarchy relies on size, not weight. Adding a Bold TTF is a future decision.
- **Cyrillic / CJK glyph coverage** — place names in non-Latin scripts continue to arrive romanized from the API.
- **New weather glyphs** — reuse existing weather glyph system.

---

## Success criteria

1. Screenshot of View 0 on device matches the mockup at `/tmp/launch_mockup/index.html` within ±2px positioning tolerance.
2. "Andøya Space" renders natively (no tofu, no fallback to Montserrat) at 20px.
3. Countdown is visually dominant — subjective test: "can I read it from 2m away faster than anything else on screen?"
4. Long mission names (≥18 chars) truncate cleanly with ellipsis at 25px, no overflow into countdown column.
5. All 7 themes render without color contrast failures on the hero.
6. Total flash size increase < 100 KB.

---

## Open questions (to confirm during review)

1. Label wording: `LIFTOFF IN` for all counting states, or switch to `T-MINUS` when < 1h? **Default: always `LIFTOFF IN`** for consistency.
2. Weather color: warm amber `0xFFB74D`, or match `t.infoText` (white)? **Default: amber.**
3. Badge radius: 6 (current) or 8 (mockup)? **Default: 8.**
4. Mission name: keep quotes from the mockup (`"Onward and Upward"`) or drop them? **Default: drop** — they steal width for long names.

These will be resolved on implementation; none block the plan.
