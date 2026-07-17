# Next Session — 2026-07-17

Product exploration and the decision sheet for Monday live in
[`FUTURE_IDEAS.md`](FUTURE_IDEAS.md).

## Current Baseline

- `r282` was the latest checked-in baseline; this quality pass identifies its
  firmware build as `r283` for unambiguous flash verification.
- Firmware, Astro landing page, and Swift companion build locally.
- Display DMA uses `DB_CHUNK_ROWS=64`; the r282 LVGL draw buffer is a single
  1/10-screen buffer in internal SRAM.
- The current quality pass improves touch classification, feed-button hit maps,
  INFO density, Now Playing fallback state, QR buffer reuse, embedded-page JSON
  escaping, companion connection truthfulness, CI version injection, and landing
  accessibility/responsiveness.

## Hardware Verification Still Required

The device was not connected during this pass. On the next attached session:

1. Flash the current build and confirm the boot banner/build tag.
2. Exercise slow taps and short horizontal swipes on every page.
3. Tap `NXT`, `SKIP`, and `QR` near their shared edges; each band must dispatch
   only its own action.
4. Switch through all seven themes repeatedly and watch PSRAM/free-heap telemetry.
5. Check that INFO shows all six rows without clipping.
6. Stop/restart Companion and confirm `Waiting`/`Connected` plus the device-side
   Now Playing stale state are truthful.

## Recommended Next Work

- Add pairing/authentication for the local firmware API and setup access point.
- Replace permissive TLS (`setInsecure`) with verified HTTPS and explicit failure
  states; do not silently downgrade to HTTP.
- Add firmware parser/gesture host tests and companion unit tests to CI.
- Resolve repository licensing and redistribution policy for bundled DOOM assets
  before publishing another binary release.
