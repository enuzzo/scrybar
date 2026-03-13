# ScryBar Firmware (M0)

> ScryBar is a mass of sensors, pixels, and unresolved ambition sitting on your desk, pretending to be furniture. It tells time in 13 languages like it's poetry, checks weather you could learn by opening a window, and scrolls news you'll swear you already read. All of this on an ESP32 that didn't ask for this life. We could have stopped at a blinking LED. We didn't. We gave it six live views, a QR code generator, and an existential purpose. Is it overengineered? Absolutely. Does it do anything you couldn't do faster on your phone? Let's not go there.

Minimal firmware sketch for incremental smoke testing on Waveshare ESP32-S3-Touch-LCD-3.49. ✨

## 1) Setup 🛠️

1. Connect the board via USB-C (data cable).
2. Open Arduino IDE.
3. Install/update `esp32 by Espressif Systems` from Boards Manager.
4. Install libraries:
   - `GFX Library for Arduino` (Library Manager)
   - `SensorLib` (for QMI8658 IMU tests)
5. Open:
   - `scrybar.ino`
   - keep `build_opt.h` in the repo root (`-DHAVE_CONFIG_H`, `-DSCRYBAR_PRBOOM`)
6. Configure `Tools`:
   - `Board`: `ESP32S3 Dev Module`
   - `Port`: board USB serial port
   - `USB CDC On Boot`: `Enabled`
   - `Flash Size`: `16MB (128Mb)`
   - `Partition Scheme`: `Custom`
   - `PSRAM`: `OPI PSRAM`
   - `CPU Frequency`: `240MHz (WiFi)`
   - `Upload Speed`: `921600` (fallback `460800` if unstable)

## 2) Secrets (not versioned) 🔐

1. Copy `secrets.h.example` to `secrets.h`.
2. Fill SSID/password/API keys (placeholders are fine during setup).
3. `secrets.h` is git-ignored via `.gitignore`.
4. Never place secrets in `config.h` or other tracked files.

## 3) Compile & Upload 🚀

1. Click `Verify`.
2. Click `Upload`.
3. If it hangs on `Connecting...`, enter boot mode:
   - hold `BOOT`
   - press and release `RST`
   - release `BOOT`
4. Open Serial Monitor at `115200`.

## 4) Expected Logs/Display 📟

- Banner `ScryBar | M0.1 Serial Hello`
- Chip/heap/psram/flash/sdk/reset details
- Firmware build id:
  - `[FW] Build=...`
- With `TEST_WIFI=1`: STA connection and assigned IP
- Automatic Wi-Fi roaming/retry cycle: each configured SSID is tried for ~10s, then next SSID, continuously.
- With `TEST_NTP=1`: NTP sync and `local_time=...`
- Periodic heartbeat:
  - `[HEARTBEAT] uptime=...`
- With `TEST_DISPLAY=1` and NTP synced:
  - live clock on display (`HH:MM:SS` + date)

## 5) Feature Toggles 🎛️

Enable/disable tests in `config.h`:

- `TEST_SERIAL_INFO`
- `TEST_BACKLIGHT`
- `TEST_DISPLAY`
- `TEST_TOUCH`
- `TEST_I2C_SCAN`
- `TEST_IMU`
- `TEST_WIFI`
- `TEST_NTP`
- `DOOM_SPIKE_ENABLED`
- `WEB_CONFIG_ENABLED` (runtime web config UI su LAN)

Current implemented milestones:

- `TEST_SERIAL_INFO` (M0.1)
- `TEST_BACKLIGHT` (M0.2)
- `TEST_I2C_SCAN` (M0.3)
- `TEST_DISPLAY` (M0.4, AXS15231B via Arduino_GFX)
- `TEST_IMU` (M0.5, QMI8658 + shake detect)
- `TEST_WIFI` (M0.6, STA connect with timeout + reason)
- `TEST_NTP` (M0.7, NTP time sync)
- Live display clock (M0.8, redraw optimized to reduce flicker)
- `DOOM_SPIKE_ENABLED` (donor `prboom-go` view on direct framebuffer path)

## 6) Web Config Foundation (LAN) 🌐

When WiFi is connected and `WEB_CONFIG_ENABLED=1`, ScryBar starts a lightweight web UI:

- URL: `http://<device-ip>:8080`
- Page: `GET /`
- JSON read: `GET /api/config`
- JSON/form update: `POST /api/config`
- Form update (UI): `POST /config`
- Force refresh weather/RSS: `POST /reload` (or API `POST /api/reload`)

Current scope:

- Runtime config persisted in NVS: weather city/lat/lon, logo URL, and up to 5 RSS feeds (friendly name + URL + max posts).
- LAN page includes place search autocomplete, single-feed composer workflow, and in-page feed list with edit/delete.
- Web UI now uses a Tron-grid animated background with responsive mobile tuning and reduced-motion fallback.
- External font/icon CSS is loaded asynchronously so the page can paint even when CDN paths are slow.

## 7) Page Flow (Touch) 👆

- Boot page: `HOME` (clock + weather), unchanged.
- Swipe left from `HOME`: `AUX` (RSS page), then `WIKI`, then `ANSI`, then `DOOM`.
- Swipe right from `HOME`: `INFO` (technical panel, includes IP + web port).
- `INFO` is intentionally placed "before" home (iPhone-widget style).
- `INFO` net block includes: Wi-Fi state, SSID, power mode (`CHARGING/BATTERY`) + battery %, IP/DNS/MAC.
- `AUX` and `WIKI` share the same interaction model: `SKIP` next article, `NXT` next feed, QR on-demand.
- `WIKI` uses 3 fixed source families (`Featured`, `On this day`, `Random Article`) with up to 3 items each; refresh cadence follows `RSS_REFRESH_MS` (default firmware: 15 min).
- Wiki summaries are normalized to plain text (HTML/entities sanitized), and right-side thumbnails are rendered when metadata/media is available.
- QR in AUX/WIKI opens on demand and falls back immediately to full URL while short-link generation is pending.
- Physical side buttons:
  - `BOOT` (left): single click -> `HOME`
  - `PWR` (center): short press screensaver toggle; hold `3s` -> soft-off / wake to `HOME`
  - `RST` (right): hardware reset
- Screensaver auto-idle target is `2h` on both USB and battery.

Serial page shortcuts:

- `VIEW0` / `VIEWINFO` -> `INFO`
- `VIEW1` / `VIEWHOME` -> `HOME`
- `VIEW2` / `VIEWAUX` / `VIEWRSS` -> `AUX`
- `VIEW3` / `VIEWWIKI` -> `WIKI`
- `VIEWANSI` -> `ANSI`
- `VIEW4` / `VIEWDOOM` / `DOOM` -> `DOOM`
- `VIEWFIRST` -> first main page (`HOME`, excludes INFO)
- `VIEWLAST` -> last main page (`DOOM`)
- `SAVERON` / `SAVEROFF` / `SAVERSTAT` -> manual screensaver control/status

## 8) DOOM Integration Notes 🎮

- Current donor: `ducalex/retro-go` -> `prboom-go` only, not the whole framework.
- Runtime glue:
  - `src/doom/scrybar_prboom.cpp`
  - `src/doom/scrybar_prboom_runtime.h`
  - `build_opt.h`
- Current build needs `PartitionScheme=custom` because the stock presets are too small once DOOM is included.
- Display model:
  - DOOM/ANSI bypass LVGL widgets and own the direct framebuffer path
  - DOOM content is centered 4:3 inside the `640x172` strip
  - side bands are used for HUD + touch buttons
- Controls:
  - IMU is active only in DOOM
  - left band = `USE`
  - right band = `FIRE`
  - center tap = recenter neutral
  - swipe = exit view

## 9) Canonical Orientation 🧭

- Physical reference: `USB-C/power left`, `speaker top`, `microphone bottom`.
- Firmware flag: `DISPLAY_FLIP_180=1` (default).
- If you intentionally mount the board reversed, set `DISPLAY_FLIP_180=0` and reflash.

## 10) Changelog 🧾

- See `CHANGELOG.md` for release-by-release notes (`DB-M0-r089` and later).
