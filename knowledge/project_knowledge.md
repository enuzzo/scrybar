# ScryBar Project Knowledge

Stable technical context for all AI assistants.

## Repository Layout

- `scrybar.ino`: main firmware sketch in repository root.
- `assets/`, `src/`, `tools/`: firmware resources, drivers, and utilities.
- `lvgl_validation/`: isolated validation sketch for touch/UI experiments.
- `knowledge/`: shared, sanitized cross-assistant knowledge (public, versioned).
- `.codex/`: local private working memory (not versioned).

## Core Stack

- Firmware: Arduino sketch (`scrybar.ino`)
- MCU: ESP32-S3
- Display/touch target: Waveshare ESP32-S3-Touch-LCD-3.49
- LVGL path available for UI validation and production features.

## Canonical Board Profile

Use these compile/upload parameters as baseline:

- board: `esp32:esp32:esp32s3`
- CPU: `240MHz`
- Flash: `16MB`
- PSRAM: `OPI`
- Flash mode: `QIO`
- Partition: `app3M_fat9M_16MB`
- Upload speed: `921600`
- USB mode: `hwcdc`, CDC on boot enabled

## Build Commands (Template)

Compile:

```bash
arduino-cli compile --clean \
  --build-path /tmp/arduino-build-scrybar \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi \
  .
```

Upload:

```bash
arduino-cli upload -p <PORT> \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi \
  --input-dir /tmp/arduino-build-scrybar \
  .
```

## Configuration Boundaries

- Runtime secrets must stay in local non-versioned `secrets.h`.
- `secrets.h.example` is versioned and must contain placeholders only.
- `config.h` and docs must remain secret-free.

## Current Product Behaviors (High Level)

- Multi-SSID Wi-Fi fallback with non-blocking retry cycle.
- Touch-driven page navigation (HOME/AUX/GPT/INFO views).
- Weather + RSS + word clock integrated UX.
- Power/battery diagnostics visible in INFO panel.

## LVGL Configuration

Key settings in `lv_conf.h`:

- `LV_COLOR_16_SWAP=1` — required for AXS15231B byte order (see DECISIONS.md).
- `LV_FONT_MONTSERRAT_38=1` — required for word clock font (`lvglFontClock`).

Key setting in display driver:

- `full_refresh=1` — required with current software rotation.

## Display Orientation

Default: `DISPLAY_FLIP_180=1` in `config.h` (USB-C left, speaker top, mic bottom).
Effect: 180° rotation via HW mirror (esp_lcd) + `rotation+2` (Arduino_GFX); touch uses direct mapping (`x=rawX`, `y=rawY`), no axis swap.

## Touch Anti-Ghost Filtering (AXS15231B)

Discard touch frames where:

- `point_count` is 0 or > 5
- any coordinate ≥ `0x0FFF` (sentinel value)
- raw coords outside panel bounds (`rawX >= width || rawY >= height`)

Real idle ghost observed: `points=117` with spurious coordinates.

## Power Policy

- 5s hold: soft-off (re-awakeable via 5–6s hold).
- Hard-off: serial command `PWROFFHARD` only — not mapped to the physical button by design.
- At boot: always re-assert `SYS_EN=HIGH` via TCA9554 to support battery fallback on USB disconnect.
- Battery monitor: ADC1 CH3, 12dB attenuation, ×3 voltage divider.

## Serial Command Reference

| Command | Effect |
|---|---|
| `VIEW` | Toggle HOME ↔ AUX page |
| `VIEW0` / `VIEWHOME` | Force HOME page |
| `VIEW1` / `VIEWAUX` | Force AUX page |
| `BATSTAT` | Print battery status |
| `SAVERON` | Force screensaver on |
| `PWROFFHARD` | Hard power-off (requires power cycle to recover) |

Runtime summary (`[SUMMARY]`) emitted every 30s: build/wifi/ntp/ui/weather state.

## Screenshot Workflow

Capture serial frame dump with `tools/capture_snapshot.py --port <PORT> --out-dir screenshots`.
With `LV_COLOR_16_SWAP=1`, the frame on wire is `rgb565be` — pass `--pix-fmt rgb565be` if forcing manual decode.
Preferred order: send `SAVERON` via serial first, then run capture script (avoids `--pre-cmd` race).

## Recurring Gotchas

- Avoid default bare board settings that imply wrong flash/PSRAM profile.
- Validate touch/display orientation after mapping changes.
- Validate LVGL color/byte-order assumptions when modifying render pipeline.
- `TEST_TOUCH` in `config.h` must be `1` for swipe navigation to function (boot log shows `[SKIP] TEST_TOUCH=0` if disabled).
- `arduino-cli monitor` may close during RST due to USB re-enumeration; reconnect manually.
- Use `lv_color_hex(0xRRGGBB)` — do not pass RGB565 values to `lv_color_hex`.
- For the degree symbol in LVGL labels use UTF-8 `"\xC2\xB0"`, not `char(176)`.
- Prefer small, verifiable changes and immediate runtime checks.

## Public Logging Rule

If you add session notes to versioned docs:

- keep only generalized lessons
- avoid local identifiers
- do not copy raw serial logs with personal/network metadata
