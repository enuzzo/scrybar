# ScryBar

[![Arduino](https://img.shields.io/badge/Arduino-Firmware-00979D?style=for-the-badge&logo=arduino&logoColor=white)](https://arduino.cc/)
[![ESP32-S3](https://img.shields.io/badge/ESP32--S3-Waveshare_3.49"-E7352C?style=for-the-badge&logo=espressif&logoColor=white)](https://www.espressif.com/)
[![LVGL](https://img.shields.io/badge/LVGL-8.x-6B21A8?style=for-the-badge)](https://lvgl.io/)
[![Languages](https://img.shields.io/badge/Word_Clock-14_languages-F59E0B?style=for-the-badge)](#word-clock-languages)
[![Views](https://img.shields.io/badge/Views-4_live_views-3B82F6?style=for-the-badge)](#views)
[![License](https://img.shields.io/badge/License-MIT-10B981?style=for-the-badge)](./LICENSE)

## Theme Previews (HOME + Weather)

| ScryBar Default | Cyberpunk 2077 | Toxic Candy | Tokyo Transit | Minimal Brutalist Mono |
|---|---|---|---|---|
| ![ScryBar Default HOME Weather](assets/readme_previews/home_weather_scrybar-default.png) | ![Cyberpunk 2077 HOME Weather](assets/readme_previews/home_weather_cyberpunk-2077.png) | ![Toxic Candy HOME Weather](assets/readme_previews/home_weather_toxic-candy.png) | ![Tokyo Transit HOME Weather](assets/readme_previews/home_weather_tokyo-transit.png) | ![Minimal Brutalist Mono HOME Weather](assets/readme_previews/home_weather_minimal-brutalist-mono.png) |

> Yes, that is cyberpunk in Latin. If you want neon UI saying *Hora septima est*, ScryBar will not judge.

> A mass of sensors, pixels, and unresolved ambition sitting on your desk, pretending to be furniture.
> It tells time in fourteen languages — Italian, English, French, German, Spanish, Portuguese, Latin, Esperanto, Neapolitan, Klingon, 1337, Shakespearean, Valley Girl, and Bellazio — like it's poetry,
> checks weather you could learn by opening a window, and scrolls news you've already read.
> All of this on an ESP32 that didn't ask for this life.

**ScryBar** is an open-source ESP32-S3 desk companion with a fantasy twist. One 3.49" touchscreen, four live views, and a word clock that composes real sentences in fourteen languages — from Italian and Latin to Klingon, 1337 Speak, Shakespearean English, and Bellazio — not uppercase block-letter tiles, actual grammar — rendered at 240 MHz in LVGL on hardware that costs less than a good lunch.

The name comes from *scrying*: the practice of seeing what matters from afar. That is what ScryBar does from your desk.

It is not a smart home gadget. Smart home gadgets have apps.
This has *physics-tuned touch filtering, a hand-composed Italian word clock, and an INFO panel that tells you things about your battery you probably didn't want to know.*

---

## Why This Exists

Every ESP32 project starts the same way: blink an LED.

Then you add a display because a blinking LED isn't really telling you anything. Then a touch controller because a display without interaction is just a slow TV. Then Wi-Fi because weather exists. Then NTP because the clock needs to know what time it is. Then RSS because why pull weather if you're pulling nothing else. Then QR codes because URLs are for people who type on phones with two thumbs.

By the time you stop, you've built something that tells time in Italian like it's dictating verse, shows a weather summary with an icon, scrolls headlines from three feeds in rotation, supports preferred-known-WiFi selection, and opens a web config UI on your LAN when you can't find the USB cable.

We gave it four live views, a QR code generator, and an existential purpose. Is it overengineered? Absolutely. Does it do anything you couldn't do faster on your phone? Let's not go there. But reflashing firmware at 2 AM because a glyph is three pixels off and literally nobody will ever notice is not a hobby — it's a clinical condition. ScryBar exists so the rest of your devices don't have to suffer.

---

## Table of Contents

- [Hardware](#hardware)
- [Views](#views)
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

The physical profile: a horizontal bar that sits flat on your desk. Wide enough to hold four views of information. Narrow enough that it stops pretending to be a monitor and commits to being furniture that has opinions.

---

## Views

Four views, navigated by swipe. Left or right, like flipping pages. There is no tap-to-navigate because tapping is for widgets.

```
          swipe right                swipe left
  ┌───────────────────────────────────────────────────────────────┐
  │   INFO   ◄── HOME (clock+weather) ──► AUX (RSS) ──► WIKI     │
  │ (tech)        (default boot)          (headlines)   (wiki)   │
  └───────────────────────────────────────────────────────────────┘
```

**HOME** — Word clock in natural sentence form, switchable across 14 languages: Italiano, English, Français, Deutsch, Español, Português, Latina, Esperanto, Napoletano, tlhIngan Hol (Klingon), 1337 Speak, Shakespearean English, Valley Girl, and Bellazio. Not uppercase tiles — actual grammar, composed at runtime. Plus weather icon, temperature, and humidity. Typography is theme-driven at runtime (Montserrat/Space Mono/Delius Unicase/Chakra Petch/IBM Plex Mono) with auto-fit sizing so the sentence always fills the panel cleanly.

**AUX** — RSS rotation. Up to 5 configurable feeds. `SKIP` moves to next article, `NXT` moves to next feed, and QR opens only on demand.

**WIKI** — Dedicated Wikipedia stream (3 feed families, up to 3 items per family). Same ergonomics as AUX (`SKIP`, `NXT`, on-demand QR), with summary-first body text and remote thumbnail image shown on the right when available. Refresh cadence follows firmware feed timers (`RSS_REFRESH_MS`, default 15 minutes).

**INFO** — Diagnostics panel. Wi-Fi state, SSID, IP, DNS, MAC, power mode (`CHARGING/BATTERY`), and battery percentage. Placed before HOME in the swipe order — left of boot — like an iPhone widget page you only visit when something feels wrong.

Physical buttons:

- `PWR` (center): short press opens screensaver (debounced against false micro-presses).
- `BOOT` (left): single click jumps to `HOME`.
- `RST` (right): hardware reset.

Auto-idle screensaver target is currently `2h` on both USB and battery.

---

## Word Clock Languages

The word clock composes time as a real sentence, not a grid of lit tiles. All 14 languages are built-in and selectable from the LAN web UI without reflashing. The web UI separates them into two groups.

**Creative & Constructed** (shown first in the web UI selector):

| Code | Language | Example (3:15) |
|---|---|---|
| `bellazio` | Bellazio | *Raga, le tre e un quarto. For real.* |
| `val` | Valley Girl | *It's like quarter past three, totally* |
| `l33t` | 1337 Speak | *1T'5 QU4R73R P457 7HR33* |
| `sha` | Shakespearean English | *Verily, 'tis quarter past three* |
| `nap` | Napoletano | *so' 'e tre e nu quarto* |
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

Language setting persists to NVS — survives power cycles. The full UI (weather labels, status strings, RSS panel) follows the selected language.

---

## How It Works

At boot, ScryBar:

1. Asserts `SYS_EN=HIGH` via TCA9554 (battery fallback safety on USB disconnect).
2. Cycles Wi-Fi SSIDs in non-blocking retry loops — 10 seconds per SSID, then next, indefinitely (or prioritizes the selected known SSID when set from web config).
3. If no known SSID is reachable, starts Wi-Fi Direct setup AP fallback (`2.4 GHz only`) so you can still open the control UI and provision a new network.
4. Syncs NTP once connected.
5. Renders `HOME` and enters the main loop.

Every 30 seconds, a `[SUMMARY]` block prints to serial: build, Wi-Fi, NTP, UI, and weather state. It is the firmware's heartbeat and the fastest way to know if something has gone quietly wrong.

Touch events pass through a multi-layer anti-ghost filter before reaching the swipe detector:

```
raw I2C frame
  │
  ├─ discard if point_count == 0 or > 5
  ├─ discard if any coordinate ≥ 0x0FFF  (sentinel value)
  ├─ discard if rawX ≥ width or rawY ≥ height
  │
  └─ valid frame ──► swipe accumulator ──► page transition
```

The filter exists because the AXS15231B produces ghost frames at idle — real observed case: `points=117` with spurious coordinates. Without filtering, the display navigates itself at random while sitting on a desk doing nothing. This is not acceptable behavior in furniture.

---

## Quick Start

**Prerequisites:** `arduino-cli`, `esp32` board package by Espressif, libraries listed in `firmware_readme.md`.

### Compile

```bash
arduino-cli compile --clean \
  --build-path /tmp/arduino-build-scrybar \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi \
  .
```

### Upload

```bash
arduino-cli upload -p <PORT> \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi \
  --input-dir /tmp/arduino-build-scrybar \
  .
```

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

Runtime config persists to NVS. Survives power cycles. Writable without reflash.

Configurable from the UI: preferred known Wi-Fi network (`Auto` rotation or one saved SSID), Wi-Fi Direct mode (`off`, `auto fallback`, `always on`), provisioning of new Wi-Fi credentials via in-page `2.4 GHz` scan + password, weather city, latitude/longitude, logo URL, and up to 5 RSS feeds — each with a friendly name, URL, and max post count. The feed composer is in-page: search, add, edit, delete, no page reloads.

Web UI loading behavior is resilient by design: critical styles are inline, while remote font/icon stylesheets are loaded asynchronously to keep first paint responsive even with slow CDN paths.

Wi-Fi password UX detail: the password field is always rendered in monospaced glyphs (theme-agnostic) and has an eye toggle for show/hide, so uppercase-heavy themes do not turn provisioning into guesswork.

Provisioned Wi-Fi credentials are stored in NVS and survive reboot/reflash (until NVS erase).

### Wi-Fi Field Recovery (When You Are In a Random Parking Lot)

If ScryBar cannot join any known network, this is the recovery flow:

1. Firmware keeps rotating known SSIDs from `secrets.h` and runtime NVS credentials.
2. If still offline (and direct mode is `auto` or `on`), setup AP starts: `ScryBar-Setup-XXXX`.
3. Join that AP from phone/laptop (2.4 GHz only).
4. Open `http://192.168.4.1:8080`.
5. Use `Scan networks`, pick SSID, enter password, save.

About captive portal behavior:

- Captive DNS/probe redirects are implemented and often work.
- Some OSes still decide they are too cool for captive popups on a given day.
- That is fine: the QR and direct IP are the canonical path and do not lie.
- If popup fails, scan QR or type `192.168.4.1:8080` manually and continue.

---

## Serial Command Reference

Commands sent over Serial at 115200 baud.

| Command | Effect |
|---|---|
| `VIEW` | Toggle HOME ↔ AUX |
| `VIEWFIRST` | Jump to first main view (`HOME`, excludes INFO) |
| `VIEWLAST` | Jump to last main view (`WIKI`) |
| `VIEW0` / `VIEWINFO` | Force INFO page |
| `VIEW1` / `VIEWHOME` | Force HOME page |
| `VIEW2` / `VIEWAUX` / `VIEWRSS` | Force AUX/RSS page |
| `VIEW3` / `VIEWWIKI` | Force WIKI page |
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

ScryBar can dump the current frame buffer over serial for capture and archival.

```bash
# Step 1 — force a stable frame (send SAVERON via serial first)

# Step 2 — capture
python3 tools/capture_snapshot.py --port <PORT> --out-dir screenshots
```

The frame on wire is `rgb565be` (big-endian) due to `LV_COLOR_16_SWAP=1`. If manual decoding is needed:

```bash
python3 tools/capture_snapshot.py --port <PORT> --out-dir screenshots --pix-fmt rgb565be
```

Send `SAVERON` before the capture script to avoid a race between the render cycle and the dump. This is documented, expected, and not something we are going to fix because the workaround works.

---

## Orientation

Default: `DISPLAY_FLIP_180=1`.

Physical reference: USB-C port on the left, speaker on top, microphone on the bottom. The 180° rotation is applied via hardware mirror (`esp_lcd`) combined with `rotation+2` in Arduino_GFX. Touch mapping at this orientation: `x=rawX`, `y=rawY` — no axis swap.

If you intentionally mount the board reversed — USB-C right, speaker bottom — set `DISPLAY_FLIP_180=0` and reflash. The touch mapping adjusts with it.

---

## Security

`secrets.h` is git-ignored and never committed. `secrets.h.example` is committed with placeholders only.

- **Never** commit `secrets.h`. The `.gitignore` has your back, but paranoia is a feature.
- If a secret ever lands in git history: rotate it immediately, then clean history.
- `config.h` and all versioned docs must remain credential-free. Always.

---

## Open Source Spirit

If you fork ScryBar, make it yours:

- swap feeds and weather locations,
- redesign views or add new ones,
- switch the word clock language from the web UI — 14 already built-in (see the `composeWordClockSentence*` family), or add your own,
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
