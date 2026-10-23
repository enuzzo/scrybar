# Display quality baseline — 2026-10-23

Static source extraction plus native LVGL 8 glyph/geometry measurements. Synthetic conditions; no device, live data or physical acceptance.

Firmware tag: `r320`. Source SHA-256: `14c991795ec12ed5739964b1d1e69f66f8a52f33821eb9c1b3fc73478f4ddcd6`.

Checked 7 themes × 32 selected color roles = 224 pairs, 14 languages / 196 UiStrings entries, and 476 text fixtures with the actual installed LVGL/fonts.

The thresholds are design benchmarks from [W3C text contrast](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html) and [non-text contrast](https://www.w3.org/WAI/WCAG22/Understanding/non-text-contrast.html), not a physical-display compliance claim. The JSON retains both token RGB888 and quantized RGB565 estimates. Antialiasing, panel calibration and ambient light require visual review.

## Measured contrast findings

Only active roles are counted as findings. Legacy token inventory is separately marked. Known runtime conditions (usage/temperature/pressed state) are synthetic; colors and expressions come from firmware.

| Theme | Role | RGB888 ratio | RGB565 ratio | Target | Source |
|---|---|---:|---:|---:|---|
| scrybar-default | feed.button.NXT.normal | 3.767 | 3.816 | 4.5 | scrybar.ino:10480 |
| scrybar-default | wiki.random.badge | 2.376 | 2.350 | 4.5 | scrybar.ino:14806 |
| cyberpunk-2077 | feed.header.meta | 3.543 | 3.597 | 4.5 | scrybar.ino:11364 |
| cyberpunk-2077 | launch.header.fetch | 1.034 | 1.040 | 4.5 | scrybar.ino:11259 |
| cyberpunk-2077 | wiki.random.badge | 2.376 | 2.350 | 4.5 | scrybar.ino:14806 |
| toxic-candy | feed.button.NXT.normal | 3.789 | 3.770 | 4.5 | scrybar.ino:10480 |
| toxic-candy | wiki.random.badge | 2.376 | 2.350 | 4.5 | scrybar.ino:14806 |
| tokyo-transit | launch.header.fetch | 1.006 | 1.004 | 4.5 | scrybar.ino:11259 |
| tokyo-transit | feed.button.NXT.normal | 3.700 | 3.759 | 4.5 | scrybar.ino:10480 |
| tokyo-transit | wiki.random.badge | 2.376 | 2.350 | 4.5 | scrybar.ino:14806 |
| minimal-brutalist-mono | launch.header.fetch | 1.634 | 1.607 | 4.5 | scrybar.ino:11259 |
| minimal-brutalist-mono | feed.button.NXT.normal | 3.732 | 3.793 | 4.5 | scrybar.ino:10480 |
| minimal-brutalist-mono | wiki.random.badge | 2.376 | 2.350 | 4.5 | scrybar.ino:14806 |
| mint-protocol | header.title | 4.081 | 4.035 | 4.5 | scrybar.ino:11241 |
| mint-protocol | feed.header.meta | 3.366 | 3.429 | 4.5 | scrybar.ino:11364 |
| mint-protocol | info.header.endpoint | 3.312 | 3.386 | 4.5 | scrybar.ino:11170 |
| mint-protocol | stats.axis.meta | 4.264 | 4.253 | 4.5 | scrybar.ino:15902 |
| mint-protocol | qr.hint | 2.963 | 3.048 | 4.5 | scrybar.ino:11375 |
| mint-protocol | launch.header.fetch | 1.000 | 1.000 | 4.5 | scrybar.ino:11259 |
| mint-protocol | launch.weather | 1.731 | 1.744 | 4.5 | scrybar.ino:15314 |
| mint-protocol | feed.button.NXT.normal | 4.081 | 4.035 | 4.5 | scrybar.ino:10480 |
| mint-protocol | stats.usage.65 | 1.731 | 1.744 | 3 | scrybar.ino:16003 |
| mint-protocol | stats.temperature.45 | 4.264 | 4.253 | 4.5 | scrybar.ino:16008 |
| mint-protocol | stats.temperature.75 | 1.731 | 1.744 | 4.5 | scrybar.ino:16008 |
| mint-protocol | stats.temperature.90 | 3.191 | 3.220 | 4.5 | scrybar.ino:16008 |
| mint-protocol | stats.spark.cpu.40 | 2.843 | 2.854 | 3 | scrybar.ino:16017 |
| mint-protocol | stats.spark.cpu.65 | 1.731 | 1.744 | 3 | scrybar.ino:16017 |
| mint-protocol | stats.spark.gpu.40 | 2.023 | 1.989 | 3 | scrybar.ino:16017 |
| mint-protocol | stats.spark.gpu.65 | 1.731 | 1.744 | 3 | scrybar.ino:16017 |
| mint-protocol | wiki.random.badge | 2.376 | 2.350 | 4.5 | scrybar.ino:14806 |
| cathode-ray | qr.hint | 3.744 | 3.667 | 4.5 | scrybar.ino:11375 |
| cathode-ray | feed.button.NXT.normal | 3.799 | 3.889 | 4.5 | scrybar.ino:10480 |
| cathode-ray | wiki.random.badge | 2.376 | 2.350 | 4.5 | scrybar.ino:14806 |

## Fonts, formats and text bounds

Format signatures: **0 errors**. UiStrings missing glyph fixtures: **0**. UiStrings fallback fixtures: **0**.

Bounds use actual extracted geometry at 640×172. `scroll` and `dot` overflow is recorded as intentional behavior, not automatically treated as a defect. RSS and QR wrapping are measured with their actual fonts. Forecast fixtures cover every short weather label from the corresponding language table; wind fixtures include 0/18.52/123/999. Raw UiStrings and formatted variants are both retained in JSON. This checks font coverage, not grammatical correctness.

| Fixture | Font | Unwrapped width | Wrapped height | Bounds | Observation |
|---|---|---:|---:|---|---|
| launch.TBD | lvglFontCountdown() | 64 | 46 | 0×0 (unbounded) | U+0042 fallback, U+0044 fallback |
| transit.unconfigured | lvglFontSmall() | 155 | 23 | 0×0 (unbounded) | U+1F689 missing |

UiStrings fields currently without active callsites: `touchToClose`, `touchToCloseAnywhere`, `generatingQr`. QR paths currently contain English literals; localized QR measurements are intended-table fixtures until those bindings are connected.

## Copy recommendations

Recommendations below are editorial findings, not automatic replacements. Source pointers describe the baseline snapshot.

- **P1 pirate.wind_units** — Wind uses a km/h value with a knots label; convert only Pirate by dividing by 1.852, retaining nautical character. `src/ui_strings.h:270`. ([source](https://www.aoml.noaa.gov/hrd/tcfaq/G4.html))
- **P3 pirate.forecast_time** — Three bells are not three hours. Use an unambiguous 3h label with a Pirate suffix; retain the rest of the voice. `src/ui_strings.h:272`. ([source](https://www.mycg.uscg.mil/News/Article/3616329/eight-bells-and-all-is-well/))
- **P3 german.rss_sync** — Replace broken hybrid 'RSS-Feed wird sync...' with 'RSS wird synchronisiert...' and check wrapping. `src/ui_strings.h:99`.
- **P3 esperanto.touch** — Tusu means cough; the touch imperative is Tuŝu. Dosis supports ŝ. Preserve this distinction rather than stripping the accent. `src/ui_strings.h:181`. ([source](https://www.retavortaro.de/revo/art/tus.html))
- **P3 accents** — Restore accents in directly rendered French/Spanish fields (Météo, où, Génération, conexión). RSS content is still folded to ASCII by sanitizeAsciiBuffer: a table edit alone cannot fix that path. `src/ui_strings.h:75`.
- **Constraint voices** — Preserve Bellazio slang (boh/tipo/slay), Pirate phrasing and Shakespeare's Prithee/anon; do not turn stylistic choices into generic English/Italian. `src/ui_strings.h:209`.
- **P3 web_hero** — Move the adverb: 'Everything you could check faster on your phone, but won't.' Keep the joke and tone. `scrybar.ino:4969`.
- **P3 web_view_descriptions** — INFO description currently refers to word clock, and Mac Stats omits network. Describe actual page contents. `scrybar.ino:5053`.

## Verification still required

- Capture and inspect 7 themes × 10 pages (INFO, HOME, RSS/AUX, WIKI, NOW PLAYING, DOOM, TRANSIT, LAUNCH, MAC STATS, BAMBU). Mark disabled/unavailable pages explicitly; this audit does not stand in for 70 inspected screenshots.
- Exercise Mint warning values, imminent Transit times, Launch TBD/liftoff, Wiki Random without favicon, QR in 14 languages and long/offline/stale states.
- Compare patched output against this source-hashed baseline, inspect a contact sheet, and confirm representative physical-panel results.
- Preserve theme-specific geometry and hue. Bambu/DOOM/artwork and the unified screensaver intentionally contain theme-independent parts.
- Web theme token records are not currently consumed: `appendWebThemeCssVars()` has no callers; the Web page uses fixed `kWebCssCore`. Do not silently expand this LCD repair into a Web theming rewrite.

## Reproduce

```sh
PYTHONDONTWRITEBYTECODE=1 python3 tools/audit_ui_quality_native.py --out-prefix docs/design/2026-10-23-display-quality/baseline
```

Build products, logs and native executables remain in the external temporary build directory. No firmware, USB, NVS, network or Git operation is performed.
