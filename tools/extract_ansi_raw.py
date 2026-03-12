#!/usr/bin/env python3
"""Extract raw binary .ans files from hex-encoded C header arrays.

Reads each *_ans.h header in src/ansi/, parses the hex byte values,
and writes the raw binary to data/ansi/<name>.ans.

Usage:
    python3 tools/extract_ansi_raw.py
"""

import os
import re
import sys

HEADER_DIR = os.path.join(os.path.dirname(__file__), '..', 'src', 'ansi')
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), '..', 'data', 'ansi')

def extract_bytes_from_header(path):
    """Parse a C header with a uint8_t array and return raw bytes."""
    with open(path, 'r') as f:
        text = f.read()

    # Find the array body between { and };
    m = re.search(r'\{([^}]+)\}', text, re.DOTALL)
    if not m:
        return None

    body = m.group(1)
    hex_values = re.findall(r'0x([0-9a-fA-F]{2})', body)
    return bytes(int(h, 16) for h in hex_values)

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    headers = sorted(f for f in os.listdir(HEADER_DIR) if f.endswith('_ans.h'))
    if not headers:
        print(f"No *_ans.h files found in {HEADER_DIR}")
        sys.exit(1)

    total = 0
    for hdr in headers:
        path = os.path.join(HEADER_DIR, hdr)
        raw = extract_bytes_from_header(path)
        if raw is None:
            print(f"  SKIP {hdr} (no array found)")
            continue

        # Derive output name: f_45_fire_ans.h -> 45-fire.ans
        name = hdr.replace('_ans.h', '')
        # Strip leading f_ prefix if present
        if name.startswith('f_'):
            name = name[2:]
        name = name.replace('_', '-')
        out_path = os.path.join(OUTPUT_DIR, name + '.ans')

        with open(out_path, 'wb') as f:
            f.write(raw)

        total += len(raw)
        print(f"  {hdr:45s} -> {name}.ans  ({len(raw):,} bytes)")

    print(f"\nExtracted {len(headers)} files, {total:,} bytes total -> {OUTPUT_DIR}")

if __name__ == '__main__':
    main()
