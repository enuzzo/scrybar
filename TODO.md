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

## 3. GPT / Oracle Page — Full Rethink

**Current state:** not designed for the device. The layout was not reasoned for 320×170 and does not belong to the design system. Needs a clean restart.

**Goals:**
- Define what this page *does* before touching a pixel. Is it:
  - A query/response surface (type or voice → answer)?
  - An ambient oracle (rotating AI-generated content, no interaction)?
  - A hybrid?
- Design for the physical constraints first: strip display, single-touch, no keyboard.
- If interaction is needed, design a minimal on-device input method (or delegate to the web UI / LAN).
- Establish a visual identity for the page that feels distinct from HOME/AUX but coherent with the system.

**Open questions to resolve before design:**
- Is voice input in scope (hardware support? mic already present)?
- What is the data source? (local LLM, cloud API, cached responses?)
- What is the failure/loading state? The display is always-on — blank or spinner is not acceptable.

---

## 4. Design System Audit — Device vs Web

**Context:** the current design system was built for the web config UI and has been partially applied to the device UI. The two contexts have different constraints and the device side shows it.

**Goals:**
- Define a formal design token set for the device: type sizes, spacing units, color palette, icon style.
- Audit all four views (HOME, AUX, GPT, INFO) against the token set.
- The web config UI can share color/brand tokens but has its own layout rules — keep them separate.
- Document decisions in `knowledge/decisions.md` as they are made.

**Non-goals:**
- Do not port the Tron-grid web aesthetic to the device. The device needs its own language.

---

## Notes

- Items 2, 3, 4 are interdependent — do the design system audit (4) before redesigning individual pages.
- Item 1 (languages) is independent and can be picked up anytime after a successful compile baseline.
- Before any UI work: confirm compile is clean on current codebase (pending post-office check).
