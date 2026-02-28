# ScryBar Project Knowledge

Stable technical context for all AI assistants.

## Repository Layout

- `ScryBar.ino`: main firmware sketch in repository root.
- `assets/`, `src/`, `tools/`: firmware resources, drivers, and utilities.
- `lvgl_validation/`: isolated validation sketch for touch/UI experiments.
- `knowledge/`: shared, sanitized cross-assistant knowledge (public, versioned).
- `.codex/`: local private working memory (not versioned).

## Core Stack

- Firmware: Arduino sketch (`ScryBar.ino`)
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

## Recurring Gotchas

- Avoid default bare board settings that imply wrong flash/PSRAM profile.
- Validate touch/display orientation after mapping changes.
- Validate LVGL color/byte-order assumptions when modifying render pipeline.
- Prefer small, verifiable changes and immediate runtime checks.

## Public Logging Rule

If you add session notes to versioned docs:

- keep only generalized lessons
- avoid local identifiers
- do not copy raw serial logs with personal/network metadata
