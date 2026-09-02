#!/usr/bin/env python3
"""Convert pre-rendered Meteocons PNG frames to an LVGL 8 RGB565+A header."""

from __future__ import annotations

import argparse
import pathlib

from PIL import Image


ICONS = [
    "clear-day",
    "clear-night",
    "mostly-clear-day",
    "mostly-clear-night",
    "partly-cloudy-day",
    "partly-cloudy-night",
    "overcast",
    "fog-day",
    "fog-night",
    "drizzle",
    "rain",
    "extreme-rain",
    "sleet",
    "snow",
    "extreme-snow",
    "thunderstorms-day",
    "thunderstorms-night",
    "thunderstorms-day-hail",
    "thunderstorms-night-hail",
]


def symbol(value: str) -> str:
    return value.replace("-", "_")


def rgba_to_lvgl(data: bytes) -> bytes:
    output = bytearray()
    for offset in range(0, len(data), 4):
        red, green, blue, alpha = data[offset : offset + 4]
        rgb565 = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
        # ScryBar uses LV_COLOR_16_SWAP=1.
        output.extend(((rgb565 >> 8) & 0xFF, rgb565 & 0xFF, alpha))
    return bytes(output)


def emit_array(name: str, data: bytes) -> list[str]:
    lines = [f"const uint8_t {name}[] = {{"]
    for offset in range(0, len(data), 18):
        row = ", ".join(f"0x{byte:02X}" for byte in data[offset : offset + 18])
        lines.append(f"  {row},")
    lines.extend(["};", ""])
    return lines


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
    parser.add_argument("--frames", type=int, default=10)
    parser.add_argument("--size", type=int, default=84)
    parser.add_argument("--forecast-size", type=int, default=30)
    args = parser.parse_args()

    frame_root = args.root / "assets" / "meteocons" / "frames"
    output = args.root / "assets" / "meteocons" / "generated" / "meteocons_lvgl.h"
    output.parent.mkdir(parents=True, exist_ok=True)

    lines = [
        "#pragma once",
        "#include <lvgl.h>",
        "#include <stdint.h>",
        "",
        f"static constexpr uint8_t METEOCON_FRAME_COUNT = {args.frames};",
        f"static constexpr uint8_t METEOCON_SIZE = {args.size};",
        f"static constexpr uint8_t METEOCON_FORECAST_SIZE = {args.forecast_size};",
        "",
        "enum MeteoconIconId : uint8_t {",
    ]
    for icon in ICONS:
        lines.append(f"  METEOCON_{symbol(icon).upper()},")
    lines.extend(["  METEOCON_ICON_COUNT,", "};", ""])

    descriptors: list[list[str]] = []
    forecast_descriptors: list[str] = []
    for icon in ICONS:
        icon_descriptors: list[str] = []
        for frame_index in range(args.frames):
            png_path = frame_root / icon / f"{frame_index:02d}.png"
            if not png_path.exists():
                raise FileNotFoundError(f"Missing rendered frame: {png_path}")
            with Image.open(png_path) as image:
                rgba = image.convert("RGBA")
                if rgba.size != (args.size, args.size):
                    raise RuntimeError(f"Unexpected size for {png_path}: {rgba.size}")
                data = rgba_to_lvgl(rgba.tobytes())
            base = f"meteocon_{symbol(icon)}_{frame_index}"
            lines.extend(emit_array(f"{base}_map", data))
            lines.extend([
                f"const lv_img_dsc_t {base} = {{",
                "  .header = {",
                "    .cf = LV_IMG_CF_TRUE_COLOR_ALPHA,",
                "    .always_zero = 0,",
                "    .reserved = 0,",
                f"    .w = {args.size},",
                f"    .h = {args.size},",
                "  },",
                f"  .data_size = sizeof({base}_map),",
                f"  .data = {base}_map,",
                "};",
                "",
            ])
            icon_descriptors.append(f"&{base}")
        descriptors.append(icon_descriptors)

        forecast_path = frame_root.parent / "forecast" / f"{icon}.png"
        if not forecast_path.exists():
            raise FileNotFoundError(f"Missing rendered forecast icon: {forecast_path}")
        with Image.open(forecast_path) as image:
            rgba = image.convert("RGBA")
            if rgba.size != (args.forecast_size, args.forecast_size):
                raise RuntimeError(f"Unexpected size for {forecast_path}: {rgba.size}")
            data = rgba_to_lvgl(rgba.tobytes())
        forecast_base = f"meteocon_forecast_{symbol(icon)}"
        lines.extend(emit_array(f"{forecast_base}_map", data))
        lines.extend([
            f"const lv_img_dsc_t {forecast_base} = {{",
            "  .header = {",
            "    .cf = LV_IMG_CF_TRUE_COLOR_ALPHA,",
            "    .always_zero = 0,",
            "    .reserved = 0,",
            f"    .w = {args.forecast_size},",
            f"    .h = {args.forecast_size},",
            "  },",
            f"  .data_size = sizeof({forecast_base}_map),",
            f"  .data = {forecast_base}_map,",
            "};",
            "",
        ])
        forecast_descriptors.append(f"&{forecast_base}")

    lines.append("const lv_img_dsc_t* const METEOCON_FRAMES[METEOCON_ICON_COUNT][METEOCON_FRAME_COUNT] = {")
    for icon, icon_descriptors in zip(ICONS, descriptors):
        lines.append(f"  {{ {', '.join(icon_descriptors)} }}, // {icon}")
    lines.extend(["};", ""])
    lines.append("const lv_img_dsc_t* const METEOCON_FORECASTS[METEOCON_ICON_COUNT] = {")
    for icon, forecast_descriptor in zip(ICONS, forecast_descriptors):
        lines.append(f"  {forecast_descriptor}, // {icon}")
    lines.extend(["};", ""])
    output.write_text("\n".join(lines), encoding="utf-8")
    print(f"Generated {output} ({output.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
