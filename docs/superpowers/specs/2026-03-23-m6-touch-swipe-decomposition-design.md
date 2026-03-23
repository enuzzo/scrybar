# M6 — Decompose `handleTouchSwipeInput()` Design Spec

**Date:** 2026-03-23
**Milestone:** M6 (Firmware Polishing Roadmap)
**Scope:** `scrybar.ino` — `handleTouchSwipeInput()` (lines 9748–10093, 345 lines)

---

## Problem

`handleTouchSwipeInput()` is a 345-line function implementing a touch state machine with gesture classification, LVGL page dragging, DOOM-specific touch zones, feed deck button handling, and carousel swipe navigation — all in a single function. The logic has 3 temporal phases (touch-down, tracking, release) each with view-specific branching. This makes it hard to modify any single interaction without understanding the full state flow.

## Approach

**Phase-based decomposition** — the function naturally has 3 phases that execute in different frames. Extract the **release/gesture dispatch** logic (the largest and most complex part) into view-specific handlers. The orchestrator retains the state machine skeleton (~80 lines) and dispatches to handlers on release.

Unlike M3-M5 (which were linear sequences), this function is a state machine — the orchestrator must remain the single entry point that manages `g_touchDown`, `g_touchAwaitRelease`, etc.

## Architecture

```
handleTouchSwipeInput()              ~80 lines (state machine + dispatch)
├── Phase 1: screensaver + release-gate (inline, ~30 lines)
├── Phase 2: touch-down + tracking (inline, ~45 lines)
└── Phase 3: release → gesture dispatch
    ├── handleFeedDeckButtonRelease() ~50 lines (QR/SKIP/NXT tap on AUX/WIKI)
    ├── handlePageDragRelease()       ~35 lines (LVGL drag → swipe or snap-back)
    ├── handleDoomTouchRelease()      ~55 lines (USE/FIRE/recenter/swipe-exit)
    ├── handleCarouselSwipe()         ~20 lines (generic page swipe)
    └── handleFeedDeckTapRelease()    ~25 lines (news-area tap, QR-close overlay)
```

### Why keep Phase 1+2 inline

Phases 1 (screensaver wake + release gate) and 2 (touch-down registration + LVGL drag tracking) are tightly coupled to the state machine variables (`g_touchDown`, `g_touchAwaitRelease`, `g_touchPageDragging`, etc.) and execute on different frames than Phase 3. Extracting them would require passing 8+ mutable state refs with no readability benefit. The orchestrator stays at ~80 lines which is at the limit but justified for a state machine.

### Release Handlers

All release handlers receive a `TouchReleaseInfo` struct (computed once in the orchestrator on release):

```c
struct TouchReleaseInfo {
  int16_t dx, dy;
  uint32_t durMs;
  bool horizontalIntent;
  bool pageSwipe;
  bool isTap;
  bool isBtnTap;
  TouchAuxButton auxBtnDown;
  uint8_t doomTouchZone;
};
```

This replaces 10+ local variables computed at the release point. Each handler receives `const TouchReleaseInfo &r` and reads only what it needs.

### Handler Details

#### `handleFeedDeckButtonRelease(const TouchReleaseInfo &r)` ~50 lines

Handles QR/SKIP/NXT button taps on AUX and WIKI feed deck views. Contains `#if TEST_WIFI && RSS_ENABLED` guards for SKIP and NXT actions.

#### `handlePageDragRelease(const TouchReleaseInfo &r)` ~35 lines

Handles LVGL page drag commit (swipe → `stepUiPage()`) or snap-back. Contains `#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI` guard.

#### `handleDoomTouchRelease(const TouchReleaseInfo &r)` ~55 lines

Handles DOOM-specific touch: swipe-exit, USE (left zone), FIRE (right zone), game launch, center-tap recalibrate. Contains `#if TEST_DISPLAY && DOOM_SPIKE_ENABLED` and `#if DB_HAS_PRBOOM_DONOR` guards.

#### `handleCarouselSwipe(const TouchReleaseInfo &r)` ~20 lines

Generic horizontal swipe → `stepUiPage()`. No `#if` guards. Also includes the DOOM fallback swipe-exit path (`#if TEST_DISPLAY`).

#### `handleFeedDeckTapRelease(const TouchReleaseInfo &r)` ~25 lines

News-area tap → advance to next RSS/wiki item. QR modal close on overlay tap. Contains `#if TEST_WIFI && RSS_ENABLED` guard.

### Orchestrator Flow (on release)

```
1. Compute TouchReleaseInfo
2. Clear button press visuals
3. if auxBtnDown → handleFeedDeckButtonRelease() → return
4. if pageDragging → handlePageDragRelease() → return
5. if DOOM page → handleDoomTouchRelease() → return
6. if timeout (>3s) → return
7. if QR modal open → close overlay → return
8. if pageSwipe → handleCarouselSwipe() → return
9. if feed deck tap → handleFeedDeckTapRelease() → return
10. if HOME tap → toggleClockMode()
```

## Conditional Compilation

| Guard | Function |
|-------|----------|
| `#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI && SCREENSAVER_ENABLED` | orchestrator Phase 1 (screensaver wake) |
| `#if TEST_DISPLAY && DOOM_SPIKE_ENABLED` | orchestrator Phase 2 (DOOM touch-down) + `handleDoomTouchRelease()` |
| `#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI` | orchestrator Phase 2 (LVGL drag) + `handlePageDragRelease()` |
| `#if TEST_WIFI && RSS_ENABLED` | `handleFeedDeckButtonRelease()` + `handleFeedDeckTapRelease()` |
| `#if TEST_DISPLAY` | `handleCarouselSwipe()` (DOOM exit fallback) |
| `#if DB_HAS_PRBOOM_DONOR` | `handleDoomTouchRelease()` (prboom launch) |

The `#else` stub (line 10094-10097) remains unchanged.

## Invariants

1. **Identical touch behavior** — same gestures, same thresholds, same serial output
2. **State machine integrity** — `g_touchDown`, `g_touchAwaitRelease`, `g_touchPageDragging` managed only by orchestrator
3. **All `#if` guards preserved**
4. **Touch miss counter logic preserved** — 12-frame hold-off
5. **Feed deck button visual feedback preserved** — press/release
6. **No function >80 lines** — orchestrator at ~80 (state machine justified)
7. **No new files** — all in `scrybar.ino`
8. **`#else` stub unchanged**

## Measurable Targets

| Metric | Before | After |
|--------|--------|-------|
| `handleTouchSwipeInput()` lines | 345 | ~80 |
| Largest handler | — | ~55 lines (`handleDoomTouchRelease`) |
| Functions >80 lines | 1 (345) | 0 |

## Verification Steps

1. `arduino-cli compile --clean` — zero warnings
2. Upload to device, hard reset
3. **Swipe carousel:** swipe through INFO ↔ HOME ↔ AUX ↔ WIKI ↔ DOOM
4. **DOOM touch:** USE (left), FIRE (right), center-tap recalibrate, swipe-exit
5. **Feed deck:** QR button, SKIP button, NXT button, news-area tap
6. **Screensaver:** touch to wake
7. **Clock mode:** tap left panel on HOME

## Build Tag

Bump `FW_BUILD_TAG` to `DB-M6-r205` and `FW_RELEASE_DATE` to `2026-03-23`.

## Commit

```
refactor(firmware): M6 — decompose handleTouchSwipeInput (345 → ~80 lines)
```
