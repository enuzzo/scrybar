# Decisions

Concise ADR-style log for stable project decisions.

Entry format:

## YYYY-MM-DD - Title

- Context:
- Decision:
- Impact/Tradeoffs:

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
