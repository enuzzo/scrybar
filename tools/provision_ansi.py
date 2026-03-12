#!/usr/bin/env python3
"""Build a FAT filesystem image with ANSI art files and flash it to the device.

Reads raw .ans files from data/ansi/, builds a FAT image sized to the
device FAT partition (5.9 MB), and flashes it via esptool.

Prerequisites:
    pip3 install esptool

Usage:
    python3 tools/provision_ansi.py [--port /dev/cu.usbmodem14101]

The FAT partition offset and size are read from partitions.csv.
"""

import argparse
import os
import struct
import subprocess
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR   = os.path.join(SCRIPT_DIR, '..')
DATA_DIR   = os.path.join(ROOT_DIR, 'data', 'ansi')

# From partitions.csv: fatfs partition at 0xA10000, size 0x5F0000
FAT_OFFSET = 0xA10000
FAT_SIZE   = 0x5F0000  # 6,160,384 bytes (~5.9 MB)

def find_mkfatfs():
    """Try to locate mkfatfs or fatfsimage tool."""
    # ESP32 Arduino toolchain ships mkfatfs
    for name in ['mkfatfs', 'fatfsimage']:
        try:
            subprocess.run([name, '--help'], capture_output=True)
            return name
        except FileNotFoundError:
            pass
    return None

def build_fatfs_image(output_path, data_dir, size):
    """Build a FAT filesystem image containing files from data_dir."""
    tool = find_mkfatfs()
    if tool:
        subprocess.run([
            tool, '-c', data_dir, '-s', str(size), output_path
        ], check=True)
        return True

    # Fallback: use ESP-IDF fatfsgen.py if available
    fatfsgen = None
    arduino_esp32 = os.path.expanduser('~/Library/Arduino15/packages/esp32/hardware/esp32')
    if os.path.isdir(arduino_esp32):
        for ver in sorted(os.listdir(arduino_esp32), reverse=True):
            candidate = os.path.join(arduino_esp32, ver, 'tools', 'mkfatfs')
            if os.path.isfile(candidate):
                fatfsgen = candidate
                break

    if fatfsgen:
        subprocess.run([
            fatfsgen, '-c', data_dir, '-s', str(size), output_path
        ], check=True)
        return True

    # Last resort: use Python fatfs builder
    print("WARNING: mkfatfs not found. Trying python-based fatfs builder...")
    try:
        from esp_idf_fatfs import FatFS
        fatfs = FatFS(size)
        for fname in sorted(os.listdir(data_dir)):
            fpath = os.path.join(data_dir, fname)
            if os.path.isfile(fpath):
                with open(fpath, 'rb') as f:
                    fatfs.create_file('/' + fname, f.read())
        with open(output_path, 'wb') as f:
            f.write(fatfs.to_binary())
        return True
    except ImportError:
        pass

    print("ERROR: No FAT image builder found. Install mkfatfs or esptool with fatfs support.")
    sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description='Provision ANSI art files to ESP32 FAT partition')
    parser.add_argument('--port', '-p', default=None,
                        help='Serial port (auto-detect if omitted)')
    parser.add_argument('--dry-run', action='store_true',
                        help='Build image only, do not flash')
    args = parser.parse_args()

    if not os.path.isdir(DATA_DIR):
        print(f"ERROR: {DATA_DIR} not found. Run tools/extract_ansi_raw.py first.")
        sys.exit(1)

    ans_files = sorted(f for f in os.listdir(DATA_DIR) if f.endswith('.ans'))
    if not ans_files:
        print(f"ERROR: No .ans files in {DATA_DIR}")
        sys.exit(1)

    total = sum(os.path.getsize(os.path.join(DATA_DIR, f)) for f in ans_files)
    print(f"Found {len(ans_files)} ANSI files ({total:,} bytes)")

    with tempfile.NamedTemporaryFile(suffix='.bin', delete=False) as tmp:
        img_path = tmp.name

    try:
        print(f"Building FAT image ({FAT_SIZE:,} bytes)...")
        build_fatfs_image(img_path, DATA_DIR, FAT_SIZE)
        print(f"Image built: {img_path} ({os.path.getsize(img_path):,} bytes)")

        if args.dry_run:
            print("Dry run — not flashing.")
            return

        port = args.port
        if not port:
            import glob
            ports = glob.glob('/dev/cu.usbmodem*')
            if not ports:
                print("ERROR: No USB serial port found. Specify --port.")
                sys.exit(1)
            port = ports[0]
            print(f"Auto-detected port: {port}")

        print(f"Flashing FAT partition at 0x{FAT_OFFSET:X}...")
        subprocess.run([
            sys.executable, '-m', 'esptool',
            '--chip', 'esp32s3',
            '--port', port,
            '--baud', '921600',
            'write_flash', hex(FAT_OFFSET), img_path
        ], check=True)

        print("Done! ANSI files provisioned to FAT partition.")
    finally:
        if os.path.exists(img_path):
            os.unlink(img_path)

if __name__ == '__main__':
    main()
