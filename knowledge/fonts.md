# Fonts — ScryBar (r286+)

Single source of truth for every `scry_font_dosis_*` file that ships
in the firmware. If something here disagrees with the `.c` files, **regenerate
the `.c` files** — never hand-edit them. The generator is
`tools/regen_fonts.sh` and its top-of-file comment tells the same story as
this document.

## Typeface

Only one typeface: **Dosis** (Google Fonts, SIL OFL).

Two official static weights from the Google Fonts Dosis source:

| Weight   | TTF in `assets/fonts/`       | `wght` value |
|----------|------------------------------|--------------|
| Regular  | `Dosis-Regular.ttf`          | 400          |
| SemiBold | `Dosis-SemiBold.ttf`         | 600          |

We don't ship Bold / ExtraBold / Light. If a future layout needs a different
weight, add that official static file and keep the same OFL notice. The complete
license is bundled as `assets/fonts/Dosis-OFL-1.1.txt`.

## Charset

Every layout font (everything except the narrow countdown) has the same
three glyph ranges:

| Range           | Name                   | Covers                                    |
|-----------------|------------------------|-------------------------------------------|
| `U+0020..007E`  | Basic Latin (ASCII)    | English, numbers, punctuation             |
| `U+00A0..00FF`  | Latin-1 Supplement     | ø, ñ, ç, ü, é, ö, ß, ·, ±, ...            |
| `U+0100..017F`  | Latin Extended-A       | ı, ğ, ş, č, ž, ł, ā, ő, ...               |

This covers every launch/station place name we actually receive from
`rocketlaunch.live` and Transitous: Andøya, Kourou, São Paulo, München, İzmir,
Chişinău, Łódź, etc. Cyrillic, Greek, CJK and Arabic are **explicitly out of
scope** — those names arrive romanized from the APIs.

The narrow **countdown font** uses only 16 glyphs so the 60px file stays
manageable (~50 KB instead of ~700 KB with Latin Extended):

```
0x20  (space)
0x2B  +
0x2D  -
0x30-0x39  0..9
0x3A  :
0x54  T    (for the countdown prefix)
0x64  d    (for "2d 18:42" day format)
```

## Generated font files

### Regular family — 12 sizes

Every size that any accessor in `scrybar.ino` currently needs. Add to
`REGULAR_SIZES` in `tools/regen_fonts.sh` if you need a new one.

| Size | File                                    | Fallback              |
|------|-----------------------------------------|-----------------------|
| 12   | `scry_font_dosis_12.c`         | `lv_font_montserrat_14` |
| 14   | `scry_font_dosis_14.c`         | `lv_font_montserrat_14` |
| 16   | `scry_font_dosis_16.c`         | `lv_font_montserrat_16` |
| 18   | `scry_font_dosis_18.c`         | `lv_font_montserrat_18` |
| 20   | `scry_font_dosis_20.c`         | `lv_font_montserrat_20` |
| 22   | `scry_font_dosis_22.c`         | `lv_font_montserrat_22` |
| 23   | `scry_font_dosis_23.c`         | `lv_font_montserrat_24` |
| 24   | `scry_font_dosis_24.c`         | `lv_font_montserrat_24` |
| 25   | `scry_font_dosis_25.c`         | `lv_font_montserrat_24` |
| 30   | `scry_font_dosis_30.c`         | `lv_font_montserrat_30` |
| 32   | `scry_font_dosis_32.c`         | `lv_font_montserrat_32` |
| 38   | `scry_font_dosis_38.c`         | `lv_font_montserrat_38` |

### SemiBold family — 5 sizes

Narrow set — only the places that need tangible typographic emphasis. Add to
`SEMIBOLD_SIZES` in the generator script if you find another genuine need.

| Size | File                                            | Fallback              | Primary use                               |
|------|-------------------------------------------------|-----------------------|-------------------------------------------|
| 18   | `scry_font_dosis_semibold_18.c`        | `lv_font_montserrat_18` | Provider badge labels and compact emphasis |
| 20   | `scry_font_dosis_semibold_20.c`        | `lv_font_montserrat_20` | Primary page-header titles               |
| 25   | `scry_font_dosis_semibold_25.c`        | `lv_font_montserrat_24` | LAUNCH hero mission name, Now Playing title |
| 32   | `scry_font_dosis_semibold_32.c`        | `lv_font_montserrat_32` | Reserved — emphatic clock / weather       |
| 38   | `scry_font_dosis_semibold_38.c`        | `lv_font_montserrat_38` | Reserved — future oversized headlines     |

### Countdown font — single narrow file

| Size | File                                            | Charset        | Use                               |
|------|-------------------------------------------------|---------------|-----------------------------------|
| 60   | `scry_font_dosis_countdown_60.c`       | 15 glyphs     | LAUNCH hero countdown value only  |

## Accessors in `scrybar.ino`

Semantic accessors live around `scrybar.ino:13042` and above
`// ── SemiBold emphasis family (r256) ──`. Use these — never reference the
generated symbol directly from page code.

### Regular (existing)

```c
lvglFontTiny()      // 14px  — small caption
lvglFontMini()      // 16px  — metadata, secondary rows
lvglFontSmall()     // 18px  — secondary labels and compact values
lvglFontMeta()      // 20px  — transit destinations, launch location
lvglFontRssNews()   // 22px  — RSS headline body
lvglFontBody()      // 24px  — generic body
lvglFontTitle()     // 30px  — page titles
lvglFontClock()     // 32px  — clock glyphs
lvglFontBig()       // 32px  — big glyphs (weather etc.)
```

Plus the Now Playing accessors and the screen saver accessors.

### SemiBold (NEW r256)

```c
lvglFontSmallBold()   // 18px  — badges and compact emphasis
lvglFontHeaderTitle() // 20px  — primary page-header title
lvglFontLaunchName()  // 25px  — LAUNCH hero mission name
lvglFontClockBold()   // 32px  — reserved (future emphatic clock)
lvglFontBigBold()     // 38px  — reserved (future oversized headline)
```

### Countdown special

```c
lvglFontCountdown()   // 60px narrow SemiBold — LAUNCH hero countdown only
```

## Regeneration

```bash
tools/regen_fonts.sh              # everything
tools/regen_fonts.sh regular      # Regular family only
tools/regen_fonts.sh semibold     # SemiBold family only
tools/regen_fonts.sh countdown    # narrow countdown only
```

The script is idempotent. Safe to run any time.

After regenerating you may need to touch `scrybar.ino` only if you **added or
removed a size** (new `LV_FONT_DECLARE` + accessor). Existing sizes are
in-place overwrites.

## Gotchas we hit before

1. **Variable fallback sizes.** The old generator used `lv_font_montserrat_24`
   as a blanket fallback. A 14px country label falling back to 24px Montserrat
   blows up the row height. Every layout font now falls back to the
   size-matched Montserrat (or the next-nearest available in `lv_conf.h`).
2. **Charset drift.** Before r256, each `.c` file was generated with whatever
   flags the author remembered that day. Some had only ASCII, some had
   Latin-1, some had Extended-A. Every font is now generated by
   `regen_fonts.sh` from a single shared charset definition.
3. **Missing `·` (U+00B7).** The QR tap hint uses a middle dot. This lives in
   Latin-1 Supplement, not ASCII, so if anyone regenerates a font without
   `0xA0-0xFF` the dot tofus. The shared charset definition prevents this.
4. **Tofu on international place names.** "Andøya" used to fall back to
   Montserrat for the `ø` glyph (different typeface, different baseline). Now
   Dosis renders it natively.

## Lv_conf.h fallback coverage

The available Montserrat fallbacks are declared in `lv_conf.h` lines 18-28:
`14, 16, 18, 20, 22, 24, 28, 30, 32, 36, 38`. Sizes without a natural match
(23, 25) round **up** so fallbacks never make text smaller than intended.
