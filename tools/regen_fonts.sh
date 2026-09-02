#!/usr/bin/env bash
# regen_fonts.sh — Canonical Dosis font generator for ScryBar firmware.
#
# Single source of truth for every scry_font_dosis_*.c file in src/fonts/.
# Re-run this any time the font strategy changes (new size, new charset, etc.)
# so the fleet of .c files stays consistent. NEVER hand-edit the generated files.
#
# Strategy (r286):
#   - Typeface: Dosis, two weights from the official Google Fonts source:
#       * Regular  (wght=400)  from assets/fonts/Dosis-Regular.ttf
#       * SemiBold (wght=600)  from assets/fonts/Dosis-SemiBold.ttf
#     Dosis is licensed under SIL OFL 1.1; the bundled license is kept at
#     assets/fonts/Dosis-OFL-1.1.txt. Both weights share compatible metrics.
#   - Charset for layout fonts (everything except the narrow countdown):
#       Basic Latin          U+0020..U+007E
#       Latin-1 Supplement   U+00A0..U+00FF  (ø, ñ, ç, ü, é, ö, ß, ·, ...)
#       Latin Extended-A     U+0100..U+017F  (ı, ğ, ş, č, ž, ł, ...)
#     This covers every launch/station place name we receive from
#     rocketlaunch.live and Transitous. Cyrillic, Greek, CJK and Arabic are
#     explicitly out of scope (their names arrive romanized).
#   - Countdown font uses a narrow 16-glyph set so the 60px SemiBold file stays
#     small (~50 KB instead of ~700 KB with Latin Extended).
#   - Fallback for every generated font is the size-matched Montserrat already
#     bundled in lv_conf.h. Missing glyphs render at the base font size instead
#     of ballooning to the default 24px — keeps layouts stable.
#
# Usage:
#   tools/regen_fonts.sh                 # regenerate everything
#   tools/regen_fonts.sh regular         # only Regular family
#   tools/regen_fonts.sh semibold        # only SemiBold family
#   tools/regen_fonts.sh countdown       # only the narrow 60px countdown
#
# Requirements:
#   - lv_font_conv  (npm install -g lv_font_conv)
#   - Runs from the project root (not from tools/).

set -euo pipefail

cd "$(dirname "$0")/.."

TTF_REGULAR="assets/fonts/Dosis-Regular.ttf"
TTF_SEMIBOLD="assets/fonts/Dosis-SemiBold.ttf"
OUT_DIR="src/fonts"

# Layout charset — used by every size except the narrow countdown font.
LAYOUT_RANGES=(-r 0x20-0x7E -r 0xA0-0xFF -r 0x100-0x17F)

# Countdown charset — only the glyphs actually used in "T-HH:MM:SS" /
# "T-Dd HH:MM" / "LIFTOFF". Keeps the 60px file around 50 KB.
# Glyphs: space, +, -, 0..9, :, T, d (rest of "LIFTOFF" still falls back).
COUNTDOWN_RANGES=(-r 0x20 -r 0x2B -r 0x2D -r 0x30-0x39 -r 0x3A -r 0x54 -r 0x64)

normalize_output() {
  # lv_font_conv currently leaves multiple blank lines at EOF. Keep generated
  # files reproducible and friendly to `git diff --check`.
  perl -0pi -e 's/\n+\z/\n/' "$1"
}

# Map each generated size to a size-matched lv_font_montserrat_* fallback. The
# list on the right is what lv_conf.h currently enables (14/16/18/20/22/24/
# 28/30/32/36/38). Sizes without a natural match round UP to the nearest
# available, so layouts never shrink unexpectedly on fallback.
fallback_for_size() {
  case "$1" in
    12) echo "lv_font_montserrat_14" ;;
    14) echo "lv_font_montserrat_14" ;;
    16) echo "lv_font_montserrat_16" ;;
    18) echo "lv_font_montserrat_18" ;;
    20) echo "lv_font_montserrat_20" ;;
    22) echo "lv_font_montserrat_22" ;;
    23) echo "lv_font_montserrat_24" ;;
    24) echo "lv_font_montserrat_24" ;;
    25) echo "lv_font_montserrat_24" ;;
    30) echo "lv_font_montserrat_30" ;;
    32) echo "lv_font_montserrat_32" ;;
    38) echo "lv_font_montserrat_38" ;;
    *)  echo "lv_font_montserrat_24" ;;
  esac
}

# Families to generate.
#
# Regular:  all sizes that existed before + the ones used by other pages.
# SemiBold: narrow set — only where typographic emphasis is needed:
#           18 -> provider badges and compact emphasis
#           20 -> primary page-header titles
#           25 -> hero mission name (LAUNCH), Now Playing title
#           32 -> clock, big weather
#           38 -> oversized titles / future headline pages
REGULAR_SIZES=(12 14 16 18 20 22 23 24 25 30 32 38)
SEMIBOLD_SIZES=(18 20 25 32 38)

gen_layout() {
  local weight="$1"   # "Regular" or "SemiBold"
  local size="$2"
  local ttf
  local fontname
  local outfile

  case "$weight" in
    Regular)
      ttf="$TTF_REGULAR"
      fontname="scry_font_dosis_${size}"
      outfile="${OUT_DIR}/scry_font_dosis_${size}.c"
      ;;
    SemiBold)
      ttf="$TTF_SEMIBOLD"
      fontname="scry_font_dosis_semibold_${size}"
      outfile="${OUT_DIR}/scry_font_dosis_semibold_${size}.c"
      ;;
    *)
      echo "unknown weight: $weight" >&2
      exit 1
      ;;
  esac

  local fallback
  fallback=$(fallback_for_size "$size")

  echo ">> ${weight} ${size}px -> ${outfile}  (fallback: ${fallback})"
  lv_font_conv \
    --size "$size" \
    --bpp 4 \
    --no-compress \
    --format lvgl \
    --lv-include lvgl.h \
    --font "$ttf" \
    "${LAYOUT_RANGES[@]}" \
    --lv-fallback "$fallback" \
    --lv-font-name "$fontname" \
    -o "$outfile"
  normalize_output "$outfile"
}

gen_countdown() {
  local outfile="${OUT_DIR}/scry_font_dosis_countdown_60.c"
  echo ">> SemiBold 60px (countdown narrow) -> ${outfile}"
  lv_font_conv \
    --size 60 \
    --bpp 4 \
    --no-compress \
    --format lvgl \
    --lv-include lvgl.h \
    --font "$TTF_SEMIBOLD" \
    "${COUNTDOWN_RANGES[@]}" \
    --lv-fallback lv_font_montserrat_24 \
    --lv-font-name scry_font_dosis_countdown_60 \
    -o "$outfile"
  normalize_output "$outfile"
}

gen_regular_family() {
  for s in "${REGULAR_SIZES[@]}"; do
    gen_layout Regular "$s"
  done
}

gen_semibold_family() {
  for s in "${SEMIBOLD_SIZES[@]}"; do
    gen_layout SemiBold "$s"
  done
}

target="${1:-all}"
case "$target" in
  all)
    gen_regular_family
    gen_semibold_family
    gen_countdown
    ;;
  regular)
    gen_regular_family
    ;;
  semibold)
    gen_semibold_family
    ;;
  countdown)
    gen_countdown
    ;;
  *)
    echo "usage: $0 [all|regular|semibold|countdown]" >&2
    exit 1
    ;;
esac

echo
echo "Done. Re-compile the firmware and check that every LV_FONT_DECLARE in"
echo "scrybar.ino still matches a generated file. If you added/removed a size,"
echo "update REGULAR_SIZES / SEMIBOLD_SIZES at the top of this script."
