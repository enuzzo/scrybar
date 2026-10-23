# Display quality audit — 2026-10-23

Static source extraction plus native LVGL 8 glyph/geometry measurements. Synthetic conditions; no device, live data or physical acceptance.

Firmware tag: `r321`. Source SHA-256: `76cd1d728e86ba8a78807e9a77fb9eee8705cc028fe50b4365a28219c3fc84de`.

Checked 7 themes × 175 selected color roles = 1225 pairs, 14 languages / 196 UiStrings entries, and 478 text fixtures with the actual installed LVGL/fonts.

The thresholds are design benchmarks from [W3C text contrast](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html) and [non-text contrast](https://www.w3.org/WAI/WCAG22/Understanding/non-text-contrast.html), not a physical-display compliance claim. The JSON retains both token RGB888 and quantized RGB565 estimates. Antialiasing, panel calibration and ambient light require visual review.

Composited roles use the installed LVGL's `lv_color_mix` and `lv_color_to32`, including the transparent LAUNCH root's 20/255 hero veil, TRANSIT's22/255 alternate rows and the effective final text opacity. Now Playing samples13 representative cover-derived surfaces, with separate body/header ink and both status-dot states. These are finite color fixtures, not an exhaustive proof for every image. Earlier baseline reports expanded RGB565 channels with integer scaling; this audit uses the same native conversion as the PNG renderer.

## Measured contrast findings

Only active roles are counted as findings. Legacy token inventory is separately marked. Known runtime conditions (usage/temperature/pressed state) are synthetic; colors and expressions come from firmware.

| Theme | Role | RGB888 ratio | RGB565 ratio | Target | Source |
|---|---|---:|---:|---:|---|

No required contrast findings in the selected roles.

### Independent rendered-surface check

35 background samples from the full native page renderer; 0 mismatches. Renderer source SHA-256: `76cd1d728e86ba8a78807e9a77fb9eee8705cc028fe50b4365a28219c3fc84de`. The renderer and this audit used the same source snapshot.

## Fonts, formats and text bounds

Format signatures: **0 errors**. UiStrings missing glyph fixtures: **0**. UiStrings fallback fixtures: **0**.

Bounds use actual extracted geometry at 640×172. `scroll` and `dot` overflow is recorded as intentional behavior, not automatically treated as a defect. RSS and QR wrapping are measured with their actual fonts. Forecast fixtures cover every short weather label from the corresponding language table; wind fixtures include 0/18.52/123/999. Raw UiStrings and formatted variants are both retained in JSON. This checks font coverage, not grammatical correctness.

| Fixture | Font | Unwrapped width | Wrapped height | Bounds | Observation |
|---|---|---:|---:|---|---|
| launch.TBD | lvglFontLaunchCountdown(false) | 58 | 46 | 312×60 (single) | fits |
| transit.unconfigured | lvglFontSmall() | 203 | 23 | 0×0 (unbounded) | covered; bounds not measured |
| launch.numeric | lvglFontLaunchCountdown(true) | 247 | 46 | 312×60 (single) | fits |
| launch.NET.time | lvglFontLaunchCountdown(true) | 110 | 46 | 312×60 (single) | fits |

UiStrings fields currently without active callsites: `touchToClose`. Runtime QR paths reference the localized table. The unused shorter init-time field remains an inventory entry.

Internal localized RSS messages retain their Unicode accents; external article text alone uses the existing ASCII fallback.

## Copy recommendations

This is the editorial checklist established from the baseline, not automatic proofreading. Status indicates whether its known source pattern remains present; source pointers identify the original baseline location.

- **P1 pirate.wind_units — not present in current source** — Wind uses a km/h value with a knots label; convert only Pirate by dividing by 1.852, retaining nautical character. `src/ui_strings.h:270`. ([source](https://www.aoml.noaa.gov/hrd/tcfaq/G4.html))
- **P3 pirate.forecast_time — not present in current source** — Three bells are not three hours. Use an unambiguous 3h label with a Pirate suffix; retain the rest of the voice. `src/ui_strings.h:272`. ([source](https://www.mycg.uscg.mil/News/Article/3616329/eight-bells-and-all-is-well/))
- **P3 german.rss_sync — not present in current source** — Replace broken hybrid 'RSS-Feed wird sync...' with 'RSS wird synchronisiert...' and check wrapping. `src/ui_strings.h:99`.
- **P3 esperanto.touch — not present in current source** — Tusu means cough; the touch imperative is Tuŝu. Dosis supports ŝ. Preserve this distinction rather than stripping the accent. `src/ui_strings.h:181`. ([source](https://www.retavortaro.de/revo/art/tus.html))
- **P3 accents — not present in current source** — Supported accents are retained in internal localized RSS messages. External article text alone keeps its established ASCII fallback. `src/ui_strings.h:75`.
- **Constraint voices — preserve** — Preserve Bellazio slang (boh/tipo/slay), Pirate phrasing and Shakespeare's Prithee/anon; do not turn stylistic choices into generic English/Italian. `src/ui_strings.h:209`.
- **P3 web_hero — not present in current source** — Move the adverb: 'Everything you could check faster on your phone, but won't.' Keep the joke and tone. `scrybar.ino:4969`.
- **P3 web_view_descriptions — not present in current source** — INFO description currently refers to word clock, and Mac Stats omits network. Describe actual page contents. `scrybar.ino:5053`.

## Separate visual and runtime verification

- Capture and inspect 7 themes × 10 pages (INFO, HOME, RSS/AUX, WIKI, NOW PLAYING, DOOM, TRANSIT, LAUNCH, MAC STATS, BAMBU). Mark disabled/unavailable pages explicitly; this audit alone does not stand in for 70 inspected screenshots. The completed native-render matrix and its limits are recorded separately in [native-pages.md](native-pages.md).
- Exercise Mint warning values, imminent Transit times, Launch TBD/liftoff, Wiki Random without favicon, QR in 14 languages and long/offline/stale states.
- Compare patched output against this source-hashed baseline, inspect a contact sheet, and confirm representative physical-panel results.
- Preserve theme-specific geometry and hue. Bambu/DOOM/artwork and the unified screensaver intentionally contain theme-independent parts.
- The Control Surface consumes the firmware's Web theme tokens through `appendWebThemeCssVars()`. Its responsive and interaction checks are recorded separately in [the WebUI QA](../2026-10-23-webui/QA.md).

## Reproduce

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/audit_ui_quality_native.py --out-prefix docs/design/2026-10-23-display-quality/r321-final-alpha --fail-on-findings --native-surface-samples screenshots/display-quality-r321/surface-samples.json
```

Build products, logs and native executables remain in the external temporary build directory. No firmware, USB, NVS, network or Git operation is performed.
