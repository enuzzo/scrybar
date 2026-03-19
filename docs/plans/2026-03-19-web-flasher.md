# ScryBar Web Flasher Implementation Plan

> **For agentic workers:** Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let users flash ScryBar firmware from a browser via USB, with zero toolchain setup, using pre-built binaries served from GitHub Pages.

**Architecture:** GitHub Actions builds two firmware variants (Full with DOOM, Lite without) on every tagged release. The workflow assembles the static flasher page + manifests + compiled binaries into a single artifact and deploys it via `actions/deploy-pages` — no binaries are committed to git. A static HTML page uses ESP Web Tools to flash the selected variant directly over Web Serial.

**Tech Stack:** GitHub Actions, arduino-cli, ESP Web Tools (esphome.github.io/esp-web-tools), GitHub Pages (`actions/deploy-pages`), HTML/CSS/JS

---

## File Structure

```
.github/
  ci-secrets.h                  # Dummy secrets for CI builds
  workflows/
    build-firmware.yml          # CI: builds both variants, deploys to GitHub Pages

docs/
  flasher/
    index.html                  # Flasher web page (ESP Web Tools + variant picker)
    manifest-full.json          # ESP Web Tools manifest for Full variant
    manifest-lite.json          # ESP Web Tools manifest for Lite variant

# The following are assembled at CI time only (never committed to git):
#   _site/                      # Staging dir in CI
#     index.html                # Copied from docs/flasher/
#     manifest-full.json        # Copied (with version injected from tag)
#     manifest-lite.json        # Copied (with version injected from tag)
#     firmware/full/*.bin       # Full variant build output
#     firmware/lite/*.bin       # Lite variant build output
```

## Reference: ESP32-S3 Flash Layout

From `partitions.csv` and ESP32-S3 defaults:

| Part            | Offset (hex) | Offset (dec) |
|-----------------|-------------|--------------|
| Bootloader      | 0x0000      | 0            |
| Partition table | 0x8000      | 32768        |
| OTA data        | 0xE000      | 57344        |
| App (ota_0)     | 0x10000     | 65536        |

## Reference: Build Commands

Full variant (current default):
```bash
arduino-cli compile --clean \
  --build-path /tmp/scrybar-full \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  .
```

Lite variant: same command but with `build_opt.h` containing only `-DHAVE_CONFIG_H` (no `-DSCRYBAR_PRBOOM`), built to `/tmp/scrybar-lite`.

Output binaries from arduino-cli (in build path):
- `scrybar.ino.bootloader.bin`
- `scrybar.ino.partitions.bin`
- `scrybar.ino.bin`
- Plus `boot_app0.bin` from the ESP32 core package at `~/.arduino15/packages/esp32/hardware/esp32/*/tools/partitions/boot_app0.bin`

---

### Task 1: Create dummy secrets.h for CI

CI needs a `secrets.h` to compile. The dummy version uses empty/placeholder creds so the firmware boots into WiFi Direct provisioning mode.

**Files:**
- Create: `.github/ci-secrets.h`

- [ ] **Step 1: Create CI secrets file**

```cpp
#pragma once
// CI build — no real credentials.
// Device will boot into WiFi Direct setup mode.
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define WIFI_SSID_2 ""
#define WIFI_PASSWORD_2 ""
#define WIFI_SSID_3 ""
#define WIFI_PASSWORD_3 ""
#define WIFI_SSID_4 ""
#define WIFI_PASSWORD_4 ""
#define WIFI_SSID_5 ""
#define WIFI_PASSWORD_5 ""
#define SPOO_ME_API_KEY ""
#define RSS_SHORTENER_TOKEN ""
```

- [ ] **Step 2: Commit**

```bash
git add .github/ci-secrets.h
git commit -m "ci: add dummy secrets.h for CI firmware builds"
```

---

### Task 2: Create GitHub Actions build workflow

**Files:**
- Create: `.github/workflows/build-firmware.yml`

- [ ] **Step 1: Write the workflow**

The workflow:
1. Triggers on tag push (`v*`) and manual `workflow_dispatch`
2. Installs arduino-cli + ESP32 core + required libraries (pinned versions)
3. Copies `.github/ci-secrets.h` to `secrets.h`
4. Builds Full variant (default `build_opt.h`)
5. Patches `build_opt.h` to remove `-DSCRYBAR_PRBOOM`, builds Lite variant
6. Assembles a `_site/` directory with static files + binaries
7. Injects version from git tag into manifests
8. Deploys to GitHub Pages via `actions/deploy-pages` (no binary commits to git)

```yaml
name: Build Firmware & Deploy Flasher

on:
  push:
    tags: ['v*']
  workflow_dispatch:

permissions:
  contents: read
  pages: write
  id-token: write

concurrency:
  group: pages
  cancel-in-progress: false

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install arduino-cli
        uses: arduino/setup-arduino-cli@v2

      - name: Install ESP32 core and libraries
        run: |
          arduino-cli config init
          arduino-cli config add board_manager.additional_urls \
            https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
          arduino-cli core update-index
          arduino-cli core install esp32:esp32@3.1.1
          arduino-cli lib install "ArduinoJson@7.3.0" "lvgl@8.4.0"

      - name: Prepare secrets
        run: cp .github/ci-secrets.h secrets.h

      - name: Build Full variant
        run: |
          arduino-cli compile --clean \
            --build-path /tmp/scrybar-full \
            --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
            .

      - name: Build Lite variant
        run: |
          echo "-DHAVE_CONFIG_H" > build_opt.h
          arduino-cli compile --clean \
            --build-path /tmp/scrybar-lite \
            --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
            .

      - name: Find boot_app0.bin
        id: boot-app
        run: |
          BOOT_APP0=$(find ~/.arduino15/packages/esp32 -name boot_app0.bin | head -1)
          echo "path=$BOOT_APP0" >> "$GITHUB_OUTPUT"

      - name: Resolve version
        id: version
        run: |
          if [[ "$GITHUB_REF" == refs/tags/* ]]; then
            echo "tag=${GITHUB_REF#refs/tags/}" >> "$GITHUB_OUTPUT"
          else
            echo "tag=dev-$(git rev-parse --short HEAD)" >> "$GITHUB_OUTPUT"
          fi

      - name: Assemble site
        run: |
          mkdir -p _site/firmware/full _site/firmware/lite

          # Static files from repo
          cp docs/flasher/index.html _site/
          cp docs/flasher/manifest-full.json _site/
          cp docs/flasher/manifest-lite.json _site/

          # Inject version into manifests
          sed -i "s/\"version\": \"1.0.0\"/\"version\": \"${{ steps.version.outputs.tag }}\"/" \
            _site/manifest-full.json _site/manifest-lite.json

          # Full variant binaries
          cp /tmp/scrybar-full/scrybar.ino.bootloader.bin _site/firmware/full/bootloader.bin
          cp /tmp/scrybar-full/scrybar.ino.partitions.bin  _site/firmware/full/partitions.bin
          cp "${{ steps.boot-app.outputs.path }}"          _site/firmware/full/boot_app0.bin
          cp /tmp/scrybar-full/scrybar.ino.bin             _site/firmware/full/firmware.bin

          # Lite variant binaries
          cp /tmp/scrybar-lite/scrybar.ino.bootloader.bin _site/firmware/lite/bootloader.bin
          cp /tmp/scrybar-lite/scrybar.ino.partitions.bin  _site/firmware/lite/partitions.bin
          cp "${{ steps.boot-app.outputs.path }}"          _site/firmware/lite/boot_app0.bin
          cp /tmp/scrybar-lite/scrybar.ino.bin             _site/firmware/lite/firmware.bin

      - name: Upload firmware artifact
        uses: actions/upload-artifact@v4
        with:
          name: scrybar-firmware
          path: |
            _site/firmware/full/
            _site/firmware/lite/

      - name: Upload pages artifact
        uses: actions/upload-pages-artifact@v3
        with:
          path: _site/

  deploy:
    needs: build
    runs-on: ubuntu-latest
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    steps:
      - name: Deploy to GitHub Pages
        id: deployment
        uses: actions/deploy-pages@v4
```

- [ ] **Step 2: Commit**

```bash
git add .github/workflows/build-firmware.yml
git commit -m "ci: add firmware build workflow (full + lite variants)"
```

---

### Task 3: Create ESP Web Tools manifest files

**Files:**
- Create: `docs/flasher/manifest-full.json`
- Create: `docs/flasher/manifest-lite.json`

- [ ] **Step 1: Create Full manifest**

```json
{
  "name": "ScryBar Full",
  "version": "1.0.0",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "firmware/full/bootloader.bin", "offset": 0 },
        { "path": "firmware/full/partitions.bin", "offset": 32768 },
        { "path": "firmware/full/boot_app0.bin", "offset": 57344 },
        { "path": "firmware/full/firmware.bin", "offset": 65536 }
      ]
    }
  ]
}
```

- [ ] **Step 2: Create Lite manifest**

Same structure, paths point to `firmware/lite/`.

```json
{
  "name": "ScryBar Lite",
  "version": "1.0.0",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "firmware/lite/bootloader.bin", "offset": 0 },
        { "path": "firmware/lite/partitions.bin", "offset": 32768 },
        { "path": "firmware/lite/boot_app0.bin", "offset": 57344 },
        { "path": "firmware/lite/firmware.bin", "offset": 65536 }
      ]
    }
  ]
}
```

- [ ] **Step 3: Commit**

```bash
git add docs/flasher/manifest-full.json docs/flasher/manifest-lite.json
git commit -m "feat: add ESP Web Tools manifests for full and lite variants"
```

---

### Task 4: Create the flasher web page

**Files:**
- Create: `docs/flasher/index.html`

- [ ] **Step 1: Write the HTML page**

Single-page static site with:
- ScryBar branding (title, brief description)
- Two install buttons using `<esp-web-install-button>` from ESP Web Tools
- Brief explanation of Full vs Lite
- Post-flash instructions (WiFi Direct setup)
- Minimal inline CSS (dark theme to match ScryBar aesthetic)
- ESP Web Tools loaded from CDN: `https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module`

Key HTML structure:

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ScryBar Firmware Flasher</title>
  <script type="module"
    src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"></script>
  <style>
    /* Dark minimal theme, centered layout, two card buttons */
  </style>
</head>
<body>
  <h1>ScryBar Firmware Flasher</h1>
  <p>Flash ScryBar firmware directly from your browser. Requires Chrome/Edge and a USB cable.</p>

  <div class="cards">
    <div class="card">
      <h2>Full</h2>
      <p>Everything: clock, weather, RSS, Wiki, DOOM.</p>
      <esp-web-install-button manifest="manifest-full.json">
        <button slot="activate">Install Full</button>
      </esp-web-install-button>
    </div>

    <div class="card">
      <h2>Lite</h2>
      <p>Clock, weather, RSS, Wiki. No DOOM (smaller flash footprint).</p>
      <esp-web-install-button manifest="manifest-lite.json">
        <button slot="activate">Install Lite</button>
      </esp-web-install-button>
    </div>
  </div>

  <section class="post-flash">
    <h2>After flashing</h2>
    <ol>
      <li>Device boots and starts a WiFi access point (<code>ScryBar-Setup-XXXX</code>)</li>
      <li>Connect to it from your phone or laptop</li>
      <li>Open <code>http://192.168.4.1:8080</code> to configure WiFi, theme, language, and more</li>
    </ol>
  </section>

  <footer>
    <p>Requires a Chromium-based browser (Chrome, Edge, Opera) with Web Serial support.</p>
  </footer>
</body>
</html>
```

Full CSS should include:
- `body`: dark background (#0a0a0a), light text, centered max-width container, system font stack
- `.cards`: flex row, gap, wrap for mobile
- `.card`: bordered box, padding, flex-grow
- `button`: accent color matching ScryBar default theme, hover state
- `.post-flash`: subtle border-top separator
- Responsive: cards stack vertically on narrow screens

- [ ] **Step 2: Commit**

```bash
git add docs/flasher/index.html
git commit -m "feat: add web flasher page with ESP Web Tools"
```

---

### Task 5: Enable GitHub Pages

The workflow uses `actions/deploy-pages` which requires Pages to be configured for GitHub Actions deployment (not branch-based).

- [ ] **Step 1: Configure GitHub Pages source**

Go to **Settings > Pages > Build and deployment > Source: GitHub Actions**.

Or via CLI:
```bash
gh api repos/enuzzo/scrybar/pages -X PUT \
  -f build_type=workflow \
  || gh api repos/enuzzo/scrybar/pages -X POST \
    -f build_type=workflow
```

The flasher will be available at `https://enuzzo.github.io/scrybar/`.

- [ ] **Step 2: Document in README (optional)**

Add a one-liner link to the flasher in the README, e.g.:

```markdown
## Flash Firmware

Flash ScryBar directly from your browser: [Web Flasher](https://enuzzo.github.io/scrybar/)
```

- [ ] **Step 3: Commit if README was updated**

```bash
git add README.md
git commit -m "docs: add web flasher link to README"
```

---

### Task 6: Test the full pipeline

- [ ] **Step 1: Push tag to trigger CI**

```bash
git push origin feature/web-flasher
```

Once merged to main:
```bash
git tag v0.1.0-flasher
git push origin v0.1.0-flasher
```

- [ ] **Step 2: Verify CI produces binaries**

Check GitHub Actions run completes: build job succeeds, deploy job succeeds, `scrybar-firmware` artifact is downloadable.

- [ ] **Step 3: Verify web flasher page loads**

Open `https://enuzzo.github.io/scrybar/` and confirm:
- Both install buttons render
- Clicking a button prompts for USB serial port (requires Chromium browser)
- Manifest files load without CORS errors

- [ ] **Step 4: Flash a real device**

Connect ScryBar via USB, click "Install Full", verify:
- Flash completes without errors
- Device boots and starts WiFi AP
- Web config is accessible at `http://192.168.4.1:8080`

---

## Notes

- **Binary size**: Full variant is large (~10MB app binary due to DOOM WAD). GitHub Pages has a 1GB site limit, so two variants are fine.
- **No binary bloat in git**: Binaries are built in CI and deployed directly to Pages via `actions/deploy-pages`. They never touch git history.
- **Version tracking**: CI injects the git tag (or `dev-<sha>`) into manifest `version` fields automatically.
- **Library pinning**: ESP32 core and libraries are pinned to specific versions for reproducible builds. Update these when the firmware is tested against newer versions.
- **CORS**: ESP Web Tools fetches manifests and binaries via relative paths from the same origin, so no CORS issues with GitHub Pages.
- **Browser support**: Web Serial API requires Chrome/Edge/Opera 89+. Safari and Firefox are not supported. The page should note this.
- **`new_install_prompt_erase: true`**: Prompts users to erase flash on first install, which clears any stale NVS data. This is the safest default for new users.
