# LAUNCH Page Visual Polish — Action Plan

> **For agentic workers:** Use superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring the LAUNCH page visual quality to match existing views (Transit, AUX, Wiki) — fix layout waste, font consistency, separators, QR sizing, overlay quality.

**Method:** Iterative screenshot-driven refinement. Take screenshots of LAUNCH + Transit/AUX for comparison. Fix issues, recompile, re-upload, re-screenshot until visually excellent.

**Key tools:**
- Screenshot: `python3 tools/capture_snapshot.py --port /dev/cu.usbmodem83201 --out /tmp/snap.png`
- Compile: `arduino-cli compile --clean --build-path /tmp/arduino-build-scrybar --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi /Users/enuzzo/Library/CloudStorage/Dropbox/Mitnick/scrybar`
- Upload: `arduino-cli upload -p /dev/cu.usbmodem83201 --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi --input-dir /tmp/arduino-build-scrybar /Users/enuzzo/Library/CloudStorage/Dropbox/Mitnick/scrybar`

---

## Issues to Fix

### 1. Header/Navbar mismatch
- [ ] Screenshot Transit + LAUNCH side by side to compare headers
- [ ] Match LAUNCH header to Transit pattern: same height, same font (`lvglFontSmall()` = 18px for title), same layout style, same background fill approach
- [ ] Transit header has: title left, station name center, status/time right — LAUNCH header should follow similar structure

### 2. Tofu characters (missing glyphs)
- [ ] Identify which characters cause tofu: likely the `·` (U+00B7, middle dot) used as separator in "Vehicle · Pad"
- [ ] Check which glyphs are available in `scry_font_funnel_display_*` fonts
- [ ] Options: (a) add missing glyphs to the custom font, (b) use ASCII alternatives like `-` or `/` or `|`
- [ ] Check `lv_conf.h` font range settings
- [ ] Fix all separator characters in Launch code to use available glyphs

### 3. Font sizes — too small / wrong fonts
- [ ] Use project font helpers consistently:
  - Header title: `lvglFontSmall()` (18px) — matches Transit
  - Hero mission name: `lvglFontMeta()` (20px) — prominent
  - Hero vehicle/pad: `lvglFontMini()` (16px) — secondary
  - Hero countdown: `lvglFontSmall()` (18px) or `lvglFontMeta()` (20px) — bold/prominent
  - Compact row mission: `lvglFontMini()` (16px)
  - Compact row date: `lvglFontTiny()` (14px)
  - Badge labels: `lvglFontTiny()` (14px) — already set
  - Fetch time in header: `lvglFontTiny()` (14px)

### 4. Row separators (strokes/dividers)
- [ ] Add 1px horizontal dividers between rows, same as Transit's `rowSep` pattern
- [ ] Use `t.divider` color from theme
- [ ] Add separators: below hero, between compact rows

### 5. Horizontal space waste + QR size
- [ ] Current: 556px content + 84px QR. With only 4 data fields per row (badge, name, countdown/date), 556px is too wide
- [ ] Option: make QR larger (100-120px), giving more visual weight to the QR column
- [ ] Recalculate: e.g. 520px content + 120px QR → QR at ~100x100
- [ ] Or: keep 556px content but use the space better — larger fonts, more padding, breathing room
- [ ] Consider Transit's column approach: explicit X positions for each field

### 6. Detail overlay → lightbox style
- [ ] Current overlay is flat, same bg as page — no visual distinction
- [ ] Add lightbox effect: slightly darker/tinted background, or border, or shadow
- [ ] Add a semi-transparent backdrop behind overlay (darken underlying content)
- [ ] Round corners on overlay container
- [ ] Better visual hierarchy in detail: larger title, clearer sections
- [ ] Add tag pills at bottom (already in struct, not rendered yet)
- [ ] Close button more visible/styled

### 7. Iterative refinement loop
- [ ] After each fix batch: compile → upload → screenshot → evaluate
- [ ] Compare with Transit/AUX screenshots for visual consistency
- [ ] Use frontend-design skill for evaluation if available
- [ ] Repeat until quality matches existing views

---

## Reference: Transit UI patterns to match

Key patterns from `lvglInitTransitUi()`:
- Header: `lvglFontSmall()` for title, full-width fill, station name centered
- Row height: 35px with 1px separator lines (`lv_obj` with `lv_obj_set_size(sep, contentW, 1)`)
- Badge: pill with `lv_obj_set_style_radius(badge, 4, 0)`, uses `lvglFontMeta()` (20px)
- Destinations: `lvglFontMeta()` (20px)
- Alternate row tinting via bg opacity

## Current code locations (scrybar.ino)
- `lvglInitLaunchUi()` — search for "Launch Page LVGL"
- `lvglInitLaunchDetail()` — search for "lvglInitLaunchDetail"
- `lvglUpdateLaunchUi()` — search for "lvglUpdateLaunchUi"
- `lvglTickLaunchCountdown()` — search for "lvglTickLaunchCountdown"
- `lvglOpenLaunchDetail()` — search for "lvglOpenLaunchDetail"
- Theme integration — search for "Launch theme colors" in `lvglApplyThemeStyles`
- Provider colors — `launchProviderColor()`
