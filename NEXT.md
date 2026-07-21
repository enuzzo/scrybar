# Next Session — 2026-07-21

Product exploration and the decision sheet live in
[`FUTURE_IDEAS.md`](FUTURE_IDEAS.md).

## Current Baseline

- `r283` flashed and verified on device (2026-07-21): boot banner/build tag
  confirmed via INFO title, all six INFO rows render without clipping.
- Companion v0.2.4 (build 28): discoverable Quit (popover power icon + Settings
  footer button) with clean shutdown — `applicationWillTerminate` cancels poll
  tasks and terminates the macmon subprocess (verified: no orphans).
- `tools/capture_snapshot.py` gained a Pillow fallback decoder — no longer
  hard-depends on ffmpeg.
- Display DMA uses `DB_CHUNK_ROWS=64`; LVGL draw buffer is a single
  1/10-screen buffer in internal SRAM (r282).

## Hardware Verification Remaining (touch-only)

Items requiring physical interaction, still open:

1. Exercise slow taps and short horizontal swipes on every page (r283 removed
   the fast-swipe classification).
2. Tap `NXT`, `SKIP`, and `QR` near their shared edges; each band must dispatch
   only its own action (new 8×2 px hit padding).
3. Switch through all seven themes repeatedly and watch PSRAM/free-heap
   telemetry (r283 fixed the QR canvas PSRAM leak on theme change).
4. Reopen the Companion popover and confirm `Waiting`/`Connected` states are
   truthful after a stop/restart.

## Recommended Next Work

- Add pairing/authentication for the local firmware API and setup access point.
- Replace permissive TLS (`setInsecure`) with verified HTTPS and explicit failure
  states; do not silently downgrade to HTTP.
- Add firmware parser/gesture host tests and companion unit tests to CI.
- Resolve repository licensing and redistribution policy for bundled DOOM assets
  before publishing another binary release.
