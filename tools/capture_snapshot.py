#!/usr/bin/env python3
"""
Capture a screenshot from ScryBar firmware over serial.

Protocol:
- host sends: SNAP\n
- device replies:
  [SNAP][BEGIN] ts=<YYYYmmdd_HHMMSS|nosync> w=<W> h=<H> bytes=<N> fmt=<rgb565le|rgb565be>
  <N raw bytes>
  [SNAP][END] sent=<N>

Output: PNG file in target folder (default: screenshots).
Dependencies: Python stdlib + ffmpeg.
"""

from __future__ import annotations

import argparse
import os
import pathlib
import re
import select
import subprocess
import sys
import tempfile
import termios
import time
import tty


def _baud_const(baud: int):
    name = f"B{baud}"
    if not hasattr(termios, name):
        raise ValueError(f"Unsupported baud rate: {baud}")
    return getattr(termios, name)


def _open_port(path: str, baud: int) -> int:
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    tty.setraw(fd)
    attrs = termios.tcgetattr(fd)
    bconst = _baud_const(baud)
    attrs[4] = bconst
    attrs[5] = bconst
    attrs[2] |= termios.CLOCAL | termios.CREAD
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    termios.tcflush(fd, termios.TCIOFLUSH)
    return fd


def _readline_fd(fd: int, timeout_s: float) -> bytes:
    deadline = time.time() + timeout_s
    out = bytearray()
    while time.time() < deadline:
        remain = max(0.0, deadline - time.time())
        r, _, _ = select.select([fd], [], [], min(0.2, remain))
        if not r:
            continue
        b = os.read(fd, 1)
        if not b:
            continue
        out.extend(b)
        if b == b"\n":
            break
    return bytes(out)


def _read_exact_fd(fd: int, n: int, timeout_s: float) -> bytes:
    deadline = time.time() + timeout_s
    out = bytearray()
    while len(out) < n and time.time() < deadline:
        remain = max(0.0, deadline - time.time())
        r, _, _ = select.select([fd], [], [], min(0.2, remain))
        if not r:
            continue
        chunk = os.read(fd, min(4096, n - len(out)))
        if chunk:
            out.extend(chunk)
    return bytes(out)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True, help="Serial port (e.g. <SERIAL_PORT>)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--out-dir", default="screenshots", help="Output folder for PNG")
    parser.add_argument("--timeout", type=float, default=60.0, help="Timeout in seconds")
    parser.add_argument(
        "--pre-cmd",
        default="",
        help="Optional serial command sent before SNAP (e.g. VIEW1)",
    )
    parser.add_argument(
        "--pre-wait",
        type=float,
        default=1.8,
        help="Seconds to wait after opening serial before sending pre-cmd/SNAP",
    )
    parser.add_argument(
        "--pre-gap",
        type=float,
        default=0.35,
        help="Delay between pre-cmd and SNAP",
    )
    parser.add_argument(
        "--pix-fmt",
        choices=["auto", "rgb565le", "rgb565be"],
        default="auto",
        help="Pixel format decode. 'auto' uses firmware fmt field.",
    )
    args = parser.parse_args()

    out_dir = pathlib.Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    begin_re = re.compile(r"\[SNAP\]\[BEGIN\] ts=([^ ]+) w=(\d+) h=(\d+) bytes=(\d+) fmt=([^\s]+)")

    fd = _open_port(args.port, args.baud)
    try:
        if args.pre_wait > 0:
            time.sleep(args.pre_wait)
        if args.pre_cmd:
            os.write(fd, (args.pre_cmd.strip() + "\n").encode("utf-8"))
            if args.pre_gap > 0:
                time.sleep(args.pre_gap)
        os.write(fd, b"SNAP\n")

        ts = "nosync"
        width = height = total_bytes = 0
        fmt = ""

        deadline = time.time() + args.timeout
        while time.time() < deadline:
            line = _readline_fd(fd, 0.5)
            if not line:
                continue
            text = line.decode("utf-8", errors="ignore").strip()
            if text:
                print(text)
            m = begin_re.search(text)
            if m:
                ts = m.group(1)
                width = int(m.group(2))
                height = int(m.group(3))
                total_bytes = int(m.group(4))
                fmt = m.group(5)
                break

        if total_bytes <= 0 or width <= 0 or height <= 0:
            print("Did not receive snapshot begin header", file=sys.stderr)
            return 3

        raw = _read_exact_fd(fd, total_bytes, args.timeout)
        if len(raw) != total_bytes:
            print(f"Snapshot payload truncated: got {len(raw)} / {total_bytes}", file=sys.stderr)
            return 4

        _ = _readline_fd(fd, 0.5)  # trailing newline
        end_line = _readline_fd(fd, 0.5)
        if end_line:
            print(end_line.decode("utf-8", errors="ignore").strip())
    finally:
        os.close(fd)

    wire_fmt = fmt.lower()
    if args.pix_fmt == "auto":
        if wire_fmt in ("rgb565le", "rgb565be"):
            pix_fmt = wire_fmt
        else:
            print(f"Unsupported format from firmware: {fmt}", file=sys.stderr)
            return 5
    else:
        pix_fmt = args.pix_fmt
        if wire_fmt and wire_fmt != pix_fmt:
            print(f"[WARN] Overriding firmware fmt={wire_fmt} with --pix-fmt={pix_fmt}")

    with tempfile.NamedTemporaryFile(delete=False, suffix=".rgb565") as tmp:
        tmp.write(raw)
        raw_path = pathlib.Path(tmp.name)

    out_name = f"scrybar_{ts}.png" if ts != "nosync" else f"scrybar_{int(time.time())}.png"
    out_png = out_dir / out_name

    cmd = [
        "ffmpeg",
        "-y",
        "-loglevel",
        "error",
        "-f",
        "rawvideo",
        "-pix_fmt",
        pix_fmt,
        "-s",
        f"{width}x{height}",
        "-i",
        str(raw_path),
        str(out_png),
    ]
    subprocess.run(cmd, check=True)
    raw_path.unlink(missing_ok=True)

    print(f"Saved screenshot: {out_png}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
