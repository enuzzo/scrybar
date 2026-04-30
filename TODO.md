# ScryBar — Roadmap / TODO

Tracked here: planned features and rework items, in rough priority order.
Implementation happens in separate sessions with full planning before touching code.

---

## 1. Multi-language Word Clock

**Goal:** extend `composeWordClockSentence*` to support multiple languages. Start with the serious ones, end with chaos.

**Serious:**
- `en` — English
- `fr` — French
- `de` — German
- `es` — Spanish
- `pt` — Portuguese

**Chaos tier:**
- `la` — Latin (yes)
- `tlh` — Klingon (No, really.)
- `eo` — Esperanto
- `nap` — Neapolitan
- TBD: more nominations welcome

**Design notes:**
- Each language gets its own `composeWordClockSentence{Lang}()` function.
- Language selection via `config.h` define or NVS runtime key.
- Some languages will require additional font coverage — audit needed before committing to all of them.
- Klingon has no standard Unicode block; will need a workaround (transliteration or pIqaD if a LVGL-compatible font exists).

---

## 2. INFO Page — UI/UX Rework

**Current state:** functional, data-dense, but not designed *for* the device. Feels like a terminal dump more than a view.

**Goals:**
- Rethink layout for 320×170 strip display — no wasted vertical space, clear visual hierarchy.
- Group data semantically: network block, power block, system block.
- Use the device design system consistently (typography scale, spacing, color roles).
- Make it *glanceable*, not just readable. One-second scan should answer "is everything OK?".

**Specific items:**
- Visual indicator for power state (CHARGING vs BATTERY) — icon or color, not just text.
- Battery % with a graphical bar (even a minimal one).
- Wi-Fi signal strength if available.
- Clear section dividers that work at this resolution.
- Decide: show IP always, or only when relevant? (currently always — reconsider.)

---

## 3. Design System Audit — Device vs Web

**Context:** the current design system was built for the web config UI and has been partially applied to the device UI. The two contexts have different constraints and the device side shows it.

**Goals:**
- Define a formal design token set for the device: type sizes, spacing units, color palette, icon style.
- Audit all live views (INFO, HOME, AUX, WIKI, NOW PLAYING, DOOM, TIMETABLE, LAUNCH, MAC STATS) against the token set.
- The web config UI can share color/brand tokens but has its own layout rules — keep them separate.
- Document decisions in `knowledge/decisions.md` as they are made.

**Non-goals:**
- Do not port the Tron-grid web aesthetic to the device. The device needs its own language.

---

## Notes

- Items 2 and 3 are interdependent — do the design system audit (3) before redesigning individual pages.
- Item 1 (languages) is independent and can be picked up anytime after a successful compile baseline.
