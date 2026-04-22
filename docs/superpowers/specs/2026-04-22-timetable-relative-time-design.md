# Timetable (ex-Departures) — Relative Time + Rename (r275)

Date: 2026-04-22
Status: approved (autonomous continuation of brainstorm 2026-04-22)

## Context

User feedback from Lobo Bardella (via WhatsApp, 2026-04-21):

1. "nel departures l'ora non si aggiorna" — static absolute `HH:MM` looks like a
   stale clock. He expects a countdown like the LED sign at a physical bus stop.
2. "ed a me fa comodo sapere tra quanti minuti e non a che ora arriva il bus" —
   relative time is more useful for a glance from the desk.
3. "mi pare che gli orari li spara a caso.. controllando sul sito atm" — suspects
   data-accuracy bug on ATM Milano. No specific stop provided.
4. "io cambierei con timetable" — rename proposal. Page-level, user-facing.

## Decisions

### 1. Rename "Departures" → "Timetable"

User-facing labels only. Do NOT touch internal identifiers
(`UI_PAGE_TRANSIT`, `g_transitState`, `netFetchTransitDepartures`,
`UI_VIEW_FLAG_TRANSIT`, NVS key `transit_arr`, API endpoint paths). Internal
"transit" is the semantic name for the Transitous data source; "Timetable"
is just the screen title.

Touchpoints:

- `scrybar.ino:14249` — LVGL header label `"DEPARTURES"` → `"TIMETABLE"`.
- `scrybar.ino:4695` — web-UI views toggle `<strong>Departures</strong>` →
  `<strong>Timetable</strong>`.
- `scrybar.ino:4695` subtext — `"Live transit departure board via Transitous API."`
  → `"Live timetable via Transitous API. Trains, metro, bus, coach."`
- `scrybar.ino:4876` placeholder `"Leave empty for all departures"` → `"Leave empty to show all"`.

Out of scope: landing page (`web/`), README, release notes, existing
`knowledge/*.md` prose. Those will drift naturally when re-touched.

### 2. Time column: adaptive relative/absolute format

Current behaviour: column `time_[i]` at x=344 width 70px shows scheduled
`HH:MM` (from `d.depHour/depMinute`). The delay column to the right shows
`+Xm`/`-Xm` separately. Nothing ticks.

New behaviour:

| Minutes until effective departure | Display | Color |
|---|---|---|
| `<= 0 && >= -1` | `now` | amber `0xFFB74D` |
| `1..2` | `1'` / `2'` | amber `0xFFB74D` |
| `3..60` | `3'` .. `60'` | `t.infoText` |
| `> 60` | `HH:MM` (absolute) | `t.auxMeta` (muted) |
| `< -1` (past) | `HH:MM` (absolute) | `t.divider` (very muted) |

"Effective departure" = `depHour*60 + depMinute + (hasDelay ? delayMin : 0)`.
Midnight wrap tolerance: if `diff < -60`, add 1440 (assume next-day).

Prime notation `X'` chosen over `"X min"` / `"in X"`:

- Cross-lingua (standard minute symbol, used on European LED boards)
- Fits in 70px at 20px Funnel Display with room to spare (`60'` = 3 chars ~35px)
- Visually distinct from `HH:MM` — no ambiguity when both can appear in the same column
- Empty-looking numbers like `2'` feel urgent without shouting

Update cadence: the UI render loop already ticks every second
([scrybar.ino:17590](../../scrybar.ino:17590)). `lvglUpdateTransitUi` is called
on every tick when the page is visible, so the relative-time string
recomputes continuously. No extra timer needed.

### 3. Delay column: keep unchanged

Keep `+3m` / `-3m` indicator as-is. Gives context ("why is the countdown
3 min earlier than the schedule would suggest"). Regular departures that
the relative number has already absorbed still benefit from seeing the
reported delay.

### 4. Data-accuracy diagnosis for ATM

Can't fix without a specific stop. Add serial diagnostics so the user can
narrow it down next time. In `netFetchTransitDepartures`, after the per-row
parse, add one `Serial.printf` per departure:

```
[TRANSIT][PARSE] i=%d iso=%s rt=%d delay=%+d sched->local=%02u:%02u eff=%02u:%02u
```

This lets the user `screen /dev/cu.usbmodem83201` or `arduino-cli monitor`
while the board is visible, query a specific ATM stop, and compare what the
API returned vs. what ATM website shows. Three failure modes will each look
distinct:

- Wrong stopId → all rows wildly off or different lines entirely
- Timezone bug → all rows off by a constant hour offset
- Stale GTFS feed → rows match a past schedule but not today

## Non-goals

- No changes to fetch cadence (2 min stays polite).
- No auto-clear-filter-on-station-change (separate gotcha, out of scope).
- No SemiBold font variant at 20px (doesn't exist in font set; color encodes urgency).
- No current-clock in header (HOME page already has it; would duplicate).

## Verification plan

1. Compile → upload → reset.
2. Capture one reference screenshot per theme (7 themes) on the timetable
   page via `tools/capture_snapshot.py`.
3. Verify per-theme readability: amber urgency color contrast on each panel
   bg; `HH:MM` muted visible; `X'` prime character renders (Funnel Display
   Latin-1 charset covers `U+2019` RIGHT SINGLE QUOTATION MARK and ASCII `'`
   — we use ASCII `'`).
4. Edge cases on device:
   - Wait for a row to cross the 60→60 min boundary (visible change
     `HH:MM` → `60'`). Hard to script — accept by design if the row type
     doesn't race over that boundary in test window; code-review the branch
     cover.
   - Check a row at 1' shows in amber; watch it flip to `now`; watch `now`
     disappear when the row drops off the list (next-poll refresh).
5. Rename: web UI check. Navigate to `http://<IP>/` → Views section → see
   "Timetable". Firmware header shows "TIMETABLE" top-left.

## Rollback

Single revert of the r275 commit restores r274 behaviour. Data layout of
`TransitDeparture` unchanged; no NVS migration needed.
