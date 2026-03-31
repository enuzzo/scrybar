# DOOM View — Archive Notes

## Current Structure

DOOM is fully implemented in `src/doom/` and conditionally compiled via `DOOM_SPIKE_ENABLED`.
The page code in `scrybar.ino` is guarded by `#if TEST_DISPLAY && DOOM_SPIKE_ENABLED` throughout.

Key files:
- `src/doom/` — game engine, HUD (`doomDrawBandOverlay`), input handling
- `build_opt.h` — contains `-DDOOM_SPIKE_ENABLED=0` (or 1 to enable)
- `scrybar.ino` — `UI_PAGE_DOOM = 5`, `UI_VIEW_FLAG_DOOM = 0x10`, `kSwipePageOrder` includes it,
  `g_lvglDoomRoot`, `doomRenderSpike()`, `handleDoomTouchRelease()`, `handleCarouselSwipe()` DOOM branch

The enum value `UI_PAGE_DOOM = 5` and flag `UI_VIEW_FLAG_DOOM = 0x10` remain in the code.
`UI_VIEW_FLAG_DOOM` remains in `UI_VIEW_MASK_DEFAULT` so it's enabled when compiled in.

## How to Re-enable the Web UI Toggle

The DOOM checkbox was removed from `buildWebViewToggles()` in `scrybar.ino` and
`"view_doom"` was removed from `kViewArgs[]` in `parseViewsConfig()`.

To restore the web UI toggle:

1. In `buildWebViewToggles()`, add back after the Now Playing label:
   ```cpp
   const bool doomFeatureAvailable =
   #if TEST_DISPLAY && DOOM_SPIKE_ENABLED
       true;
   #else
       false;
   #endif
   const bool doomViewOn = doomFeatureAvailable && ((g_runtimeNetConfig.enabledViewsMask & UI_VIEW_FLAG_DOOM) != 0);
   html += F("<label class='vm-view");
   if (!doomFeatureAvailable) html += F(" disabled");
   html += F("'><input id='view_doom_cb' type='checkbox'");
   if (doomViewOn) html += F(" checked");
   if (!doomFeatureAvailable) html += F(" disabled");
   html += F("><span class='vm-view__copy'><strong>DOOM</strong><small>");
   if (doomFeatureAvailable) html += F("Swipe-reachable game page with gyro + touch controls.");
   else html += F("Not available in this firmware build.");
   html += F("</small></span></label>");
   ```

2. In `parseViewsConfig()`, re-add to `kViewArgs[]`:
   ```cpp
   {"view_doom", UI_VIEW_FLAG_DOOM},
   ```

3. Also restore `view_doom` to `webRequestHasConfigParams()` if it was removed:
   ```cpp
   if (g_webCfg.server.hasArg("view_doom")) return true;
   ```

## How to Enable DOOM Gameplay

1. Set `DOOM_SPIKE_ENABLED=1` in `build_opt.h`
2. Provide a `doom1.wad` (shareware Doom 1 WAD) at the path expected by the build.
   See `src/doom/` for the expected location and format.
3. Compile and flash.

The DOOM page will automatically appear in the swipe carousel since
`UI_VIEW_FLAG_DOOM` remains in `UI_VIEW_MASK_DEFAULT`.

## HUD Design (r184 "Bunker Console")

The DOOM HUD uses `doomDrawBandOverlay()`:
- Dark olive CRT scanlines
- Authentic DOOM palette (green/red/amber)
- Vertical meter 36x100px, horizontal meter 26px high
- Industrial USE/FIRE buttons with highlight/shadow
- MOVE/TURN readouts with +/- N values
