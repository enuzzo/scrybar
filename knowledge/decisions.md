# Decisions

Concise ADR-style log for stable project decisions.

Entry format:

## YYYY-MM-DD - Title

- Context:
- Decision:
- Impact/Tradeoffs:

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
- Decision: Hard-off is only accessible via serial command `PWROFFHARD`; the physical button handles soft-off (5s) and wake (5–6s) only.
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
