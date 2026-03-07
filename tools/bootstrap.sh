#!/usr/bin/env bash
# =============================================================================
# ScryBar — Environment Bootstrap
# =============================================================================
# Installs, checks, and validates everything needed to build and flash
# the ScryBar firmware on macOS.
#
# Usage:
#   chmod +x tools/bootstrap.sh
#   ./tools/bootstrap.sh
#
# Run from the repository root. Safe to re-run — skips things already OK.
# =============================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
RED='\033[0;31m'
GRN='\033[0;32m'
YLW='\033[1;33m'
BLD='\033[1m'
RST='\033[0m'

ok()   { echo -e "  ${GRN}✓${RST}  $*"; }
fail() { echo -e "  ${RED}✗${RST}  $*"; FAILURES=$((FAILURES + 1)); }
info() { echo -e "  ${YLW}→${RST}  $*"; }
hdr()  { echo -e "\n${BLD}$*${RST}"; }

FAILURES=0

hdr "═══════════════════════════════════════════"
hdr " ScryBar Bootstrap"
hdr "═══════════════════════════════════════════"

# ---------------------------------------------------------------------------
# 1. Homebrew
# ---------------------------------------------------------------------------
hdr "1. Homebrew"
if command -v brew &>/dev/null; then
  ok "brew $(brew --version | head -1 | awk '{print $2}')"
else
  info "Installing Homebrew..."
  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
  ok "brew installed"
fi

# ---------------------------------------------------------------------------
# 2. Core CLI tools
# ---------------------------------------------------------------------------
hdr "2. Core CLI tools"

install_brew_if_missing() {
  local pkg="$1"
  local cmd="${2:-$1}"
  if command -v "$cmd" &>/dev/null; then
    ok "$pkg ($(command -v "$cmd"))"
  else
    info "Installing $pkg..."
    brew install "$pkg"
    ok "$pkg installed"
  fi
}

install_brew_if_missing git git
install_brew_if_missing ripgrep rg
install_brew_if_missing ccache ccache
install_brew_if_missing tokei tokei
install_brew_if_missing clang-format clang-format

# ---------------------------------------------------------------------------
# 3. arduino-cli
# ---------------------------------------------------------------------------
hdr "3. arduino-cli"
if command -v arduino-cli &>/dev/null; then
  ok "arduino-cli $(arduino-cli version 2>/dev/null | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)"
else
  info "Installing arduino-cli..."
  brew install arduino-cli
  ok "arduino-cli installed"
fi

# Init config if missing
if [ ! -f "$HOME/.arduino15/arduino-cli.yaml" ]; then
  info "Initialising arduino-cli config..."
  arduino-cli config init
fi

# Ensure ESP32 board URL is registered
ESP32_URL="https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"
CURRENT_URLS=$(arduino-cli config get board_manager.additional_urls 2>/dev/null || echo "")
if echo "$CURRENT_URLS" | grep -q "espressif"; then
  ok "ESP32 board URL registered"
else
  info "Adding ESP32 board URL..."
  arduino-cli config add board_manager.additional_urls "$ESP32_URL"
  ok "ESP32 board URL added"
fi

# ---------------------------------------------------------------------------
# 4. ESP32 Arduino Core
# ---------------------------------------------------------------------------
hdr "4. ESP32 Arduino Core (esp32:esp32)"
arduino-cli core update-index --additional-urls "$ESP32_URL" &>/dev/null

if arduino-cli core list 2>/dev/null | grep -q "esp32:esp32"; then
  CORE_VER=$(arduino-cli core list 2>/dev/null | grep "esp32:esp32" | awk '{print $2}')
  ok "esp32:esp32 $CORE_VER"
else
  info "Installing esp32:esp32 core (this may take a few minutes)..."
  arduino-cli core install esp32:esp32 --additional-urls "$ESP32_URL"
  ok "esp32:esp32 installed"
fi

# ---------------------------------------------------------------------------
# 5. Arduino Libraries
# ---------------------------------------------------------------------------
hdr "5. Arduino Libraries"

install_lib_if_missing() {
  local lib="$1"
  if arduino-cli lib list 2>/dev/null | grep -qi "^${lib}"; then
    ok "\"$lib\" library present"
  else
    info "Installing \"$lib\"..."
    arduino-cli lib install "$lib"
    ok "\"$lib\" installed"
  fi
}

install_lib_if_missing "ArduinoJson"
install_lib_if_missing "lvgl"

# ---------------------------------------------------------------------------
# 6. Python 3 + pip packages (screenshot tool)
# ---------------------------------------------------------------------------
hdr "6. Python 3 + pip packages"

if command -v python3 &>/dev/null; then
  PY_VER=$(python3 --version 2>&1 | awk '{print $2}')
  ok "python3 $PY_VER"
else
  fail "python3 not found — install via 'brew install python3'"
fi

check_pip_pkg() {
  local pkg="$1"
  local import="${2:-$1}"
  if python3 -c "import $import" &>/dev/null; then
    ok "pip: $pkg"
  else
    info "Installing pip package: $pkg..."
    pip3 install "$pkg" --quiet
    ok "pip: $pkg installed"
  fi
}

check_pip_pkg "pyserial" "serial"
check_pip_pkg "Pillow" "PIL"

# ---------------------------------------------------------------------------
# 7. lv_font_conv (optional — only needed for font regeneration)
# ---------------------------------------------------------------------------
hdr "7. lv_font_conv (optional — LVGL font generation)"

if command -v npm &>/dev/null; then
  if npm list -g lv_font_conv &>/dev/null 2>&1; then
    ok "lv_font_conv (global npm)"
  else
    info "lv_font_conv not installed. To install: npm install -g lv_font_conv"
    info "(only needed if regenerating custom LVGL fonts)"
  fi
else
  info "npm not found — lv_font_conv skipped (optional)"
fi

# ---------------------------------------------------------------------------
# 8. Test compile
# ---------------------------------------------------------------------------
hdr "8. Test compile (full build)"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_PATH="/tmp/arduino-build-scrybar"
FQBN="esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi"

# Check secrets.h exists
if [ ! -f "$REPO_ROOT/secrets.h" ]; then
  if [ -f "$REPO_ROOT/secrets.h.example" ]; then
    info "secrets.h not found — copying from secrets.h.example"
    info "Edit $REPO_ROOT/secrets.h with your WiFi/API credentials before flashing."
    cp "$REPO_ROOT/secrets.h.example" "$REPO_ROOT/secrets.h"
    ok "secrets.h created from example"
  else
    fail "secrets.h missing and no secrets.h.example found"
  fi
else
  ok "secrets.h present"
fi

info "Compiling (may take 2-3 min first time)..."
if arduino-cli compile --clean \
    --build-path "$BUILD_PATH" \
    --fqbn "$FQBN" \
    "$REPO_ROOT" &>/tmp/scrybar_build.log; then
  FLASH=$(grep -oE '[0-9]+%' /tmp/scrybar_build.log | grep -v "^100" | tail -1 || echo "?")
  RAM=$(grep -oE '[0-9]+%' /tmp/scrybar_build.log | tail -1 || echo "?")
  ok "Compile OK — Flash: $FLASH  RAM: $RAM"
else
  fail "Compile FAILED — see /tmp/scrybar_build.log"
  tail -20 /tmp/scrybar_build.log | sed 's/^/     /'
fi

# ---------------------------------------------------------------------------
# 9. Device check
# ---------------------------------------------------------------------------
hdr "9. Device (optional)"
PORTS=$(ls /dev/cu.usbmodem* 2>/dev/null || true)
if [ -n "$PORTS" ]; then
  ok "Device found: $PORTS"
  info "To flash:  arduino-cli upload -p $PORTS --fqbn \"$FQBN\" --input-dir $BUILD_PATH ."
  info "To monitor: arduino-cli monitor -p $PORTS --config baudrate=115200"
else
  info "No device connected (plug in via USB-C to flash)"
fi

# ---------------------------------------------------------------------------
# 10. Summary
# ---------------------------------------------------------------------------
hdr "═══════════════════════════════════════════"
if [ "$FAILURES" -eq 0 ]; then
  echo -e " ${GRN}${BLD}ALL CHECKS PASSED — ready to build & flash.${RST}"
else
  echo -e " ${RED}${BLD}$FAILURES issue(s) found — fix the ✗ items above.${RST}"
fi
hdr "═══════════════════════════════════════════"
echo ""
