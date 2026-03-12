# ScryBar

[![Arduino](https://img.shields.io/badge/Arduino-Firmware-00979D?style=for-the-badge&logo=arduino&logoColor=white)](https://arduino.cc/)
[![ESP32-S3](https://img.shields.io/badge/ESP32--S3-Waveshare_3.49"-E7352C?style=for-the-badge&logo=espressif&logoColor=white)](https://www.espressif.com/)
[![LVGL](https://img.shields.io/badge/LVGL-8.x-6B21A8?style=for-the-badge)](https://lvgl.io/)
[![Languages](https://img.shields.io/badge/Word_Clock-13_languages-F59E0B?style=for-the-badge)](#word-clock-languages)
[![Views](https://img.shields.io/badge/Views-5_live_views-3B82F6?style=for-the-badge)](#views)
[![License](https://img.shields.io/badge/License-MIT-10B981?style=for-the-badge)](./LICENSE)

## Theme Previews (HOME + Weather)

| ScryBar Default | Cyberpunk 2077 | Toxic Candy | Tokyo Transit | Minimal Brutalist Mono |
|---|---|---|---|---|
| ![ScryBar Default HOME Weather](assets/readme_previews/home_weather_scrybar-default.png) | ![Cyberpunk 2077 HOME Weather](assets/readme_previews/home_weather_cyberpunk-2077.png) | ![Toxic Candy HOME Weather](assets/readme_previews/home_weather_toxic-candy.png) | ![Tokyo Transit HOME Weather](assets/readme_previews/home_weather_tokyo-transit.png) | ![Minimal Brutalist Mono HOME Weather](assets/readme_previews/home_weather_minimal-brutalist-mono.png) |

> Yes, that is cyberpunk in Latin. If you want neon UI saying *Hora septima est*, ScryBar will not judge.

> It tells time in thirteen languages, checks weather you could learn by opening a window, scrolls news you've already read, and browses Wikipedia random articles.
> All of this on an ESP32 that didn't ask for this life.

**ScryBar** is an open-source ESP32-S3 desk companion. One 3.49" touchscreen, five live views, a word clock that composes real sentences in thirteen languages — from Italian and Latin to Klingon, 1337 Speak, and Bellazio — actual grammar, not uppercase tiles — plus a Wikipedia viewer, a pillarboxed DOOM port, and a web config UI.

## DOOM on ScryBar

![DOOM HUD on ScryBar](assets/readme_previews/doom_hud_sci_fi.png)

Real `prboom` gameplay now runs directly on the 640x172 ScryBar display with a centered 4:3 frame, game-styled side HUD, IMU controls, and touch bands for `USE` / `FIRE`.

---

## Table of Contents

- [Hardware](#hardware)
- [Views](#views)
- [DOOM View](#doom-view)
- [Word Clock Languages](#word-clock-languages)
- [How It Works](#how-it-works)
- [Quick Start](#quick-start)
- [Secrets](#secrets)
- [Feature Toggles](#feature-toggles)
- [Web Config (LAN)](#web-config-lan)
- [Serial Command Reference](#serial-command-reference)
- [Screenshot Workflow](#screenshot-workflow)
- [Orientation](#orientation)
- [Security](#security)
- [Acknowledgments](#acknowledgments)
- [Open Source Spirit](#open-source-spirit)
- [License](#license)

---

## Hardware

| Component | Spec | Role |
|---|---|---|
| **MCU** | ESP32-S3, 240 MHz, dual-core | The brain. 16MB flash, OPI PSRAM in octal mode — because LVGL needs room to think. |
| **Board** | Waveshare ESP32-S3-Touch-LCD-3.49 | The whole stack in one unit: display, touch controller, power management, battery connector. |
| **Display** | AXS15231B, 3.49", 320×170 | The face. Horizontal strip format. `LV_COLOR_16_SWAP=1` because it expects RGB565 big-endian and is not open to discussion about this. |
| **Touch** | AXS15231B integrated | Single-point touch. Carefully filtered for ghost frames and sentinel coordinates. |
| **Power** | USB-C + optional LiPo | Charging and battery fallback managed via TCA9554 GPIO expander. Always re-asserted at boot. |

The physical profile: a horizontal bar that sits flat on your desk. Wide enough to host five modes of mischief. Narrow enough that it stops pretending to be a monitor and commits to being furniture that has opinions.

---

## Views

Five views, navigated by swipe.

```
  INFO ◄─► HOME ◄─► AUX (RSS) ◄─► WIKI ◄─► DOOM
```

**HOME** — Word clock in natural sentence form (13 languages), weather icon, temperature, humidity. Theme-driven typography with auto-fit sizing.

**AUX** — RSS rotation with up to 5 configurable feeds. `SKIP`/`NXT`/`QR` controls.

**WIKI** — Wikipedia stream: Featured Article, On This Day, and Random Article. Language is independently selectable (8 real languages) from system language via web UI. Same `SKIP`/`NXT`/`QR` controls as AUX.

**DOOM** — `prboom-go` donor port adapted for ScryBar. Centered `4:3` live framebuffer on the `640x172` strip, sci-fi side HUD, `TITLEPIC` welcome screen, tap `FIRE` to boot the core.

**INFO** — Diagnostics: Wi-Fi, IP, DNS, MAC, power mode, battery.

Physical buttons:

- `PWR` (center): short press opens screensaver (debounced against false micro-presses).
- `BOOT` (left): single click jumps to `HOME`.
- `RST` (right): hardware reset.

Auto-idle screensaver target is currently `2h` on both USB and battery.

## DOOM View

The DOOM integration intentionally does **not** import all of `retro-go`. ScryBar vendors only the `prboom-go` core under `src/doom/prboom/` plus a local runtime shim.

- Donor baseline: `ducalex/retro-go` -> `prboom-go` only
- Render path: direct framebuffer, not LVGL widgets
- Live frame: `320x200` source -> centered `230x172` 4:3 presentation
- Touch:
  - left band = `USE`
  - right band = `FIRE`
  - center tap = recenter IMU neutral
  - swipe = exit DOOM
- IMU:
  - active only inside DOOM
  - neutral captured after a short stable settle window on entry

Current boot flow is deliberate: entering DOOM shows the title screen first, and the first tap on `FIRE` starts the live core.

---

## Word Clock Languages

13 languages, all selectable from the web UI without reflashing. Setting persists to NVS.

**Creative & Constructed:**

| Code | Language | Example (3:15) |
|---|---|---|
| `bellazio` | Bellazio | *Raga, le tre e un quarto. For real.* |
| `val` | Valley Girl | *It's like quarter past three, totally* |
| `l33t` | 1337 Speak | *1T'5 QU4R73R P457 7HR33* |
| `sha` | Shakespearean English | *Verily, 'tis quarter past three* |
| `eo` | Esperanto | *estas kvarono post la tri* |
| `la` | Latina | *hora tertia et quadrans* |
| `tlh` | tlhIngan Hol (Klingon) | *wej rep wa'maH vagh tup* |

**Modern Languages:**

| Code | Language | Example (3:15) |
|---|---|---|
| `en` | English | *it's quarter past three* |
| `it` | Italiano *(default)* | *sono le tre e un quarto* |
| `es` | Español | *son las tres y cuarto* |
| `fr` | Français | *il est trois heures et quart* |
| `de` | Deutsch | *es ist viertel nach drei* |
| `pt` | Português | *são três e quinze* |

---

## How It Works

At boot: assert SYS_EN via TCA9554, cycle Wi-Fi SSIDs (or use preferred SSID), fall back to setup AP if no known network, sync NTP, render HOME.

Serial `[SUMMARY]` every 30s: build, Wi-Fi, NTP, UI, weather state.

Touch passes through anti-ghost filtering (AXS15231B produces spurious frames at idle).

---

## Quick Start

**Prerequisites:** `arduino-cli`, `esp32` board package by Espressif, libraries listed in `firmware_readme.md`.

### Compile

```bash
arduino-cli compile --clean \
  --build-path /tmp/arduino-build-scrybar \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  .
```

### Upload

```bash
arduino-cli upload -p <PORT> \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  --input-dir /tmp/arduino-build-scrybar \
  .
```

This repo now ships a checked-in `partitions.csv` and uses `PartitionScheme=custom` because the DOOM build no longer fits the stock large-app presets.

If upload hangs on `Connecting...`, enter boot mode: hold `BOOT`, press and release `RST`, release `BOOT`. This is not a bug. It is a handshake.

Open Serial Monitor at **115200 baud**. You will see the boot banner, chip diagnostics, and — if `TEST_WIFI=1` — a connection attempt cycling through every configured SSID in sequence.

---

## Secrets

`secrets.h` is local-only and git-ignored. It holds Wi-Fi credentials and API keys.

```bash
cp secrets.h.example secrets.h
# fill in your credentials
```

`secrets.h.example` is committed and contains placeholders only (`<WIFI_SSID>`, `<API_KEY>`, etc.). Never put credentials in `config.h` or any versioned file. The `.gitignore` handles this by design, not accident.

---

## Feature Toggles

Enable or disable subsystems in `config.h`. Nothing is compiled in unless explicitly toggled on.

| Toggle | Milestone | What it activates |
|---|---|---|
| `TEST_SERIAL_INFO` | M0.1 | Chip, heap, flash diagnostics at boot |
| `TEST_BACKLIGHT` | M0.2 | Display backlight PWM test |
| `TEST_I2C_SCAN` | M0.3 | I2C bus scan, prints found addresses |
| `TEST_DISPLAY` | M0.4 | Display init via AXS15231B (Arduino_GFX) |
| `TEST_IMU` | M0.5 | QMI8658 accelerometer + shake detection |
| `TEST_WIFI` | M0.6 | STA connection with multi-SSID retry |
| `TEST_NTP` | M0.7 | NTP sync, prints `local_time=...` |
| `DOOM_SPIKE_ENABLED` | — | DOOM page + donor runtime glue |
| `TEST_TOUCH` | — | **Required for swipe navigation.** Boot log shows `[SKIP] TEST_TOUCH=0` if disabled. |
| `DISPLAY_FLIP_180` | — | 180° rotation (USB-C left, speaker top). Default `1`. |
| `WEB_CONFIG_ENABLED` | — | LAN web config UI on port 8080 when Wi-Fi is connected. |

---

## Web Config (LAN)

When Wi-Fi is connected and `WEB_CONFIG_ENABLED=1`, ScryBar starts a lightweight HTTP server.

```
http://<DEVICE_IP>:8080
```

| Endpoint | Method | Does |
|---|---|---|
| `GET /` | — | Config UI (Tron-grid themed, responsive, reduced-motion fallback) |
| `GET /api/config` | — | Current config as JSON |
| `GET /api/wifi/scan` | — | Scan nearby `2.4 GHz` Wi-Fi networks (bounded timeout, safe in AP setup mode) |
| `GET /api/wifi/setup-qr.svg` | — | SVG QR for setup URL (`http://192.168.4.1:8080` in AP mode) |
| `POST /api/config` | JSON body | Update config fields |
| `POST /config` | Form body | Update config via form UI |
| `POST /reload` | — | Force refresh weather and RSS feeds |

Config persists to NVS. Configurable: Wi-Fi preferred SSID, Wi-Fi Direct mode, new Wi-Fi provisioning (scan + password), weather city/lat/lon, logo URL, up to 5 RSS feeds, system language, Wikipedia language, and UI theme.

### Wi-Fi Field Recovery

If no known network is reachable, setup AP starts (`ScryBar-Setup-XXXX`, 2.4 GHz). Join it and open `http://192.168.4.1:8080` to scan and provision a new network.

---

## Serial Command Reference

Commands sent over Serial at 115200 baud.

| Command | Effect |
|---|---|
| `VIEW` | Toggle HOME ↔ AUX |
| `VIEWFIRST` | Jump to first main view (`HOME`, excludes INFO) |
| `VIEWLAST` | Jump to last main view (`DOOM`) |
| `VIEW0` / `VIEWINFO` | Force INFO page |
| `VIEW1` / `VIEWHOME` | Force HOME page |
| `VIEW2` / `VIEWAUX` / `VIEWRSS` | Force AUX/RSS page |
| `VIEW3` / `VIEWWIKI` | Force WIKI page |
| `VIEW4` / `VIEWDOOM` / `DOOM` | Force DOOM page |
| `BATSTAT` | Print battery status |
| `SAVERON` | Force screensaver on |
| `SAVEROFF` | Force screensaver off |
| `SAVERSTAT` | Print screensaver state + active idle target |
| `WIFIDIRECT` | Print Wi-Fi direct mode/AP status |
| `WIFIDIRECT off|auto|on` | Set Wi-Fi direct mode and persist to NVS |
| `PWROFFHARD` | Hard power-off — **requires a hardware power cycle to recover. Not mapped to the physical button by design.** |

A `[SUMMARY]` block is emitted automatically every 30 seconds: build, Wi-Fi, NTP, UI, and weather state. Read it like a flight data recorder.

---

## Screenshot Workflow

```bash
python3 tools/capture_snapshot.py --port <PORT> --out-dir screenshots
```

Send `SAVERON` first to freeze the frame. Wire format is `rgb565be`.

---

## Security

`secrets.h` is git-ignored and never committed. `secrets.h.example` is committed with placeholders only.

- **Never** commit `secrets.h`. The `.gitignore` has your back, but paranoia is a feature.
- If a secret ever lands in git history: rotate it immediately, then clean history.
- `config.h` and all versioned docs must remain credential-free. Always.

---

## Acknowledgments

The DOOM integration is based on `prboom-go` from **[ducalex/retro-go](https://github.com/ducalex/retro-go)**, but ScryBar vendors only the donor core and uses its own display/input glue. That separation is intentional.

## Open Source Spirit

If you fork ScryBar, make it yours:

- swap feeds and weather locations,
- redesign views or add new ones,
- switch the word clock language from the web UI — 13 built-in, or add your own,
- publish your variant and share improvements back.

Small screen. Wide horizon.

---

## License

[MIT](./LICENSE). Use it, fork it, modify it, put it on a desk somewhere and tell people it's art (it is).
Keep the copyright notice. No warranty. No liability. No hard feelings.

---

<div align="center">

*Built with Arduino, LVGL, too many filter constants, and the unwavering belief*
*that a word clock in Klingon on an ESP32 is objectively better than anything else on your desk.*

</div>
