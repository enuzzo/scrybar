#!/usr/bin/env python3

import argparse
import pathlib
import struct
from typing import Iterable


def read_lumps(wad_bytes: bytes):
    ident, lump_count, dir_offset = struct.unpack_from("<4sII", wad_bytes, 0)
    if ident not in (b"IWAD", b"PWAD"):
        raise ValueError(f"unsupported WAD header: {ident!r}")
    lumps = {}
    for i in range(lump_count):
        entry_off = dir_offset + i * 16
        file_pos, size, raw_name = struct.unpack_from("<II8s", wad_bytes, entry_off)
        name = raw_name.split(b"\0", 1)[0].decode("ascii")
        lumps[name] = wad_bytes[file_pos:file_pos + size]
    return lumps


def decode_patch(patch: bytes):
    width, height, left_off, top_off = struct.unpack_from("<HHhh", patch, 0)
    del left_off, top_off
    column_offsets = struct.unpack_from(f"<{width}I", patch, 8)
    pixels = bytearray(width * height)

    for x, col_off in enumerate(column_offsets):
        offset = col_off
        last_top = -1
        while True:
            top_delta = patch[offset]
            if top_delta == 0xFF:
                break
            post_len = patch[offset + 1]
            offset += 3  # topdelta, length, unused byte

            # Tall patches encode wraparound by increasing top_delta relative to
            # previous posts; TITLEPIC does not need this, but keep it correct.
            if top_delta <= last_top:
                top = last_top + top_delta
            else:
                top = top_delta
            last_top = top

            for y in range(post_len):
                py = top + y
                if 0 <= py < height:
                    pixels[py * width + x] = patch[offset + y]

            offset += post_len + 1  # post data + trailing unused byte

    return width, height, bytes(pixels)


def format_bytes(data: Iterable[int], cols: int = 16) -> str:
    buf = []
    line = []
    for i, value in enumerate(data, start=1):
        line.append(f"0x{value:02X}")
        if i % cols == 0:
            buf.append("  " + ", ".join(line) + ",")
            line = []
    if line:
        buf.append("  " + ", ".join(line) + ",")
    return "\n".join(buf)


def main():
    parser = argparse.ArgumentParser(description="Extract DOOM TITLEPIC + PLAYPAL into a C header.")
    parser.add_argument("wad", type=pathlib.Path, help="Path to doom1.wad")
    parser.add_argument("out", type=pathlib.Path, help="Output header path")
    args = parser.parse_args()

    wad_bytes = args.wad.read_bytes()
    lumps = read_lumps(wad_bytes)
    playpal = lumps.get("PLAYPAL")
    titlepic = lumps.get("TITLEPIC")
    if playpal is None:
        raise ValueError("PLAYPAL lump not found")
    if titlepic is None:
        raise ValueError("TITLEPIC lump not found")
    if len(playpal) < 768:
        raise ValueError("PLAYPAL lump is too small")

    width, height, pixels = decode_patch(titlepic)
    palette = playpal[:768]

    header = f"""#pragma once
// Generated from DOOM shareware IWAD TITLEPIC + PLAYPAL.
// Source donor: ducalex/retro-go prboom-go/components/prboom/data/doom1.wad

#include <stdint.h>
#include <pgmspace.h>

static constexpr uint16_t kDoomTitlePicWidth = {width};
static constexpr uint16_t kDoomTitlePicHeight = {height};

static const uint8_t kDoomTitlePicPalette[768] PROGMEM = {{
{format_bytes(palette)}
}};

static const uint8_t kDoomTitlePicPixels[{len(pixels)}] PROGMEM = {{
{format_bytes(pixels)}
}};
"""

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(header)


if __name__ == "__main__":
    main()
