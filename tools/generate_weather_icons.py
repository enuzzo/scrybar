#!/usr/bin/env python3
"""
Generate LVGL8 image header from local PNG weather icons.

Input icons are expected in:
  assets/weather_icons/PNG/

Output header:
  assets/weather_icons/generated/weather_icons_lvgl_local.h

Uses ffmpeg for scaling/padding and RGBA extraction.
"""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import tempfile


ICON_PRESETS = {
    "local": {
        "icon_dir": ("assets", "weather_icons", "PNG"),
        "output": ("assets", "weather_icons", "generated", "weather_icons_lvgl_local.h"),
        "prefix": "image_weather",
        "map": [
            ("sun", "Cute Weather Icon_Sunny.png"),
            ("cloud", "Cute Weather Icon_Cloudy.png"),
            ("rain", "Cute Weather Icon_Rainy.png"),
            ("thunder", "Cute Weather Icon_Thunderstorm.png"),
            ("snow", "Cute Weather Icon_Snowy.png"),
            ("night", "Cute Weather Icon_Partly Cloudy.png"),
        ],
    },
    "minimal": {
        "icon_dir": ("assets", "weather_icons_min", "PNG"),
        "output": ("assets", "weather_icons_min", "generated", "weather_icons_lvgl_min.h"),
        "prefix": "image_weather_min",
        "map": [
            ("sun", "Sun.png"),
            ("cloud", "cloud.png"),
            ("rain", "Rain.png"),
            ("thunder", "thunderstorm.png"),
            ("snow", "cloud weather.png"),
            ("night", "moon.png"),
        ],
    },
}


def run_ffmpeg(src: pathlib.Path, dst_raw: pathlib.Path, size: int) -> None:
    cmd = [
        "ffmpeg",
        "-y",
        "-loglevel",
        "error",
        "-i",
        str(src),
        "-vf",
        (
            f"scale={size}:{size}:force_original_aspect_ratio=decrease,"
            f"pad={size}:{size}:(ow-iw)/2:(oh-ih)/2:color=0x00000000,"
            "format=rgba"
        ),
        "-f",
        "rawvideo",
        "-pix_fmt",
        "rgba",
        str(dst_raw),
    ]
    subprocess.run(cmd, check=True)


def rgba_to_lvgl_true_color_alpha(raw_rgba: bytes, *, byte_swap: bool) -> bytes:
    out = bytearray()
    for i in range(0, len(raw_rgba), 4):
        r = raw_rgba[i]
        g = raw_rgba[i + 1]
        b = raw_rgba[i + 2]
        a = raw_rgba[i + 3]

        # RGB565
        c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        lo = c & 0xFF
        hi = (c >> 8) & 0xFF

        # LV_COLOR_16_SWAP=1 expects swapped byte order in image data as well.
        if byte_swap:
            out.append(hi)
            out.append(lo)
        else:
            out.append(lo)
            out.append(hi)

        out.append(a)
    return bytes(out)


def emit_c_array(name: str, data: bytes) -> list[str]:
    lines: list[str] = []
    lines.append(f"const uint8_t {name}[] = {{")
    row: list[str] = []
    for b in data:
        row.append(f"0x{b:02X}")
        if len(row) == 18:
            lines.append("  " + ", ".join(row) + ",")
            row = []
    if row:
        lines.append("  " + ", ".join(row) + ",")
    lines.append("};")
    lines.append(f"const uint32_t {name}_len = sizeof({name});")
    return lines


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--root",
        type=pathlib.Path,
        default=pathlib.Path(__file__).resolve().parents[1],
        help="Project root directory (contains assets/)",
    )
    parser.add_argument("--size", type=int, default=60, help="Output icon size in pixels")
    parser.add_argument(
        "--preset",
        choices=sorted(ICON_PRESETS.keys()),
        default="local",
        help="Input/output mapping preset.",
    )
    parser.add_argument(
        "--byte-swap",
        action="store_true",
        default=True,
        help="Write RGB565 bytes swapped (for LV_COLOR_16_SWAP=1)",
    )
    args = parser.parse_args()

    project_root = args.root
    preset = ICON_PRESETS[args.preset]
    icon_dir = project_root.joinpath(*preset["icon_dir"])
    out_header = project_root.joinpath(*preset["output"])
    symbol_prefix = preset["prefix"]
    icon_map = preset["map"]
    out_header.parent.mkdir(parents=True, exist_ok=True)

    hdr: list[str] = []
    hdr.append("#pragma once")
    hdr.append("#include <lvgl.h>")
    hdr.append("#include <stdint.h>")
    hdr.append("")

    for symbol, filename in icon_map:
        src = icon_dir / filename
        if not src.exists():
            raise FileNotFoundError(f"Missing icon: {src}")

        raw_path = pathlib.Path(tempfile.gettempdir()) / f"db_{symbol}_{args.size}.rgba"
        run_ffmpeg(src, raw_path, args.size)
        raw = raw_path.read_bytes()
        expected = args.size * args.size * 4
        if len(raw) != expected:
            raise RuntimeError(f"Unexpected RGBA size for {symbol}: {len(raw)} (expected {expected})")

        lv_data = rgba_to_lvgl_true_color_alpha(raw, byte_swap=args.byte_swap)
        array_name = f"{symbol_prefix}_{symbol}_map"
        hdr.extend(emit_c_array(array_name, lv_data))
        hdr.append("")
        hdr.append(f"const lv_img_dsc_t {symbol_prefix}_{symbol} = {{")
        hdr.append("  .header = {")
        hdr.append("    .cf = LV_IMG_CF_TRUE_COLOR_ALPHA,")
        hdr.append("    .always_zero = 0,")
        hdr.append("    .reserved = 0,")
        hdr.append(f"    .w = {args.size},")
        hdr.append(f"    .h = {args.size},")
        hdr.append("  },")
        hdr.append(f"  .data_size = {array_name}_len,")
        hdr.append(f"  .data = {array_name},")
        hdr.append("};")
        hdr.append("")

    out_header.write_text("\n".join(hdr) + "\n", encoding="utf-8")
    print(f"Generated {out_header} ({out_header.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
