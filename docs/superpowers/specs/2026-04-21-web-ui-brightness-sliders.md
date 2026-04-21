# Web UI Dual Brightness Sliders (USB vs Battery, Live + Save)

_Design spec — 2026-04-21 · target r261._

## Goal

Expose backlight brightness in the web config UI as two sliders — one for USB-C power, one for battery. Live-apply while dragging; persist on form save.

## Background

Currently `applyEnergyPolicy` (scrybar.ino:1702) sets backlight to `100` when on USB and to `ENERGY_BACKLIGHT_ON_BATTERY` (`72`, defined in config.h) when on battery. Power source is detected via `batteryExternalPowerLikelyNow()`. Both values are hardcoded — there is no way to tune them without reflashing.

## User requirement

"Possiamo mettere due slider di luminosità nella web-ui? Uno per quando è connessa col cavo, e uno quando si usa la batteria." — User, 2026-04-21.

## Design

### Config model

Two new fields on `RuntimeNetConfig`:

```c
uint8_t backlightUsbPct;   // 10..100, default 100
uint8_t backlightBatPct;   // 10..100, default 72
```

Defaults on first boot (no NVS entry) match today's hardcoded behavior so existing devices see no visible change until the user moves a slider.

### Range

10..100%. 0–9% is forbidden: 0% turns the display off (user can't recover via the touch screen), and below 10% the AXS15231B is effectively unreadable at the device's viewing distance. Enforced server-side in both the live endpoint and the form save path.

### Live endpoint — `POST /api/brightness`

Body (application/x-www-form-urlencoded): `usb=<int>&batt=<int>`.

Handler:
1. Parses both integers, clamps to `[10, 100]`.
2. Writes them into the in-memory `g_runtimeNetConfig` fields.
3. Calls `applyEnergyPolicy(millis(), /*force=*/true)` which now reads the config fields instead of hardcoded constants.
4. Returns `200 {"ok":true}` on success, `400 {"error":"..."}` on parse error.

Does NOT write NVS. Persistence happens via the existing form-save path. If the user drags the slider and closes the browser without saving, the live values stay in RAM until next reboot, at which point the saved NVS values win.

### `applyEnergyPolicy` change

Replace:
```c
const uint8_t targetBacklight = g_batt.energySaverActive ? ENERGY_BACKLIGHT_ON_BATTERY : 100;
```
with:
```c
const uint8_t targetBacklight = g_batt.energySaverActive
    ? g_runtimeNetConfig.backlightBatPct
    : g_runtimeNetConfig.backlightUsbPct;
```

`ENERGY_BACKLIGHT_ON_BATTERY` becomes the initialization default for `backlightBatPct`, nothing else.

### Form save path

`applyRuntimeConfigFromRequest` (scrybar.ino:5349) picks up two new arg names:
- `bl_usb` → `backlightUsbPct`
- `bl_batt` → `backlightBatPct`

Clamp to `[10, 100]`. Persists via the existing NVS serialization once the config save path commits — no new NVS plumbing.

### Web UI

New `vm-card` inserted between Themes and Views:

```
┌────────────────────────────────────────┐
│ Luminosità display                     │
│                                        │
│ USB-C  [──────────●──────]   78 %      │
│ Batt   [────●───────────]   42 %      │
└────────────────────────────────────────┘
```

- Two `<input type="range" min="10" max="100" step="1">` each with a live `<output>` showing the current value.
- `oninput` handler on each: updates the `<output>` immediately, and debounces a `fetch('/api/brightness', {method:'POST', body:...})` at 120 ms.
- Both inputs also submit with `bl_usb` / `bl_batt` names as part of the normal form save.
- Style: follows existing `vm-card` pattern — no new CSS primitives. Uses the slider thumb style already in the page if any; otherwise relies on browser default (polish can come later).

Single `<script>` block near the card; <40 lines of JS, no frameworks, no imports.

### Non-goals

- No gamma / logarithmic scaling (linear 10–100 is fine for this hardware).
- No "auto" mode / ambient-light sensor integration (no sensor on this board).
- No per-theme brightness overrides.
- No touch-screen controls — this is web-UI only.
- No API versioning — `/api/brightness` ships as v1 and stays v1.

### Error handling

- Invalid integer → 400 `{"error":"usb must be 10..100"}`.
- Missing arg → 400 `{"error":"usb required"}`.
- Out of range → clamped silently on the live path (drag jitter shouldn't 400), rejected explicitly on the form path (explicit intent).

### Rollback

Revert the r261 commit. `backlightUsbPct` / `backlightBatPct` fields on the config struct become dead but harmless; next build without the patch reads the old hardcoded constants again.

## Verification

1. Open web UI, move USB slider → device backlight updates within ~150 ms (while on USB).
2. Unplug USB (battery mode) → backlight jumps to the saved batt value.
3. Move Battery slider → immediate update (while on battery).
4. Plug USB back in → backlight jumps to the saved USB value.
5. Click Save → reboot device → values persist.
6. Try sending `usb=0` via curl → 400 clamped.

## Build

Bumps `FW_BUILD_TAG` to `r261`, `FW_RELEASE_DATE` to `2026-04-21`.
