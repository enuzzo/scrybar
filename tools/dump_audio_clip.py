#!/usr/bin/env python3
"""
Download the latest recorded ScryBar audio clip as WAV.

Optional test flow:
- trigger GPTREC on serial
- wait for recording completion
- fetch WAV
- compute audio metrics
- append a regression CSV log

Examples:
  python3 tools/dump_audio_clip.py --host <DEVICE_IP> --port 8080
  python3 tools/dump_audio_clip.py --host <DEVICE_IP> --serial-port <SERIAL_PORT>
"""

from __future__ import annotations

import argparse
import datetime as dt
import math
import os
import pathlib
import struct
import sys
import termios
import time
import tty
import urllib.error
import urllib.request
import wave


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


def _trigger_rec(serial_port: str, baud: int, settle_s: float) -> None:
    fd = _open_port(serial_port, baud)
    try:
        if settle_s > 0:
            time.sleep(settle_s)
        os.write(fd, b"GPTREC\n")
    finally:
        os.close(fd)


def _analyze_wav(path: pathlib.Path) -> dict[str, float]:
    with wave.open(str(path), "rb") as w:
        channels = w.getnchannels()
        sampwidth = w.getsampwidth()
        rate = w.getframerate()
        frames = w.getnframes()
        raw = w.readframes(frames)
    if sampwidth != 2:
        raise RuntimeError(f"Unsupported sample width: {sampwidth} bytes")
    vals = struct.unpack("<" + "h" * (len(raw) // 2), raw)
    if channels == 2:
        left = vals[0::2]
        right = vals[1::2]
    else:
        left = vals
        right = ()

    def _stats(arr):
        if not arr:
            return 0.0, 0.0, 0.0
        peak = float(max(abs(x) for x in arr))
        rms = math.sqrt(sum(x * x for x in arr) / len(arr))
        nz = float(sum(1 for x in arr if x != 0)) / float(len(arr))
        return peak, rms, nz

    l_peak, l_rms, l_nz = _stats(left)
    r_peak, r_rms, r_nz = _stats(right)
    return {
        "channels": float(channels),
        "sample_rate": float(rate),
        "frames": float(frames),
        "l_peak": l_peak,
        "l_rms": l_rms,
        "l_nonzero_ratio": l_nz,
        "r_peak": r_peak,
        "r_rms": r_rms,
        "r_nonzero_ratio": r_nz,
    }


def _append_csv(csv_path: pathlib.Path, row: dict[str, float | str]) -> None:
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    header = [
        "timestamp",
        "file",
        "bytes",
        "channels",
        "sample_rate",
        "frames",
        "l_peak",
        "l_rms",
        "l_nonzero_ratio",
        "r_peak",
        "r_rms",
        "r_nonzero_ratio",
    ]
    exists = csv_path.exists()
    with csv_path.open("a", encoding="utf-8") as f:
        if not exists:
            f.write(",".join(header) + "\n")
        vals = [str(row[k]) for k in header]
        f.write(",".join(vals) + "\n")


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Fetch ScryBar audio clip WAV dump")
    p.add_argument("--host", required=True, help="ScryBar IP or hostname")
    p.add_argument("--port", type=int, default=8080, help="Web UI/API port (default: 8080)")
    p.add_argument("--out-dir", default="screenshots", help="Output folder (default: screenshots)")
    p.add_argument("--timeout", type=float, default=12.0, help="HTTP timeout in seconds")
    p.add_argument("--serial-port", default="", help="If set, send GPTREC before fetching")
    p.add_argument("--serial-baud", type=int, default=115200, help="Serial baud for GPTREC")
    p.add_argument("--serial-settle", type=float, default=0.25, help="Delay after opening serial")
    p.add_argument("--rec-wait", type=float, default=5.6, help="Seconds to wait after GPTREC")
    p.add_argument("--log-csv", default="screenshots/audio_regression.csv", help="CSV log path")
    p.add_argument("--no-log", action="store_true", help="Do not append metrics to CSV")
    return p.parse_args()


def main() -> int:
    args = parse_args()
    if args.serial_port:
        _trigger_rec(args.serial_port, args.serial_baud, args.serial_settle)
        if args.rec_wait > 0:
            time.sleep(args.rec_wait)

    out_dir = pathlib.Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    out_file = out_dir / f"scrybar_audio_{stamp}.wav"
    url = f"http://{args.host}:{args.port}/api/audio/clip.wav"

    req = urllib.request.Request(url, method="GET")
    try:
        with urllib.request.urlopen(req, timeout=args.timeout) as resp:
            ctype = resp.headers.get("Content-Type", "")
            body = resp.read()
            if resp.status != 200:
                msg = body.decode("utf-8", errors="replace")
                print(f"[audio-dump] HTTP {resp.status} {msg}", file=sys.stderr)
                return 2
            if "audio/wav" not in ctype and not body.startswith(b"RIFF"):
                print(f"[audio-dump] unexpected content-type: {ctype}", file=sys.stderr)
                return 3
            out_file.write_bytes(body)
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8", errors="replace")
        print(f"[audio-dump] HTTP {e.code} {body}", file=sys.stderr)
        return 2
    except Exception as e:
        print(f"[audio-dump] request failed: {e}", file=sys.stderr)
        return 1

    size_b = out_file.stat().st_size
    metrics = _analyze_wav(out_file)
    print(f"[audio-dump] saved: {out_file} ({size_b} bytes)")
    print(
        "[audio-dump] "
        f"L peak={metrics['l_peak']:.0f} rms={metrics['l_rms']:.1f} nz={metrics['l_nonzero_ratio']:.3f} | "
        f"R peak={metrics['r_peak']:.0f} rms={metrics['r_rms']:.1f} nz={metrics['r_nonzero_ratio']:.3f}"
    )
    if not args.no_log:
        row = {
            "timestamp": dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "file": out_file.name,
            "bytes": str(size_b),
            "channels": f"{metrics['channels']:.0f}",
            "sample_rate": f"{metrics['sample_rate']:.0f}",
            "frames": f"{metrics['frames']:.0f}",
            "l_peak": f"{metrics['l_peak']:.0f}",
            "l_rms": f"{metrics['l_rms']:.2f}",
            "l_nonzero_ratio": f"{metrics['l_nonzero_ratio']:.6f}",
            "r_peak": f"{metrics['r_peak']:.0f}",
            "r_rms": f"{metrics['r_rms']:.2f}",
            "r_nonzero_ratio": f"{metrics['r_nonzero_ratio']:.6f}",
        }
        _append_csv(pathlib.Path(args.log_csv), row)
        print(f"[audio-dump] log: {args.log_csv}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
