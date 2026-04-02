# Screensaver Redesign: "Pasture Simulator"

**Date:** 2026-04-02
**Status:** Draft
**Scope:** Evolve the existing ASCII cow screensaver from a flat animation into a living micro-world with day/night cycle, random events, cow state machine, and expanded hacker-flavored thoughts.

## Context

The current screensaver (since r-early) is functional but feels unfinished:
- Cow walks left/right with random walk bursts (2-6 steps, 180ms each, 1-5s pause)
- 10×2 twinkling stars (`.` `:` `o`, 3 brightness levels, 520ms fade, 3.5-12s wait)
- Scrolling grass field with tiny tree (`/^\|`)
- Thought balloon: 25s cycle (10s off, 15s on), philosophical quotes in 13 languages
- Footer: HH:MM DD/MM with jitter anti-burn-in

The cow walks, stops, walks, shows a thought, repeats. No environmental changes, no surprises, no personality beyond the quotes.

## Design Goals

1. **More life and surprises** — random events break monotony
2. **More atmosphere** — sky changes with real time, the world "breathes"
3. **More hacker/nerd personality** — thoughts that make you smile
4. **Zero new LVGL objects** — reuse existing layer structure, add state to the struct
5. **Minimal RAM** — ~36 bytes added to `ScreensaverState` (with alignment padding), all strings in PROGMEM
6. **Same wake/sleep contract** — 2h timeout, touch/IMU/button wake, 900ms guard

## Architecture

### Existing structure (unchanged)

```
g_saver.root          (hidden when inactive)
├── g_saver.sky       (label, text content for sky/clouds)
├── g_saver.starObj[10][2]  (20 label objects, positioned absolutely)
├── g_saver.field     (label, scrolling grass pattern)
├── g_saver.cow       (label, ASCII art cow)
├── g_saver.balloon   (label, thought text)
├── g_saver.balloonTail (label, dashed tail)
└── g_saver.footer    (label, time/date)
```

No new LVGL objects are created. New visual effects (shooting star, UFO, clouds, rain) are rendered by reusing the existing `sky` label content and `starObj` labels.

### New state fields in `ScreensaverState`

```cpp
// Day/night
uint8_t  skyPhase = 0;       // 0=night, 1=dawn, 2=day, 3=dusk
uint32_t skyNextMs = 0;      // next sky-phase check (every 60s)

// Cow state machine
uint8_t  cowState = 0;       // 0=GRAZE, 1=IDLE, 2=SLEEP, 3=RUN, 4=STARE_UP
uint8_t  cowChewFrame = 0;   // 0-1 animation frame for mouth
uint32_t cowChewNextMs = 0;  // next chew toggle (400ms)
uint32_t cowStateNextMs = 0; // when to re-evaluate state

// Random events
uint8_t  eventActive = 0;    // 0=none, 1=shooting_star, 2=ufo, 3=satellite, 4=rain, 5=matrix_glitch
uint32_t eventEndMs = 0;     // when current event ends
uint32_t eventCooldownMs = 0; // min time before next event (3min after last)
int16_t  eventX = 0;         // x position for moving events (shooting star, UFO, satellite)
int8_t   eventDir = 1;       // direction for moving events
uint32_t starBorrowedMask = 0; // bitmask: bit (r*2+s) set = slot borrowed by event

// Thought categories
uint8_t  thoughtCategory = 0; // 0=philosophy, 1=hacker, 2=meta, 3=weather, 4=easter_egg
```

Total added: ~36 bytes (including alignment padding). Struct goes from ~500 bytes to ~536 bytes.

Field ordering note: group `uint32_t` fields together and `uint8_t` fields together to minimize padding. The `starBorrowedMask` field tracks which `starObj` slots are temporarily used by events — `lvglScreenSaverUpdateStars()` skips any slot whose bit is set.

## Feature Details

### 1. Day/Night Cycle (sky layer)

Checked every 60 seconds. Uses `getLocalTime()` (already available from NTP).

| Phase | Hours | Stars | Sky content | Clouds |
|-------|-------|-------|-------------|--------|
| Night | 21:00-05:00 | Full (all star slots active) | Empty (dark) | No |
| Dawn  | 05:00-07:00 | Fading (max brightness reduced to 2, then 1) | Gradient line: `░░▒▒▓▓` rising | No |
| Day   | 07:00-19:00 | None (all hidden) | Cloud strings drift | Yes |
| Dusk  | 19:00-21:00 | Appearing (first few start cycling) | Gradient line: `▓▓▒▒░░` sinking | No |

**Implementation:** `lvglScreenSaverUpdateSkyPhase(nowMs)` runs every 60s. Sets `skyPhase` and adjusts star visibility caps. Dawn/dusk gradient is rendered as a single line in the `sky` label text. Clouds are 1-2 short strings (`_.--""--._ `) that drift by updating the sky label with spaces prepended (reuses `fieldBuf` temporarily during rebuild).

**Fallback:** If NTP not synced, stays in night mode (current behavior, stars always active).

### 2. Cow State Machine

Replaces the current simple walk logic. States:

**GRAZE (0)** — Default. Walks as now (6px steps, 180ms, random bursts) but with chewing animation: cow art alternates between two frames every 400ms (open/closed mouth — single char difference in the art).

**IDLE (1)** — Stands still. No walking. Occasional head turn (flip cow art direction without moving). Duration: 3-8 seconds. Thought balloon more likely to appear during IDLE.

**SLEEP (2)** — Only at night (skyPhase==0). Different cow art: cow lying down (shorter, wider). `Z z z` rendered above cow via balloon label. Near the tree position. Duration: 30-120 seconds, then brief IDLE before possible GRAZE.

**RUN (3)** — Rare (1 every 15-30 min). Cow moves at 3x speed (18px steps, 120ms). Duration: 3-5 seconds. Triggered randomly or as reaction to UFO event. Cow art uses the normal sprite but the step speed makes it feel like a sprint.

**STARE_UP (4)** — Triggered by celestial events (shooting star, UFO, satellite). Cow stops, balloon shows "?" or "!!". Duration: matches event duration. Cow art unchanged but walking stops.

**Transition logic** (evaluated when `cowStateNextMs` expires):

```
if night:
  60% → SLEEP, 20% → IDLE, 15% → GRAZE, 5% → RUN
if day:
  50% → GRAZE, 30% → IDLE, 20% → RUN
```

STARE_UP is never rolled randomly — it is triggered exclusively by celestial events (shooting star, UFO, satellite). When the event ends, the cow returns to the state it was in before STARE_UP (or GRAZE if that state was SLEEP during a day event).

### 3. Random Events

Global event system. Only one event active at a time. 3-minute cooldown between events.

**Shooting Star** (`eventActive=1`)
- Frequency: every 5-15 minutes
- Duration: 1.5 seconds
- Visual: One `starObj` label is repurposed — text set to `--*`, positioned at top of screen, moved 40px/step horizontally and 4px/step downward
- Cow reaction: STARE_UP
- Night/dusk only

**UFO** (`eventActive=2`)
- Frequency: every 30-60 minutes
- Duration: 8 seconds
- Visual: One `starObj` label repurposed — text set to `<==>`, drifts slowly left-to-right at top
- Cow reaction: STARE_UP, then RUN (50% chance)
- Any time of day (it's a UFO, they don't care about schedules)

**Satellite** (`eventActive=3`)
- Frequency: every 20-40 minutes
- Duration: 20 seconds
- Visual: One `starObj` label repurposed — text `.`, moves steadily across top row
- Cow reaction: STARE_UP (30% chance, otherwise doesn't notice)
- Night only

**Rain** (`eventActive=4`)
- Frequency: every 30-60 minutes
- Duration: 2-3 minutes
- Visual: 4-6 `starObj` labels repurposed as rain drops — text `|` or `'`, positioned at random x, reset to top when they reach the field. Updated every step (55ms) moving down 6px
- Cow reaction: walks toward tree position (fixed x based on fieldScroll), enters IDLE under tree
- Day only (night has stars, can't sacrifice the labels)
- When rain ends: cow resumes normal behavior

**Matrix Glitch** (`eventActive=5`)
- Frequency: every 45-90 minutes
- Duration: 3 seconds
- Visual: 2-3 `starObj` labels stacked vertically, cycling through random ASCII chars (0-9, A-F), colored green (`0x00FF00`) regardless of theme
- Cow reaction: none (it's a glitch in the matrix, not in the pasture)
- Any time
- Color: `0x00FF00` on most themes. On Cathode Ray (which already uses phosphor green), use amber `0xFFAA00` to distinguish from normal UI

**Implementation:** `lvglScreenSaverUpdateEvent(nowMs)` checks cooldown, rolls for new event if none active, updates position of active event. Uses existing `starObj` labels (slots [rows-1][0] and [rows-1][1] etc.) — these slots are temporarily "borrowed" from the star display and returned when the event ends.

**Star label borrowing protocol:**
- When an event starts, it sets bits in `starBorrowedMask` (bit = `r*2+s`) for each borrowed slot
- `lvglScreenSaverUpdateStars()` checks the bitmask and **skips** any slot with its bit set
- When the event ends, it clears the bits, resets the label text/position/color, and the star animation resumes naturally on the next cycle
- On screensaver deactivation (`lvglSetScreenSaverActive(false)`), `starBorrowedMask` is zeroed

**Phase transition safety:**
- Rain is day-only. If `skyPhase` transitions to dusk/night while rain is active, the rain event is **forcibly ended** in `lvglScreenSaverUpdateSkyPhase()` — labels are released, `eventActive` zeroed, and cooldown timer set
- Shooting star and satellite are night/dusk-only. If `skyPhase` transitions to day while one is active, it is forcibly ended the same way
- UFO and Matrix glitch are phase-agnostic, no forced termination needed

### 4. Expanded Thoughts

Current: single array of ~6-8 philosophical quotes per language.

New: categorized system with weather awareness.

**Categories:**

```cpp
enum ThoughtCategory : uint8_t {
  THOUGHT_PHILOSOPHY = 0,  // 30% — existing quotes + new ones
  THOUGHT_HACKER     = 1,  // 30% — new
  THOUGHT_META       = 2,  // 15% — new
  THOUGHT_WEATHER    = 3,  // 15% — context-aware based on skyPhase/event
  THOUGHT_EASTER_EGG = 4,  // 10% — rare, unique
};
```

**New quotes (English examples, other languages follow same pattern):**

Hacker category:
- "sudo rm -rf /grass"
- "my udder has 256 bits of entropy"
- "is this the matrix or just good pasture"
- "segfault in rumination module"
- "404: meaning not found"
- "I run on grass, not JavaScript"
- "localhost:8080/pasture"
- "have you tried turning the fence off and on"

Meta category:
- "I've been walking back and forth for hours"
- "someone is watching me on a tiny screen"
- "am I screensaver or screensavee"
- "this field is exactly 640 pixels wide"
- "the tree never moves. suspicious."

Weather-aware category (selected based on `skyPhase`):
- Night: "nice stars tonight", "is that a satellite or a pixel", "3 AM and still no answers"
- Dawn: "another sunrise. still a cow.", "the gradient is beautiful today"
- Day: "that cloud looks like a TCP packet", "solar powered contemplation"
- Dusk: "golden hour. still chewing.", "sunset commits are the best"
- Rain event: "rain again. at least I'm not a server.", "cloud computing, literally"
- UFO event: "I saw something. nobody will believe me."

Easter egg category:
- "01101101 01101111 01101111" (binary for "moo")
- "mooooo" (just a long moo, that's it)
- "< this space intentionally left blank >"

**Implementation:** `lvglScreenSaverQuotePackForLang()` is extended to accept a `ThoughtCategory` parameter. Each language gets a `kSaverQuotes*Hacker[]`, `kSaverQuotes*Meta[]`, `kSaverQuotes*Weather[]`, and `kSaverQuotes*Easter[]` array (can be smaller than philosophy — 4-6 entries each). Weather quotes are selected by `skyPhase` and current event. Category is rolled randomly at balloon-show time using the weighted probabilities above.

For languages with few speakers (Klingon, L33t, Shakespearean, Valley Girl, Bellazio), the hacker/meta/weather categories can fall back to English to avoid excessive translation overhead.

### 5. Cow ASCII Art Variants

Current: 2 sprites (left, right), each 6 lines.

New: 4 sprites total:

```
kCowLeft       — walking left (existing)
kCowRight      — walking right (existing)
kCowLeftChew   — walking left, mouth variant (one char diff)
kCowRightChew  — walking right, mouth variant (one char diff)
kCowSleep      — lying down (new, ~3 lines tall, wider)
```

The chew variants differ by one character in the mouth area (`(o_o)` → `(o-o)` or similar minimal change). This creates a subtle chewing animation when toggled every 400ms.

The sleep sprite:

```
       Z z z
   __(oo)__
  /__|__|__\~
  ~~~~~~~~~~
```

(Final art to be refined during implementation. Max width: 18 characters, matching the existing cow sprites. The LVGL label uses `LV_SIZE_CONTENT` so shorter text is fine — it shrinks vertically from 6 to 3-4 lines.)

### 6. Clouds (day only)

During `skyPhase==2` (day), the `sky` label text is rebuilt every 2 seconds to show 1-2 cloud strings drifting left-to-right. Clouds are simple ASCII:

```
  _.--""--._
```

Position tracked by a counter that increments the leading-space count. When a cloud exits the right edge, it wraps to the left with a random vertical row offset.

Cloud text is built in a stack-local `char buf[128]` (not shared with `fieldBuf`). The `sky` label text is set via `lv_label_set_text()` which copies internally. During night the sky label is set to empty string (no visible text), during day it shows clouds. No new LVGL object needed.

### 7. Activation/Deactivation Reset

On `lvglSetScreenSaverActive(true)` (in addition to existing init logic):
- `eventActive = 0`, `eventEndMs = 0`, `eventCooldownMs = nowMs + 30000` (30s grace before first event)
- `starBorrowedMask = 0`
- `cowState = GRAZE` (always start grazing, regardless of time — will transition naturally)
- `skyPhase` evaluated immediately via `lvglScreenSaverUpdateSkyPhase(nowMs)` (not deferred to 60s timer)
- `thoughtCategory` reset to 0

On `lvglSetScreenSaverActive(false)` (in addition to existing cleanup):
- `eventActive = 0`, `starBorrowedMask = 0` (ensure no stale borrow state)

## What Does NOT Change

- `SCREENSAVER_ENABLED`, `SCREENSAVER_IDLE_USB_MS`, `SCREENSAVER_IDLE_BATTERY_MS`, `SCREENSAVER_STEP_MS` in config.h
- `initLvglScreensaverUi()` layout (no new LVGL objects created)
- `lvglSetScreenSaverActive(bool on)` signature and wake/sleep contract
- `handleScreenSaverLoop(uint32_t nowMs)` signature (internal logic changes)
- Theme color tokens: `saverSky`, `saverField`, `saverCow`, `saverStarLow/Mid/High`
- Wake triggers: touch, IMU shake, power button
- 900ms wake guard
- 13 language support (philosophy category keeps all existing quotes verbatim)

## Performance

- Sky phase check: 1 `getLocalTime()` call every 60s — negligible
- Event updates: position arithmetic on 1-2 labels per 55ms step — negligible
- Cow state machine: one comparison per step — negligible
- New strings: ~4-8KB flash for hacker/meta/weather/easter quotes across all languages (core 7 languages get full sets; niche languages fall back to English for new categories)
- No new PSRAM allocations, no new LVGL objects, no new FreeRTOS tasks

## Testing

- Set `SCREENSAVER_IDLE_USB_MS` to 5000 (5s) during dev for quick activation
- Verify each event type triggers and renders correctly
- Verify sky phase transitions at hour boundaries (mock time or wait)
- Verify all 5 cow states are reachable
- Verify balloon text shows all categories
- Verify sleep sprite renders correctly
- Verify rain doesn't conflict with night stars
- Verify Matrix glitch green color works across all themes
- Serial log: `[SCRNSVR] event=<name>`, `[SCRNSVR] cowState=<name>`, `[SCRNSVR] skyPhase=<name>`

## File Changes

| File | Change |
|------|--------|
| `scrybar.ino` | Extend `ScreensaverState` struct, add cow states enum, add event enum, refactor `handleScreenSaverLoop()`, add `lvglScreenSaverUpdateSkyPhase()`, add `lvglScreenSaverUpdateEvent()`, add cow state machine logic, add new quote arrays, add sleep/chew cow art, modify `lvglScreenSaverUpdateBalloon()` for categories |
| `config.h` | No changes |

Single file change. All screensaver logic is self-contained in `scrybar.ino`.
