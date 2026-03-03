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
- Decision: Introduce `WORD_CLOCK_LANG_DEFAULT` in `config.h`, a global `g_wordClockLang[8]`, a `composeWordClockSentenceActive()` dispatcher, and NVS key `wc_lang` for persistence. The web config UI exposes a language selector.
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
  - Added 4 new languages: `l33t` (1337 Speak), `sha` (Shakespearean English, rotating exclamations via `h12%6`), `val` (Valley Girl), `genz` (Italian Gen Z — boh/tipo/letteralmente/ngl/slay).
  - Revamped Neapolitan (`nap`) from scratch using wikibooks Napoletano resources: authentic number words (`seje`, `unnece`, `dudece`, `cinche`, `diece`, `vinte`), authentic "manco" structure for "to" times (`'e quatte manco nu quarto`), correct raddoppiamento (`ll'una` for 1 o'clock), authentic month/weekday names (`jennaro`, `dummeneca`, etc.), and authentic weather vocab (`assulato`, `schizzechea`, `tempurale`).
  - Split web UI language `<select>` into two `<optgroup>` groups: "Creative & Constructed" (genz, val, l33t, sha, nap, eo, la, tlh) on top, "Modern Languages" (en, it, es, fr, de, pt) below. Each standard language is labelled in its own language.
  - `kAllowed[]` updated to 14 entries; all 5 dispatchers updated.
- Impact/Tradeoffs: Language count reaches 14. `ui_strings.h` grows to 14 `UiStrings` instances. Adding further languages is mechanical: add `composeWordClockSentence*`, `weatherCodeShort*`, `weatherCodeUiLabel*`, `formatDate*`, `kUiLang_*`, register in `kAllowed[]`, add to `kLangsFun[]` or `kLangsStd[]`, add to all 5 dispatchers.

---

## 2026-02-28 - Klingon Word Clock Uses ASCII Transliteration

- Context: pIqaD (native Klingon script) has no coverage in the Montserrat 38 font loaded on device. Including a second font would significantly increase flash usage.
- Decision: Klingon word clock uses ASCII transliteration (tlhIngan Hol romanization). Format: `"DaH [ora] rep [minuti] tup"`. Example: `"DaH wej rep wa'maH vagh tup"` = 3:15.
- Impact/Tradeoffs: No font change required; all Klingon strings are ASCII and fit comfortably within the 96-char sentence buffer. Downside: purists may object to non-pIqaD rendering.
