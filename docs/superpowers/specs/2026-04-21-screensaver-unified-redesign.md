# Screensaver Unified Redesign — Nordic Night, Stacked Strip, Balanced Scope (r260)

_Design spec — approved 2026-04-21._

## Background

Before r260, the Pasture Simulator screensaver pulled its colors from each of the 7 UI themes via `UiThemeLvglTokens.saverSky/saverField/saverCow/saverBalloon/saverFooter/saverStarLow/saverStarMid/saverStarHigh`. In practice this had three failure modes:

1. **Visual drift across themes.** Some themes rendered stars prominently, others hid them in near-background noise. Grass was visible in some, almost invisible in others. There was no single "this is what the screensaver looks like" reference.
2. **Element overlap.** Depending on theme + canvas geometry, the scrolling grass row could visually intersect the bottom-right clock, producing illegible frames.
3. **Maintenance cost.** 7 themes × 8 saver color slots = 56 hand-tuned values that rarely get revisited, plus five random event systems (shooting star, satellite, UFO, rain, matrix glitch) and four cow states, all adding surface without proportional visual payoff.

User asked for a single unified look, cross-theme, with a clear identity and the visual problems (grass overlap, unseen stars) fixed.

## Decisions

Three brainstorming screens validated these choices on 2026-04-21:

1. **Palette: Nordic Night** (option C) — a cinematic night-sky feel with deep indigo background and blue/white star gradient, selected over Phosphor Green, Amber Vintage, and Monochrome Ink.
2. **Layout: Stacked Strip** (option A) — four vertically-separated zones, guaranteeing no overlap between grass and clock or between cow and grass, selected over Open Sky (stars + cow mixed).
3. **Scope: Balanced** (option B) — keep stars, grass, cow (graze/sleep), balloon (2 categories), shooting star, satellite; park UFO, rain, matrix, extra cow states, extra balloon categories, and sky phases for future feature-by-feature sessions.

## Palette (fixed, cross-theme)

| Slot       | Hex       | Intent                              |
|------------|-----------|-------------------------------------|
| `bg`       | `#0A0F1E` | Canvas background — deep indigo     |
| `star_hi`  | `#FFFFFF` | Brightest stars (flicker peak)      |
| `star_mid` | `#B8D0FF` | Mid-brightness stars                |
| `star_lo`  | `#4A5878` | Baseline star field, dim slate      |
| `cow`      | `#E8EEFF` | Cow ASCII art — cream white         |
| `grass`    | `#2F5940` | Grass strip + drifting tree — teal  |
| `balloon`  | `#9FB0D6` | Thought bubble text — soft blue-gray |
| `clock`    | `#C8D6FF` | Clock + date footer — cool white    |

These become hardcoded constants (`kSaverPalette*`) in `scrybar.ino`. Every call site that previously read `theme.saverXxx` switches to the constant. The per-theme saver color fields stay on `UiThemeLvglTokens` for ABI stability but are no longer read by the saver code path.

## Layout (zones in device pixels)

Canvas is 640×172. Zones run top to bottom, never overlap:

```
y=0     ────────────────────────────────────────────
          SKY BAND                        (72 px)
          6 rows of 3-layer ASCII stars
y=72    ────────────────────────────────────────────
          COW BAND                        (60 px)
          2 rows optional thought balloon
          3 rows cow ASCII
y=132   ────────────────────────────────────────────
          GRASS STRIP                     (18 px)
          1 row scroll pattern `~`~~^~`...
          with occasional drifting tree `/^\|`
y=150   ────────────────────────────────────────────
          CLOCK STRIP                     (22 px)
          `HH:MM  DD/MM`, bottom-right
          anti-burn-in jitter (±3 px)
y=172   ────────────────────────────────────────────
```

Boundaries are enforced by setting `lv_obj_set_pos()` / `lv_obj_set_size()` / `lv_obj_align()` exactly to these coordinates on the init path. The cow is drawn at roughly y=96 (balloon at y=78 when visible). No element may cross a zone boundary.

## Feature scope

### IN (Balanced — polished)

- **3-layer star field.** `star_lo` background scatter, `star_mid` and `star_hi` random flickers with jitter timing 420–1180 ms (`star_mid`) and 3500–12000 ms (`star_hi`).
- **Grass strip.** Existing `~`~~^~` scroll pattern + drifting `/^\|` tree, color `kSaverPaletteGrass`.
- **Cow ASCII.** 3-line art, two states only:
  - `COW_GRAZE`  → `__(o_o)__ / /__|__|__\~ / ~~~~~~~~~~`
  - `COW_SLEEP`  → `Z z z` overlay + `__(o_o)__ / /__|__|__\~` (closed-eye variant)
  - Transitions: GRAZE ↔ SLEEP every 8000–20000 ms.
- **Balloon thoughts.** 2 categories only:
  - `PHILOSOPHY`  (50% roll)
  - `EASTER_EGG`  (25% roll)
  - 25% no balloon (pure scene)
  - Multi-language pool preserved (7 languages), categories `HACKER` and `WEATHER` filtered out at selection time.
- **Events.** 2 only:
  - `SAVER_EVENT_SHOOTING_STAR` (base probability 4% per eval tick)
  - `SAVER_EVENT_SATELLITE` (base probability 2% per eval tick)
  - Cooldown preserved from existing implementation.
- **Clock footer.** `HH:MM  DD/MM`, bottom-right, anti-burn-in jitter preserved, color `kSaverPaletteClock`.

### OUT (parked — see `memory/screensaver_deferred_features.md`)

- `COW_RUN`, `COW_STARE_UP` states
- Balloon categories `HACKER`, `WEATHER`
- Events `SAVER_EVENT_UFO`, `SAVER_EVENT_RAIN`, `SAVER_EVENT_MATRIX`
- Sky phase transitions (day / dusk / night / dawn)
- Per-theme saver color variation

These features are not deleted from source — they are gated out at selection time (events never roll into the disabled slots; cow state machine never transitions to parked states; balloon category roll skips parked categories). If a future session resurrects one, the infrastructure is already there.

## Implementation plan

1. **Palette constants.** Add `static constexpr uint32_t kSaverPaletteBg/StarHi/StarMid/StarLo/Cow/Grass/Balloon/Clock` near the top of `scrybar.ino` (right after the existing `SCREENSAVER_*` macros region).
2. **Replace theme reads.** Grep for every `theme.saverSky/saverField/saverCow/saverBalloon/saverFooter/saverStarLow/saverStarMid/saverStarHigh` and `t.saverXxx`. Swap each for the corresponding `kSaverPalette*`. Note the field `saverFooter` maps to `kSaverPaletteClock` (the footer *is* the clock in the current implementation).
3. **Background color.** Set `g_saver.root` bg to `kSaverPaletteBg` in init — this overrides the previous `theme.screenBg`.
4. **Zone boundaries.**
   - `g_saver.sky`: `set_pos(0, 0)`, `set_size(cW, 72)`.
   - `g_saver.cow`: position anchors at `(centered, 96)` (3-row art fits inside 72..132).
   - `g_saver.balloon`: anchors at `(166, 78)` (existing), `set_size((cW*56)/100, LV_SIZE_CONTENT)` and `LONG_CLIP`. Balloon top at 78, 3 lines of 14px fit before cow starts at 96.
   - `g_saver.balloonTail`: anchors at `(200, 90)` (between balloon and cow).
   - `g_saver.field` (grass): `set_pos(0, 132)`, single row of `kSaverFieldFont`.
   - `g_saver.footer` (clock): `lv_obj_align(..., LV_ALIGN_BOTTOM_RIGHT, -10, -4)` stays — places at y=168 (22px from bottom minus 4px pad). Jitter stays ±3 px in both axes to prevent burn-in.
5. **Feature gating.** In `lvglScreenSaverQuotePackForLang()` restrict to 2 categories. In `lvglScreenSaverUpdateEvent()` change the event roll to only fire `SAVER_EVENT_SHOOTING_STAR` or `SAVER_EVENT_SATELLITE` (existing thresholds preserved for the two that stay). In cow state machine, remove transitions to `COW_RUN` / `COW_STARE_UP`.
6. **Build tag.** Bump `FW_BUILD_TAG` to `r260` and `FW_RELEASE_DATE` to `2026-04-21`.
7. **Compile + flash.** `arduino-cli compile` + `arduino-cli upload`. Hard reset via RTS.
8. **Verify.** Force screensaver on via `SAVERON` serial command; snapshot with `tools/capture_snapshot.py`. Diff visually against the mockup: stars visible at 3 brightness levels, grass line cleanly at y=132–150, clock bottom-right, cow centered in middle band.
9. **Documentation.** Append decision entry to `knowledge/decisions.md`. No need to add a new gotcha — the r259 underflow one stands.
10. **Commit + push.**

## Non-goals

- Changing the screensaver activation threshold (stays at 2h idle USB / battery).
- Adding a setting to toggle screensaver features on/off from the web UI.
- Rewriting the cow ASCII art or adding a larger cow — the 3-line art is kept as is.
- Internationalization changes — the 7-language balloon pool stays intact.
- Memory optimization — the existing star arrays and balloon buffers are already tight.

## Risk and rollback

Risk: the deferred features (UFO / rain / matrix / extra cow states / hacker+weather balloon / sky phase) are gated at selection sites, not deleted. If a gating condition is buggy one of them fires anyway. Mitigation: force a single roll path through `SAVER_EVENT_SHOOTING_STAR`/`SATELLITE` by replacing the roll switch, not by adding a condition per event. Similar for cow states.

Rollback: revert the r260 commit. All 7 themes still have their saver color fields intact, so the previous per-theme behavior returns cleanly.

## Approval gate

Design approved verbally by user on 2026-04-21 after viewing all three brainstorming mockups (palette, layout, feature-scope) and the final combined preview. User granted full implementation autonomy with a 2-hour window to complete and flash.
