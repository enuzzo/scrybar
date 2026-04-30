# LAUNCH Hero — Cinematic Asymmetric — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the LAUNCH page View 0 (hero) with a two-column "Cinematic Asymmetric" layout: mission info left, dominant 60px countdown right. Add a new big countdown font and extend existing fonts with Latin-1 Supplement + Latin Extended-A so international place names like "Andøya" render natively.

**Architecture:** All changes live in `scrybar.ino` (single-file Arduino project) plus new/regenerated files under `src/fonts/`. The View 1 (compact rows) code stays untouched. No data-layer changes — same `g_launchState.items[0]` fields, only rendering is reshaped. Font regeneration uses the existing `lv_font_conv` toolchain (already installed at `/opt/homebrew/bin/lv_font_conv`).

**Tech Stack:** LVGL 8.x, ESP32-S3, arduino-cli, lv_font_conv (npm), Funnel Display Regular TTF.

**Spec reference:** `docs/superpowers/specs/2026-04-10-launch-hero-cinematic-redesign.md`

**Verification model:** No unit tests — this is embedded LVGL UI on physical hardware. Each UI-touching task ends with a device screenshot compared against the mockup at `/tmp/launch_mockup/index.html` (served on port 9091). Visual diffs must match within ±2px.

---

## File Structure

| Path | Role | Action |
|------|------|--------|
| `src/fonts/scry_font_funnel_display_countdown_60.c` | NEW — 60px digits/colon/d/space countdown font | create |
| `src/fonts/scry_font_funnel_display_14.c` | existing | regenerate (+Latin Extended) |
| `src/fonts/scry_font_funnel_display_16.c` | existing | regenerate (+Latin Extended) |
| `src/fonts/scry_font_funnel_display_18.c` | existing | regenerate (+Latin Extended) |
| `src/fonts/scry_font_funnel_display_20.c` | existing | regenerate (+Latin Extended) |
| `src/fonts/scry_font_funnel_display_25.c` | existing | regenerate (+Latin Extended) |
| `scrybar.ino` | firmware (single file, ~17K lines) | modify: font DECLAREs, accessors, LvglLaunchUi struct, lvglInitLaunchUi hero portion, lvglUpdateLaunchUi, lvglTickLaunchCountdown |
| `config.h` | build tag | bump `FW_BUILD_TAG` to `r256`, `FW_RELEASE_DATE` to today |

---

## Task 1: Generate new 60px countdown font

**Files:**
- Create: `src/fonts/scry_font_funnel_display_countdown_60.c`
- Modify: `scrybar.ino` (add `LV_FONT_DECLARE` + `lvglFontCountdown()` accessor)

- [ ] **Step 1: Verify `lv_font_conv` is installed**

Run:
```bash
which lv_font_conv
```

Expected output: `/opt/homebrew/bin/lv_font_conv`

If missing: `npm install -g lv_font_conv` and re-run.

- [ ] **Step 2: Generate the countdown font file**

Run from the project root:
```bash
lv_font_conv \
  --size 60 \
  --bpp 4 \
  --no-compress \
  --format lvgl \
  --lv-include lvgl.h \
  --font assets/fonts/FunnelDisplay-Regular.ttf \
  -r 0x20 -r 0x2B -r 0x2D -r 0x30-0x39 -r 0x3A -r 0x64 \
  --lv-fallback lv_font_montserrat_24 \
  --lv-font-name scry_font_funnel_display_countdown_60 \
  -o src/fonts/scry_font_funnel_display_countdown_60.c
```

Glyph set breakdown:
- `0x20` space
- `0x2B` `+`
- `0x2D` `-`
- `0x30-0x39` `0-9`
- `0x3A` `:`
- `0x64` `d` (for "2d 18:42" day format)

Expected result: file created at `src/fonts/scry_font_funnel_display_countdown_60.c`.

- [ ] **Step 3: Verify file size is reasonable**

Run:
```bash
ls -l src/fonts/scry_font_funnel_display_countdown_60.c
```

Expected: 15-40 KB (15 glyphs × 60px × 4bpp + lookup tables).

- [ ] **Step 4: Add `LV_FONT_DECLARE` line**

File: `scrybar.ino`, after line 91 (where `scry_font_funnel_display_38` is declared).

Change:
```c
LV_FONT_DECLARE(scry_font_funnel_display_38);
```

To:
```c
LV_FONT_DECLARE(scry_font_funnel_display_38);
LV_FONT_DECLARE(scry_font_funnel_display_countdown_60);
```

- [ ] **Step 5: Add `lvglFontCountdown()` accessor**

File: `scrybar.ino`, find the line (around 13046):
```c
static const lv_font_t* lvglFontBig()      { return &scry_font_funnel_display_32; }
```

Add immediately after it:
```c
static const lv_font_t* lvglFontCountdown(){ return &scry_font_funnel_display_countdown_60; }
```

- [ ] **Step 6: Compile to verify the font file is valid and linked**

Run:
```bash
arduino-cli compile --clean \
  --build-path /tmp/arduino-build-scrybar \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  <REPO_ROOT>
```

Expected: compile succeeds with no errors referencing the new font.

If you get `undefined reference to scry_font_funnel_display_countdown_60`, the `.c` file isn't being picked up — verify it's in `src/fonts/` and the build includes that directory.

- [ ] **Step 7: Commit**

```bash
git add src/fonts/scry_font_funnel_display_countdown_60.c scrybar.ino
git commit -m "$(cat <<'EOF'
feat(fonts): add 60px countdown font (digits + colon + d)

Minimal glyph set (15 glyphs) so the file stays small (~20-30 KB). Will drive the new dominant countdown in the LAUNCH hero redesign. No fallback needed for this narrow charset since it's only used for pre-formatted countdown strings.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Regenerate fonts 12/14/16/18/20/25 with Latin Extended charset

**Files:**
- Modify: `src/fonts/scry_font_funnel_display_12.c`
- Modify: `src/fonts/scry_font_funnel_display_14.c`
- Modify: `src/fonts/scry_font_funnel_display_16.c`
- Modify: `src/fonts/scry_font_funnel_display_18.c`
- Modify: `src/fonts/scry_font_funnel_display_20.c`
- Modify: `src/fonts/scry_font_funnel_display_25.c`

Glyph ranges added to each:
- `0x0020-0x007E` Basic Latin (already present)
- `0x00A0-0x00FF` Latin-1 Supplement (NEW — ø, ñ, ç, ü, é, ö, ß, and friends)
- `0x0100-0x017F` Latin Extended-A (NEW — ı, ğ, ş, č, ž, ł, and friends)

- [ ] **Step 1a: Regenerate font 12**

Run from project root:
```bash
lv_font_conv \
  --size 12 \
  --bpp 4 \
  --no-compress \
  --format lvgl \
  --lv-include lvgl.h \
  --font assets/fonts/FunnelDisplay-Regular.ttf \
  -r 0x20-0x7E -r 0xA0-0xFF -r 0x100-0x17F \
  --lv-fallback lv_font_montserrat_24 \
  --lv-font-name scry_font_funnel_display_12 \
  -o src/fonts/scry_font_funnel_display_12.c
```

- [ ] **Step 1: Regenerate font 14**

Run from project root:
```bash
lv_font_conv \
  --size 14 \
  --bpp 4 \
  --no-compress \
  --format lvgl \
  --lv-include lvgl.h \
  --font assets/fonts/FunnelDisplay-Regular.ttf \
  -r 0x20-0x7E -r 0xA0-0xFF -r 0x100-0x17F \
  --lv-fallback lv_font_montserrat_24 \
  --lv-font-name scry_font_funnel_display_14 \
  -o src/fonts/scry_font_funnel_display_14.c
```

Expected: file overwritten, no errors on stderr.

- [ ] **Step 2: Regenerate font 16**

Run:
```bash
lv_font_conv \
  --size 16 \
  --bpp 4 \
  --no-compress \
  --format lvgl \
  --lv-include lvgl.h \
  --font assets/fonts/FunnelDisplay-Regular.ttf \
  -r 0x20-0x7E -r 0xA0-0xFF -r 0x100-0x17F \
  --lv-fallback lv_font_montserrat_24 \
  --lv-font-name scry_font_funnel_display_16 \
  -o src/fonts/scry_font_funnel_display_16.c
```

- [ ] **Step 3: Regenerate font 18**

Run:
```bash
lv_font_conv \
  --size 18 \
  --bpp 4 \
  --no-compress \
  --format lvgl \
  --lv-include lvgl.h \
  --font assets/fonts/FunnelDisplay-Regular.ttf \
  -r 0x20-0x7E -r 0xA0-0xFF -r 0x100-0x17F \
  --lv-fallback lv_font_montserrat_24 \
  --lv-font-name scry_font_funnel_display_18 \
  -o src/fonts/scry_font_funnel_display_18.c
```

- [ ] **Step 4: Regenerate font 20**

Run:
```bash
lv_font_conv \
  --size 20 \
  --bpp 4 \
  --no-compress \
  --format lvgl \
  --lv-include lvgl.h \
  --font assets/fonts/FunnelDisplay-Regular.ttf \
  -r 0x20-0x7E -r 0xA0-0xFF -r 0x100-0x17F \
  --lv-fallback lv_font_montserrat_24 \
  --lv-font-name scry_font_funnel_display_20 \
  -o src/fonts/scry_font_funnel_display_20.c
```

- [ ] **Step 5: Regenerate font 25**

Run:
```bash
lv_font_conv \
  --size 25 \
  --bpp 4 \
  --no-compress \
  --format lvgl \
  --lv-include lvgl.h \
  --font assets/fonts/FunnelDisplay-Regular.ttf \
  -r 0x20-0x7E -r 0xA0-0xFF -r 0x100-0x17F \
  --lv-fallback lv_font_montserrat_24 \
  --lv-font-name scry_font_funnel_display_25 \
  -o src/fonts/scry_font_funnel_display_25.c
```

- [ ] **Step 6: Verify file sizes grew (sanity check)**

Run:
```bash
wc -l src/fonts/scry_font_funnel_display_12.c src/fonts/scry_font_funnel_display_14.c src/fonts/scry_font_funnel_display_16.c src/fonts/scry_font_funnel_display_18.c src/fonts/scry_font_funnel_display_20.c src/fonts/scry_font_funnel_display_25.c
```

Expected: each file's line count ~3x larger than before (Latin Extended adds ~300 glyphs vs original ~95). If any file is the same size, the `-r` range didn't take — re-run.

- [ ] **Step 7: Compile to verify all fonts link**

Run:
```bash
arduino-cli compile --clean \
  --build-path /tmp/arduino-build-scrybar \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  <REPO_ROOT>
```

Expected: compile succeeds. The output should show increased flash usage vs Task 1 baseline (noting ~40-75 KB added).

If a file is missing the fallback include or has syntax issues, compilation will fail on that specific font — regenerate that single file.

- [ ] **Step 8: Commit**

```bash
git add src/fonts/scry_font_funnel_display_12.c src/fonts/scry_font_funnel_display_14.c src/fonts/scry_font_funnel_display_16.c src/fonts/scry_font_funnel_display_18.c src/fonts/scry_font_funnel_display_20.c src/fonts/scry_font_funnel_display_25.c
git commit -m "$(cat <<'EOF'
feat(fonts): extend 12/14/16/18/20/25 with Latin-1 Supplement + Latin Extended-A

Adds coverage for ø, ñ, ç, ü, é, ö, ß, ı, ğ, ş, č, ž, ł and friends so international place names from rocketlaunch.live render natively instead of falling back to Montserrat. Enables "Andøya", "São Paulo", "Kourou", "München" on the LAUNCH hero.

Cyrillic, Greek, CJK, Arabic explicitly out of scope — those names already arrive romanized from the API.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Extend `LvglLaunchUi` struct + add weather accent helper

**Files:**
- Modify: `scrybar.ino` (struct `LvglLaunchUi` ~line 1133; helper near `lvglResolvedHeaderBg` ~line 9790)

- [ ] **Step 1: Add new fields to `LvglLaunchUi`**

File: `scrybar.ino`, find (around line 1144):
```c
  lv_obj_t *heroBadge = nullptr;
  lv_obj_t *heroBadgeLabel = nullptr;
  lv_obj_t *heroName = nullptr;
  lv_obj_t *heroVehiclePad = nullptr;
  lv_obj_t *heroCountdown = nullptr;
```

Replace with:
```c
  lv_obj_t *heroBadge = nullptr;
  lv_obj_t *heroBadgeLabel = nullptr;
  lv_obj_t *heroName = nullptr;
  lv_obj_t *heroVehiclePad = nullptr;
  lv_obj_t *heroCountdownLabel = nullptr;  // "LIFTOFF IN" label above countdown
  lv_obj_t *heroCountdown = nullptr;
  lv_obj_t *heroQrHint = nullptr;          // "tap · scan qr" affordance under weather
```

- [ ] **Step 2: Add `lvglLaunchWeatherAccent()` helper**

File: `scrybar.ino`, find (around line 9879-9883):
```c
    return t.weatherGlyphOffline;
  }
  return weatherSecondary;
}
```

Add immediately after that closing brace, before `// ── LVGL style helpers (M9) ───...`:
```c

// Warm accent for the LAUNCH hero weather readout. Theme-aware: if the theme
// defines a sunny glyph color, use it; otherwise default to amber. The weather
// is a low-priority ambient hint — warm reads as "friendly" without competing
// with the countdown.
static inline uint32_t lvglLaunchWeatherAccent(const UiThemeLvglTokens &t) {
  return t.weatherGlyphSunny ? t.weatherGlyphSunny : 0xFFB74D;
}
```

- [ ] **Step 3: Compile to verify struct + helper are valid**

Run:
```bash
arduino-cli compile --clean \
  --build-path /tmp/arduino-build-scrybar \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  <REPO_ROOT>
```

Expected: compile succeeds. The new struct fields are unused so far (warnings are fine, errors are not).

- [ ] **Step 4: Commit**

```bash
git add scrybar.ino
git commit -m "$(cat <<'EOF'
feat(launch): struct fields + weather accent helper for hero redesign

Adds heroCountdownLabel and heroQrHint to LvglLaunchUi plus lvglLaunchWeatherAccent() helper. No behavior change yet — consumed by the hero rewrite in the next commit.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Rewrite `lvglInitLaunchUi()` hero portion

**Files:**
- Modify: `scrybar.ino` hero init block (~lines 14035-14141, from `// ==== VIEW 0: Hero...` through the end of the hero section, just before `// ==== VIEW 1: Compact ====`)

This task rewrites only the hero init — View 1 (compact rows) and the QR overlay init stay untouched.

- [ ] **Step 1: Locate the current hero init block**

Run:
```bash
grep -n "VIEW 0: Hero" scrybar.ino
grep -n "VIEW 1: Compact" scrybar.ino
```

Expected output like:
```
14035:  // ==== VIEW 0: Hero — "Mission Control" layout ====
14143:  // ==== VIEW 1: Compact (2 rows, missions 2-3) ====
```

Note the exact line numbers — you'll replace the range between them (exclusive of the VIEW 1 header).

- [ ] **Step 2: Replace the hero init block**

File: `scrybar.ino`. Replace everything from the line `// ==== VIEW 0: Hero — "Mission Control" layout ====` through the blank line immediately before `// ==== VIEW 1: Compact (2 rows, missions 2-3) ====`.

New block:
```c
  // ==== VIEW 0: Hero — "Cinematic Asymmetric" layout (r256) ====
  // Two columns with a 16px gutter. Left col (x=0..312): badge / mission name /
  // vehicle|pad / location / country. Right col (x=328..640): LIFTOFF IN label
  // + dominant 60px countdown + weather + tap hint.
  g_launchUi.heroBg = lv_obj_create(g_lvglLaunchRoot);
  lv_obj_set_size(g_launchUi.heroBg, cW, bodyH);
  lv_obj_set_pos(g_launchUi.heroBg, 0, bodyY);
  lv_obj_set_style_border_width(g_launchUi.heroBg, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_launchUi.heroBg, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_launchUi.heroBg, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_launchUi.heroBg, lv_color_hex(altTint), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_launchUi.heroBg, 20, LV_PART_MAIN);  // ~8% veil
  lv_obj_clear_flag(g_launchUi.heroBg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(g_launchUi.heroBg, LV_OBJ_FLAG_CLICKABLE);

  // Column geometry. Coordinates are canvas-relative (add bodyY when positioning
  // children of heroBg). bodyY = 30, bodyH = 142.
  const int16_t leftX       = 16;
  const int16_t leftW       = 296;   // col width minus 2x inset
  const int16_t rightX      = 328;
  const int16_t rightW      = cW - rightX;  // 312

  // ── Left column ────────────────────────────────────────
  // 1) Provider badge — pill, auto width (set in lvglUpdateLaunchUi)
  const int16_t badgeH = 30;
  g_launchUi.heroBadge = lv_obj_create(g_launchUi.heroBg);
  lv_obj_set_size(g_launchUi.heroBadge, 180, badgeH);  // initial; updated per mission
  lv_obj_set_pos(g_launchUi.heroBadge, leftX, 8);       // body-y=8 → canvas-y=38
  lv_obj_set_style_radius(g_launchUi.heroBadge, 8, 0);
  lv_obj_set_style_border_width(g_launchUi.heroBadge, 0, LV_PART_MAIN);
  lvglSetBgFlat(g_launchUi.heroBadge, t.headerBg);
  lv_obj_set_style_bg_opa(g_launchUi.heroBadge, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_launchUi.heroBadge, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_launchUi.heroBadge, LV_OBJ_FLAG_SCROLLABLE);

  g_launchUi.heroBadgeLabel = lv_label_create(g_launchUi.heroBadge);
  lv_label_set_text(g_launchUi.heroBadgeLabel, "");
  lv_obj_set_style_text_font(g_launchUi.heroBadgeLabel, lvglFontSmall(), 0);  // 18px
  lvglSetTextHex(g_launchUi.heroBadgeLabel, 0xFFFFFF);
  lv_obj_set_style_text_align(g_launchUi.heroBadgeLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(g_launchUi.heroBadgeLabel, LV_ALIGN_CENTER, 0, 2);

  // 2) Mission name — 25px, LV_LABEL_LONG_DOT for ellipsis on long names
  g_launchUi.heroName = lv_label_create(g_launchUi.heroBg);
  lv_label_set_text(g_launchUi.heroName, "");
  lv_obj_set_style_text_font(g_launchUi.heroName, lvglNowPlayingTitleFont(), 0);  // 25px
  lvglSetTextHex(g_launchUi.heroName, t.infoText);
  lv_obj_set_pos(g_launchUi.heroName, leftX, 42);  // body-y=42 → canvas-y=72
  lv_obj_set_size(g_launchUi.heroName, leftW, 32);
  lv_label_set_long_mode(g_launchUi.heroName, LV_LABEL_LONG_DOT);

  // 3) Vehicle | Pad — 16px muted
  g_launchUi.heroVehiclePad = lv_label_create(g_launchUi.heroBg);
  lv_label_set_text(g_launchUi.heroVehiclePad, "");
  lv_obj_set_style_text_font(g_launchUi.heroVehiclePad, lvglFontMini(), 0);  // 16px
  lvglSetTextHex(g_launchUi.heroVehiclePad, t.auxMeta);
  lv_obj_set_pos(g_launchUi.heroVehiclePad, leftX, 76);  // body-y=76 → canvas-y=106
  lv_obj_set_size(g_launchUi.heroVehiclePad, leftW, 18);
  lv_label_set_long_mode(g_launchUi.heroVehiclePad, LV_LABEL_LONG_DOT);

  // 4) Location — 20px white
  g_launchUi.heroLocation = lv_label_create(g_launchUi.heroBg);
  lv_label_set_text(g_launchUi.heroLocation, "");
  lv_obj_set_style_text_font(g_launchUi.heroLocation, lvglFontMeta(), 0);  // 20px
  lvglSetTextHex(g_launchUi.heroLocation, t.infoText);
  lv_obj_set_pos(g_launchUi.heroLocation, leftX, 96);  // body-y=96 → canvas-y=126
  lv_obj_set_size(g_launchUi.heroLocation, leftW, 22);
  lv_label_set_long_mode(g_launchUi.heroLocation, LV_LABEL_LONG_DOT);

  // 5) Country — 14px muted, bottom edge y=166 (6px clear of body bottom)
  g_launchUi.heroCountry = lv_label_create(g_launchUi.heroBg);
  lv_label_set_text(g_launchUi.heroCountry, "");
  lv_obj_set_style_text_font(g_launchUi.heroCountry, lvglFontTiny(), 0);  // 14px
  lvglSetTextHex(g_launchUi.heroCountry, t.auxMeta);
  lv_obj_set_pos(g_launchUi.heroCountry, leftX, 120);  // body-y=120 → canvas-y=150
  lv_obj_set_size(g_launchUi.heroCountry, leftW, 16);
  lv_label_set_long_mode(g_launchUi.heroCountry, LV_LABEL_LONG_DOT);

  // ── Right column ────────────────────────────────────────
  // 1) "LIFTOFF IN" label — 14px caps, tracked, muted, centered
  g_launchUi.heroCountdownLabel = lv_label_create(g_launchUi.heroBg);
  lv_label_set_text(g_launchUi.heroCountdownLabel, "LIFTOFF IN");
  lv_obj_set_style_text_font(g_launchUi.heroCountdownLabel, lvglFontTiny(), 0);  // 14px
  lvglSetTextHex(g_launchUi.heroCountdownLabel, t.auxMeta);
  lv_obj_set_style_text_letter_space(g_launchUi.heroCountdownLabel, 3, 0);
  lv_obj_set_style_text_align(g_launchUi.heroCountdownLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(g_launchUi.heroCountdownLabel, rightX, 12);  // body-y=12 → canvas-y=42
  lv_obj_set_size(g_launchUi.heroCountdownLabel, rightW, 16);

  // 2) Countdown value — 60px bold-ish, centered, no "T-" prefix
  g_launchUi.heroCountdown = lv_label_create(g_launchUi.heroBg);
  lv_label_set_text(g_launchUi.heroCountdown, "--:--:--");
  lv_obj_set_style_text_font(g_launchUi.heroCountdown, lvglFontCountdown(), 0);  // 60px NEW
  lvglSetTextHex(g_launchUi.heroCountdown, t.infoText);
  lv_obj_set_style_text_letter_space(g_launchUi.heroCountdown, -2, 0);
  lv_obj_set_style_text_align(g_launchUi.heroCountdown, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(g_launchUi.heroCountdown, rightX, 32);  // body-y=32 → canvas-y=62
  lv_obj_set_size(g_launchUi.heroCountdown, rightW, 68);

  // 3) Weather — 16px warm accent, centered
  g_launchUi.heroWeather = lv_label_create(g_launchUi.heroBg);
  lv_label_set_text(g_launchUi.heroWeather, "");
  lv_obj_set_style_text_font(g_launchUi.heroWeather, lvglFontMini(), 0);  // 16px
  lvglSetTextHex(g_launchUi.heroWeather, lvglLaunchWeatherAccent(t));
  lv_obj_set_style_text_align(g_launchUi.heroWeather, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(g_launchUi.heroWeather, rightX, 94);  // body-y=94 → canvas-y=124
  lv_obj_set_size(g_launchUi.heroWeather, rightW, 20);

  // 4) QR tap hint — 12px muted 70%, caps, bottom-aligned with country
  g_launchUi.heroQrHint = lv_label_create(g_launchUi.heroBg);
  lv_label_set_text(g_launchUi.heroQrHint, "TAP \xC2\xB7 SCAN QR");  // middle dot U+00B7 now in charset
  lv_obj_set_style_text_font(g_launchUi.heroQrHint, &scry_font_funnel_display_12, 0);
  lvglSetTextHex(g_launchUi.heroQrHint, t.auxMeta);
  lv_obj_set_style_text_opa(g_launchUi.heroQrHint, LV_OPA_70, 0);
  lv_obj_set_style_text_letter_space(g_launchUi.heroQrHint, 2, 0);
  lv_obj_set_style_text_align(g_launchUi.heroQrHint, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(g_launchUi.heroQrHint, rightX, 120);  // body-y=120 → canvas-y=150
  lv_obj_set_size(g_launchUi.heroQrHint, rightW, 16);

  // Remove the old hero separator line — the gutter does the job in the new layout.
  // heroWindow field is left in the struct but unused in View 0; keep nullptr.
  g_launchUi.heroWindow = nullptr;

```

**Important:** the middle dot `·` (U+00B7, `\xC2\xB7` in UTF-8) lives in the Latin-1 Supplement range added in Task 2 (`0xA0-0xFF`), so it will render natively. If Task 2 wasn't done, this character will tofu — verify the Latin-1 regen actually happened before running Task 4 on hardware.

- [ ] **Step 3: Also make sure `scry_font_funnel_display_12` is declared**

Run:
```bash
grep -n "scry_font_funnel_display_12" scrybar.ino | head -3
```

Expected: a `LV_FONT_DECLARE` line. If missing:

File: `scrybar.ino`, find the block of `LV_FONT_DECLARE` lines around line 80-91, add:
```c
LV_FONT_DECLARE(scry_font_funnel_display_12);
```

(This font file already exists in `src/fonts/` but may not be declared yet.)

- [ ] **Step 4: Compile**

Run:
```bash
arduino-cli compile --clean \
  --build-path /tmp/arduino-build-scrybar \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  <REPO_ROOT>
```

Expected: compile succeeds. Warnings about unused `heroWindow` are fine.

- [ ] **Step 5: Upload**

Run:
```bash
arduino-cli upload -p /dev/cu.usbmodem83201 \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  --input-dir /tmp/arduino-build-scrybar \
  <REPO_ROOT>
```

Expected: upload succeeds, device resets automatically.

- [ ] **Step 6: Navigate to LAUNCH and capture a screenshot**

Run (wait a few seconds after the reset for LVGL to finish drawing):
```bash
python3 -c "import serial,time; s=serial.Serial('/dev/cu.usbmodem83201',115200); time.sleep(2); s.write(b'SAVEROFF\nLAUNCH\n'); time.sleep(2); s.close()"
python3 tools/capture_snapshot.py --port /dev/cu.usbmodem83201 --out /tmp/launch_task4.png
```

Expected: a PNG saved in `/tmp/launch_task4.png/`.

**View the screenshot.** At this point the countdown still has `T-` prefix and says wrong things — that's fine. What you're verifying in Task 4 is ONLY the layout/positions:

Check:
- Badge top-left, provider text visible, ~180px wide (we'll make it auto-fit in Task 5)
- Mission name on row 2, not overlapping badge
- "Vehicle | Pad" below mission name
- Location and country at the bottom of the left column
- "LIFTOFF IN" label centered in right column near the top
- Big countdown centered in the middle-right — this is where you should see the 60px font
- Weather below countdown (may still use old color — Task 5 will update)
- `TAP · SCAN QR` hint at the bottom of the right column aligned with country

The data may still look wrong (formatting not updated yet) but the layout must match the mockup. If the countdown is tiny or in the wrong spot, the font/label wiring is broken — stop and debug before continuing.

- [ ] **Step 7: Commit**

```bash
git add scrybar.ino
git commit -m "$(cat <<'EOF'
feat(launch): rewrite hero as Cinematic Asymmetric layout

Replaces the full-width hero with a two-column design: mission info left, dominant 60px countdown right. Adds heroCountdownLabel ("LIFTOFF IN") and heroQrHint ("TAP · SCAN QR") labels. Data population and countdown format changes come in the next commit — this commit only reshapes the init geometry.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Update data population — countdown format, badge auto-width, weather, label states

**Files:**
- Modify: `scrybar.ino` — `lvglUpdateLaunchUi()` (hero section, ~line 14368) and `lvglTickLaunchCountdown()` (~line 14447)

- [ ] **Step 1: Update the hero population block in `lvglUpdateLaunchUi()`**

File: `scrybar.ino`. Locate the block starting with `// ---- View 0: Hero (mission 0, all details) ----` (~line 14368). The current code sets `headerCenter`, `heroBadgeLabel`, `heroName`, `heroBadge` bg color, `heroVehiclePad`, and bottom-zone fields.

Find the existing block (roughly from `const LaunchItem &hero = g_launchState.items[0];` down to the end of the hero population, before the View 1 compact loop).

Replace with:
```c
  // ---- View 0: Hero (mission 0, all details) ----
  const LaunchItem &hero = g_launchState.items[0];
  lv_label_set_text(g_launchUi.headerCenter, hero.name);

  // Provider badge — auto-size to text + 28px padding (14 left + 14 right).
  lv_label_set_text(g_launchUi.heroBadgeLabel, hero.provider);
  lvglSetBgFlat(g_launchUi.heroBadge, launchProviderColor(hero.providerSlug));
  lv_obj_set_style_bg_opa(g_launchUi.heroBadge, LV_OPA_COVER, LV_PART_MAIN);
  {
    const size_t plen = strlen(hero.provider);
    lv_coord_t txtW = lv_txt_get_width(hero.provider, plen, lvglFontSmall(), 0, LV_TEXT_FLAG_NONE);
    lv_coord_t badgeW = txtW + 28;
    if (badgeW < 82)  badgeW = 82;
    if (badgeW > 280) badgeW = 280;
    lv_obj_set_width(g_launchUi.heroBadge, badgeW);
  }

  // Mission name — no quotes in the rendered text; ellipsis handled by LV_LABEL_LONG_DOT.
  lv_label_set_text(g_launchUi.heroName, hero.name);

  // Vehicle | Pad (ASCII pipe separator — middle dot would tofu in older fonts and adds nothing).
  char vpBuf[96];
  if (hero.vehicle[0] && hero.pad[0])
    snprintf(vpBuf, sizeof(vpBuf), "%s | %s", hero.vehicle, hero.pad);
  else if (hero.vehicle[0])
    snprintf(vpBuf, sizeof(vpBuf), "%s", hero.vehicle);
  else if (hero.pad[0])
    snprintf(vpBuf, sizeof(vpBuf), "%s", hero.pad);
  else
    vpBuf[0] = '\0';
  lv_label_set_text(g_launchUi.heroVehiclePad, vpBuf);

  // Left-column bottom: location + country.
  lv_label_set_text(g_launchUi.heroLocation, hero.location[0] ? hero.location : "");
  lv_label_set_text(g_launchUi.heroCountry,  hero.country[0]  ? hero.country  : "");

  // Right-column: weather (accent color refreshed in case theme changed).
  lvglSetTextHex(g_launchUi.heroWeather, lvglLaunchWeatherAccent(t));
  if (hero.weather[0]) {
    lv_label_set_text(g_launchUi.heroWeather, hero.weather);
  } else {
    lv_label_set_text(g_launchUi.heroWeather, "");
  }

  // Countdown value is driven by lvglTickLaunchCountdown() — don't set it here.
  // Label text defaults to "LIFTOFF IN"; the tick handler switches it to NET
  // or other states when needed.
  lv_label_set_text(g_launchUi.heroCountdownLabel, "LIFTOFF IN");
  lvglSetTextHex(g_launchUi.heroCountdownLabel, t.auxMeta);

  // QR tap hint is static — no per-mission update. Visibility is always on in View 0.
  lv_obj_clear_flag(g_launchUi.heroQrHint, LV_OBJ_FLAG_HIDDEN);
```

**Note on field names:** the code assumes `LaunchItem` has `weather`, `location`, `country` as `char[]` fields. Check the struct definition (grep `struct LaunchItem`) — if any field name differs, substitute it. The existing `lvglUpdateLaunchUi()` already uses these names, so they exist.

- [ ] **Step 2: Verify `LaunchItem` field names match**

Run:
```bash
grep -n "struct LaunchItem" scrybar.ino
```

Then read the struct and confirm it has fields: `name`, `provider`, `providerSlug`, `vehicle`, `pad`, `location`, `country`, `weather`, `t0Epoch`, `hasT0`. If any name differs, fix the code in Step 1 accordingly before compiling.

- [ ] **Step 3: Rewrite `lvglTickLaunchCountdown()` for the new format**

File: `scrybar.ino`, find `static void lvglTickLaunchCountdown()` (~line 14447). Replace the function body (but keep the signature and the view-rotate block at the end).

New body:
```c
static void lvglTickLaunchCountdown() {
  if (!g_lvglLaunchRoot || g_uiPageMode != UI_PAGE_LAUNCH) return;
  if (g_launchState.count == 0) return;

  const UiThemeLvglTokens &t = activeUiTheme().lvgl;
  const uint32_t now32 = millis();
  const LaunchItem &hero = g_launchState.items[0];

  if (!hero.hasT0) {
    // NET state — no precise T-0, show date window in the value slot.
    lv_label_set_text(g_launchUi.heroCountdownLabel, "NET");
    lv_label_set_text(g_launchUi.heroCountdown, hero.netWindow[0] ? hero.netWindow : "TBD");
    lvglSetTextHex(g_launchUi.heroCountdownLabel, t.auxMeta);
    lvglSetTextHex(g_launchUi.heroCountdown, t.auxMeta);
  } else {
    const time_t now = time(nullptr);
    const int32_t delta = (int32_t)(hero.t0Epoch - now);

    char buf[24];
    uint32_t textColor = t.infoText;
    const char *labelTxt = "LIFTOFF IN";
    uint32_t labelColor  = t.auxMeta;

    if (delta <= 0) {
      // Imminent / elapsed — keep it cinematic, go red and show absolute seconds.
      snprintf(buf, sizeof(buf), "00:00:00");
      labelTxt   = "LIFTOFF";
      labelColor = 0xFF5252;
      textColor  = 0xFF5252;
    } else if (delta < 60) {
      // Final minute — red tint, HH:MM:SS but really 00:00:SS
      snprintf(buf, sizeof(buf), "00:00:%02d", (int)delta);
      textColor = 0xFF5252;
    } else if (delta < 86400) {
      int h = delta / 3600;
      int m = (delta % 3600) / 60;
      int s = delta % 60;
      snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
    } else {
      int d = delta / 86400;
      int h = (delta % 86400) / 3600;
      int m = (delta % 3600) / 60;
      if (d > 99) snprintf(buf, sizeof(buf), "%dd", d);
      else        snprintf(buf, sizeof(buf), "%dd %02d:%02d", d, h, m);
    }

    lv_label_set_text(g_launchUi.heroCountdownLabel, labelTxt);
    lv_label_set_text(g_launchUi.heroCountdown, buf);
    lvglSetTextHex(g_launchUi.heroCountdownLabel, labelColor);
    lvglSetTextHex(g_launchUi.heroCountdown, textColor);
  }

  // Auto-rotate views every 10 seconds (pause while QR is open)
  if (!g_launchUi.qrModalOpen && g_launchState.count > 1 &&
      (now32 - g_launchUi.lastViewRotateMs) >= 10000UL) {
    g_launchUi.lastViewRotateMs = now32;
    uint8_t next = (g_launchUi.viewIndex + 1) % 2;
    Serial.printf("[LAUNCH] view rotate %d -> %d (count=%d)\n",
                  (int)g_launchUi.viewIndex, (int)next, (int)g_launchState.count);
    lvglSetLaunchView(next);
  }
}
```

**If `LaunchItem` has no `netWindow` field:** replace `hero.netWindow[0] ? hero.netWindow : "TBD"` with just `"TBD"` and accept that the NET state shows a literal `TBD`. This is fine for a first pass — the data layer can grow a `netWindow` field in a follow-up.

- [ ] **Step 4: Bump the build tag**

File: `config.h`. Replace:
```c
#define FW_BUILD_TAG "r255"
#define FW_RELEASE_DATE "2026-04-09"
```

With:
```c
#define FW_BUILD_TAG "r256"
#define FW_RELEASE_DATE "2026-04-10"
```

- [ ] **Step 5: Compile**

Run:
```bash
arduino-cli compile --clean \
  --build-path /tmp/arduino-build-scrybar \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  <REPO_ROOT>
```

Expected: compile succeeds. If an error mentions a `LaunchItem` field that doesn't exist, drop back to Step 2 and fix the substitution.

- [ ] **Step 6: Upload**

Run:
```bash
arduino-cli upload -p /dev/cu.usbmodem83201 \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  --input-dir /tmp/arduino-build-scrybar \
  <REPO_ROOT>
```

- [ ] **Step 7: Screenshot and visual verify against mockup**

Run:
```bash
python3 -c "import serial,time; s=serial.Serial('/dev/cu.usbmodem83201',115200); time.sleep(2); s.write(b'SAVEROFF\nLAUNCH\n'); time.sleep(2); s.close()"
python3 tools/capture_snapshot.py --port /dev/cu.usbmodem83201 --out /tmp/launch_task5.png
```

**View the screenshot** at `/tmp/launch_task5.png/scrybar_*.png` and compare against the mockup at `http://localhost:9091` (Main variant — Isar Aerospace).

Checklist:
- [ ] Badge width fits the provider name + 14px padding either side (no more hard-coded 180)
- [ ] Mission name is rendered at 25px, no quotes, ellipsis on overflow
- [ ] Vehicle | Pad uses ASCII pipe, not middle dot
- [ ] Location renders with correct glyphs (if the current `hero.location` contains ø, å, é etc., they must not tofu)
- [ ] Country aligned 6px above body bottom
- [ ] "LIFTOFF IN" label centered in right column, caps, tracked
- [ ] Countdown at 60px, no "T-" prefix, format `HH:MM:SS` or `Dd HH:MM`
- [ ] Weather centered below countdown in warm amber
- [ ] "TAP · SCAN QR" at bottom of right column, 12px muted 70%
- [ ] Tap anywhere on the hero opens QR (unchanged behavior)

If any item fails, STOP. Fix the specific issue (don't start Task 6 yet). Re-run Steps 5-7.

- [ ] **Step 8: Commit**

```bash
git add scrybar.ino config.h
git commit -m "$(cat <<'EOF'
feat(launch): hero data population + countdown format for Cinematic Asymmetric (r256)

- Dynamic badge width based on provider text (min 82, max 280, +28 padding)
- Mission name without quotes, LV_LABEL_LONG_DOT for ellipsis
- Vehicle | Pad with ASCII pipe separator
- Location + country populated from hero data — international glyphs now render natively thanks to Latin-1/Latin Extended fonts
- Weather in warm amber accent via lvglLaunchWeatherAccent()
- Countdown format stripped of "T-" prefix (the LIFTOFF IN label carries the semantics), with HH:MM:SS / Dd HH:MM formats
- Label states: LIFTOFF IN (default), LIFTOFF (≤0), NET (no T-0 yet)
- Imminent tint (≤60s) goes red on the countdown

r256.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Final verification + push

**Files:**
- None (verification-only task)

- [ ] **Step 1: Capture before/after screenshots for the commit record**

Run:
```bash
mkdir -p knowledge/screenshots
python3 -c "import serial,time; s=serial.Serial('/dev/cu.usbmodem83201',115200); time.sleep(2); s.write(b'SAVEROFF\nLAUNCH\n'); time.sleep(4); s.close()"
python3 tools/capture_snapshot.py --port /dev/cu.usbmodem83201 --out knowledge/screenshots
```

The latest snapshot inside `knowledge/screenshots/` is the r256 reference.

- [ ] **Step 2: Force carousel to View 0 (hero) by power-cycling the device**

The carousel rotates every 10 seconds. To grab a clean View 0 screenshot, the simplest method is to re-upload (which resets `viewIndex=0` in init) and capture within 10s:

```bash
arduino-cli upload -p /dev/cu.usbmodem83201 \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  --input-dir /tmp/arduino-build-scrybar \
  <REPO_ROOT>
python3 -c "import serial,time; s=serial.Serial('/dev/cu.usbmodem83201',115200); time.sleep(3); s.write(b'SAVEROFF\nLAUNCH\n'); time.sleep(3); s.close()"
python3 tools/capture_snapshot.py --port /dev/cu.usbmodem83201 --out knowledge/screenshots
```

- [ ] **Step 3: Test international glyph rendering (manual)**

The current `g_launchState.items[0]` may not contain accented characters. To test the Latin Extended font regeneration actually works:

Run:
```bash
python3 -c "
import serial,time
s=serial.Serial('/dev/cu.usbmodem83201',115200); time.sleep(0.3)
# LAUNCH is already the current page; just verify no tofu in logs
s.write(b'LAUNCHDETAIL 0\n'); time.sleep(1)
print(s.read_all().decode(errors='replace'))
s.close()"
```

If the current live launch data contains any non-ASCII character in location/country, look for tofu in the screenshot. If not, skip this step — the font change is verified structurally by the successful compile in Task 2.

- [ ] **Step 4: Confirm flash size increase is within budget (<100 KB)**

Run:
```bash
arduino-cli compile --clean \
  --build-path /tmp/arduino-build-scrybar \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  <REPO_ROOT> 2>&1 | grep -E "Sketch uses|Global variables"
```

Compare the "Sketch uses" number against the pre-r256 value (can be checked from the previous build cache or from git log). Expected delta: +40-80 KB. If it's more than +100 KB, something went wrong in the font generation (likely `--bpp 8` or missing `--no-compress`) — investigate.

- [ ] **Step 5: Push to origin**

```bash
git push origin main
```

- [ ] **Step 6: Update session memory with the landing state**

Append to the session's MEMORY.md (see CLAUDE.md convention) a short note:
```
## r256 — LAUNCH hero Cinematic Asymmetric
- New 60px countdown font (scry_font_funnel_display_countdown_60)
- Fonts 14/16/18/20/25 regenerated with Latin-1 Supplement + Latin Extended-A
- Hero layout: two-column, LIFTOFF IN label + 60px countdown right
- heroWindow field deprecated in hero (still in struct, set nullptr)
- TODO: View 1 (compact) redesign next session
```

---

## Out of scope (confirmed with user)

- **View 1 (compact rows)** — deferred to a separate brainstorming session
- **Detail overlay (QR lightbox)** — already reworked in r255, untouched
- **Cyrillic / CJK / Arabic charsets** — explicitly declined
- **Bold font variant** — only Funnel Display Regular available, visual hierarchy via size
- **Countdown states Live/Scrubbed/Success beyond simple coloring** — data layer doesn't expose those fields yet

---

## If things go wrong — rollback

Each task commits independently. To back out a specific task:

```bash
git log --oneline -10
git revert <commit-sha>
arduino-cli compile --clean ...
arduino-cli upload ...
```

The fonts can be independently reverted — if Task 2 fails but Task 1 succeeds, you can revert only the Task 2 commit and the countdown font in Task 1 stays functional.
