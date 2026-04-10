# Decisions

Concise ADR-style log for stable project decisions.

Entry format:

## YYYY-MM-DD - Title

- Context:
- Decision:
- Impact/Tradeoffs:

---

## 2026-04-10 - Font Unification: Funnel Display Regular + SemiBold, canonical generator (r256)

- Context: Before r256, every `scry_font_funnel_display_*.c` file was generated ad-hoc with whatever `lv_font_conv` flags the author remembered that session. Charsets drifted (some ASCII-only, some Latin-1), fallback sizes were inconsistent (some used a blanket `montserrat_24` which wrecked 14px row heights), and the `FunnelDisplay-Regular.ttf` source had unknown provenance. We needed weight variation (SemiBold) for the LAUNCH hero and international glyph coverage (Andøya, São Paulo, München) across the board. User asked for "the best and most future-proof way, without useless extras — we've had font problems too often".
- Decision: Single typeface **Funnel Display**, two weights **Regular** (wght=400) + **SemiBold** (wght=600), both static-instanced from the same variable font via `fonttools varLib.mutator` so metrics match. Canonical generator script `tools/regen_fonts.sh` as the single source of truth (targets: `all / regular / semibold / countdown`, shared charset and size-matched Montserrat fallback table defined once at the top, **never hand-edit the generated `.c` files**). Regular family: 12 sizes (12/14/16/18/20/22/23/24/25/30/32/38). SemiBold family: narrow set (18/25/32/38) with accessors `lvglFontSmallBold`/`lvglFontLaunchName`/`lvglFontClockBold`/`lvglFontBigBold`. Charset `0x0020–007E` Basic Latin + `0x00A0–00FF` Latin-1 Supplement + `0x0100–017F` Latin Extended-A (Cyrillic/Greek/CJK/Arabic out of scope — names arrive romanized). Narrow countdown font `scry_font_funnel_display_countdown_60.c` (SemiBold TTF, 16 glyphs: `space + - 0..9 : T d`). Fallback rule: each font size-matches Montserrat (12→14, 14→14, 16→16, 18→18, 20→20, 22→22, 23→24, 24→24, 25→24, 30→30, 32→32, 38→38) — never blanket-fallback to 24. Full doc: `knowledge/fonts.md`.
- Impact/Tradeoffs: Flash cost ~+310 KB net (2.53 MB / 15% of 16 MB) — well within the partition budget. Every future font change is a one-liner in the generator script; no more "which flags did I use last time" archaeology. Adding a new size is three files touched (`tools/regen_fonts.sh`, new generated `.c`, `LV_FONT_DECLARE`+accessor in `scrybar.ino`) in one commit. Commits: `e1889cd` (unification) plus follow-ups for countdown `T` glyph.

---

## 2026-04-10 - LAUNCH hero: Cinematic Asymmetric redesign (r256)

- Context: The r253/r255 LAUNCH hero was a full-width single-column layout with the countdown relegated to a secondary role. User wanted a cinematic feel where the countdown dominates visually, with mission info sitting quietly beside it — readable from across the room.
- Decision: Two-column layout in the 640×142 body. Left column (x=0..312): provider badge pill → mission name (SemiBold 25) → vehicle | pad (16 muted, ASCII pipe separator) → location (20) → country (14 muted, 6px clear of bottom). Right column (x=328..640): `LIFTOFF IN` label (16 tracked caps) → dominant 60px narrow SemiBold countdown with `T-` prefix → weather (20px amber) → `TAP · SCAN QR` affordance (14 muted 70%). Countdown formats: `T-HH:MM:SS` when delta<24h, `T-Dd HH:MM` longer, `T-00:00:SS` red in the final minute, sign-less `00:00:00` + red `LIFTOFF` label at/past T-0, `NET` label with `HH:MM` window or `TBD` when `hasT0` is false. `cmdViewLaunch` (`VIEW7`/`LAUNCH` command) resets `viewIndex=0` + calls `lvglSetLaunchView(0)` so manual navigation is deterministic. Weather accent hardcoded `0xFFB74D` amber via `lvglLaunchWeatherAccent(const UiThemeLvglTokens&)` — signature preserved so a future theme-aware variant can swap the body without changing call sites. The theme has no `weatherGlyphSunny` field — don't reach for it. View 1 (compact rows) explicitly NOT touched, deferred to a future session.
- Impact/Tradeoffs: Mission name uses new `lvglFontLaunchName()` (SemiBold 25), badge label uses `lvglFontSmallBold()` (SemiBold 18), countdown uses narrow SemiBold 60 — all sourced from the unified font system. `heroWindow` field left in `LvglLaunchUi` struct but set to `nullptr` in the new init (binary-compatible, any accidental reference crashes cleanly). Spec: `docs/superpowers/specs/2026-04-10-launch-hero-cinematic-redesign.md`. Plan: `docs/superpowers/plans/2026-04-10-launch-hero-cinematic-redesign.md`. Reference screenshot: `knowledge/screenshots/launch_hero_cinematic_r256.png`. Commits: `faa7324` (init geometry) → `1051890` (data + format + r256 bump) → `c3e7b10` (T- prefix + bigger weather) → `9aea2f7` (LIFTOFF IN 16px + QR hint 14px + VIEW7 reset).

---

## 2026-04-01 - Transit: Global API Testing + Fallback Hardening (r247)

- Context: Global testing of Transitous API across 11 stations in 8 countries revealed three new issues: (1) **French RER/Transilien mission codes** — headsign is populated but contains 4-letter internal codes (e.g., "ROPO") instead of destination names; `tripTo.name` has the real destination ("Corbeil-Essonnes"). (2) **Unhandled mode values** — API returns `COACH`, `METRO`, `LONG_DISTANCE`, `NIGHT_RAIL`, `HIGHSPEED_RAIL` which fell through to default grey in `transitCategoryColor()` and `transitIsBus()`. (3) **Renfe near-white routeColor** — `#F2F5F5` (luma ~244) makes badges invisible on light themes.
- Decision: (1) Detect French mission codes: if headsign is exactly 4 uppercase ASCII letters AND `tripTo.name` is available, prefer `tripTo.name`. This covers all known RATP/SNCF mission codes without breaking real short destinations (which are title-case in this API). (2) Add all new mode values to `transitCategoryColor()`: `HIGHSPEED_RAIL`/`LONG_DISTANCE` → red, `NIGHT_RAIL` → dark purple, `METRO` → same as SUBWAY, `COACH` → same as BUS. Add `COACH` to `transitIsBus()` (pill shape). (3) Add routeColor luma guard: if badge color luma > 230, skip API color and use mode-based fallback.
- Impact/Tradeoffs: Mission code heuristic has theoretical false-positive risk for 4-letter all-caps destinations (e.g., "BERN", "LYON", "METZ") but in practice these appear in title case in the Transitous API. The luma=230 threshold skips very light colors while preserving legitimate pastel route colors. Tested stations: Frankfurt Hbf (DE), Berlin Hbf (DE), Paris Gare de Lyon (FR), Zürich HB (CH), Wien Hbf (AT), Madrid Atocha (ES), SF Caltrain (US — empty/no data), NY Penn Station (US, LIRR), Heathrow T5 (UK), Amsterdam Schiphol (NL, FlixBus only), Victoria Coach Station (UK). Additional findings: geocode relevance can return wrong feed (Wien→Ukrainian railways, Frankfurt→nl-OpenOV cross-border only); SF Caltrain had no schedule data for the queried date; UK coach headsigns include departure prefix ("Belgravia, Victoria - Poole").

---

## 2026-04-01 - Transit: GTFS Feed Field Variations + headsign Fallback (r240–r246)

- Context: Transit Departure Board (r240+) uses the Transitous/Motis stoptimes API (`/api/v1/stoptimes?stopId=…`). Three systematic field variations exist across GTFS providers: (1) **Trenitalia (Italy national)** exports `headsign: ""` in all entries (NeTEx format gap); destination is in `tripTo.name`. (2) **Destination filter bug** (r243): original code used `strncasecmp(headsign, filter, len)` — "starts with" semantics — but UI documents "partial match"; trains from a city named in the filter were silently dropped because their outbound headsigns don't start with the city name. (3) **Missing coverage**: some stop IDs from the Transitous geocode return empty `stopTimes` arrays — either a parent-station ID with no departures or a GTFS feed with no data for that region.
- Decision: (1) Parse `tripTo.name` from each stoptimes entry alongside `tripFrom.name`. Use `headsign` when present, fall back to `tripTo.name` (covers Trenitalia + any future NeTEx-origin feed). (2) Replace `strncasecmp` filter with `transitHeadsignContains()` — case-insensitive `strstr`-style substring search. (3) When `count == 0` after a successful HTTP 200 fetch, log `stopId` and `arrFilter` values and dump the first 400 bytes of the payload to serial for debugging. (4) Distinguish "Loading…" (never fetched) from "No departures found" in future UI work.
- Impact/Tradeoffs: Now works for Trenord, Trenitalia, UK rail (London Liverpool Street confirmed), Emilia-Romagna regional (Fidenza). Destination filter now behaves as documented: `"Milano"` matches `"Milano Centrale"`, `"Malpensa via Milano"`, etc. The `headsign → tripTo.name` fallback is safe because when `headsign` is populated it takes priority; `tripTo.name` is only used as a last resort. GOTCHA: some stop IDs from the geocode autocomplete may still return 0 results if the underlying GTFS feed has no departures (regional data gaps). Users should verify the stop via the Transitous web map. **Testing targets for future sessions**: German DB, French SNCF, Swiss SBB, Austrian ÖBB, Spanish Renfe, US Caltrain/BART/NJTransit, Japanese JR, airport people-movers (Heathrow Express, CDG CDGVAL).

---

## 2026-03-27 - Power Button UX: 1.5s Hold + Hard-Off (r218)

- Context: `PWR_HOLD_SHUTDOWN_MS=3000` — 3 seconds with zero visual feedback caused users to release before the threshold. Soft-off (`shutdownFromPowerButton(false)`) only turned off the backlight and entered busy-wait; on battery the MCU kept running. GPIO16 was confirmed correct via serial scan (`[PWR] raw level change: 1 -> 0` detected). Short presses (≤200ms) toggled the screensaver, compounding confusion.
- Decision: (1) Reduce `PWR_HOLD_SHUTDOWN_MS` and `PWR_HOLD_WAKE_MS` to 1500ms. (2) Use `shutdownFromPowerButton(true)` (hard-off) for the long-press: tries TCA9554 SYS_EN cut first (true power-off on battery), falls back to soft-off only if USB holds the chip alive.
- Impact/Tradeoffs: Power-off now feels responsive (1.5s ≈ consumer standard). On battery: true cut via TCA9554 EXIO6. On USB: soft-off fallback (expected — USB supplies the chip regardless). GOTCHA: no visual countdown during hold — if UX still feels unclear, add a progress indicator in a future pass.

---

## 2026-03-27 - Two New Themes: Mint Protocol + Cathode Ray (r217)

- Context: ScryBar had 5 themes, all dark or high-contrast. No light theme existed. A phosphor-green CRT aesthetic was requested. Weather card on Mint Protocol was invisible (screenBg #DBE8DB vs panelBg #FFFFFF — luma gap too small). Light/dark detection was hardcoded per-theme.
- Decision: (1) Add `mint-protocol` (LIGHT — sage green, screenBg #DBE8DB, weatherCardBg #EEF7EF tinted to separate from panelBg). (2) Add `cathode-ray` (DARK — phosphor #33FF33 + amber #FFAA00 on near-black #0A0A08, sharp corners via `lvglThemeIsCathodeRay()`). (3) Replace per-theme light/dark flag with `lvglColorLuma(screenBg) >= 128` auto-detection — zero-config, works for any future theme. (4) Extend landing page: CSS vars in `tokens.css`, pills in `ThemeSelector.astro`.
- Impact/Tradeoffs: 7 themes total. Light theme now available. Luma-based detection means new themes need no boilerplate flag. GOTCHA: pure white weatherCardBg on Mint is visually indistinguishable from panelBg — always tint slightly. GOTCHA: Cathode Ray `--text-muted` must stay dark (#2D5A2D), not light green, or it's unreadable on near-black.

---

## 2026-03-27 - Favicon LRU Cache + CI Full-Site Deployment (r216)

- Context: Favicon cache used FIFO eviction (`memmove` shifting all entries when slot 0 was evicted), which could invalidate `lv_img_dsc_t` pointers LVGL was actively referencing. Wiki deck had no favicon prefetch. Three dead preprocessor constants (`RSS_FAVICON_CACHE_SIZE`, `RSS_FAVICON_MAX_BYTES`, `RSS_FAVICON_RETRY_MS`) were defined but never used. CI workflow was missing `pngle` and `GFX Library for Arduino` libs (added in r215), and deployed only the standalone flasher page instead of the full Astro landing page.
- Decision: (1) Replace FIFO eviction with true LRU: add `lastAccessMs` field to `FaviconCacheEntry`, touch on every cache hit (fetch, prefetch, display read), evict the entry with smallest `lastAccessMs` in-place (no `memmove`). (2) Add Wiki favicon prefetch after `updateWikiFromFeed()`. (3) Remove dead constants. (4) Rewrite CI to build Astro landing page + firmware, deploy `web/dist/` with firmware binaries to GitHub Pages. Add env-based `PAGES_BASE`/`PAGES_SITE` to astro.config.mjs for CI mode (local dev unaffected). (5) Enable GitHub Pages at `enuzzo.github.io/scrybar/` with Actions deployment.
- Impact/Tradeoffs: Eliminates pointer invalidation bug (no memmove). Actively displayed favicons survive cache pressure. Wiki shows favicons now (e.g. it.wikipedia.org). CI deploys full landing page with working flasher at `enuzzo.github.io/scrybar/`. Flash/RAM unchanged (41%/63%).

---

## 2026-03-25 - Unified Typeface: Funnel Display + RSS Favicons (r215)

- Context: Each theme had its own font (Space Mono for cyberpunk, Delius Unicase for toxic candy, built-in Montserrat for default). Different font metrics caused recurring layout bugs (text clipping, inconsistent line counts). RSS source badges were text-only ("NYT", "BBC") with no visual brand identity.
- Decision: (1) Replace all per-theme fonts with Funnel Display (Google Fonts, SIL OFL, condensed display face) as the single typeface across all themes. 12 sizes generated (12-38px), ASCII range with Montserrat fallback. (2) Add favicon system: Google Favicon API (`/s2/favicons?domain=X&sz=32`) → pngle streaming PNG decode → RGB565 PSRAM cache (8 slots, 2KB each). Alpha-blended onto white bg. Prefetched after RSS fetch. (3) Header text y-offset +2px for optical centering with new font metrics. (4) Remove INFO header border stroke.
- Impact/Tradeoffs: 26 font functions (240 lines, theme dispatch) → 21 one-liners. 19 old font files removed, 12 new. Flash -300KB (42%→41%). +10KB from pngle lib. RSS now shows 4 lines (was 3). Favicon cache uses ~16KB PSRAM. DOOM HUD loses monospace aesthetic but gains layout consistency. Zero per-theme font bugs going forward.

---

## 2026-03-25 - M10: Final Polish — Zero Functions >200 Lines

- Context: Two functions remained over the 150-line target: `updateLvglUi()` (210 lines) with a ~130-line weather display block containing 3× identical icon/glyph toggle patterns and 2× identical offline placeholder blocks; `loadRuntimeNetConfigFromNvs()` (202 lines) with inline RSS feed loop (39 lines) and language migration logic (32 lines). Hardcoded WMO code `2` appeared 4× as offline fallback icon.
- Decision: Extract weather helpers (`lvglShowWeatherMainIcon`, `lvglShowWeatherForecastIcon`, `lvglSetWeatherOfflineLabels`) + `lvglUpdateWeatherDisplay()`. Extract NVS helpers (`nvsLoadRssFeeds`, `nvsLoadLanguageConfig`). Add `kWmoFallbackCode` named constant.
- Impact/Tradeoffs: `updateLvglUi()` 210→79, `loadRuntimeNetConfigFromNvs()` 202→132. Functions >200 lines: 0 (all M1-M10 targets met). Sketch 14,879→14,859 (-20 net). Flash/RAM unchanged. **Firmware polishing roadmap COMPLETE.**

---

## 2026-03-24 - M8+M9: Stack Buffer Audit + LVGL Style Helper Library

- Context: Firmware had 3 stack-local buffers >256B (leftCol[512], httpFallback[320], title3[260]) risking stack overflow during concurrent LVGL + HTTP operations. Three LVGL functions (lvglApplyThemeStyles 295 lines, lvglInitFeedDeck 256 lines, lvglInitNowPlayingUi 225 lines) contained massive repetition: 23 identical bg_color+bg_grad_color pairs, 14 guarded text color patterns, 10+ identical 10-line container init blocks, and 3 identical 16-line button init blocks.
- Decision: M8: Move leftCol to PSRAM heap (`heap_caps_malloc` + `heap_caps_free`); shrink httpFallback and title3 to [256] (content fits). M9: Extract 7 shared inline helpers (`lvglSetBgFlat`, `lvglSetBgFlatR`, `lvglSetTextHex`, `lvglSetHeaderBorder`, `lvglSetBtnBorder`, `lvglCreatePanel`, `lvglCreateDeckButton`) and apply across all three functions. Extract feed deck theming loop into `lvglApplyThemeStylesFeedDecks()`.
- Impact/Tradeoffs: Stack buffers >256B: 3 → 0. Function sizes: 295→156, 256→161, 225→148. Net -161 lines (14,879 total). Flash/RAM unchanged (42%/63%). Helpers are reusable for future LVGL code. `lvglCreatePanel()` sets bg_opa=COVER and bg_grad_color=bg_color by default — callers must override if transparency or gradient is needed.

---

## 2026-03-24 - M7: Global State Grouping into 16 Structs

- Context: Firmware had 244 individual `g_`-prefixed globals scattered across `scrybar.ino`, making it hard to reason about subsystem boundaries and increasing cognitive load when navigating the codebase. The polishing roadmap targeted <80 `g_` names via struct grouping.
- Decision: Group 209 globals into 16 typed structs (`BatteryState`, `WifiState`, `TouchState`, `DoomState`, `ScreensaverState`, `ClockState`, `PerfCounters`, `WebConfigState`, `LvglClockUi`, `LvglWeatherUi`, `LvglInfoUi`, `DisplayHwState`, etc.). Each struct instance retains a `g_` prefix for grep-ability (e.g. `g_batt.voltage`, `g_wifiSt.connected`). Struct definitions sit inside the same `#if` feature-gate blocks as the original variables. Automated via `tools/m7_rename_globals.py` (Python transform script with declaration block replacement + word-boundary rename pass).
- Impact/Tradeoffs: 244 → 51 `g_` names (16 struct instances + 35 truly independent). Zero behavioral change. Flash/RAM unchanged (42%/63%). Struct member ordering is intentionally identical to the original declaration order to minimize diff noise. The transform script is kept in `tools/` for reference but is single-use. Gotcha discovered: enum types used inside structs (e.g. `TouchAuxButton`) must be forward-declared before the struct definition — the script initially placed the struct before the enum, caught at compile time and fixed.

---

## 2026-03-23 - Remove spoo.me URL Shortener

- Context: QR codes for RSS/Wiki used spoo.me to shorten URLs before encoding. This caused blocking HTTP calls (~2-3s), burned free API credits, added privacy leak (all URLs transit a third party), and was a single point of failure.
- Decision: Remove spoo.me entirely. QR codes use the original direct URL. At 172×172px (full viewport height), even URLs of 150+ chars produce scannable QR codes on any modern phone.
- Impact/Tradeoffs: QR generation is now instant (no network call). Zero external dependency. Zero API credits. Simpler code. Future consideration: if QR readability ever becomes an issue, evaluate self-hosted shortener or user-provided API key — but 172px QR is more than sufficient.

---

## 2026-03-23 - QR Hint Text Hardcoded English (Not Localized)

- Context: QR overlay hint text ("Tap anywhere to close") was using `activeUiStrings()->touchToClose`, which localized it per language — e.g., "74P 4NYwH3R3 70 CL053" in l33t speak. This made the hint unreadable.
- Decision: QR hint is hardcoded English: "Tap anywhere to close". System/UX labels that are about device operation (not content) should always be universally readable.
- Impact/Tradeoffs: Consistent UX across all 13 languages. The `touchToClose` field remains in UiStrings for potential future use elsewhere.

---

## 2026-03-24 - Web UI: Fixed scrybar-default Theme + Flat Layout

- Context: The web config UI used a CSS bridge layer (`appendWebThemeCssVars` → `--txt`/`--acc1` intermediaries → vibemilk aliases) so the page restyled with every display theme switch. Maintaining per-theme visual parity was high-effort for a config tool. Hero section used a card-in-card pattern (`hero-top-card` inside `hero`). `vm-card--inner` wrappers added unnecessary DOM depth. Mobile padding wasted ~50px horizontal space on 375px screens.
- Decision: (1) Replaced CSS bridge with hardcoded scrybar-default tokens directly in `:root{}` — one set of variable names, zero indirection. (2) Removed `data-theme` from `<body>`, removed `appendWebThemeCssVars` call from web builder path. (3) Flattened hero: removed `hero-top-card` and `hero-copy` wrappers, logo/release/lede flow directly in `section.hero`. (4) Removed all `vm-card--inner` wrappers from Views, WiFi, RSS builders. (5) Tightened mobile CSS: `vm-wrap` 12px/10px, `vm-card` 14px/12px, grid gap 8px, logo 40px. (6) Simplified Google Fonts load to Montserrat only (other theme fonts no longer needed). Theme selector dropdown stays in the form — it still drives the ESP32 display theme.
- Impact/Tradeoffs: Simpler `buildWebCssBlock()` (one line), less PROGMEM (~1.2KB saved from bridge removal + font URL), fewer visual bugs, better mobile density. `UiThemeWebTokens` struct and `appendWebThemeCssVars()` remain in codebase for design system use but are no longer called by the embedded page.

---

## 2026-03-20 - Systematic Firmware Polishing Pass (M1-M10)

- Context: Firmware at r199 is feature-complete and stable but has accumulated structural debt: 10 functions over 200 lines, 56 duplicated language dispatchers (1,855 lines), 249 global variables, and inconsistent memory allocation patterns. Landing page migration to Astro SSG is done; focus shifts to production-grade code quality.
- Decision: Execute a structured 10-milestone polishing roadmap (`knowledge/firmware_polishing_roadmap.md`) targeting: function decomposition (no function >150 lines), table-driven language dispatch, global state grouping into structs, stack buffer audit, and web page builder refactoring. One milestone per session, zero regressions, device verification mandatory.
- Impact/Tradeoffs: No new features during polishing. Every milestone must compile, upload, and pass visual verification. The codebase becomes significantly more maintainable and ready for the next feature wave. Risk is low — pure refactoring with behavioral parity checks.

---

## 2026-03-20 - Landing Page Migration from React SPA to Astro SSG

- Context: The landing page was built with Vite + React 19, producing a ~140KB gzipped JS bundle for what is entirely static content. SEO was poor (SPA shell), Lighthouse scores suboptimal.
- Decision: Replace with Astro 5 SSG. All React components converted to `.astro` files with vanilla JS inlined for interactivity (theme switch, language cycling, glare). Unified typography (Syne + Montserrat) across all 5 themes to prevent layout shift. CSS color transitions (~0.45s) for seamless theme switching.
- Impact/Tradeoffs: Production output drops from ~146KB to ~10.5KB gzipped (93% reduction). Zero JS files in build output. SEO-perfect static HTML. Build time under 600ms. Tradeoff: Astro is an additional toolchain dependency, but minimal (single `astro` package, no framework plugins).

---

## 2026-03-19 - Switch from MediaRemote C API to JXA/osascript Bridge for Now Playing

- Context: macOS 15.4 introduced entitlement verification in `mediaremoted` that blocks unsigned apps from using `MRMediaRemoteGetNowPlayingInfo` and related C function APIs. The companion's `MediaRemoteBridge` (dlopen + semaphore approach) stopped receiving data entirely — callbacks returned `Operation not permitted` (error code 3). Notification registration was silently accepted but never fired. Multiple workarounds attempted (separate queues, notification-based caching) all failed because the block is at the framework/daemon level.
- Decision: Replace the direct C function bridge with a JXA (JavaScript for Automation) script executed via `/usr/bin/osascript`. The system binary has full MediaRemote entitlements. The script accesses `MRNowPlayingRequest` (Obj-C class) through the JXA bridge, returning JSON with title, artist, album, duration, elapsed, playback state, client bundle ID, and artwork identifier. Artwork resolution uses a three-tier strategy: direct URL (Apple Music), template expansion (Podcasts `{w}x{h}bb.{f}`), or iTunes Search API fallback (TIDAL and others). Process runs in `Task.detached` to avoid blocking the main thread.
- Impact/Tradeoffs: Works reliably across TIDAL, Apple Music, Podcasts, and any other MediaRemote-registered source. ~50-100ms per osascript invocation (acceptable at 1s polling). Binary artwork data (`kMRMediaRemoteNowPlayingInfoArtworkData`) is NOT extractable via JXA (blob type not bridged), so artwork must come from URLs or API fallback. Future macOS updates could change JXA bridge behavior, but Apple can't revoke entitlements from `/usr/bin/osascript` without breaking the entire automation ecosystem.

---

## 2026-03-18 - Local macOS Companion Uses `MediaRemote.framework` as Primary Now Playing Source

- Context: The product needs a practical, universal `Now Playing` feed for a local GitHub-distributed macOS companion, not an App Store-safe media app. Public Apple media APIs are oriented around publishing metadata for the current app, while the Mac already exposes a system-wide Now Playing feed internally.
- Decision: Use private `MediaRemote.framework` as the primary source for the companion's system-wide now-playing metadata, and keep app-specific providers such as `Music.app` only as fallback/special-case integrations.
- Impact/Tradeoffs: The companion can read the same Now Playing session shown by macOS Control Center, including title/artist/album/playback state and artwork bytes. The tradeoff is reliance on private Apple API, which is acceptable for a local hacker tool but may require maintenance after macOS updates.

---

## 2026-03-11 - DOOM Uses Extracted `prboom-go` Donor and Direct Framebuffer Path

- Context: ScryBar needed real DOOM on-device, but importing all of `retro-go` would have carried too much platform baggage and the LVGL page model was not appropriate for live game frames.
- Decision: Vendor only the `prboom-go` donor core under `src/doom/prboom/`, add a thin ScryBar runtime shim (`src/doom/scrybar_prboom.cpp`), embed the current IWAD/title assets locally, and render DOOM through the existing direct canvas + `dispFlush()` path instead of LVGL widgets.
- Impact/Tradeoffs: Real gameplay works on hardware with centered 4:3 pillarboxing and side-band HUD/touch controls. The app footprint is much larger, so the project now depends on a checked-in custom partition layout and tighter RAM budgeting.

---

## 2026-03-11 - DOOM IMU Runs Only Inside DOOM and Calibrates After Settle

- Context: Leaving IMU polling active globally wasted CPU and spammed serial logs, while taking the first IMU sample as neutral on page entry caused drift and visible HUD flicker.
- Decision: Enable accelerometer/gyro only while `UI_PAGE_DOOM` is active; on entry or recenter, reset the tilt filter, wait a short arm delay, require a stable low-motion window, average multiple samples for neutral, and smooth deltas before quantization.
- Impact/Tradeoffs: DOOM controls are stable at rest and the rest of the product stays quiet/cheap when DOOM is not active. There is a small calibration settle delay when entering the page, accepted as the safer baseline.

---

## 2026-03-11 - Keep Display DMA Flush Chunks at 32 Rows

- Context: During DOOM integration, larger `64`-row DMA buffers increased internal heap pressure enough to break unrelated TLS handshakes for RSS/Wikipedia.
- Decision: Keep `DB_CHUNK_ROWS=32` as the stable default for the rotated direct-flush display pipeline.
- Impact/Tradeoffs: Slightly more DMA chunk launches per full frame, but better coexistence between graphics throughput and HTTPS/network reliability.

---

## 2026-02-28 - Public Cross-Assistant Knowledge Layer

- Context: Project needs a provider-agnostic memory layer reusable by Codex, Claude, Gemini, and humans.
- Decision: Keep shared instructions and stable knowledge in versioned `knowledge/`; keep private operational logs in local non-versioned folders.
- Impact/Tradeoffs: Better portability and transparency; less room for private/debug detail in public docs, which must stay sanitized.

---

## 2026-02-28 - LV_COLOR_16_SWAP=1 for AXS15231B Display

- Context: AXS15231B display controller expects RGB565 bytes in big-endian order; default LVGL output is little-endian, producing wrong colors.
- Decision: Set `LV_COLOR_16_SWAP=1` in `lv_conf.h` as a permanent baseline for this hardware.
- Impact/Tradeoffs: Correct colors on the target display; frame dumps on wire are `rgb565be` and must be decoded accordingly by capture tools.

---

## 2026-02-28 - Hard Power-Off Not Mapped to Physical Button

- Context: Mapping hard-off to the physical button risked device entering a non-recoverable state (requiring a hardware power cycle) from an accidental long-press.
- Decision: Hard-off is only accessible via serial command `PWROFFHARD`; the physical button handles soft-off (`3s`) and wake (`3s`) only.
- Impact/Tradeoffs: Safer user-facing UX with no accidental hard lock; technical hard-off still reachable for diagnostics.

---

## 2026-02-28 - Word Clock as Natural Italian Sentence

- Context: Two display styles were considered: uppercase block-word (common scrabble-tile aesthetic) and lowercase natural sentence.
- Decision: Use natural Italian sentence form via `composeWordClockSentenceIt` — no uppercase blocks.
- Impact/Tradeoffs: More readable at a glance and consistent with the "readable, not gimmicky" product principle; requires a dedicated large font (`Montserrat 38`) for legibility.

---

## 2026-02-28 - full_refresh=1 in LVGL Display Driver

- Context: Partial refresh with the current software rotation on AXS15231B produced tearing and buffer misalignment artifacts.
- Decision: Set `full_refresh=1` in the LVGL display driver as permanent baseline.
- Impact/Tradeoffs: Eliminates tearing artifacts; marginally higher bus bandwidth per frame, acceptable on this hardware.

---

## 2026-02-28 - Multi-Language Word Clock Infrastructure

- Context: Word clock was hard-coded Italian. The TODO roadmap planned multi-language support; Klingon was identified as the first addition.
- Decision: Introduce `WORD_CLOCK_LANG_DEFAULT` in `config.h`, a global `g_wordClockLang[16]`, a `composeWordClockSentenceActive()` dispatcher, and NVS key `wc_lang` for persistence. The web config UI exposes a language selector.
- Impact/Tradeoffs: Clean extension path for new languages (add `composeWordClockSentence<Lang>()` + register in dispatcher + whitelist in web handler). NVS key `wc_lang` is 7 chars, well within the ESP32 NVS 15-char key limit.

---

## 2026-02-28 - Full UI Display Localization via g_wordClockLang

- Context: Weather panel, RSS panel, and touch hints were hard-coded in Italian while the word clock already supported 10 languages via `g_wordClockLang`.
- Decision: Add `src/ui_strings.h` with a `UiStrings` struct and 10 static instances (it, en, fr, de, es, pt, la, eo, nap, tlh). Add `activeUiStrings()` dispatcher that returns the correct instance. Add `weatherCodeUiLabel()`/`weatherCodeShort()` dispatchers mirroring the word clock dispatcher pattern. Replace all hard-coded Italian UI strings in `scrybar.ino` with calls through these dispatchers.
- Impact/Tradeoffs: UI language is now fully consistent with the word clock language setting. No explicit refresh needed — render functions are called every frame. Flash cost is ~8 KB (negligible on 16 MB flash). The web config UI remains in English by design.

---

## 2026-03-01 - README and GitHub Description Reflect Full Language Roster

- Context: README still described ScryBar as an "Italian word clock" with other languages listed as "coming soon", despite 10 languages being fully implemented since r136.
- Decision: Update README tagline, intro, HOME view description, Open Source Spirit section, and footer to reflect all 10 built-in languages. Add dedicated `## Word Clock Languages` section with table and example sentences. Update GitHub repo description and topics accordingly.
- Impact/Tradeoffs: Accurate public representation; Klingon, Latin, Esperanto, and Neapolitan are key discoverability hooks for stars and forks. GitHub topics added: `word-clock`, `esp32-s3`, `lvgl`, `iot`, `embedded`, `waveshare`, `open-source`, and others.

---

## 2026-03-03 - r140: 4 New Languages, Neapolitan Revamp, Web UI optgroup

- Context: Word clock had 10 languages (r139). Roadmap planned fun/creative languages. Neapolitan was implemented but with generic Italian-style number words and no authentic "manco" structure. Web UI language `<select>` had no visual grouping between fun and standard languages.
- Decision:
  - Added 4 new languages: `l33t` (1337 Speak), `sha` (Shakespearean English, rotating exclamations via `h12%6`), `val` (Valley Girl), `bellazio` (Italian Bellazio — boh/tipo/letteralmente/for real/slay).
  - Revamped Neapolitan (`nap`) from scratch using wikibooks Napoletano resources: authentic number words (`seje`, `unnece`, `dudece`, `cinche`, `diece`, `vinte`), authentic "manco" structure for "to" times (`'e quatte manco nu quarto`), correct raddoppiamento (`ll'una` for 1 o'clock), authentic month/weekday names (`jennaro`, `dummeneca`, etc.), and authentic weather vocab (`assulato`, `schizzechea`, `tempurale`).
  - Split web UI language `<select>` into two `<optgroup>` groups: "Creative & Constructed" (bellazio, val, l33t, sha, nap, eo, la, tlh) on top, "Modern Languages" (en, it, es, fr, de, pt) below. Each standard language is labelled in its own language.
  - `kAllowed[]` updated to 14 entries; all 5 dispatchers updated.
- Impact/Tradeoffs: Language count reaches 14. `ui_strings.h` grows to 14 `UiStrings` instances. Adding further languages is mechanical: add `composeWordClockSentence*`, `weatherCodeShort*`, `weatherCodeUiLabel*`, `formatDate*`, `kUiLang_*`, register in `kAllowed[]`, add to `kLangsFun[]` or `kLangsStd[]`, add to all 5 dispatchers.

---

## 2026-02-28 - Klingon Word Clock Uses ASCII Transliteration

- Context: pIqaD (native Klingon script) has no coverage in the Montserrat 38 font loaded on device. Including a second font would significantly increase flash usage.
- Decision: Klingon word clock uses ASCII transliteration (tlhIngan Hol romanization). Format: `"DaH [ora] rep [minuti] tup"`. Example: `"DaH wej rep wa'maH vagh tup"` = 3:15.
- Impact/Tradeoffs: No font change required; all Klingon strings are ASCII and fit comfortably within the 96-char sentence buffer. Downside: purists may object to non-pIqaD rendering.

---

## 2026-03-04 - Unified Runtime Theming Across Firmware, Web UI, and Design System

- Context: Theme styling had to stay coherent across three surfaces: LVGL device UI, embedded web control surface, and standalone design system documentation.
- Decision: Centralize runtime themes in firmware through `kUiThemes[]` with two token sets per theme (`UiThemeLvglTokens` + `UiThemeWebTokens`) and keep the same theme ids in design system (`scrybar-default`, `cyberpunk-2077`, `toxic-candy`, `tokyo-transit`, `minimal-brutalist-mono`).
- Impact/Tradeoffs: Theme switching is now one conceptual model across product and docs; adding a new theme requires touching both firmware tokens and design-system CSS variables, but naming remains consistent and migration risk is lower.

---

## 2026-03-04 - Static Non-Variable Font Pipeline for Theme Fonts

- Context: Variable fonts produced unreliable rendering/conversion behavior in the ESP32 LVGL toolchain and risked regressions after flash.
- Decision: Use static TTFs only and generate LVGL fonts via `lv_font_conv --no-compress` in fixed sizes. Cyberpunk uses Space Mono; Toxic Candy uses Delius Unicase; default continues with Montserrat built-ins.
- Impact/Tradeoffs: Predictable output and stable embedded rendering; larger flash footprint due to multiple generated sizes, but acceptable with `app3M_fat9M_16MB` partition.

---

## 2026-03-04 - Clock Sentence Auto-Fit by Theme Font Candidates

- Context: Clock text had inconsistent visual fill across themes and languages when fixed font sizes were used.
- Decision: Introduce runtime auto-fit for `g_lvglClockL1`, selecting the largest fitting font from a theme-specific ordered candidate list and applying line spacing per selected font height.
- Impact/Tradeoffs: Better readability and consistent visual hierarchy; slight runtime layout overhead on clock text updates, acceptable on ESP32-S3.

---

## 2026-03-04 - Weather Panel Readability Rule for Transparent Icon Pack

- Context: Weather icons are authored for transparency over light backgrounds and degraded visually on dark themed weather cards.
- Decision: Enforce a light weather background with dark text/glyph fallback when active theme weather colors do not meet readability thresholds.
- Impact/Tradeoffs: Theme purity is slightly reduced in weather panel for some palettes, but icon legibility and UI clarity are consistently preserved.

---

## 2026-03-04 - Theme Exploration Catalog in Public Knowledge

- Context: Theme ideation was happening in chat threads and risked being lost across sessions, making it harder for future assistants and contributors to continue consistently.
- Decision: Add and maintain `knowledge/theme_proposals_catalog.md` as the canonical backlog for visual theme concepts, including palette seeds, static font recommendations, references, and implementation notes.
- Impact/Tradeoffs: Better continuity and discoverability; requires periodic curation to keep links and priorities fresh.

---

## 2026-03-04 - Minimal Brutalist Mono Theme as First-Class Runtime Theme

- Context: Third production theme was requested with a strict monochrome brutalist visual language, but still needed to remain swappable with the same runtime theming model used by existing themes.
- Decision: Add `minimal-brutalist-mono` to the shared theme registry (`kUiThemes[]`) with both web and LVGL token sets, wire it into design system selector/JS support, and map typography to `IBM Plex Mono` for web and `scry_font_space_mono_*` for LVGL fallback consistency.
- Impact/Tradeoffs: Fast runtime swaps across firmware UI and web UI remain intact; visual identity is distinct and high-contrast, while embedded font pipeline stays stable without introducing new LVGL font assets.

---

## 2026-03-04 - Bellazio Clock Grammar + Slang Always-On Policy

- Context: Bellazio clock strings had grammatical defects (`a le`) and numeric minute rendering that reduced natural Italian readability.
- Decision: For `bellazio`, enforce minute words (`cinque`, `dieci`, `venti`, `venticinque`) and correct prepositions (`all'`, `alle`), while keeping slang always present via rotating lead/closer phrases and additional variants such as `una roba tipo ...`.
- Impact/Tradeoffs: Output remains playful and varied without sacrificing grammatical correctness; deterministic variation avoids visual flicker while preserving humor in daily use.

---

## 2026-03-05 - Screensaver Cow Thoughts Localized by `wc_lang`

- Context: The screensaver balloon had Italian-only quotes while the rest of the UI already followed the runtime language pivot (`wc_lang`).
- Decision: Introduce language-specific quote packs for all supported language codes and resolve the active pack at runtime via `g_wordClockLang`, with Italian as default fallback.
- Impact/Tradeoffs: Screensaver now feels consistent with full UI localization; modest flash growth due to additional strings, but still well within current partition headroom.

---

## 2026-03-05 - Exclude Unused Audio Stack from Active Compile Path

- Context: Legacy `src/audio` codec/microphone stack was being compiled in the sketch build pipeline even though no symbols were linked in the final firmware.
- Decision: Move the whole audio subtree from `src/audio` to `vendor/audio`, keeping sources in-repo but outside the Arduino sketch compile path.
- Impact/Tradeoffs: No runtime behavior change and no feature loss for active firmware; cleaner build surface and less compile-time noise, while preserving the codebase for possible future audio reactivation.

---

## 2026-03-05 - UTF-8/Entity Sanitization Pipeline for Display-Safe Text

- Context: A growing set of language strings and RSS titles included typographic UTF-8 glyphs (smart quotes, ellipsis, bullets, accented forms) not always covered by active LVGL fonts, causing replacement boxes on screen.
- Decision: Add a shared transliteration pipeline (`decodeHtmlEntitiesToAscii` + UTF-8 codepoint folding) and apply it to clock sentence, date line, and RSS text surfaces before render. Replace known non-ASCII Bellazio literals with ASCII-safe equivalents.
- Impact/Tradeoffs: Display output is now robust against unsupported glyphs and unpredictable feed encodings. Minor semantic loss on diacritics/special symbols is accepted in favor of guaranteed legibility on embedded fonts.

---

## 2026-03-05 - Web UI Dropdown for Known Wi-Fi Selection

- Context: Wi-Fi credentials are already configured in `secrets.h`, but the web panel lacked a direct control to prioritize one known SSID without editing firmware.
- Decision: Add a `wifi_pref_ssid` dropdown in web config populated from known SSIDs, persist preferred SSID to NVS key `wifi_pref`, expose it via `/api/config`, and trigger reconnect logic when preference changes.
- Impact/Tradeoffs: Faster field switching between known networks and clearer operator control. Scope is intentionally limited to preconfigured SSIDs (no runtime password entry), preserving security and simplicity.

---

## 2026-03-05 - Add Wi-Fi Direct Fallback + Runtime Provisioning

- Context: Device needed true field mobility: if none of the known SSIDs is reachable, operators still need a way to open web config and onboard a new AP/hotspot without reflashing.
- Decision: Introduce Wi-Fi direct setup mode (`off|auto|on`) with AP+STA behavior, auto-fallback AP on prolonged disconnect, `GET /api/wifi/scan` (2.4 GHz list), and runtime credential storage in NVS (`wifi_dyn_*`) merged with `secrets.h` credentials for reconnect rotation.
- Impact/Tradeoffs: Stronger real-world reliability and faster deployment in unknown environments (hotspots/open APs). Slightly higher NVS write surface and additional complexity in Wi-Fi state machine accepted to remove reflashing dependency.

---

## 2026-03-05 - Harden AP Setup Reliability (Scan Timeout + QR Handler Stack Safety)

- Context: In AP setup mode, Wi-Fi scan requests could appear stuck in UI, and the setup flow experienced panic/reboot instability tied to heavy QR generation callback stack use.
- Decision: Make `/api/wifi/scan` bounded and non-hanging (timeout-aware async scan completion, explicit `scan_timeout`/`scan_failed` signaling), add front-end abort timeout handling, and move qrcodegen work buffers from callback stack to static storage.
- Impact/Tradeoffs: Setup experience becomes deterministic under poor RF conditions and AP-only mode; no more indefinite "Scanning..." UX and no stack-canary panic from QR rendering path. Slight static RAM increase accepted for stability.

---

## 2026-03-05 - Force Monospace + Visibility Toggle for Wi-Fi Password Input

- Context: Theme typography can be stylized or uppercase-heavy, making password entry visually ambiguous during provisioning.
- Decision: Enforce monospaced rendering for `wifi_new_password` regardless of theme and add explicit eye toggle for show/hide state in web config UI.
- Impact/Tradeoffs: Better operator accuracy and faster debugging in field setups, with negligible UI complexity increase.

---

## 2026-03-05 - Keep RSS Deck Stable and Add Wiki as Dedicated View

- Context: Wiki ingestion was introduced as content expansion, but a regression risked replacing existing RSS runtime feeds/userspace behavior.
- Decision: Preserve AUX/RSS behavior unchanged (runtime-configurable up to 5 feeds) and add Wiki as a separate dedicated view (`UI_PAGE_WIKI`) with its own fixed 3-source rotation and independent state.
- Impact/Tradeoffs: Existing RSS operators keep their current feed setup and controls; Wiki adds extra value without configuration churn. Slightly higher code/UI complexity is accepted to isolate concerns and avoid regressions.

---

## 2026-03-13 - Power Long-Press Triggers While Held and Wake Returns to Home

- Context: Requiring power-button release after the hold window felt unnatural compared to normal consumer devices, and restart-based wake could re-enter transient pages like DOOM instead of the default home view.
- Decision: Trigger physical `PWR` long-press actions as soon as the `3s` hold threshold is crossed while the button is still held. On soft-off wake, resume in-place without `esp_restart()`, force `UI_PAGE_HOME`, and ignore the still-held key until release to avoid immediate re-trigger.
- Impact/Tradeoffs: Power behavior now feels device-like and predictable; wake is faster and consistently returns to clock/weather. Slightly more button state complexity is accepted to prevent accidental re-shutdown loops.

---

## 2026-03-06 - Debounced Power Short-Press Path for Screensaver

- Context: Field behavior showed sporadic unintended screensaver activation likely caused by button bounce/noise on the power key line.
- Decision: Add explicit press-side debounce (`45ms`) plus minimum short-press duration (`90ms`) before triggering screensaver; sub-threshold pulses are ignored as glitches.
- Impact/Tradeoffs: Greatly reduced false positives with negligible interaction latency increase; genuine taps still feel immediate to users.

---

## 2026-03-06 - Wiki Thumbnail Compatibility via PNG Preference + JPEG JFIF Normalization

- Context: Remote Wiki images (especially modern JPEG variants) were inconsistently decoded by LVGL on target firmware builds.
- Decision: Prefer Wikimedia PNG thumb variants when derivable, and normalize JPEG payloads by injecting a JFIF APP0 marker when missing before decode.
- Impact/Tradeoffs: Better thumbnail render success rate across feeds; modest memory/cpu overhead for payload normalization and larger thumb budget.

---

## 2026-03-06 - Keep Web Config Responsive During Feed I/O

- Context: Web UI could feel stalled while firmware was in blocking RSS/WIKI network operations.
- Decision: Pump HTTP config server loop during long I/O sections and load remote web CSS assets asynchronously (critical layout remains inline).
- Impact/Tradeoffs: Faster perceived Web UI readiness and reduced request starvation during feed fetches; small increase in loop complexity.

---

## 2026-03-20 - r197/r198: Web UI Vibemilk DS Migration + Layout Flattening

- Context: The web config UI had grown to ~35KB of inline HTML with 5 CSS keyframe animations, 8 FX grid divs, backdrop-filter blur, Font Awesome CDN dependency (~40 icon refs), and 3 responsive breakpoints. Heavy for an ESP32 serving over WiFi, and non-functional offline in AP setup mode (Font Awesome CDN unreachable).
- Decision: (1) Replace all custom CSS with a Vibemilk DS v3 subset (~3KB minified) using `vm-*` classes. (2) Add a CSS bridge layer mapping firmware tokens (`--txt`, `--acc1`, etc.) to vibemilk standard names (`--text-primary`, `--accent-primary`). (3) Remove all animations, FX grid, backdrop-filter. (4) Replace Font Awesome icons with HTML entity emoji (`&#x1F3A8;` etc.) or plain text. (5) Flatten layout: one visible `vm-card` per section, no inner bordered containers. View items use `border-bottom` separators. RSS rows use left accent bar. System Info is plain grid. (6) Google Fonts loaded async with system fallback for offline AP mode.
- Impact/Tradeoffs: ~35KB → ~20KB output. Zero CDN dependencies for functionality. All 5 themes render correctly. All 8 config sections and JS logic preserved. Mobile and desktop layouts work from single 768px breakpoint. Raw UTF-8 emoji in `F()` strings were double-encoded through the Arduino toolchain — fixed by using HTML numeric entities exclusively.

---

## 2026-03-08 - r159: Full LVGL Widget Tree for WIKI Page (Wiki Deck)

- Context: After r158 promoted WIKI to an independent page with its own `g_lvglWikiRoot`, the page showed only a static "WIKI" placeholder label. All RSS/Wiki content rendering was still routed through the shared AUX deck widgets (`g_lvglAux*`), making touch buttons and QR modal non-functional on WIKI.
- Decision: Duplicate the AUX deck widget set inside `g_lvglWikiRoot` (20+ `g_lvglWiki*` globals mirroring `g_lvglAux*`). Add `lvglUpdateWikiDeck()` as an independent render function always reading from `g_wiki`. Add `lvglFeed*` dispatch helpers in the touch handler so button detection, button visual state, QR modal open/close, and news-tap all route to the correct deck based on `g_uiPageMode`. Split `updateLvglUi()` dispatch: `UI_PAGE_AUX` → `lvglUpdateAuxRss`, `UI_PAGE_WIKI` → `lvglUpdateWikiDeck`. Mirror theming (`lvglApplyThemeStyles`) and font assignment (`lvglApplyThemeFonts`) for Wiki widgets. Also added full toolchain setup documentation to `knowledge/project_knowledge.md`.
- Impact/Tradeoffs: ~400 lines added; RAM/flash footprint unchanged (70%/48%). Both decks are fully independent and testable separately. Each has its own QR modal state, lastItemShown counter, and QR payload cache. The AUX↔WIKI shared logic in `uiPageUsesAuxDeck()` is retained for swipe/drag guards (harmless) while per-deck dispatch is used for all interactive operations. Future refactor opportunity: `FeedDeckUi` struct to replace parallel globals.

---

## 2026-03-07 - r158: Remove Photo/Thumbnail Code and Add 4th Independent WIKI Page

- Context: RSS and Wiki decks had grown a large photo/thumbnail/favicon pipeline (~750 lines: 17 functions, 4 preload steps, 3 cache structs, multiple PSRAM image buffers). The pipeline added complexity and RAM pressure without reliable benefit on embedded LVGL fonts. Separately, the WIKI view was sharing ordinal 2 with AUX via special-case code rather than being a true independent page.
- Decision: (1) Remove all favicon/thumbnail/photo code from RSS and Wiki entirely — no images, no cache structs, no binary HTTP fetch, no JFIF normalization. Keep only `wikiPreloadMetaStep` for text-summary enrichment. (2) Promote WIKI to ordinal 3 as a fully independent swipeable page (`g_lvglWikiRoot`), removing all AUX↔WIKI special-case logic from the touch handler and using uniform `stepUiPage(±1, false)` + `kMaxPageOrd=3`. Edge damping now triggers at cur==0 (left) and cur==3 (right).
- Impact/Tradeoffs: ~1,000 lines removed (code + config constants); display pipeline is simpler and more reliable. 4-page navigation is uniform and extensible. WIKI page currently shows a placeholder "WIKI" label — full deck UI (header + news + badge + QR) is the next implementation target. lv_tileview was evaluated and rejected: HIDDEN pages cost zero CPU while tileview renders all visible tiles during transitions, which is worse for this 4-page layout.

---

## 2026-03-29 - r219-r225: Performance Overhaul — Network Isolation + Display Pipeline

- Context: Swipe animations stuttered whenever network fetches ran on Core 0. RSS took 2-3s, Wiki 7-10s, weather up to 75s (streaming loop bug) — all blocking LVGL on Core 0. Display flush used a pixel-by-pixel copy loop, single LVGL buffer, and HTTP/1.0 with Connection:close.
- Decision: **Phase 1** (r219): Move all HTTP I/O to a dedicated FreeRTOS `netTask` pinned to Core 1, with `xQueueCreate/xQueueSend` dispatch from Core 0 and `xSemaphoreMutex` for result hand-off. `mbedtls_platform_set_calloc_free(psramCalloc, psramFree)` redirects TLS heap to PSRAM in the netTask. Weather, RSS, Wiki, favicons, wiki-meta, now-playing all run on Core 1. **Phase 2** (r220): `DB_CHUNK_ROWS` 32→64 (halves DMA semaphore overhead), tile T 8→16 (PSRAM cache-line aligned), `lvglDisplayFlushCb` pixel loop → `memcpy`, LVGL double-buffer (2×215KB PSRAM), adaptive LVGL cadence 8ms (animating) / 20ms (idle). **Phase 3** (r221-r225): `extractJsonNumber*/ArrayNumber*/ArrayString*` migrated from `String`→`const char*`, weather URL `String` concatenation (8 allocs) → `snprintf` into `char[400]`, `http.getString()` for reliable chunked response handling, `useHTTP10(true)` / `Connection: close` removed from all 3 fetch functions (HTTP/1.1 default), `lastFetchMs != 0` guard added to weather time-gate (matched RSS/wiki pattern; fixed ~60s weather boot delay).
- Impact/Tradeoffs: flush avg 21ms→16ms, max ~19ms (idle). LVGL handler avg ~1.5ms. netTask stack HWM 6384→9792 bytes remaining after HTTP/1.1 switch. Weather resolves at boot in ~226ms (was 60s+ delay). Zero queue overflows in steady state. free_heap stable at 21.6KB internal. PSRAM used: ~430KB for dual LVGL buffers. Core 0 never blocks on HTTP — swipes are smooth during all network activity.

---
