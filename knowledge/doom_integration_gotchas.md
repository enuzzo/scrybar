# DOOM Integration Gotchas

Public technical note for the ScryBar DOOM integration shipped on 2026-03-11.

## Scope

This documents how DOOM was integrated on ScryBar, what broke during the port,
and which fixes became part of the stable baseline.

## Integration Shape

- Donor strategy: use `ducalex/retro-go` only as a donor for `prboom-go`, not as
  a full framework import.
- Donor code lives under `src/doom/prboom/`.
- ScryBar-specific glue lives in:
  - `src/doom/scrybar_prboom.cpp`
  - `src/doom/scrybar_prboom_runtime.h`
  - `scrybar.ino` (`doomScrybarBlitIndexedFrame`, DOOM page UI, touch/IMU glue)
- Build flags required by the donor are injected through `build_opt.h`:
  - `-DHAVE_CONFIG_H`
  - `-DSCRYBAR_PRBOOM`
- Current embedded assets:
  - `src/doom/doom1.wad`
  - `src/doom/doom_iwad.S`
  - `src/doom/doom_titlepic.h`
  - `tools/extract_doom_titlepic.py`

## Rendering Model

- ScryBar uses a direct framebuffer path for DOOM, not LVGL widgets.
- Logical canvas stays `640x172`.
- Live DOOM content is rendered as centered 4:3 content inside that strip:
  - frame size: `230x172`
  - centered X offset: `205`
  - left/right pillarbox bands are used for HUD and touch actions
- The DOOM page owns the display similarly to the ANSI page:
  - LVGL drag animation is bypassed while the page is active
  - DOOM blits into `g_canvasBuf`
  - `dispFlush()` pushes the rotated canvas to the panel

## Controls Baseline

- `MOVE`: IMU tilt forward/back
- `TURN`: IMU left/right
- left band touch: `USE`
- right band touch: `FIRE`
- center tap: recenter / neutral recalibration
- swipe: exit DOOM page
- IMU is active only while `UI_PAGE_DOOM` is visible

## Critical Gotchas and Fixes

### 1. Do not import all of retro-go

Problem:
- Pulling the whole `retro-go` tree would bring display/input assumptions that do
  not match ScryBar and would enlarge the compile surface for no benefit.

Fix:
- Vendor only the `prboom-go` core and write a local platform shim.

### 2. Default partitions are too small

Problem:
- Once `prboom`, embedded IWAD/title assets, and the existing firmware all lived
  in one binary, the default large-app partition presets were no longer enough.

Fix:
- Use a checked-in `partitions.csv` with:
  - `app0 = 0xA00000`
  - `fatfs = 0x5F0000`
- Compile/upload with `PartitionScheme=custom`.

### 3. Internal RAM exhaustion prevents DOOM from starting cleanly

Problem:
- The original DOOM task stack and 8-bit framebuffer could fail when allocated
  from internal heap.

Fix:
- Prefer PSRAM for the DOOM task stack and framebuffer.
- `src/doom/scrybar_prboom.cpp` explicitly retries framebuffer allocation in
  PSRAM and uses `xTaskCreatePinnedToCoreWithCaps(..., MALLOC_CAP_SPIRAM)` when
  available.

### 4. Large display DMA chunks broke unrelated HTTPS feeds

Problem:
- After DOOM integration, `mbedTLS` handshakes for RSS/Wikipedia started failing
  intermittently because display DMA buffers were consuming too much internal heap.

Fix:
- Keep `DB_CHUNK_ROWS=32` instead of `64`.
- Tradeoff is slightly more chunk launches during flush, but enough internal heap
  remains for TLS.

### 5. First-sample IMU neutral is wrong

Problem:
- Capturing neutral immediately on DOOM entry caused visible drift and flicker.
- A hidden bug made this worse: the complementary filter state was not being reset
  when re-entering DOOM, so a stale tilt estimate contaminated the new neutral.

Fix:
- On entry/recenter:
  - reset tilt filter state
  - wait a short arm delay
  - require a stable window with touch released and low gyro motion
  - average multiple stable samples before accepting neutral
  - smooth deltas before quantization

### 6. IMU must not run globally

Problem:
- Leaving gyro/accel live outside DOOM wastes CPU and floods serial with data that
  is irrelevant to the rest of the product.

Fix:
- Gate sensor enable/disable to `UI_PAGE_DOOM`.
- Keep `IMU_VERBOSE_SERIAL=0` by default.

### 7. Swipe exit conflicted with USE/FIRE bands

Problem:
- If a swipe started from a touch band, it could be consumed as `USE`/`FIRE`
  instead of exiting the page.

Fix:
- In DOOM, swipe detection takes precedence over band button handling.

### 8. Booting directly into gameplay is bad UX

Problem:
- Starting `prboom` immediately on page entry gave no “landing” state and made it
  harder to confirm that the page and control bands were working before gameplay.

Fix:
- Render `TITLEPIC` first.
- Start `prboom` only on `FIRE`.

## Operational Checklist

1. Build with:
   - `PartitionScheme=custom`
   - `PSRAM=opi`
   - `build_opt.h` present
2. Flash and enter DOOM.
3. Confirm title screen appears first.
4. Let the device settle; verify both HUD axes sit at zero.
5. Tap `FIRE` to start gameplay.
6. Verify swipe still exits the page.

## Current State

- First frame and live gameplay both work on real hardware.
- Title screen gate is in place.
- IMU neutral is stable at rest.
- HUD is pillarboxed and game-styled.
- Remaining work is gameplay tuning, not basic integration.
